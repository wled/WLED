# MSGEQ7 Usermod

A drop-in replacement for the **audioreactive** usermod that uses a software
implementation of the classic MSGEQ7 seven-band graphic equalizer IC, with
optional support for a physical MSGEQ7 chip.

Produces the identical `um_data_t` 8-slot structure as audioreactive, so all
existing WLED audio-reactive effects work without modification.

---

## Backends

### Software emulation (default)

Captures audio via an I2S or ADC microphone (same hardware as audioreactive),
then runs the samples through **seven second-order IIR bandpass filters** at the
MSGEQ7 center frequencies:

| Band | Center frequency |
|------|-----------------|
| 0 | 63 Hz |
| 1 | 160 Hz |
| 2 | 400 Hz |
| 3 | 1 000 Hz |
| 4 | 2 500 Hz |
| 5 | 6 250 Hz |
| 6 | 16 000 Hz |

The filter outputs are peak-hold envelope-detected, log-compressed to match the
real chip's output characteristic, and interpolated to the 16-channel
`fftResult[]` array expected by WLED effects.

**Sample rate: 44 100 Hz** (required for the 16 kHz band).

### Hardware chip

Reads a physical MSGEQ7 IC via three GPIO pins using the standard
strobe/reset/OUT protocol. The chip already outputs log-compressed analog
voltages, so no software compression is applied in this path.

---

## Activation

Add to your `platformio_override.ini` (do **not** activate alongside
`audioreactive` — both register as `USERMOD_ID_AUDIOREACTIVE`):

```ini
[env:esp32dev]
custom_usermods = msgeq7
```

---

## Hardware wiring

### Software backend — I2S microphone (INMP441 example)

```
INMP441    ESP32
-------    -----
VDD   →   3.3V
GND   →   GND
L/R   →   GND  (selects left channel)
WS    →   GPIO 15  (pinWS)
SCK   →   GPIO 14  (pinSCK)
SD    →   GPIO 32  (pinSD)
```

Configure the matching pins in the WLED settings page under **MSGEQ7**.

### Hardware chip backend

```
MSGEQ7     ESP32
-------    -----
VDD   →   5V (or 3.3V with level shifter on STROBE/RESET)
GND   →   GND
STROBE →  GPIO 33  (pinStrobe, digital output)
RESET  →  GPIO 27  (pinReset, digital output)
OUT    →  GPIO 34  (pinOut, ADC1 input only — ADC2 conflicts with WiFi)
AUDIO IN → audio source via coupling capacitor (see MSGEQ7 datasheet)
```

> **Important**: `pinOut` must be on **ADC1** (GPIO 32–39 on classic ESP32).
> ADC2 pins are unusable while WiFi is active.

---

## Settings

| Setting | Description | Default |
|---------|-------------|---------|
| enabled | Enable/disable the usermod | off |
| useHwChip | Use physical MSGEQ7 chip instead of software emulation | off |
| dmType | Microphone type (software backend only) | 1 (Generic I2S) |
| pinSD / pinWS / pinSCK / pinMCLK | I2S pins | unset |
| pinStrobe / pinReset / pinOut | Hardware chip pins | unset |
| gain | Input amplification (128 = unity) | 128 |
| squelch | Noise floor on 0..255 output scale — signals below this are zeroed | 10 |
| filterQ | Biquad filter Q factor (software backend only) | 1.0 |
| attackMs | Envelope attack time in ms | 15 |
| decayMs | Envelope decay time in ms | 80 |

### dmType values (software backend)

| Value | Microphone |
|-------|-----------|
| 0 | ADC analog (classic ESP32 only) |
| 1 | Generic I2S digital (INMP441, ICS-43434, etc.) |
| 2 | ES7243 |
| 3 | SPH0645 |
| 4 | Generic I2S with master clock |
| 5 | PDM |
| 6 | ES8388 (LyraT / AudioKit) |

---

## Comparison with audioreactive

| Feature | audioreactive | msgeq7 |
|---------|--------------|--------|
| Frequency resolution | 256 FFT bins | 7 bands + parabolic interp |
| External lib dep | arduinoFFT 2.x | **none** |
| RAM (filter buffers) | ~4 KB | ~700 B |
| UDP audio sync | yes | no |
| AGC | PI controller | manual gain slider |
| Dynamic palettes | yes (3 palettes) | no |
| Hardware chip support | no | yes |

### Effects with degraded output

- **Rocktaves** — octave detection relies on FFT bin resolution; with 7 bands
  the octave separation is coarse and the effect behaviour changes noticeably.
- **FFT_MajorPeak-dependent effects** (Freqmap, Waterfall, Freqwave, Gravfreq,
  Ripplepeak) — parabolic interpolation is applied to smooth between bands, but
  resolution is lower than full FFT. The effect still works; transitions between
  frequency regions are less granular.

All GEQ/bar-chart effects (2D GEQ, DJ Light, Funky Plank, Blurz, Particle GEQ,
PS GEQ, Noisemove) and volume-driven effects work at full quality.

---

## Validation — sine sweep test

Enable debug output by adding `-D SR_DEBUG` to your build flags, then run the
included sweep analysis script after capturing serial output:

```bash
# Capture serial output while a sine sweep plays through the microphone
# (use minicom, screen, or PlatformIO's monitor command)
pio device monitor -e esp32dev > sweep_log.txt

# Analyse the captured log
python3 usermods/msgeq7/tools/sweep_analyze.py sweep_log.txt
```

The script plots band amplitudes vs. time and highlights the expected peak
sequence (band 0 first, band 6 last) for a low-to-high frequency sweep.

---

## Source layout

```
usermods/msgeq7/
  msgeq7.cpp          — WLED usermod glue: AudioSource init, PinManager,
                        um_data registration, JSON config, registration
  msgeq7_engine.h     — Self-contained DSP engine: constants, shared state,
                        biquad filter bank, FreeRTOS SW processing task,
                        physical chip GPIO protocol
  audio_source.h      — Copied verbatim from usermods/audioreactive/ (do not edit)
  library.json        — PlatformIO library manifest
```

The engine header is designed to be included from `msgeq7.cpp` only. If the
PoC proves successful and this code is merged into audioreactive, `msgeq7_engine.h`
is the portable piece — it depends only on `wled.h` and `audio_source.h`.

---

## Source attribution

Biquad bandpass filter mathematics follow the Audio EQ Cookbook by Robert
Bristow-Johnson: https://www.w3.org/TR/audio-eq-cookbook/

MSGEQ7 center frequencies and strobe/reset protocol from the MSGEQ7 datasheet
(Mixed Signal Integration, Inc.).

Filter processing uses the ESP-IDF `esp-dsp` library
(`dsps_biquad_f32_ae32`/`_aes3`), which is included with the ESP32 Arduino
framework and requires no additional library dependency.
