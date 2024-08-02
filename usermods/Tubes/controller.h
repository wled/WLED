#pragma once

#include <EEPROM.h>
#include "wled.h"
#include "FX.h"
#include "updater.h"
#include "sound.h"

#include "beats.h"

#include "pattern.h"
#include "palettes.h"
#include "effects.h"
#include "led_strip.h"
#include "global_state.h"
#include "node.h"

#define EEPSIZE 2560

const static uint8_t DEFAULT_MASTER_BRIGHTNESS = 200;
const static uint8_t DEFAULT_TUBE_BRIGHTNESS = 120;
const static uint8_t DEFAULT_TANK_BRIGHTNESS = 240;
#define DEFAULT_WLED_FX FX_MODE_RAINBOW_CYCLE

#define STATUS_UPDATE_PERIOD 2000

#define MIN_COLOR_CHANGE_PHRASES 4
#define MAX_COLOR_CHANGE_PHRASES 10

#define ROLE_EEPROM_LOCATION 2559
#define BOOT_OPTIONS_EEPROM_LOCATION 2551

// #define IDENTIFY_STUCK_PATTERNS
// #define IDENTIFY_STUCK_PALETTES

typedef struct {
  bool debugging;
  uint8_t brightness;

  uint8_t reserved[12];
} ControllerOptions;

typedef struct {
  TubeState current;
  TubeState next;
} TubeStates;

typedef enum ControllerRole : uint8_t {
  UnknownRole = 0,
  DefaultRole = 10,         // Turn on in power saving mode
  CampRole = 50,            // Turn on in non-power-saving mode
  InstallationRole = 100,   // Disable power-saving mode completely
  SmallArtRole = 120,       // < 1/2 the pixels, scale the art
  LegacyRole = 190,         // LEGACY: 1/2 the pixels, no "power saving" necessary, no scaling
  MasterRole = 200          // Controls all the others
} ControllerRole;

typedef struct BootOptions {
  unsigned int default_power_save:2;
} BootOptions;

#define BOOT_OPTION_POWER_SAVE_DEFAULT 0
#define BOOT_OPTION_POWER_SAVE_OFF 1
#define BOOT_OPTION_POWER_SAVE_ON 2

typedef struct {
  char key;
  uint8_t arg;
} Action;

#define NUM_VSTRIPS 3

#define DEBOUNCE_TIME 40

class Button {
  public:
    Timer debounceTimer;
    uint8_t pin;
    bool lastPressed = false;

  void setup(uint8_t p) {
    pin = p;
    pinMode(pin, INPUT_PULLUP);
    debounceTimer.start(0);
  }

  bool pressed() {
    if (digitalRead(pin) == HIGH) {
      return !debounceTimer.ended();
    }

    debounceTimer.start(DEBOUNCE_TIME);
    return true;
  }

  bool triggered() {
    // Triggers BOTH low->high AND high->low
    bool p = pressed();
    bool lp = lastPressed;
    lastPressed = p;
    return p != lp;
  }
};

class PatternController : public MessageReceiver {
  public:
    const static int FRAMES_PER_SECOND = 60;  // how often we animate, in frames per second
    const static int REFRESH_PERIOD = 1000 / FRAMES_PER_SECOND;  // how often we animate, in milliseconds

    uint8_t num_leds;
    VirtualStrip *vstrips[NUM_VSTRIPS];
    uint8_t next_vstrip = 0;
    bool canOverride = false;
    uint8_t paletteOverride = 0;
    uint8_t patternOverride = 0;
    uint16_t wled_fader = 0;
    ControllerRole role;
    bool power_save = true;  // Power save ALWAYS starts on. Some roles just ignore it
    uint8_t flashColor = 0;

    AutoUpdater updater = AutoUpdater();
    Sounder sound = Sounder();

    Timer graphicsTimer;
    Timer updateTimer;
    Timer paletteOverrideTimer;
    Timer patternOverrideTimer;
    Timer flashTimer;
    Timer selectTimer;

#ifdef USELCD
    Lcd *lcd;
#endif
    LEDs *led_strip;
    BeatController *beats;
    Effects *effects;
    LightNode *node;

    ControllerOptions options;
    char key_buffer[20] = {0};

    Energy energy=Chill;
    TubeState current_state;
    TubeState next_state;

    // When a pattern is boring, spice it up a bit with more effects
    bool isBoring = false;

  PatternController(uint8_t num, BeatController *b) : num_leds(num), beats(b) {
#ifdef USELCD
    lcd = new Lcd();
#endif
    led_strip = new LEDs(num_leds);
    effects = new Effects();
    node = new LightNode(this);
    // mesh = new BLEMeshNode(this);

    for (uint8_t i=0; i < NUM_VSTRIPS; i++) {
#ifdef DOUBLED
      vstrips[i] = new VirtualStrip(num_leds * 2 + 1);
#else
      vstrips[i] = new VirtualStrip(num_leds);
#endif
    }
  }

  bool isMasterRole() {
#if defined(GOLDEN) || defined(CHRISTMAS)
    return true;
#endif
    return role >= MasterRole;
  }

