/*
 * WLED Arduino IDE compatibility file.
 * 
 * Where has everything gone?
 * 
 * In April 2020, the project's structure underwent a major change. 
 * Global variables are now found in file "wled.h"
 * Global function declarations are found in "fcn_declare.h"
 * 
 * Usermod compatibility: Existing wled06_usermod.ino mods should continue to work. Delete usermod.cpp.
 * New usermods should use usermod.cpp instead.
 */

#ifdef WLED_DEBUG_HEAP
void heap_caps_alloc_failed_hook(size_t requested_size, uint32_t caps, const char *function_name)
{
  Serial.printf("*** %s failed to allocate %d bytes with ",function_name, requested_size);
  if (caps & (1 <<  0)) Serial.print("Executable ");
  if (caps & (1 <<  1)) Serial.print("32-Bit_Aligned ");
  if (caps & (1 <<  2)) Serial.print("8-Bit_Aligned ");
  if (caps & (1 <<  3)) Serial.print("DMA ");
  if (caps & (1 << 10)) Serial.print("SPI_RAM ");
  if (caps & (1 << 11)) Serial.print("Internal ");
  if (caps & (1 << 12)) Serial.print("Default ");
  if (caps & (1 << 13)) Serial.print("IRAM+unaligned ");
  if (caps & (1 << 14)) Serial.print("Retention_DMA ");
  if (caps & (1 << 15)) Serial.print("RTC_fast ");
  Serial.print("capabilities - largest free block: "+String(heap_caps_get_largest_free_block(caps)));

  size_t largest_free = heap_caps_get_largest_free_block(caps);
  size_t total_free   = heap_caps_get_free_size(caps);
  float fragmentation = 100.0f;
  if ((largest_free > 1) && (total_free > largest_free))
    fragmentation = 100.f * (1.0f - (float(largest_free) / float(total_free)) );
  Serial.print("; \t available: " + String(total_free));
  Serial.print(" (frag "); Serial.print(fragmentation, 2); Serial.println("%).");

  if (!heap_caps_check_integrity_all(false)) {
    Serial.println("*** Heap CORRUPTED: "+String(heap_caps_check_integrity_all(true)));
  }
}

#if 0  // softhack007 did not get this hook to work
void esp_heap_trace_free_hook(void* ptr)
{
  if (ptr == nullptr) {
    Serial.println("** free: attempt to free nullptr."); 
  } else {
    size_t blocksize = heap_caps_get_allocated_size(ptr);
    if ((blocksize < 1) || (blocksize > 256000))
      Serial.println("**** free: bad pointer to " + String(blocksize) + "bytes.");
    else
      Serial.println("** free " + String(blocksize) + "bytes.");
  }
}
#endif
#endif

#include "wled.h"

unsigned long lastMillis = 0; //WLEDMM
unsigned long loopCounter = 0; //WLEDMM

unsigned long lps = 0; // loops per second
//unsigned long lps2 = 0; // lps without "show"
//unsigned long long showtime = 0; // time spent in "show" (micros)

void setup() __attribute__((used)); // needed for -flto
void setup() {
  #ifdef WLED_DEBUG_HEAP
  esp_err_t error = heap_caps_register_failed_alloc_callback(heap_caps_alloc_failed_hook);
  #endif
  WLED::instance().setup();
}

void loop() __attribute__((used)); // needed for -flto
void loop() {
  //WLEDMM show loops per second
#ifdef WLED_DEBUG
  loopCounter++;
  //if (millis() - lastMillis >= 10000) {
  if (millis() - lastMillis >= 8000) {
    long delta = millis() - lastMillis;
    if (delta > 0) {
      lps = (loopCounter*1000U) / delta;
      //if (delta > (showtime / 1000)) lps2 = (loopCounter*1000U) / (delta - (showtime / 1000));
      USER_PRINTF("%lu lps\t", lps);
      USER_PRINTF("%u fps\t", strip.getFps());
      //USER_PRINTF("%lu lps without show\t\t", lps2);
      USER_PRINTF("target frametime %dms\t", int(strip.getFrameTime()));
      USER_PRINTF("target FPS %d\n", int(strip.getTargetFps()));
    }
    lastMillis = millis();
    loopCounter = 0;
    //showtime = 0;
  }
#endif
  WLED::instance().loop();
}
