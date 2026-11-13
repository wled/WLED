#pragma once
#ifndef WLED_POWER_ANIMATION_H
#define WLED_POWER_ANIMATION_H

#include <stdint.h>

/*
 * Optional per-pixel power on/off animation, applied to the output in WS2812FX::show().
 *
 * ON:  a soft front sweeps left to right (0..ON_SWEEP_MS) while the lit part fades up to ~90%,
 *      then all pixels rise together to full brightness (ON_MS).
 * OFF: the whole strip fades out while a soft edge sweeps right to left; black at OFF_MS.
 * Widths are fractions of the strip (on a 78 pixel strip: 12 px trail narrowing to 0.25 px, 3 px OFF edge).
 *
 * Each new phase starts from the levels last sent to the LEDs, so reversing mid-sweep is continuous
 * (ON never dims a pixel, OFF never brightens one). Requests for the state already targeted are ignored.
 *
 * request() only stores the target and may be called from any task. Everything else runs in the
 * task that calls WS2812FX::show(). Positions use the logical pixel index; on a matrix the column is used.
 * All per-frame math is integer (Q16), so boards without an FPU are not slowed down.
 */
class PowerAnimation {
  public:
    static constexpr uint32_t ON_SWEEP_MS = 2000;
    static constexpr uint32_t ON_MS       = 2500;
    static constexpr uint32_t OFF_MS      = 2000;
    static constexpr uint8_t  ON_PLATEAU  = 230; // ~90% at the end of the ON sweep

    // buffer must hold 2 * count bytes and stay valid until detach()
    // shownOn: whether the LEDs currently show the "on" state; targetOn: the requested state
    void attach(uint8_t *buffer, uint16_t count, uint16_t columns, bool shownOn, bool targetOn) {
      _last = buffer;
      _seed = buffer + count;
      _count = count;
      _columns = (columns && columns <= count) ? columns : count;
      _halfPitch = (1UL << 30) / _columns;
      _on = shownOn;
      _target = targetOn;
      _active = false;
      for (unsigned i = 0; i < count; i++) _last[i] = shownOn ? 255 : 0;
    }
    void detach()                     { _last = _seed = nullptr; _count = 0; _active = false; }
    bool isAttached() const           { return _last != nullptr; }
    uint16_t count() const            { return _count; }
    void request(bool on)             { _target = on; }
    bool target() const               { return _target; }
    bool isRunning() const            { return _last && (_active || _target != _on); }
    uint8_t sentLevel(unsigned i) const { return (_last && i < _count) ? _last[i] : 255; } // for live previews

    // prepares a frame at time now (ms); returns false if output passes through unchanged
    bool beginFrame(uint32_t now) {
      if (!_last) return false;
      const bool target = _target;
      if (target != _on) start(target, now);
      if (!_active) { _frame = idleFrame(); return !_on; }
      uint32_t t = now - _start + _offset;
      const uint32_t end = _on ? ON_MS : OFF_MS;
      if (t >= end) { t = end; _active = false; }
      _frame = frameAt(_on, t);
      return true;
    }

    // level (0-255) for pixel i in the frame prepared by beginFrame(); also records it as sent
    uint8_t level(unsigned i) {
      if (i >= _count) return 255;
      const uint8_t shape = shapeLevel(_frame, position(i));
      const uint8_t seed = _seed[i];
      const uint8_t out = _frame.on ? (shape > seed ? shape : seed) : (shape < seed ? shape : seed);
      _last[i] = out;
      return out;
    }

  private:
    static constexpr int32_t  ONE = 65536;           // Q16 1.0; also the right end of the strip
    static constexpr int32_t  ON_GAIN_START = 7864;  // 12% of the plateau at the very start
    static constexpr int32_t  PLATEAU_Q16 = (ON_PLATEAU * ONE + 127) / 255;
    static constexpr int32_t  ON_TRAIL = 10082;      // 0.1538 (12/78) of the strip at the start of the sweep
    static constexpr int32_t  ON_FRONT = 210;        // 0.0032 (0.25/78): the front's minimum width
    static constexpr int32_t  ON_TAPER = 42598;      // trail narrows from 65% of the sweep ...
    static constexpr int32_t  ON_TAPER_LEN = 22938;  // ... over the remaining 35%
    static constexpr int32_t  OFF_EDGE = 2521;       // 0.0385 (3/78) of the strip

    struct Frame {
      bool on = false;
      int32_t uniform = -1; // 0-255 for all pixels, or -1 for the per-pixel shape below
      int32_t edge = 0;     // Q16 front (ON) or edge (OFF) position
      int32_t width = 1;    // Q16 ramp width
      uint32_t inverse = 0; // 2^32 / width
      int32_t gain = 0;     // Q16 level of fully lit pixels
    };

