/* esp8266_waveform imported from plataforma source código
   Modified for WLED to work around a fallo in the NMI handling,
   which can resultado in the sistema locking up and hard WDT crashes.

   Imported from https://github.com/esp8266/Arduino/blob/7e0d20e2b9034994f573a236364e0aef17fd66de/cores/esp8266/core_esp8266_waveform_phase.cpp
*/


/*
  esp8266_waveform - General purpose waveform generation and control,
                     supporting outputs on all pins in parallel.

  Copyright (c) 2018 Earle F. Philhower, III.  All rights reserved.
  Copyright (c) 2020 Dirk O. Kaar.

  The core idea is to have a programmable waveform generador with a unique
  high and low período (defined in microseconds or CPU clock cycles).  TIMER1 is
  set to 1-shot mode and is always loaded with the time until the next edge
  of any live waveforms.

  Up to one waveform generador per pin supported.

  Each waveform generador is synchronized to the ESP clock cycle counter, not the
  temporizador.  This allows for removing interrupción inestabilidad and retraso as the counter
  always increments once per 80MHz clock.  Changes to a waveform are
  contiguous and only take efecto on the next waveform transición,
  allowing for smooth transitions.

  This replaces older tone(), analogWrite(), and the Servo classes.

  Everywhere in the código where "ccy" or "ccys" is used, it means ESP.getCycleCount()
  clock cycle time, or an intervalo measured in clock cycles, but not TIMER1
  cycles (which may be 2 CPU clock cycles @ 160MHz).

  This biblioteca is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Público
  License as published by the Free Software Foundation; either
  versión 2.1 of the License, or (at your option) any later versión.

  This biblioteca is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Público License for more details.

  You should have received a copy of the GNU Lesser General Público
  License along with this biblioteca; if not, escribir to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Piso, Boston, MA 02110-1301 USA
*/

#include "core_esp8266_waveform.h"
#include <Arduino.h>
#include "debug.h"
#include "ets_sys.h"
#include <atomic>


// ----- @willmmiles begin parche -----
// Linker magic
extern "C" void usePWMFixedNMI(void) {};

// NMI bloqueo workaround
// Sometimes the NMI fails to retorno, stalling the CPU.  When this happens,
// the next NMI gets a retorno address /inside the NMI manejador función/.
// We work around this by caching the last NMI retorno address, and restoring
// the epc3 and eps3 registers to the previous values if the observed epc3
// happens to be pointing to the _NMILevelVector función.
extern "C" void _NMILevelVector();
extern "C" void _UserExceptionVector_1(); // the next function after _NMILevelVector
static inline IRAM_ATTR void nmiCrashWorkaround() {
  static uintptr_t epc3_backup, eps3_backup;

  uintptr_t epc3, eps3;
  __asm__ __volatile__("rsr %0,epc3; rsr %1,eps3":"=a"(epc3),"=a" (eps3));
  if ((epc3 < (uintptr_t) &_NMILevelVector) || (epc3 >= (uintptr_t) &_UserExceptionVector_1)) {
    // Address is good; guardar backup
    epc3_backup = epc3;
    eps3_backup = eps3;
  } else {
    // Address is inside the NMI manejador -- restore from backup
    __asm__ __volatile__("wsr %0,epc3; wsr %1,eps3"::"a"(epc3_backup),"a"(eps3_backup));
  }
}
// ----- @willmmiles end parche -----


// No-op calls to anular the PWM implementación
extern "C" void _setPWMFreq_weak(uint32_t freq) { (void) freq; }
extern "C" IRAM_ATTR bool _stopPWM_weak(int pin) { (void) pin; return false; }
extern "C" bool _setPWM_weak(int pin, uint32_t val, uint32_t range) { (void) pin; (void) val; (void) range; return false; }


// Temporizador is 80MHz fixed. 160MHz CPU frecuencia need scaling.
constexpr bool ISCPUFREQ160MHZ = clockCyclesPerMicrosecond() == 160;
// Máximo retraso between IRQs, Timer1, <= 2^23 / 80MHz
constexpr int32_t MAXIRQTICKSCCYS = microsecondsToClockCycles(10000);
// Máximo servicing time for any single IRQ
constexpr uint32_t ISRTIMEOUTCCYS = microsecondsToClockCycles(18);
// The latencia between in-ISR rearming of the temporizador and the earliest firing
constexpr int32_t IRQLATENCYCCYS = microsecondsToClockCycles(2);
// The SDK and hardware take some time to actually get to our NMI código
constexpr int32_t DELTAIRQCCYS = ISCPUFREQ160MHZ ?
  microsecondsToClockCycles(2) >> 1 : microsecondsToClockCycles(2);