  void setup()
  {
    EEPROM.begin(EEPSIZE);
    role = (ControllerRole)EEPROM.read(ROLE_EEPROM_LOCATION);
    if (role == 255) {
      role = UnknownRole;
    }
    Serial.printf("Role = %d\n", role);

    auto b = EEPROM.read(BOOT_OPTIONS_EEPROM_LOCATION);
    Serial.printf("EEPROM read: %d\n", b);
    EEPROM.end();

    BootOptions* boot = (BootOptions*)&b;
    switch (boot->default_power_save) {
      case BOOT_OPTION_POWER_SAVE_OFF:
        power_save = 0;
        break;
      case BOOT_OPTION_POWER_SAVE_ON:
        power_save = 1;
        break;
      default:
        power_save = (role < CampRole);
        break;
    }

    if (role <= CampRole)
      strip.ablMilliampsMax = min(ABL_MILLIAMPS_DEFAULT,700);  // Really limit for batteries
    else if (role <= InstallationRole)
      strip.ablMilliampsMax = 1000;
    else
      strip.ablMilliampsMax = 1400;

    node->setup();

    if (role >= MasterRole) {
      node->reset(3850 + role); // MASTER ID
      options.brightness = DEFAULT_MASTER_BRIGHTNESS;
    } else if (role >= LegacyRole) {
        options.brightness = DEFAULT_TUBE_BRIGHTNESS;
    } else if (role == InstallationRole) {
        options.brightness = DEFAULT_TANK_BRIGHTNESS;
    } else {
        options.brightness = DEFAULT_TUBE_BRIGHTNESS;
    }
#if defined(GOLDEN) || defined(CHRISTMAS)
    node->reset(0xFFF);
#endif
    options.debugging = false;
    load_options(options, true);

#ifdef USELCD
    lcd->setup();
#endif
    set_next_pattern(0);
    set_next_palette(0);
    set_next_effect(0);
    next_state.pattern_phrase = 0;
    next_state.palette_phrase = 0;
    next_state.effect_phrase = 0;
    set_wled_palette(0); // Default palette
    set_wled_pattern(0, 128, 128); // Default pattern

    sound.setup();

    updateTimer.start(STATUS_UPDATE_PERIOD); // Ready to send an update as soon as we're able to
    Serial.println("Controller: ok");
  }

  void do_pattern_changes() {
    uint16_t phrase = current_state.beat_frame >> 12;
    bool changed = false;

    if (phrase >= next_state.pattern_phrase) {
#ifdef IDENTIFY_STUCK_PATTERNS
      Serial.println("Time to change pattern");
#endif
      load_pattern(next_state);
      next_state.pattern_phrase = phrase + set_next_pattern(phrase);

      // Don't change pattern and others at the same time
      while (next_state.pattern_phrase == next_state.palette_phrase || next_state.pattern_phrase == next_state.effect_phrase) {
        next_state.pattern_phrase += random8(1,3);
      }
      changed = true;
    }
    if (phrase >= next_state.palette_phrase) {
#ifdef IDENTIFY_STUCK_PATTERNS
      Serial.println("Time to change palette");
#endif
      load_palette(next_state);
      next_state.palette_phrase = phrase + set_next_palette(phrase);

      // Don't change palette and others at the same time
      while (next_state.palette_phrase == next_state.pattern_phrase || next_state.palette_phrase == next_state.effect_phrase) {
        next_state.palette_phrase += random8(1,3);
      }
      changed = true;
    }
    if (phrase >= next_state.effect_phrase) {
#ifdef IDENTIFY_STUCK_PATTERNS
      Serial.println("Time to change effect");
#endif
      load_effect(next_state);
      next_state.effect_phrase = phrase + set_next_effect(phrase);

      // Don't change palette and others at the same time
      while (next_state.effect_phrase == next_state.pattern_phrase || next_state.effect_phrase == next_state.palette_phrase) {
        next_state.effect_phrase += random8(1,3);
      }
      changed = true;
    }

    if (changed) {
      next_state.print();
      Serial.println();
    }
  }

  void cancelOverrides() {
    // Release the WLED overrides and take over control of the strip again.
    paletteOverrideTimer.stop();
    patternOverrideTimer.stop();
  }

  void enterSelectMode() {
    selectTimer.start(20000);
  }

  bool isSelecting() {
    return !selectTimer.ended();
  }

  bool isSelected() {
    return updater.status == Ready;
  }

  void select(bool selected = true) {
    if (selected)
      updater.ready();
    else {
      updater.stop();
      WiFi.softAPdisconnect(true);
    }
  }

  void deselect() {
    select(false);
  }

  void set_palette_override(uint8_t value) {
    if (!canOverride)
      return;
    if (value == paletteOverride)
      return;
      
    paletteOverride = value;
    if (value) {
      Serial.println("WLED has control of palette.");
      paletteOverrideTimer.start(300000); // 5 minutes of manual control
    } else {
      Serial.println("Turning off WLED control of palette.");
      paletteOverrideTimer.stop();
      set_wled_palette(current_state.palette_id);
    }
  }

