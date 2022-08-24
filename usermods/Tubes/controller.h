#pragma once

#include <EEPROM.h>
#include "wled.h"
#include "FX.h"
#include "updater.h"

#include "beats.h"

#include "pattern.h"
#include "palettes.h"
#include "effects.h"
#include "led_strip.h"
#include "global_state.h"
#include "node.h"

const static uint8_t DEFAULT_MASTER_BRIGHTNESS = 100;
const static uint8_t DEFAULT_TUBE_BRIGHTNESS = 128;
#define DEFAULT_WLED_FX FX_MODE_RAINBOW_CYCLE

#define STATUS_UPDATE_PERIOD 2000

#define MIN_COLOR_CHANGE_PHRASES 4
#define MAX_COLOR_CHANGE_PHRASES 20

#define ROLE_EEPROM_LOCATION 2559

#define IDENTIFY_STUCK_PATTERNS

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
  LegacyRole = 190,         // 1/2 the pixels, no "power saving" necessary
  MasterRole = 200          // Controls all the others
} ControllerRole;

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

  void setup(uint8_t pin) {
    this->pin = pin;
    pinMode(pin, INPUT_PULLUP);
    this->debounceTimer.start(0);
  }

  bool pressed() {
    if (digitalRead(this->pin) == HIGH) {
      return !this->debounceTimer.ended();
    }

    this->debounceTimer.start(DEBOUNCE_TIME);
    return true;
  }

  bool triggered() {
    // Triggers BOTH low->high AND high->low
    bool p = this->pressed();
    bool lp = this->lastPressed;
    this->lastPressed = p;
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
    uint8_t paletteOverride = 0;
    uint8_t patternOverride = 0;
    uint16_t wled_fader = 0;
    ControllerRole role;
    bool power_save = true;  // Power save ALWAYS starts on. Some roles just ignore it
    uint8_t flashColor = 0;

    AutoUpdater updater = AutoUpdater();

    Timer graphicsTimer;
    Timer updateTimer;
    Timer paletteOverrideTimer;
    Timer patternOverrideTimer;
    Timer flashTimer;

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

  PatternController(uint8_t num_leds, BeatController *beats) {
    this->num_leds = num_leds;
#ifdef USELCD
    this->lcd = new Lcd();
#endif
    this->led_strip = new LEDs(num_leds);
    this->beats = beats;
    this->effects = new Effects();
    this->node = new LightNode(this);
    // this->mesh = new BLEMeshNode(this);

    for (uint8_t i=0; i < NUM_VSTRIPS; i++) {
#ifdef DOUBLED
      this->vstrips[i] = new VirtualStrip(num_leds * 2 + 1);
#else
      this->vstrips[i] = new VirtualStrip(num_leds);
#endif
    }
  }

  bool isMaster() {
    return this->role == MasterRole;
  }

  void setup()
  {
    this->node->setup();
    EEPROM.begin(2560);
    this->role = (ControllerRole)EEPROM.read(ROLE_EEPROM_LOCATION);
    if (this->role == 255) {
      this->role = UnknownRole;
    }
    EEPROM.end();
    Serial.printf("Role = %d", this->role);

    this->power_save = (this->role < CampRole);
    this->options.brightness = DEFAULT_TUBE_BRIGHTNESS;
    this->options.debugging = false;
    switch (role) {
      case MasterRole:
        this->node->reset(4050); // MASTER ID
        this->options.brightness = DEFAULT_MASTER_BRIGHTNESS;
        break;

      case LegacyRole:
        this->options.brightness = DEFAULT_TUBE_BRIGHTNESS;
        break;
      
      default:
        this->options.brightness = DEFAULT_TUBE_BRIGHTNESS;
        break;
    }
    this->load_options(this->options);

#ifdef USELCD
    this->lcd->setup();
#endif
    this->set_next_pattern(0);
    this->set_next_palette(0);
    this->set_next_effect(0);
    this->next_state.pattern_phrase = 0;
    this->next_state.palette_phrase = 0;
    this->next_state.effect_phrase = 0;
    this->set_wled_palette(0); // Default palette
    this->set_wled_pattern(0, 128, 128); // Default pattern

    this->updateTimer.start(STATUS_UPDATE_PERIOD); // Ready to send an update as soon as we're able to
    Serial.println("Patterns: ok");
  }

  void do_pattern_changes() {
    uint16_t phrase = this->current_state.beat_frame >> 12;
    bool changed = false;

    if (phrase >= this->next_state.pattern_phrase) {
#ifdef IDENTIFY_STUCK_PATTERNS
      Serial.println("Time to change pattern");
#endif
      this->load_pattern(this->next_state);
      this->next_state.pattern_phrase = phrase + this->set_next_pattern(phrase);
      changed = true;
    }
    if (phrase >= this->next_state.palette_phrase) {
#ifdef IDENTIFY_STUCK_PATTERNS
      Serial.println("Time to change palette");
#endif
      this->load_palette(this->next_state);
      this->next_state.palette_phrase = phrase + this->set_next_palette(phrase);
      changed = true;
    }
    if (phrase >= this->next_state.effect_phrase) {
#ifdef IDENTIFY_STUCK_PATTERNS
      Serial.println("Time to change effect");
#endif
      this->load_effect(this->next_state);
      this->next_state.effect_phrase = phrase + this->set_next_effect(phrase);
      changed = true;
    }

    if (changed) {
      // For now, WLED doesn't handle transitioning pattern & palette well.
      // Stagger them
      if (this->next_state.pattern_phrase == this->next_state.palette_phrase) {
          this->next_state.palette_phrase += random8(1,3);
      }

      this->next_state.print();
      Serial.println();
    }
  }

  void cancelOverrides() {
    // Release the WLED overrides and take over control of the strip again.
    this->paletteOverrideTimer.stop();
    this->patternOverrideTimer.stop();
  }

  void set_palette_override(uint8_t value) {
    if (value == this->paletteOverride)
      return;
      
    this->paletteOverride = value;
    if (value) {
      Serial.println("WLED has control of palette.");
      this->paletteOverrideTimer.start(300000); // 5 minutes of manual control
    } else {
      Serial.println("Turning off WLED control of palette.");
      this->paletteOverrideTimer.stop();
      this->set_wled_palette(this->current_state.palette_id);
    }
  }

  void set_pattern_override(uint8_t value, uint8_t auto_mode) {
    if (value == DEFAULT_WLED_FX && !this->patternOverride)
      return;
    if (value == this->patternOverride)
      return;

    this->patternOverride = value;
    if (value) {
      Serial.println("WLED has control of patterns.");
      this->patternOverrideTimer.start(300000); // 5 minutes of manual control
      transitionDelay = 500;  // Short transitions
    } else {
      Serial.println("Turning off WLED control of patterns.");
      this->patternOverrideTimer.stop();
      transitionDelay = 8000; // Back to long transitions

      uint8_t param = modeParameter(auto_mode);
      this->set_wled_pattern(auto_mode, param, param);
    }
  }

  void update()
  {
    this->read_keys();
    
    // Update the mesh
    this->node->update();

    // Update patterns to the beat
    this->update_beat();

    Segment& segment = strip.getMainSegment();

    // Detect manual overrides & update the current state to match.
    if (this->paletteOverride && (this->paletteOverrideTimer.ended() || !apActive)) {
      this->set_palette_override(0);
    } else if (segment.palette != this->current_state.palette_id) {
      this->set_palette_override(segment.palette);
    }
    
    uint8_t wled_mode = gPatterns[this->current_state.pattern_id].wled_fx_id;
    if (wled_mode < 10)
      wled_mode = DEFAULT_WLED_FX;
    if (this->patternOverride && (this->patternOverrideTimer.ended() || !apActive)) {
      this->set_pattern_override(0, wled_mode);
    } else if (segment.mode != wled_mode) {
      this->set_pattern_override(segment.mode, wled_mode);
    }

    do_pattern_changes();

    if (this->graphicsTimer.every(REFRESH_PERIOD)) {
      this->updateGraphics();
    }

    // Update current status
    if (this->updateTimer.every(STATUS_UPDATE_PERIOD)) {
      // Transmit less often when following
      if (!this->node->is_following() || random(0, 5) == 0) {
        this->send_update();
      }
    }

    this->updater.update();

#ifdef USELCD
    if (this->lcd->active) {
      this->lcd->size(1);
      this->lcd->write(0,56, this->current_state.beat_frame);
      this->lcd->write(80,56, this->x_axis);
      this->lcd->write(100,56, this->y_axis);
      this->lcd->show();

      this->lcd->update();
    }
#endif
  }

  void handleOverlayDraw() {
    // In manual mode WLED is always active
    if (this->patternOverride) {
      this->wled_fader = 0xFFFF;
      transition_mode_point = 0;
    } else if (wled_fader == 0xFFFF) {
      // When fading down...
      // Wait until the transition has completely changed
      // before switching to new mode
      transition_mode_point = 0xFFFFU;
    } else if (wled_fader == 0) {
      // When fading up...
      // Transition to new mode immediately
      transition_mode_point = 0;
    }

    uint16_t length = strip.getLengthTotal();

    // Crossfade between the custom pattern engine and WLED
    uint8_t fader = this->wled_fader >> 8;
    if (fader < 255) {
      // Perform a cross-fade between current WLED mode and the external buffer
      for (int i = 0; i < length; i++) {
        CRGB c = this->led_strip->getPixelColor(i);
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
    if (this->power_save && this->role < InstallationRole) {
      // Screen door effect to save power
      for (int i = 0; i < length; i++) {
        if (i % 2) {
            strip.setPixelColor(i, CRGB::Black);
        }
      }
    }

    // Draw effects layers over whatever WLED is doing.
    // But not in manual (WLED) mode
    if (!this->patternOverride) {
      this->effects->draw(&strip);
    }

    if (this->flashColor) {
      if (flashTimer.ended())
        this->flashColor = 0;
      else {
        if (millis() % 4000 < 2000) {
          auto chsv = CHSV(this->flashColor, 255, 255);
          for (int i = 0; i < length; i++) {
            strip.setPixelColor(i, CRGB(chsv));
          }
        }
      }
    }

    this->updater.handleOverlayDraw();
  }

  void restart_phrase() {
    this->beats->start_phrase();
    this->update_beat();
    this->send_update();
  }

  void set_phrase_position(uint8_t pos) {
    this->beats->sync(this->beats->bpm, (this->beats->frac & -0xFFF) + (pos<<8));
    this->update_beat();
    this->send_update();
  }
  
  void set_tapped_bpm(accum88 bpm, uint8_t pos=15) {
    // By default, restarts at 15th beat - because this is the end of a tap
    this->beats->sync(bpm, (this->beats->frac & -0xFFF) + (pos<<8));
    this->update_beat();
    this->send_update();
  }

  void update_beat() {
    this->current_state.bpm = this->next_state.bpm = this->beats->bpm;
    this->current_state.beat_frame = particle_beat_frame = this->beats->frac;  // (particle_beat_frame is a hack)
    if (this->current_state.bpm>>8 >= 125)
      this->energy = HighEnergy;
    else if (this->current_state.bpm>>8 > 120)
      this->energy = MediumEnergy;
    else
      this->energy = Chill;
  }
  
  void send_update() {
    Serial.print("     ");
    this->current_state.print();
    Serial.print(F(" "));

    uint16_t phrase = this->current_state.beat_frame >> 12;
    Serial.print(F("    "));
    Serial.print(this->next_state.pattern_phrase - phrase);
    Serial.print(F("P "));
    Serial.print(this->next_state.palette_phrase - phrase);
    Serial.print(F("C "));
    Serial.print(this->next_state.effect_phrase - phrase);
    Serial.print(F("E: "));
    this->next_state.print();
    Serial.print(F(" "));
    Serial.println();    

    this->broadcast_state();
  }

  void background_changed() {
    this->update_background();
    this->current_state.print();
    Serial.println();
  }

  void load_options(ControllerOptions &options) {
    strip.setBrightness(options.brightness);
  }

  void load_pattern(TubeState &tube_state) {
    if (this->current_state.pattern_id == tube_state.pattern_id 
        && this->current_state.pattern_sync_id == tube_state.pattern_sync_id)
      return;

    this->current_state.pattern_phrase = tube_state.pattern_phrase;
    this->current_state.pattern_id = tube_state.pattern_id % gPatternCount;
    this->current_state.pattern_sync_id = tube_state.pattern_sync_id;
    this->isBoring = gPatterns[this->current_state.pattern_id].control.energy == Boring;

    Serial.print(F("Change pattern "));
    this->background_changed();
  }

  bool isShowingWled() {
    return this->current_state.pattern_id >= numInternalPatterns;
  }

  uint8_t modeParameter(uint8_t mode) {
    switch (this->energy) {
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
      if (def.control.energy <= this->energy)
        break;
    }
#ifdef IDENTIFY_STUCK_PATTERNS
    Serial.printf("Next pattern will be %d\n", pattern_id);
#endif

    this->next_state.pattern_id = pattern_id;
    this->next_state.pattern_sync_id = this->randomSyncMode();

    switch (def.control.duration) {
      case ExtraShortDuration: return random8(2, 6);
      case ShortDuration: return random8(5,15);
      case MediumDuration: return random8(15,25);
      case LongDuration: return random8(35,45);
      case ExtraLongDuration: return random8(70, 100);
    }
    return 5;
  }

  void load_palette(TubeState &tube_state) {
    if (this->current_state.palette_id == tube_state.palette_id)
      return;

    this->current_state.palette_phrase = tube_state.palette_phrase;
    this->current_state.palette_id = tube_state.palette_id % gGradientPaletteCount;
    set_wled_palette(this->current_state.palette_id);
  }

  // Choose the palette to display at the next palette cycle
  // Return the number of phrases until the next palette cycle
  uint16_t set_next_palette(uint16_t phrase) {
    // Don't select the built-in palettes
    this->next_state.palette_id = random8(6, gGradientPaletteCount);
    auto phrases = random8(MIN_COLOR_CHANGE_PHRASES, MAX_COLOR_CHANGE_PHRASES);
    if (this->isBoring) {
      phrases /= 2;
    }
    return phrases;
  }

  void load_effect(TubeState &tube_state) {
    if (this->current_state.effect_params.effect == tube_state.effect_params.effect && 
        this->current_state.effect_params.pen == tube_state.effect_params.pen && 
        this->current_state.effect_params.chance == tube_state.effect_params.chance)
      return;

    this->_load_effect(tube_state.effect_params);
  }

  void _load_effect(EffectParameters params) {
    this->current_state.effect_params = params;
  
    Serial.print(F("Change effect "));
    this->current_state.print();
    Serial.println();
    
    this->effects->load(this->current_state.effect_params);
  }

  // Choose the effect to display at the next effect cycle
  // Return the number of phrases until the next effect cycle
  uint16_t set_next_effect(uint16_t phrase) {
    uint8_t effect_num = random8(gEffectCount);

    // Pick a random effect to add; boring patterns get better chance at having an effect.
    EffectDef def = gEffects[effect_num];
    if (def.control.energy > this->energy) {
      def = gEffects[0];
    }

    this->next_state.effect_params = def.params;

    switch (def.control.duration) {
      case ExtraShortDuration: return random(2,3);
      case ShortDuration: return random(2,4);
      case MediumDuration: return random(4,8);
      case LongDuration: return random(6, 14);
      case ExtraLongDuration: return random(10,20);
    }
    return 1;
  }

  void update_background() {
    Background background;
    background.animate = gPatterns[this->current_state.pattern_id].backgroundFn;
    background.wled_fx_id = gPatterns[this->current_state.pattern_id].wled_fx_id;
    background.palette_id = this->current_state.palette_id;
    background.sync = (SyncMode)this->current_state.pattern_sync_id;

    // Use one of the virtual strips to render the patterns.
    // A WLED-based pattern exists on the virtual strip, but causes
    // it to do nothing since WLED merging happens in handleOverlayDraw.
    // Reuse virtual strips to prevent heap fragmentation
    for (uint8_t i = 0; i < NUM_VSTRIPS; i++) {
      this->vstrips[i]->fadeOut();
    }
    this->vstrips[this->next_vstrip]->load(background);
    this->next_vstrip = (this->next_vstrip + 1) % NUM_VSTRIPS; 

    uint8_t param = modeParameter(background.wled_fx_id);
    set_wled_pattern(background.wled_fx_id, param, param);
    set_wled_palette(background.palette_id);
  }

  bool isUnderWledControl() {
    return this->paletteOverride || this->patternOverride;
  }

  void set_wled_palette(uint8_t palette_id) {
    if (this->paletteOverride)
      palette_id = this->paletteOverride;
      
    for (uint8_t i=0; i < strip.getSegmentsNum(); i++) {
      Segment& seg = strip.getSegment(i);
      if (seg.palette == palette_id) continue;
      if (!seg.isActive()) continue;
      seg.startTransition(strip.getTransition());
      seg.palette = palette_id;
    }
    stateChanged = true;
    stateUpdated(CALL_MODE_DIRECT_CHANGE);
  }

  void set_wled_pattern(uint8_t pattern_id, uint8_t speed, uint8_t intensity) {
    if (this->patternOverride)
      pattern_id = this->patternOverride;
    else if (pattern_id == 0)
      pattern_id = DEFAULT_WLED_FX; // Never set it to solid

    // When fading IN, make the pattern transition immediate if possible
    bool fadeIn = this->wled_fader < 2000;
    for (uint8_t i=0; i < strip.getSegmentsNum(); i++) {
      Segment& seg = strip.getSegment(i);
      if (!seg.isActive()) continue;
      if (seg.mode == pattern_id) continue;
      if (fadeIn) {
        seg.startTransition(0);
      } else {
        seg.startTransition(strip.getTransition());
      }
      seg.speed = speed;
      seg.intensity = intensity;
      strip.setMode(i, pattern_id);
    }
    stateChanged = true;
    stateUpdated(CALL_MODE_DIRECT_CHANGE);
  }

  static void set_wled_brightness(uint8_t brightness) {
    strip.setBrightness(brightness);
  }

  void setBrightness(uint8_t brightness) {
    Serial.printf("brightness: %d\n", brightness);

    this->options.brightness = brightness;
    this->broadcast_options();
  }

  void setDebugging(bool debugging) {
    Serial.printf("debugging: %d\n", debugging);

    this->options.debugging = debugging;
    this->broadcast_options();
  }
  
  void togglePowerSave() {
    setPowerSave(!this->power_save);
  }

  void setPowerSave(bool power_save) {
    Serial.printf("power_save: %d\n", power_save);
    this->power_save = power_save;
  }

  void setRole(ControllerRole role) {
    this->role = role;
    Serial.printf("Role = %d", role);
    EEPROM.begin(2560);
    EEPROM.write(ROLE_EEPROM_LOCATION, role);
    EEPROM.end();
    delay(10);
    doReboot = true;
  }
  
  SyncMode randomSyncMode() {
    uint8_t r = random8(128);

    // For boring patterns, up the chance of a sync mode
    if (this->isBoring)
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
    BeatFrame_24_8 beat_frame = this->current_state.beat_frame;

    uint8_t beat_pulse = 0;
    for (int i = 0; i < 8; i++) {
      if ( (beat_frame >> (5+i)) != (lastFrame >> (5+i)))
        beat_pulse |= 1<<i;
    }
    lastFrame = beat_frame;

    this->wled_fader = 0;

    VirtualStrip *first_strip = NULL;
    for (uint8_t i=0; i < NUM_VSTRIPS; i++) {
      VirtualStrip *vstrip = this->vstrips[i];
      if (vstrip->fade == Dead)
        continue;

      // Remember the first strip
      if (first_strip == NULL)
        first_strip = vstrip;

      // Remember the strip that's actually WLED
      if (vstrip->isWled())
        this->wled_fader = vstrip->fader;
     
      vstrip->update(beat_frame, beat_pulse);
      vstrip->blend(this->led_strip->leds, this->led_strip->num_leds, this->options.brightness, vstrip == first_strip);
    }

    this->effects->update(first_strip, beat_frame, (BeatPulse)beat_pulse);
  }

  virtual void acknowledge() {
    addFlash(CRGB::Green);
  }

  void read_keys() {
    if (!Serial.available())
      return;
      
    char c = Serial.read();
    char *k = this->key_buffer;
    uint8_t max = sizeof(this->key_buffer);
    for (uint8_t i=0; *k && (i < max-1); i++) {
      k++;
    }
    if (c == 10) {
      this->keyboard_command(this->key_buffer);
      this->key_buffer[0] = 0;
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
    accum88 arg = this->parse_number(command+1);
    
    switch (command[0]) {
      case 'd':
        this->setDebugging(!this->options.debugging);
        break;
      case '~':
        doReboot = true;
        break;
      case '@':
        this->togglePowerSave();
        break;

      case '-':
        b = this->options.brightness;
        while (*command++ == '-')
          b -= 5;
        this->setBrightness(b - 5);
        break;
      case '+':
        b = this->options.brightness;
        while (*command++ == '+')
          b += 5;
        this->setBrightness(b + 5);
        return;
      case 'l':
        if (arg < 5*256) {
          Serial.println(F("nope"));
          return;
        }
        this->setBrightness(arg >> 8);
        return;

      case 'b':
        if (arg < 60*256) {
          Serial.println(F("nope"));
          return;
        }
        this->beats->set_bpm(arg);
        this->update_beat();
        this->send_update();
        return;

      case 's':
        this->beats->start_phrase();
        this->update_beat();
        this->send_update();
        return;

      case 'n':
        this->force_next();
        return;

      case 'p':
        this->next_state.pattern_phrase = 0;
        this->next_state.pattern_id = arg >> 8;
        this->next_state.pattern_sync_id = All;
        this->broadcast_state();
        return;        

      case 'm':
        this->next_state.pattern_phrase = 0;
        this->next_state.pattern_id = this->current_state.pattern_id;
        this->next_state.pattern_sync_id = arg >> 8;
        this->broadcast_state();
        return;
        
      case 'c':
        this->next_state.palette_phrase = 0;
        this->next_state.palette_id = arg >> 8;
        this->broadcast_state();
        return;
        
      case 'e':
        this->next_state.effect_phrase = 0;
        this->next_state.effect_params = gEffects[(arg >> 8) % gEffectCount].params;
        this->broadcast_state();
        return;

      case '%':
        this->next_state.effect_phrase = 0;
        this->next_state.effect_params = this->current_state.effect_params;
        this->next_state.effect_params.chance = arg;
        this->broadcast_state();
        return;

      case 'i':
        Serial.printf("Reset! ID -> %03X\n", arg >> 4);
        this->node->reset(arg >> 4);
        return;

      case 'U':
      case 'V':
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
        this->broadcast_action(action);
        return;
      }

      case 'P': {
        // Toggle power save
        Action action = {
          .key = command[0],
          .arg = !this->power_save,
        };
        this->broadcast_action(action);
        break;
      }

      case 'r':
        this->setRole((ControllerRole)(arg >> 8));
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
        Serial.println("O - offer an auto-update");
        Serial.println("==== global actions ====");
        Serial.println("A - turn on access point");
        Serial.println("W - forget WiFi client");
        Serial.println("X - restart");
        Serial.println("V### - auto-upgrade to version");
        Serial.println("M - cancel manual pattern override");
        return;

      case 'u':
        this->updater.start();
        return;

      case 'O':
        broadcast_autoupdate();
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
    uint16_t phrase = this->current_state.beat_frame >> 12;
    uint16_t next_phrase = min(this->next_state.pattern_phrase, min(this->next_state.palette_phrase, this->next_state.effect_phrase)) - phrase;
    this->next_state.pattern_phrase -= next_phrase;
    this->next_state.palette_phrase -= next_phrase;
    this->next_state.effect_phrase -= next_phrase;
    this->broadcast_state();
  }

  void broadcast_action(Action& action) {
    if (!this->node->is_following()) {
      this->onAction(&action);
    }
    this->node->sendCommand(COMMAND_ACTION, &action, sizeof(Action));
  }

  void broadcast_info(NodeInfo *info) {
    this->node->sendCommand(COMMAND_INFO, &info, sizeof(NodeInfo));
  }

  void broadcast_state() {
    this->node->sendCommand(COMMAND_STATE, &this->current_state, sizeof(TubeStates));
  }

  void broadcast_options() {
    this->node->sendCommand(COMMAND_OPTIONS, &this->options, sizeof(this->options));
  }

  void broadcast_autoupdate() {
    this->node->sendCommand(COMMAND_UPGRADE, &this->updater.current_version, sizeof(this->updater.current_version));
  }

  virtual void onCommand(CommandId command, void *data) {
    switch (command) {
      case COMMAND_INFO:
        Serial.printf("   \"%s\"\n",
          ((NodeInfo*)data)->message
        );
        return;
  
      case COMMAND_OPTIONS:
        memcpy(&this->options, data, sizeof(this->options));
        this->load_options(this->options);
        Serial.printf("[debug=%d  bri=%d]",
          this->options.debugging,
          this->options.brightness
        );
        return;

      case COMMAND_STATE: {
        auto update_data = (TubeStates*)data;

        TubeState state;
        memcpy(&state, &update_data->current, sizeof(TubeState));
        memcpy(&this->next_state, &update_data->next, sizeof(TubeState));
        state.print();
        this->next_state.print();
  
        // Catch up to this state
        this->load_pattern(state);
        this->load_palette(state);
        this->load_effect(state);
        this->beats->sync(state.bpm, state.beat_frame);
        return;
      }

      case COMMAND_UPGRADE:
        this->updater.start((AutoUpdateOffer*)data);
        return;

      case COMMAND_ACTION:
        this->onAction((Action*)data);
        return;
    }
  
    Serial.printf("UNKNOWN COMMAND %02X", command);
  }

  void onAction(Action* action) {
    switch (action->key) {
      case 'A':
        Serial.println("Turning on WiFi access point.");
        WLED::instance().initAP(true);
        return;

      case 'X':
        doReboot = true;
        return;

      case 'R':
        setRole((ControllerRole)(action->arg));
        return;

      case '@':
        Serial.print("Setting power save to %d\n");
        this->setPowerSave(action->arg);
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
        this->flashTimer.start(20000);
        this->flashColor = action->arg;
        return;

      case 'M':
        Serial.println("cancel manual mode");
        this->cancelOverrides();
        break;

      case 'V':
        // Version check: prepare for update
        if (this->updater.current_version.version >= action->arg)
          break;

        this->updater.ready();
        break;

      case 'U':
        if (this->updater.status == Ready) {
          this->updater.start();
        }
        break;

    }
  }

};