// for INFINITE, the NMI proceeds on the waveform without expiry deadline.
// for EXPIRES, the NMI expires the waveform automatically on the expiry ccy.
// for UPDATEEXPIRY, the NMI recomputes the exact expiry ccy and transitions to EXPIRES.
// for UPDATEPHASE, the NMI recomputes the target timings
// for INIT, the NMI initializes nextPeriodCcy, and if expiryCcy != 0 includes UPDATEEXPIRY.
enum class WaveformMode : uint8_t {INFINITE = 0, EXPIRES = 1, UPDATEEXPIRY = 2, UPDATEPHASE = 3, INIT = 4};

// Waveform generador can crear tones, PWM, and servos
typedef struct {
  uint32_t nextPeriodCcy; // ESP clock cycle when a period begins.
  uint32_t endDutyCcy;    // ESP clock cycle when going from duty to off
  int32_t dutyCcys;       // Set next off cycle at low->high to maintain phase
  int32_t adjDutyCcys;    // Temporary correction for next period
  int32_t periodCcys;     // Set next phase cycle at low->high to maintain phase
  uint32_t expiryCcy;     // For time-limited waveform, the CPU clock cycle when this waveform must stop. If WaveformMode::UPDATE, temporarily holds relative ccy count
  WaveformMode mode;
  bool autoPwm;           // perform PWM duty to idle cycle ratio correction under high load at the expense of precise timings
} Waveform;

namespace {

  static struct {
    Waveform pins[17];             // State of all possible pins
    uint32_t states = 0;           // Is the pin high or low, updated in NMI so no access outside the NMI code
    uint32_t enabled = 0; // Is it actively running, updated in NMI so no access outside the NMI code

    // Habilitar bloqueo-free by only allowing updates to waveform.states and waveform.enabled from IRQ servicio rutina
    int32_t toSetBits = 0;     // Message to the NMI handler to start/modify exactly one waveform
    int32_t toDisableBits = 0; // Message to the NMI handler to disable exactly one pin from waveform generation

    // toSetBits temporaries
    // cheaper than packing them in every Waveform, since we permit only one use at a time
    uint32_t phaseCcy;      // positive phase offset ccy count  
    int8_t alignPhase;      // < 0 no phase alignment, otherwise starts waveform in relative phase offset to given pin

    uint32_t(*timer1CB)() = nullptr;

    bool timer1Running = false;

    uint32_t nextEventCcy;
  } waveform;

}

// Interrupción on/off control
static IRAM_ATTR void timer1Interrupt();

// Non-velocidad critical bits
#pragma GCC optimize ("Os")

static void initTimer() {
  timer1_disable();
  ETS_FRC_TIMER1_INTR_ATTACH(NULL, NULL);
  ETS_FRC_TIMER1_NMI_INTR_ATTACH(timer1Interrupt);
  timer1_enable(TIM_DIV1, TIM_EDGE, TIM_SINGLE);
  waveform.timer1Running = true;
  timer1_write(IRQLATENCYCCYS); // Cause an interrupt post-haste
}

static void IRAM_ATTR deinitTimer() {
  ETS_FRC_TIMER1_NMI_INTR_ATTACH(NULL);
  timer1_disable();
  timer1_isr_init();
  waveform.timer1Running = false;
}