  void set_pattern_override(uint8_t value, uint8_t auto_mode) {
    if (!canOverride)
      return;
    if (value == DEFAULT_WLED_FX && !patternOverride)
      return;
    if (value == patternOverride)
      return;

    patternOverride = value;
    if (value) {
      Serial.println("WLED has control of patterns.");
      patternOverrideTimer.start(300000); // 5 minutes of manual control
      transitionDelay = 500;  // Short transitions
    } else {
      Serial.println("Turning off WLED control of patterns.");
      patternOverrideTimer.stop();
      transitionDelay = 8000; // Back to long transitions

      uint8_t param = modeParameter(auto_mode);
      set_wled_pattern(auto_mode, param, param);
    }
  }

  void update()
  {
    read_keys();
    
    // Update the mesh
    node->update();

    // Update sound meter
    sound.update();

    // Update patterns to the beat
    update_beat();

    Segment& segment = strip.getMainSegment();

    // You can only go into manual control after enabling the wifi
    if (apActive && updater.status != Ready)
      canOverride = true;

    // Detect manual overrides & update the current state to match.
    if (canOverride) {
      if (paletteOverride && (paletteOverrideTimer.ended() || !apActive)) {
        set_palette_override(0);
      } else if (segment.palette != current_state.palette_id) {
        set_palette_override(segment.palette);
      }
      
      uint8_t wled_mode = gPatterns[current_state.pattern_id].wled_fx_id;
      if (wled_mode < 10)
        wled_mode = DEFAULT_WLED_FX;
      if (patternOverride && (patternOverrideTimer.ended() || !apActive)) {
        set_pattern_override(0, wled_mode);
      } else if (segment.mode != wled_mode) {
        set_pattern_override(segment.mode, wled_mode);
      }
    }

    do_pattern_changes();

    if (graphicsTimer.every(REFRESH_PERIOD)) {
      updateGraphics();
    }

    // Update current status
    if (updateTimer.every(STATUS_UPDATE_PERIOD)) {
      // Transmit less often when following
      if (!node->isFollowing() || random(0, 4) == 0) {
        send_update();
      }
    }

    updater.update();

#ifdef USELCD
    if (lcd->active) {
      lcd->size(1);
      lcd->write(0,56, current_state.beat_frame);
      lcd->write(80,56, x_axis);
      lcd->write(100,56, y_axis);
      lcd->show();

      lcd->update();
    }
#endif
  }

  void handleOverlayDraw() {
    // In manual mode WLED is always active
    if (patternOverride) {
      wled_fader = 0xFFFF;
    }

    uint16_t length = strip.getLengthTotal();

    // Crossfade between the custom pattern engine and WLED
    uint8_t fader = wled_fader >> 8;
    if (fader < 255) {
      // Perform a cross-fade between current WLED mode and the external buffer
      for (int i = 0; i < length; i++) {
        CRGB c = led_strip->getPixelColor(i);
        if (fader > 0) {
          CRGB color2 = strip.getPixelColor(i);
          uint8_t r = blend8(c.r, color2.r, fader);
          uint8_t g = blend8(c.g, color2.g, fader);
          uint8_t b = blend8(c.b, color2.b, fader);
          c = CRGB(r,g,b);
        }
        strip.setPixelColor(i, c);
      }
    }

    // Power Save mode: reduce number of displayed pixels 
    // Only affects non-powered poles
    if (power_save && role < InstallationRole) {
      // Screen door effect to save power
      for (int i = 0; i < length; i++) {
        if (i % 2) {
            strip.setPixelColor(i, CRGB::Black);
        }
      }
    }

    sound.handleOverlayDraw();

    // Draw effects layers over whatever WLED is doing.
    // But not in manual (WLED) mode
    if (!patternOverride) {
      effects->draw(&strip);
    }

    // Make the art half-size if it has a small number of pixels
    if (role >= MasterRole || role == SmallArtRole) {
      int p = 0;
      for (int i = 0; i < length; i++) {
        CRGB c = strip.getPixelColor(i++); // i advances by 2
        CRGB c2 = strip.getPixelColor(i);
        nblend(c, c2, 128);
        if (role >= MasterRole) {
          nblend(c, CRGB::Black, 128);
        }
        strip.setPixelColor(p++, c);
      }
    }

    if (flashColor) {
      if (flashTimer.ended())
        flashColor = 0;
      else {
        if (millis() % 4000 < 2000) {
          auto chsv = CHSV(flashColor, 255, 255);
          for (int i = 0; i < length; i++) {
            strip.setPixelColor(i, CRGB(chsv));
          }
        }
      }
    }

    updater.handleOverlayDraw();
  }

  void restart_phrase() {
    beats->start_phrase();
    update_beat();
    send_update();
  }

  void set_phrase_position(uint8_t pos) {
    beats->sync(beats->bpm, (beats->frac & -0xFFF) + (pos<<8));
    update_beat();
    send_update();
  }
  
  void set_tapped_bpm(accum88 bpm, uint8_t pos=15) {
    // By default, restarts at 15th beat - because this is the end of a tap
    beats->sync(bpm, (beats->frac & -0xFFF) + (pos<<8));
    update_beat();
    send_update();
  }