    uint8_t *_last = nullptr;      // levels last sent to the LEDs
    uint8_t *_seed = nullptr;      // levels at the start of the current phase
    uint16_t _count = 0, _columns = 1;
    uint32_t _halfPitch = 0;       // 2^30 / columns
    volatile bool _target = false; // latest requested state
    bool _on = false;              // state of the current phase
    bool _active = false;          // current phase still animating
    uint32_t _start = 0;           // time of the first frame of the phase
    uint32_t _offset = 0;          // phase time at that first frame (reversals start part way)
    Frame _frame;                  // prepared by beginFrame()

    // quintic smoothstep 6x^5 - 15x^4 + 10x^3, Q16 in and out
    static int32_t smooth(int32_t x) {
      if (x <= 0) return 0;
      if (x >= ONE) return ONE;
      const uint32_t x2 = (uint32_t(x) * x) >> 16, x3 = (x2 * x) >> 16, x4 = (x3 * x) >> 16, x5 = (x4 * x) >> 16;
      const int32_t s = 10 * int32_t(x3) - 15 * int32_t(x4) + 6 * int32_t(x5);
      return s < 0 ? 0 : (s > ONE ? ONE : s);
    }
    static uint8_t toByte(int32_t q16) { return q16 <= 0 ? 0 : (q16 >= ONE ? 255 : uint8_t((q16 * 255 + 32768) >> 16)); }
    Frame idleFrame() const { Frame f; f.on = _on; f.uniform = _on ? 255 : 0; return f; }

    // shape parameters at phase time t
    static Frame frameAt(bool on, uint32_t t) {
      Frame f;
      f.on = on;
      if (on) {
        if (t >= ON_MS) { f.uniform = 255; return f; }
        if (t >= ON_SWEEP_MS) {
          f.uniform = ON_PLATEAU + (((255 - ON_PLATEAU) * smooth(int32_t((t - ON_SWEEP_MS) * ONE / (ON_MS - ON_SWEEP_MS))) + 32768) >> 16);
          return f;
        }
        const int32_t p = int32_t(t * ONE / ON_SWEEP_MS);
        f.edge = p;
        f.width = int32_t((int64_t(ON_TRAIL) * (ONE - smooth(int32_t(int64_t(p - ON_TAPER) * ONE / ON_TAPER_LEN)))) >> 16) + ON_FRONT;
        f.gain = (PLATEAU_Q16 * (ON_GAIN_START + ((int64_t(ONE - ON_GAIN_START) * smooth(p)) >> 16))) >> 16;
      } else {
        if (t >= OFF_MS) { f.uniform = 0; return f; }
        const int32_t remaining = ONE - int32_t(t * ONE / OFF_MS);
        f.edge = int32_t((int64_t(ONE + OFF_EDGE) * remaining) >> 16);
        f.width = OFF_EDGE;
        f.gain = remaining;
      }
      f.inverse = uint32_t((uint64_t(1) << 32) / uint32_t(f.width));
      return f;
    }
    static uint8_t shapeLevel(const Frame &f, int32_t pos) {
      if (f.uniform >= 0) return f.uniform;
      const int32_t d = f.edge - pos;
      if (d <= 0) return 0;
      const int32_t ramp = d >= f.width ? ONE : smooth(int32_t((uint32_t(d) * f.inverse) >> 16)); // d < width: no overflow
      return toByte((int64_t(f.gain) * ramp) >> 16);
    }

    // Q16 centre position of pixel i (left = 0, right = ONE)
    int32_t position(unsigned i) const {
      if (i >= _columns) i %= _columns;
      return ((2 * i + 1) * _halfPitch) >> 15;
    }

    // start a phase from the last sent levels: the offset is the latest phase time at which the new
    // shape is still entirely at or below (ON) / at or above (OFF) those levels (found by bisection,
    // the shape is monotonic in time for every pixel)
    void start(bool on, uint32_t now) {
      _on = on;
      _start = now;
      const uint32_t end = on ? ON_MS : OFF_MS;
      uint32_t offset = end;
      bool fromRest = true; // all pixels fully off (ON) or fully on (OFF): play the whole animation
      for (unsigned i = 0; i < _count; i++) {
        const uint8_t seed = _seed[i] = _last[i];
        if (seed != (on ? 0 : 255)) fromRest = false;
      }
      if (!fromRest) {
        for (unsigned i = 0; i < _count; i++) {
          const uint8_t seed = _seed[i];
          const int32_t pos = position(i);
          uint32_t lo = 0, hi = offset; // invariant: shape at lo keeps continuity
          const auto fits = [&](uint32_t t) { const uint8_t s = shapeLevel(frameAt(on, t), pos); return on ? s <= seed : s >= seed; };
          if (fits(hi)) continue;
          while (hi - lo > 1) {
            const uint32_t mid = (lo + hi) / 2;
            if (fits(mid)) lo = mid; else hi = mid;
          }
          offset = lo;
        }
      }
      _offset = fromRest ? 0 : offset;
      _active = true;
    }
};

#endif