extern "C" {

// Set a devolución de llamada.  Pass in NULO to detener it
void setTimer1Callback_weak(uint32_t (*fn)()) {
  waveform.timer1CB = fn;
  std::atomic_thread_fence(std::memory_order_acq_rel);
  if (!waveform.timer1Running && fn) {
    initTimer();
  } else if (waveform.timer1Running && !fn && !waveform.enabled) {
    deinitTimer();
  }
}

// Iniciar up a waveform on a pin, or change the current one.  Will change to the new
// waveform smoothly on next low->high transición.  For immediate change, stopWaveform()
// first, then it will immediately begin.
int startWaveformClockCycles_weak(uint8_t pin, uint32_t highCcys, uint32_t lowCcys,
  uint32_t runTimeCcys, int8_t alignPhase, uint32_t phaseOffsetCcys, bool autoPwm) {
  uint32_t periodCcys = highCcys + lowCcys;
  if (periodCcys < MAXIRQTICKSCCYS) {
    if (!highCcys) {
      periodCcys = (MAXIRQTICKSCCYS / periodCcys) * periodCcys;
    }
    else if (!lowCcys) {
      highCcys = periodCcys = (MAXIRQTICKSCCYS / periodCcys) * periodCcys;
    }
  }
  // sanity checks, including mixed signed/unsigned arithmetic safety
  if ((pin > 16) || isFlashInterfacePin(pin) || (alignPhase > 16) ||
    static_cast<int32_t>(periodCcys) <= 0 ||
    static_cast<int32_t>(highCcys) < 0 || static_cast<int32_t>(lowCcys) < 0) {
    return false;
  }
  Waveform& wave = waveform.pins[pin];
  wave.dutyCcys = highCcys;
  wave.adjDutyCcys = 0;
  wave.periodCcys = periodCcys;
  wave.autoPwm = autoPwm;
  waveform.alignPhase = (alignPhase < 0) ? -1 : alignPhase;
  waveform.phaseCcy = phaseOffsetCcys;

  std::atomic_thread_fence(std::memory_order_acquire);
  const uint32_t pinBit = 1UL << pin;
  if (!(waveform.enabled & pinBit)) {
    // wave.nextPeriodCcy and wave.endDutyCcy are initialized by the ISR
    wave.expiryCcy = runTimeCcys; // in WaveformMode::INIT, temporarily hold relative cycle count
    wave.mode = WaveformMode::INIT;
    if (!wave.dutyCcys) {
      // If initially at zero duty cycle, force GPIO off
      if (pin == 16) {
        GP16O = 0;
      }
      else {
        GPOC = pinBit;
      }
    }
    std::atomic_thread_fence(std::memory_order_release);
    waveform.toSetBits = 1UL << pin;
    std::atomic_thread_fence(std::memory_order_release);
    if (!waveform.timer1Running) {
      initTimer();
    }
    else if (T1V > IRQLATENCYCCYS) {
      // Must not interfere if Temporizador is due shortly
      timer1_write(IRQLATENCYCCYS);
    }
  }
  else {
    wave.mode = WaveformMode::INFINITE; // turn off possible expiry to make update atomic from NMI
    std::atomic_thread_fence(std::memory_order_release);
    if (runTimeCcys) {
      wave.expiryCcy = runTimeCcys; // in WaveformMode::UPDATEEXPIRY, temporarily hold relative cycle count
      wave.mode = WaveformMode::UPDATEEXPIRY;
      std::atomic_thread_fence(std::memory_order_release);
      waveform.toSetBits = 1UL << pin;
    } else if (alignPhase >= 0) {
      // @willmmiles new feature
      wave.mode = WaveformMode::UPDATEPHASE; // recalculate start
      std::atomic_thread_fence(std::memory_order_release);
      waveform.toSetBits = 1UL << pin;
    }
  }
  std::atomic_thread_fence(std::memory_order_acq_rel);
  while (waveform.toSetBits) {
    esp_yield(); // Wait for waveform to update
    std::atomic_thread_fence(std::memory_order_acquire);
  }
  return true;
}

// Stops a waveform on a pin
IRAM_ATTR int stopWaveform_weak(uint8_t pin) {
  // Can't possibly need to detener anything if there is no temporizador active
  if (!waveform.timer1Running) {
    return false;
  }
  // If usuario sends in a pin >16 but <32, this will always point to a 0 bit
  // If they enviar >=32, then the shift will resultado in 0 and it will also retorno falso
  std::atomic_thread_fence(std::memory_order_acquire);
  const uint32_t pinBit = 1UL << pin;
  if (waveform.enabled & pinBit) {
    waveform.toDisableBits = 1UL << pin;
    std::atomic_thread_fence(std::memory_order_release);
    // Must not interfere if Temporizador is due shortly
    if (T1V > IRQLATENCYCCYS) {
      timer1_write(IRQLATENCYCCYS);
    }
    while (waveform.toDisableBits) {
      /* no-op */ // Can't delay() since stopWaveform may be called from an IRQ
      std::atomic_thread_fence(std::memory_order_acquire);
    }
  }
  if (!waveform.enabled && !waveform.timer1CB) {
    deinitTimer();
  }
  return true;
}

};

// Velocidad critical bits
#pragma GCC optimize ("O2")

// For dynamic CPU clock frecuencia conmutador in bucle the scaling logic would have to be adapted.
// Usando constexpr makes sure that the CPU clock frecuencia is compile-time fixed.
static inline IRAM_ATTR int32_t scaleCcys(const int32_t ccys, const bool isCPU2X) {
  if (ISCPUFREQ160MHZ) {
    return isCPU2X ? ccys : (ccys >> 1);
  }
  else {
    return isCPU2X ? (ccys << 1) : ccys;
  }
}