  void request_new_bpm(accum88 new_bpm = 0) {
    // 0 = toggle 120 to 125
    if (new_bpm == 0)
      new_bpm = current_state.bpm>>8 >= 123 ? 120<<8 : 125<<8;

    if (node->isFollowing()) {
      // Send a request up to ROOT
      broadcast_bpm(new_bpm);
    } else {
      set_tapped_bpm(new_bpm, 0);
    }
  }

  void update_beat() {
    current_state.bpm = next_state.bpm = beats->bpm;
    current_state.beat_frame = particle_beat_frame = beats->frac;  // (particle_beat_frame is a hack)
    if (current_state.bpm>>8 <= 118) // Hip hop / ghettofunk
      energy = MediumEnergy;
    else if (current_state.bpm>>8 >= 125) // House & breaks
      energy = HighEnergy;
    else if (current_state.bpm>>8 > 120) // Tech house
      energy = MediumEnergy;
    else
      energy = Chill; // Deep house
  }
  
  void send_update() {
    Serial.print("     ");
    current_state.print();
    Serial.print(F(" "));

    uint16_t phrase = current_state.beat_frame >> 12;
    Serial.print(F("    "));
    Serial.print(next_state.pattern_phrase - phrase);
    Serial.print(F("P "));
    Serial.print(next_state.palette_phrase - phrase);
    Serial.print(F("C "));
    Serial.print(next_state.effect_phrase - phrase);
    Serial.print(F("E: "));
    next_state.print();
    Serial.print(F(" "));
    Serial.println();    

    broadcast_state();
  }

  void background_changed() {
    update_background();
    current_state.print();
    Serial.println();
  }

  void load_options(ControllerOptions &options, bool init=false) {
    // Power-saving devices retain their WLED brightness
    if (init || !power_save)
      strip.setBrightness(options.brightness);
  }

  void load_pattern(TubeState &tube_state) {
    if (current_state.pattern_id == tube_state.pattern_id 
        && current_state.pattern_sync_id == tube_state.pattern_sync_id)
      return;

    current_state.pattern_phrase = tube_state.pattern_phrase;
    current_state.pattern_id = tube_state.pattern_id % gPatternCount;
    current_state.pattern_sync_id = tube_state.pattern_sync_id;
    isBoring = gPatterns[current_state.pattern_id].control.energy == Boring;

    Serial.print(F("Change pattern "));
    background_changed();
  }

  bool isShowingWled() {
    return current_state.pattern_id >= numInternalPatterns;
  }

  uint8_t modeParameter(uint8_t mode) {
    switch (energy) {
      case Boring:
        // Spice things up a bit
        return 128;

      case Chill:
        return 90;

      case HighEnergy:
        return 140;

      default:
      case MediumEnergy:
        return 128;
    }
  }

  // For now, can't crossfade between internal and WLED patterns
  // If currently running an WLED pattern, only select from internal patterns.
  uint8_t get_valid_next_pattern() {
    if (isShowingWled())
      return random8(0, numInternalPatterns);
    return random8(0, gPatternCount);
  }

  // Choose the pattern to display at the next pattern cycle
  // Return the number of phrases until the next pattern cycle
  uint16_t set_next_pattern(uint16_t phrase) {
    uint8_t pattern_id;
    PatternDef def;

#ifdef IDENTIFY_STUCK_PATTERNS
    Serial.println("Changing next pattern");
#endif
    // Try 10 times to find a pattern that fits the current "energy"
    for (int i = 0; i < 10; i++) {
      pattern_id = get_valid_next_pattern();
      def = gPatterns[pattern_id];
      if (def.control.energy <= energy)
        break;
    }
#ifdef IDENTIFY_STUCK_PATTERNS
    Serial.printf("Next pattern will be %d\n", pattern_id);
#endif

    next_state.pattern_id = pattern_id;
    next_state.pattern_sync_id = randomSyncMode();

    switch (def.control.duration) {
      case ExtraShortDuration: return random8(2, 6);
      case ShortDuration: return random8(5,15);
      case MediumDuration: return random8(15,25);
      case LongDuration: return random8(20,40);
      case ExtraLongDuration: return random8(25, 60);
    }
    return 5;
  }

  void load_palette(TubeState &tube_state) {
    if (current_state.palette_id == tube_state.palette_id)
      return;

    current_state.palette_phrase = tube_state.palette_phrase;
    current_state.palette_id = tube_state.palette_id % gGradientPaletteCount;
    set_wled_palette(current_state.palette_id);
  }

  // Choose the palette to display at the next palette cycle
  // Return the number of phrases until the next palette cycle
  uint16_t set_next_palette(uint16_t phrase) {
#if defined(GOLDEN)
    uint r = random8(0, 4);
    uint colors[4] = {18, 58, 71, 111};
    next_state.palette_id = colors[r];
#elif defined(CHRISTMAS)   // 81, 107 are too bright
    uint r = random8(0, 20);
    uint colors[20] = {/*gold:*/18, 58, 71, 111, 
                      /*yes:*/25, 34, 61, 63, 81, 112,        
                      /*best yes:*/25, 34, 34, 61, 63, 81, 112,        
                      /*maybe:*/81, 28, 107};
    next_state.palette_id = colors[r];
#else
    // Don't select the built-in palettes
    next_state.palette_id = random8(6, gGradientPaletteCount);
#endif

    auto phrases = random8(MIN_COLOR_CHANGE_PHRASES, MAX_COLOR_CHANGE_PHRASES);

    // Change color more often in boring patterns
    if (isBoring) {
      phrases /= 2;
    }
    return phrases;
  }

  void load_effect(TubeState &tube_state) {
    if (current_state.effect_params.effect == tube_state.effect_params.effect && 
        current_state.effect_params.pen == tube_state.effect_params.pen && 
        current_state.effect_params.chance == tube_state.effect_params.chance)
      return;

    _load_effect(tube_state.effect_params);
  }

  void _load_effect(EffectParameters params) {
    current_state.effect_params = params;
  
    Serial.print(F("Change effect "));
    current_state.print();
    Serial.println();
    
    effects->load(current_state.effect_params);
  }

  // Choose the effect to display at the next effect cycle
  // Return the number of phrases until the next effect cycle
  uint16_t set_next_effect(uint16_t phrase) {
    uint8_t effect_num = random8(gEffectCount);

    // Pick a random effect to add; boring patterns get better chance at having an effect.
    EffectDef def = gEffects[effect_num];
    if (def.control.energy > energy) {
      def = gEffects[0];
    }

    next_state.effect_params = def.params;

    switch (def.control.duration) {
      case ExtraShortDuration: return random(1,3);
      case ShortDuration: return random(2,4);
      case MediumDuration: return random(4,7);
      case LongDuration: return random(8, 11);
      case ExtraLongDuration: return random(10,15);
    }
    return 1;
  }

  void update_background() {
    Background background;
    background.animate = gPatterns[current_state.pattern_id].backgroundFn;
    background.wled_fx_id = gPatterns[current_state.pattern_id].wled_fx_id;
    background.palette_id = current_state.palette_id;
    background.sync = (SyncMode)current_state.pattern_sync_id;

    // Use one of the virtual strips to render the patterns.
    // A WLED-based pattern exists on the virtual strip, but causes
    // it to do nothing since WLED merging happens in handleOverlayDraw.
    // Reuse virtual strips to prevent heap fragmentation
    for (uint8_t i = 0; i < NUM_VSTRIPS; i++) {
      vstrips[i]->fadeOut();
    }
    vstrips[next_vstrip]->load(background);
    next_vstrip = (next_vstrip + 1) % NUM_VSTRIPS; 

    uint8_t param = modeParameter(background.wled_fx_id);
    set_wled_pattern(background.wled_fx_id, param, param);
    set_wled_palette(background.palette_id);
  }

  bool isUnderWledControl() {
    return paletteOverride || patternOverride;
  }

  void set_wled_palette(uint8_t palette_id) {
    if (paletteOverride)
      palette_id = paletteOverride;

    Segment& seg = strip.getMainSegment();
    seg.setPalette(palette_id);

    stateChanged = true;
    stateUpdated(CALL_MODE_DIRECT_CHANGE);
  }

  void set_wled_pattern(uint8_t pattern_id, uint8_t speed, uint8_t intensity) {
    if (patternOverride)
      pattern_id = patternOverride;
    else if (pattern_id == 0)
      pattern_id = DEFAULT_WLED_FX; // Never set it to solid

    Segment& seg = strip.getMainSegment();
    seg.speed = speed;
    seg.intensity = intensity;
    seg.setMode(pattern_id);

    stateChanged = true;
    stateUpdated(CALL_MODE_DIRECT_CHANGE);
  }

  void setBrightness(uint8_t brightness) {
    Serial.printf("brightness: %d\n", brightness);

    options.brightness = brightness;
    load_options(options);

    // The master controls all followers
    if (!node->isFollowing())
      broadcast_options();
  }

  void setDebugging(bool debugging) {
    Serial.printf("debugging: %d\n", debugging);

    options.debugging = debugging;
    load_options(options);

    // The master controls all followers
    if (!node->isFollowing())
      broadcast_options();
  }
  
  void togglePowerSave() {
    setPowerSave(!power_save);
  }

  void setPowerSave(bool ps) {
    power_save = ps;
    Serial.printf("power_save: %d\n", power_save);

    // Remember this setting on the next boot
    EEPROM.begin(2560);
    auto b = EEPROM.read(BOOT_OPTIONS_EEPROM_LOCATION);
    BootOptions* boot = (BootOptions*)&b;
    if (power_save)
      boot->default_power_save = BOOT_OPTION_POWER_SAVE_ON;
    else
      boot->default_power_save = BOOT_OPTION_POWER_SAVE_OFF;
    EEPROM.write(BOOT_OPTIONS_EEPROM_LOCATION, b); // Reset all boot options
    Serial.printf("wrote: %d\n", b);
    EEPROM.end();
  }