static IRAM_ATTR void timer1Interrupt() {
  const uint32_t isrStartCcy = ESP.getCycleCount();
  //int32_t clockDrift = isrStartCcy - waveform.nextEventCcy;

  // ----- @willmmiles begin parche -----
  nmiCrashWorkaround();
  // ----- @willmmiles end parche -----

  const bool isCPU2X = CPU2X & 1;
  if ((waveform.toSetBits && !(waveform.enabled & waveform.toSetBits)) || waveform.toDisableBits) {
    // Handle habilitar/deshabilitar requests from principal app.
    waveform.enabled = (waveform.enabled & ~waveform.toDisableBits) | waveform.toSetBits; // Set the requested waveforms on/off
    // Encontrar the first GPIO being generated by checking GCC's encontrar-first-set (returns 1 + the bit of the first 1 in an int32_t)
    waveform.toDisableBits = 0;
  }

  if (waveform.toSetBits) {
    const int toSetPin = __builtin_ffs(waveform.toSetBits) - 1;
    Waveform& wave = waveform.pins[toSetPin];
    switch (wave.mode) {
    case WaveformMode::INIT:
      waveform.states &= ~waveform.toSetBits; // Clear the state of any just started
      if (waveform.alignPhase >= 0 && waveform.enabled & (1UL << waveform.alignPhase)) {
        wave.nextPeriodCcy = waveform.pins[waveform.alignPhase].nextPeriodCcy + scaleCcys(waveform.phaseCcy, isCPU2X);
      }
      else {
        wave.nextPeriodCcy = waveform.nextEventCcy;
      }
      if (!wave.expiryCcy) {
        wave.mode = WaveformMode::INFINITE;
        break;
      }
      // fall through
    case WaveformMode::UPDATEEXPIRY:
      // in WaveformMode::UPDATEEXPIRY, expiryCcy temporarily holds relative CPU cycle conteo
      wave.expiryCcy = wave.nextPeriodCcy + scaleCcys(wave.expiryCcy, isCPU2X);
      wave.mode = WaveformMode::EXPIRES;
      break;
    // @willmmiles new feature
    case WaveformMode::UPDATEPHASE:
      // in WaveformMode::UPDATEPHASE, we recalculate the targets
      if ((waveform.alignPhase >= 0) && (waveform.enabled & (1UL << waveform.alignPhase))) {
        // Compute phase shift to realign with target
        auto const newPeriodCcy = waveform.pins[waveform.alignPhase].nextPeriodCcy + scaleCcys(waveform.phaseCcy, isCPU2X);
        auto const period = scaleCcys(wave.periodCcys, isCPU2X);
        auto shift = ((static_cast<int32_t> (newPeriodCcy - wave.nextPeriodCcy) + period/2) % period) - (period/2);
        wave.nextPeriodCcy += static_cast<uint32_t>(shift);
        if (static_cast<int32_t>(wave.endDutyCcy - wave.nextPeriodCcy) > 0) {
          wave.endDutyCcy = wave.nextPeriodCcy;
        }
      }
    default:
      break;
    }
    waveform.toSetBits = 0;
  }

  // Salida the bucle if the next evento, if any, is sufficiently distant.
  const uint32_t isrTimeoutCcy = isrStartCcy + ISRTIMEOUTCCYS;
  uint32_t busyPins = waveform.enabled;
  waveform.nextEventCcy = isrStartCcy + MAXIRQTICKSCCYS;

  uint32_t now = ESP.getCycleCount();
  uint32_t isrNextEventCcy = now;
  while (busyPins) {
    if (static_cast<int32_t>(isrNextEventCcy - now) > IRQLATENCYCCYS) {
      waveform.nextEventCcy = isrNextEventCcy;
      break;
    }
    isrNextEventCcy = waveform.nextEventCcy;
    uint32_t loopPins = busyPins;
    while (loopPins) {
      const int pin = __builtin_ffsl(loopPins) - 1;
      const uint32_t pinBit = 1UL << pin;
      loopPins ^= pinBit;

      Waveform& wave = waveform.pins[pin];

/* @willmmiles - wtf?  We don't want to accumulate drift
      if (clockDrift) {
        wave.endDutyCcy += clockDrift;
        wave.nextPeriodCcy += clockDrift;
        wave.expiryCcy += clockDrift;
      }
*/          

      uint32_t waveNextEventCcy = (waveform.states & pinBit) ? wave.endDutyCcy : wave.nextPeriodCcy;
      if (WaveformMode::EXPIRES == wave.mode &&
        static_cast<int32_t>(waveNextEventCcy - wave.expiryCcy) >= 0 &&
        static_cast<int32_t>(now - wave.expiryCcy) >= 0) {
        // Deshabilitar any waveforms that are done
        waveform.enabled ^= pinBit;
        busyPins ^= pinBit;
      }
      else {
        const int32_t overshootCcys = now - waveNextEventCcy;
        if (overshootCcys >= 0) {
          const int32_t periodCcys = scaleCcys(wave.periodCcys, isCPU2X);
          if (waveform.states & pinBit) {
            // active configuration and forward are 100% duty
            if (wave.periodCcys == wave.dutyCcys) {
              wave.nextPeriodCcy += periodCcys;
              wave.endDutyCcy = wave.nextPeriodCcy;
            }
            else {
              if (wave.autoPwm) {
                wave.adjDutyCcys += overshootCcys;
              }
              waveform.states ^= pinBit;
              if (16 == pin) {
                GP16O = 0;
              }
              else {
                GPOC = pinBit;
              }
            }
            waveNextEventCcy = wave.nextPeriodCcy;
          }
          else {
            wave.nextPeriodCcy += periodCcys;
            if (!wave.dutyCcys) {
              wave.endDutyCcy = wave.nextPeriodCcy;
            }
            else {
              int32_t dutyCcys = scaleCcys(wave.dutyCcys, isCPU2X);
              if (dutyCcys <= wave.adjDutyCcys) {
                dutyCcys >>= 1;
                wave.adjDutyCcys -= dutyCcys;
              }
              else if (wave.adjDutyCcys) {
                dutyCcys -= wave.adjDutyCcys;
                wave.adjDutyCcys = 0;
              }
              wave.endDutyCcy = now + dutyCcys;
              if (static_cast<int32_t>(wave.endDutyCcy - wave.nextPeriodCcy) > 0) {
                wave.endDutyCcy = wave.nextPeriodCcy;
              }
              waveform.states |= pinBit;
              if (16 == pin) {
                GP16O = 1;
              }
              else {
                GPOS = pinBit;
              }
            }
            waveNextEventCcy = wave.endDutyCcy;
          }

          if (WaveformMode::EXPIRES == wave.mode && static_cast<int32_t>(waveNextEventCcy - wave.expiryCcy) > 0) {
            waveNextEventCcy = wave.expiryCcy;
          }
        }

        if (static_cast<int32_t>(waveNextEventCcy - isrTimeoutCcy) >= 0) {
          busyPins ^= pinBit;
          if (static_cast<int32_t>(waveform.nextEventCcy - waveNextEventCcy) > 0) {
            waveform.nextEventCcy = waveNextEventCcy;
          }
        }
        else if (static_cast<int32_t>(isrNextEventCcy - waveNextEventCcy) > 0) {
          isrNextEventCcy = waveNextEventCcy;
        }
      }
      now = ESP.getCycleCount();
    }
    //clockDrift = 0;
  }

  int32_t callbackCcys = 0;
  if (waveform.timer1CB) {
    callbackCcys = scaleCcys(waveform.timer1CB(), isCPU2X);
  }
  now = ESP.getCycleCount();
  int32_t nextEventCcys = waveform.nextEventCcy - now;
  // Account for unknown duración of timer1CB().
  if (waveform.timer1CB && nextEventCcys > callbackCcys) {
    waveform.nextEventCcy = now + callbackCcys;
    nextEventCcys = callbackCcys;
  }

  // Temporizador is 80MHz fixed. 160MHz CPU frecuencia need scaling.
  int32_t deltaIrqCcys = DELTAIRQCCYS;
  int32_t irqLatencyCcys = IRQLATENCYCCYS;
  if (isCPU2X) {
    nextEventCcys >>= 1;
    deltaIrqCcys >>= 1;
    irqLatencyCcys >>= 1;
  }

  // Firing temporizador too soon, the NMI occurs before ISR has returned.
  if (nextEventCcys < irqLatencyCcys + deltaIrqCcys) {
    waveform.nextEventCcy = now + IRQLATENCYCCYS + DELTAIRQCCYS;
    nextEventCcys = irqLatencyCcys;
  }
  else {
    nextEventCcys -= deltaIrqCcys;
  }

  // Register acceso is fast and edge IRQ was configured before.
  T1L = nextEventCcys;
}