  void setRole(ControllerRole r) {
    role = r;
    Serial.printf("Role = %d", role);
    EEPROM.begin(EEPSIZE);
    EEPROM.write(ROLE_EEPROM_LOCATION, role);
    EEPROM.write(BOOT_OPTIONS_EEPROM_LOCATION, 0); // Reset all boot options
    EEPROM.end();
    delay(10);
    doReboot = true;
  }
  
  SyncMode randomSyncMode() {
    uint8_t r = random8(128);

    // For boring patterns, up the chance of a sync mode
    if (isBoring)
      r -= 20;

    if (r < 30)
      return SinDrift;
    if (r < 50)
      return Pulse;
    if (r < 70)
      return Swing;
    if (r < 80)
      return SwingDrift;
    return All;
  }

  void updateGraphics() {
    static BeatFrame_24_8 lastFrame = 0;
    BeatFrame_24_8 beat_frame = current_state.beat_frame;

    uint8_t beat_pulse = 0;
    for (int i = 0; i < 8; i++) {
      if ( (beat_frame >> (5+i)) != (lastFrame >> (5+i)))
        beat_pulse |= 1<<i;
    }
    lastFrame = beat_frame;

    wled_fader = 0;

    VirtualStrip *first_strip = NULL;
    for (uint8_t i=0; i < NUM_VSTRIPS; i++) {
      VirtualStrip *vstrip = vstrips[i];
      if (vstrip->fade == Dead)
        continue;

      // Remember the first strip
      if (first_strip == NULL)
        first_strip = vstrip;

      // Remember the strip that's actually WLED
      if (vstrip->isWled())
        wled_fader = vstrip->fader;
     
      vstrip->update(beat_frame, beat_pulse);
      vstrip->blend(led_strip->leds, led_strip->num_leds, options.brightness, vstrip == first_strip);
    }

    effects->update(first_strip, beat_frame, (BeatPulse)beat_pulse);
  }

  virtual void acknowledge() {
    addFlash(CRGB::Green);
  }

  void read_keys() {
    if (!Serial.available())
      return;
      
    char c = Serial.read();
    char *k = key_buffer;
    uint8_t max = sizeof(key_buffer);
    for (uint8_t i=0; *k && (i < max-1); i++) {
      k++;
    }
    if (c == 10) {
      keyboard_command(key_buffer);
      key_buffer[0] = 0;
    } else {
      *k++ = c;
      *k = 0;    
    }
  }

  accum88 parse_number(char *s) {
    uint16_t n=0, d=0;
    
    while (*s == ' ')
      s++;
    while (*s) {
      if (*s < '0' || *s > '9')
        break;
      n = n*10 + (*s++ - '0');
    }
    n = n << 8;
    
    if (*s == '.') {
      uint16_t div = 1;
      s++;
      while (*s) {
        if (*s < '0' || *s > '9')
          break;
        d = d*10 + (*s++ - '0');
        div *= 10;
      }
      d = (d << 8) / div;
    }
    return n+d;
  }

  void keyboard_command(char *command) {
    // If not the lead, send it to the lead.
    uint8_t b;
    accum88 arg = parse_number(command+1);
    Serial.printf("[command=%c arg=%04x]\n", command[0], arg);

    switch (command[0]) {
      case 'd':
        setDebugging(!options.debugging);
        break;
      case '~':
        doReboot = true;
        break;
      case '@':
        togglePowerSave();
        break;

      case '-':
        b = options.brightness;
        while (*command++ == '-')
          b -= 5;
        setBrightness(b - 5);
        break;
      case '+':
        b = options.brightness;
        while (*command++ == '+')
          b += 5;
        setBrightness(b + 5);
        return;
      case 'l':
        if (arg < 5*256) {
          Serial.println(F("nope"));
          return;
        }
        setBrightness(arg >> 8);
        return;

      case 'b':
        if (arg < 60*256) {
          Serial.println(F("nope"));
          return;
        }
        request_new_bpm(arg);
        return;

      case 's':
        beats->start_phrase();
        update_beat();
        send_update();
        return;

      case 'n':
        force_next();
        return;

      case 'p':
        next_state.pattern_phrase = 0;
        next_state.pattern_id = arg >> 8;
        next_state.pattern_sync_id = All;
        broadcast_state();
        return;        

      case 'm':
        next_state.pattern_phrase = 0;
        next_state.pattern_id = current_state.pattern_id;
        next_state.pattern_sync_id = arg >> 8;
        broadcast_state();
        return;
        
      case 'c':
        next_state.palette_phrase = 0;
        next_state.palette_id = arg >> 8;
        broadcast_state();
        return;
        
      case 'e':
        next_state.effect_phrase = 0;
        next_state.effect_params = gEffects[(arg >> 8) % gEffectCount].params;
        broadcast_state();
        return;

      case '%':
        next_state.effect_phrase = 0;
        next_state.effect_params = current_state.effect_params;
        next_state.effect_params.chance = arg;
        broadcast_state();
        return;

      case 'i':
        Serial.printf("Reset! ID -> %03X\n", arg >> 4);
        node->reset(arg >> 4);
        return;

      case 'U':
      case 'V':
      case '*':
      case '(':
      case ')':
      case 'G':
      case 'A':
      case 'W':
      case 'X':
      case 'F':
      case 'R':
      case 'M': {
        Action action = {
          .key = command[0],
          .arg = (uint8_t)(arg >> 8)
        };
        broadcast_action(action);
        return;
      }

      case 'P': {
        // Toggle power save
        Action action = {
          .key = command[0],
          .arg = !power_save,
        };
        broadcast_action(action);
        break;
      }

      case 'O': {
        // Toggle sound overlay
        Action action = {
          .key = command[0],
          .arg = !sound.overlay
        };
        broadcast_action(action);
        break;
      }

      case 'r':
        setRole((ControllerRole)(arg >> 8));
        return;

      case '?':
        Serial.println(F("b###.# - set bpm"));
        Serial.println(F("s - start phrase"));
        Serial.println();
        Serial.println(F("p### - patterns"));
        Serial.println(F("m### - sync mode"));
        Serial.println(F("c### - colors"));
        Serial.println(F("e### - effects"));
        Serial.println();
        Serial.println(F("i### - set ID"));
        Serial.println(F("d - toggle debugging"));
        Serial.println(F("l### - brightness"));
        Serial.println("@ - toggle power saving mode");
        Serial.println("U - begin auto-update");
        Serial.println("P - toggle all power saves");
        Serial.println("O - toggle all sound overlays");
        Serial.println("==== global actions ====");
        Serial.println("* - enter select mode (double-click to Ready)");
        Serial.println("A - turn on access point (Ready to update)");
        Serial.println("W - forget WiFi client");
        Serial.println("X - restart");
        Serial.println("V### - auto-upgrade to version");
        Serial.println("M - cancel manual pattern override");
        return;

      case 'u':
        updater.start();
        return;

      case 0:
        // Empty command
        return;

      default:
        Serial.println("dunno?");
        return;
    }
  }

  void force_next() {
    uint16_t phrase = current_state.beat_frame >> 12;
    uint16_t next_phrase = min(next_state.pattern_phrase, min(next_state.palette_phrase, next_state.effect_phrase)) - phrase;
    next_state.pattern_phrase -= next_phrase;
    next_state.palette_phrase -= next_phrase;
    next_state.effect_phrase -= next_phrase;
    broadcast_state();
  }

  void broadcast_action(Action& action) {
    if (!node->isFollowing()) {
      onAction(&action);
    }
    node->sendCommand(COMMAND_ACTION, &action, sizeof(Action));
  }

  void broadcast_info(NodeInfo *info) {
    node->sendCommand(COMMAND_INFO, &info, sizeof(NodeInfo));
  }

  void broadcast_state() {
    node->sendCommand(COMMAND_STATE, &current_state, sizeof(TubeStates));
  }

  void broadcast_options() {
    node->sendCommand(COMMAND_OPTIONS, &options, sizeof(options));
  }

  void broadcast_autoupdate() {
    node->sendCommand(COMMAND_UPGRADE, &updater.current_version, sizeof(updater.current_version));
  }

  void broadcast_bpm(accum88 bpm) {
    // Hacked in feature: request a new BPM
    node->sendCommand(COMMAND_BEATS, &bpm, sizeof(bpm));
  }

  virtual bool onCommand(CommandId command, void *data) {
    switch (command) {
      case COMMAND_INFO:
        Serial.printf("   \"%s\"\n",
          ((NodeInfo*)data)->message
        );
        return true;
  
      case COMMAND_OPTIONS:
        memcpy(&options, data, sizeof(options));
        load_options(options);
        Serial.printf("[debug=%d  bri=%d]",
          options.debugging,
          options.brightness
        );
        return true;

      case COMMAND_STATE: {
        auto update_data = (TubeStates*)data;

        TubeState state;
        memcpy(&state, &update_data->current, sizeof(TubeState));
        memcpy(&next_state, &update_data->next, sizeof(TubeState));
        state.print();
        next_state.print();
  
        // Catch up to this state
        load_pattern(state);
        load_palette(state);
        load_effect(state);
        beats->sync(state.bpm, state.beat_frame);
        return true;
      }

      case COMMAND_UPGRADE:
        updater.start((AutoUpdateOffer*)data);
        return true;

      case COMMAND_ACTION:
        onAction((Action*)data);
        return true;

      case COMMAND_BEATS:
        // the master control ignores this request, it has its own
        // beat measuring.
        if (isMasterRole())
          return false;
        set_tapped_bpm(*(accum88*)data, 0);
        return true;
    }
  
    Serial.printf("UNKNOWN COMMAND %02X", command);
    return false;
  }

  void onAction(Action* action) {
    switch (action->key) {
      case 'A':
        Serial.println("Turning on WiFi access point.");
        WLED::instance().initAP(true);
        return;

      case 'O':
        sound.overlay = (action->arg != 0);
        return;

      case 'X':
        if (!isSelected())
          return;
        doReboot = true;
        return;

      case 'R':
        if (!isSelected())
          return;
        setRole((ControllerRole)(action->arg));
        return;

      case '@':
        Serial.print("Setting power save to %d\n");
        setPowerSave(action->arg);
        return;

      case 'W':
        Serial.println("Clearing WiFi connection.");
        strcpy(clientSSID, "");
        strcpy(clientPass, "");
        WiFi.disconnect(false, true);
        return;

      case 'G':
        Serial.println("glitter!");
        for (int i=0; i< 10; i++)
          addGlitter();
        return;

      case 'F':
        Serial.println("flash!");
        flashTimer.start(20000);
        flashColor = action->arg;
        return;

      case 'M':
        Serial.println("cancel manual mode");
        cancelOverrides();
        break;

      case '*':
      case '(':
        Serial.println("enter select mode");
        enterSelectMode();
        break;

      case ')':
        Serial.println("exit select mode");
        deselect();
        break;

      case 'V':
        // Version check: prepare for update
        if (updater.current_version.version >= action->arg)
          break;

        select();
        break;

      case 'U':
        if (!isSelected())
          return;
        updater.start();
        break;

    }
  }

#define WIZMOTE_BUTTON_ON          1
#define WIZMOTE_BUTTON_OFF         2
#define WIZMOTE_BUTTON_NIGHT       3
#define WIZMOTE_BUTTON_ONE         16
#define WIZMOTE_BUTTON_TWO         17
#define WIZMOTE_BUTTON_THREE       18
#define WIZMOTE_BUTTON_FOUR        19
#define WIZMOTE_BUTTON_BRIGHT_UP   9
#define WIZMOTE_BUTTON_BRIGHT_DOWN 8

  void force_next_pattern() {
    next_state.pattern_phrase = current_state.beat_frame >> 12;
    if (next_state.palette_phrase == next_state.pattern_phrase)
      next_state.palette_phrase += random8(0, 5);
    force_next();
  }

  void force_next_effect() {
    next_state.effect_phrase = current_state.beat_frame >> 12;
    force_next();
  }

  virtual bool onButton(uint8_t button_id) {
    bool isMaster = !this->node->isFollowing();

    switch (button_id) {
      case WIZMOTE_BUTTON_ON:
        WLED::instance().initAP(true);
        setDebugging(true);
        acknowledge();
        return true;

      case WIZMOTE_BUTTON_OFF:
        WiFi.softAPdisconnect(true);
        apActive = false;
        WiFi.disconnect(false, true);
        WLED::instance().enableWatchdog();
        apBehavior = AP_BEHAVIOR_BUTTON_ONLY;
        setDebugging(false);
        acknowledge();
        return true;

      case WIZMOTE_BUTTON_ONE:
        // Make it interesting - switch to a good pattern and sync mode
        // Only the master will respond to this
        if (!isMaster)
          return false;

        Serial.println("WizMote preset 1: de-sync");

        set_next_pattern(0);
        while (next_state.pattern_sync_id == All)
          set_next_pattern(0);

        this->force_next_pattern();
        return true;

      case WIZMOTE_BUTTON_TWO:
        // Apply an interesting effect & sync layer
        // Only the master will respond to this
        if (!isMaster)
          return false;
        
        Serial.println("WizMote preset 2: add an effect");

        set_next_effect(0);
        while (next_state.effect_params.effect == None)
          set_next_effect(0);

        this->force_next_effect();
        return true;

      case WIZMOTE_BUTTON_THREE:
        // Turn on flames.  Also up the tempo to 125
        // Only the master will respond to this
        if (!isMaster)
          return false;

        // Switch to house mode
        set_tapped_bpm(125<<8);

        Serial.println("WizMote preset 3: flames!");
        next_state.pattern_id = 63; // Fire
        next_state.pattern_sync_id = SyncMode::All;
        this->force_next_pattern();
        return true;

      case WIZMOTE_BUTTON_FOUR:
        // Make it an interesting combo
        // Only the master will respond to this
        if (!isMaster)
          return false;

        // 38: Noise 3
        Serial.println("WizMote preset 4: interesting pattern");

        set_next_pattern(0);
        next_state.pattern_id = 38; // overwrite with: Noise 3

        this->force_next_pattern();
        return true;

      case WIZMOTE_BUTTON_BRIGHT_UP:
        // Brighten (ignored if in power save mode)
        Serial.println("WizMote: brightness up");
        if (options.brightness <= 230)
          setBrightness(options.brightness + 25);
        return true;

      case WIZMOTE_BUTTON_BRIGHT_DOWN:
        // Dim (ignored if in power save mode)
        Serial.println("WizMote: brightness down");

        if (options.brightness >= 25)
          setBrightness(options.brightness - 25);
        return true;

      case WIZMOTE_BUTTON_NIGHT:
        // Chill mode
        // Only the master will respond to this
        if (!isMaster)
          return false;

        Serial.println("WizMote: chill");

        // Switch to deep house mode
        set_tapped_bpm(120<<8);

        this->force_next();
        return true;

      default:
        Serial.printf("Button %d master=%d\n", button_id, isMaster);
        return false;
    }
  }


};