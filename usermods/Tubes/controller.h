#pragma once

#include "wled.h"
#include "FX.h"
#include "updater.h"

#include "beats.h"

#include "pattern.h"
#include "palettes.h"
#include "effects.h"
#include "global_state.h"
#include "node.h"

const static uint8_t DEFAULT_MASTER_BRIGHTNESS = 200;
#define STATUS_UPDATE_PERIOD 2000

#define MIN_COLOR_CHANGE_PHRASES 2  // 4
#define MAX_COLOR_CHANGE_PHRASES 4  // 40


#define IDENTIFY_STUCK_PATTERNS

typedef struct {
  bool debugging;
  bool power_save;
  uint8_t brightness;

  uint8_t reserved[12];
} ControllerOptions;

typedef struct {
  TubeState current;
  TubeState next;
} TubeStates;

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
    bool isMaster = false;
    uint16_t wled_fader = 0;

    AutoUpdater auto_updater;

    Timer graphicsTimer;
    Timer updateTimer;

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
  
  void setup(bool isMaster)
  {
    this->node->setup();
    this->isMaster = isMaster;
    this->options.power_save = false;
    this->options.debugging = false;
    this->options.brightness = DEFAULT_MASTER_BRIGHTNESS;
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
    VirtualStrip::set_wled_pattern(DEFAULT_WLED_FX, 128, 128);

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

  void update()
  {
    this->read_keys();
    
    // Update the mesh
    this->node->update();

    // Update patterns to the beat
    this->update_beat();

    // Detect manual overrides & update the current state to match.
    Segment& segment = strip.getMainSegment();
    if (segment.palette != this->current_state.palette_id) {
      Serial.printf("Palette override = %d\n",segment.palette);
      this->next_state.palette_phrase = 0;
      this->next_state.palette_id = segment.palette;
      this->broadcast_state();
    }

    /* TODO: detect WLED manual overrides
    bool options_changed = false;
    if (segment.speed != this->options.speed) {
      Serial.printf("WLED FX speed: %d\n",segment.speed);
      this->options.speed = segment.speed;
      options_changed = true;
    }
    if (segment.intensity != this->options.intensity) {
      Serial.printf("WLED FX intensity: %d\n",segment.intensity);
      this->options.intensity = segment.intensity;
      options_changed = true;
    }
    if (options_changed)
      this->broadcast_options();
    */

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

    this->auto_updater.update();
   }

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
    // Crossfade between the custom pattern engine and WLED
    uint8_t fader = this->wled_fader >> 8;
    if (fader < 255) {
      // Perform a cross-fade between current WLED mode and the external buffer
      uint16_t length = strip.getLengthTotal();
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

    if (this->options.power_save) {
      // Screen door effectn
      uint16_t length = strip.getLengthTotal();
      for (int i = 0; i < length; i++) {
        if (i % 2) {
            CRGB c = strip.getPixelColor(i);
            strip.setPixelColor(i, CRGB(c.r>>3,c.g>>3,c.b>>3));
        }
      }
    }

    // Draw effects layers over whatever WLED is doing.
    this->effects->draw(&strip);

    CRGB c;
    switch (this->auto_updater.status) {
      case Started:
      case Connected:
      case Received:
        c = CRGB::Yellow;
        if (millis() % 1000 < 500) {
          c = CRGB::Black;
        }
        break;

      case Failed:
        c = CRGB::Red;
        break;

      case Complete:
        c = CRGB::Green;
        break;

      case Idle:
      default:
        return;
    }
    for (int i = 0; i < 20; i++) {
      strip.setPixelColor(i, c);
    }

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
    if (this->current_state.bpm >= 125>>8)
      this->energy = HighEnergy;
    else if (this->current_state.bpm > 120>>8)
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
      if (def.control.energy < this->energy)
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
    this->_load_palette(tube_state.palette_id);
  }

  void _load_palette(uint8_t palette_id) {
    this->current_state.palette_id = palette_id;
    
    Serial.println("Change palette");
    VirtualStrip::set_wled_palette(palette_id);
  }

  // Choose the palette to display at the next palette cycle
  // Return the number of phrases until the next palette cycle
  uint16_t set_next_palette(uint16_t phrase) {
    // Don't select the built-in palettes
    this->next_state.palette_id = random8(6, gGradientPaletteCount);
    return random8(MIN_COLOR_CHANGE_PHRASES, MAX_COLOR_CHANGE_PHRASES);
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
    Energy maxEnergy = this->energy;
    if (this->isBoring)
      maxEnergy = (Energy)((uint8_t)maxEnergy + 10);
    if (def.control.energy > maxEnergy)
      def = gEffects[0];

    this->next_state.effect_params = def.params;

    switch (def.control.duration) {
      case ExtraShortDuration: return 2;
      case ShortDuration: return 3;
      case MediumDuration: return 6;
      case LongDuration: return 10;
      case ExtraLongDuration: return 20;
    }
    return 1;
  }

  void update_background() {
    Background background;
    background.animate = gPatterns[this->current_state.pattern_id].backgroundFn;
    background.wled_fx_id = gPatterns[this->current_state.pattern_id].wled_fx_id;
    background.palette_id = this->current_state.palette_id;
    background.sync = (SyncMode)this->current_state.pattern_sync_id;

    // re-use virtual strips to prevent heap fragmentation
    for (uint8_t i = 0; i < NUM_VSTRIPS; i++) {
      this->vstrips[i]->fadeOut();
    }
    this->vstrips[this->next_vstrip]->load(background);
    this->next_vstrip = (this->next_vstrip + 1) % NUM_VSTRIPS; 
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
  
  void setPowerSave(bool power_save) {
    Serial.printf("power_save: %d\n", power_save);

    this->options.power_save = power_save;
    this->broadcast_options();
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
      case '@':
        this->setPowerSave(!this->options.power_save);
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

      case 'g':
        for (int i=0; i< 10; i++)
          addGlitter();
        return;

      case '?':
        Serial.println(F("b###.# - set bpm"));
        Serial.println(F("s - start phrase"));
        Serial.println();
        Serial.println(F("p### - patterns"));
        Serial.println(F("m### - sync mode"));
        Serial.println(F("c### - colors"));
        Serial.println(F("e### - effects"));
        Serial.println("w### - wled pattern");
        Serial.println();
        Serial.println(F("i### - set ID"));
        Serial.println(F("d - toggle debugging"));
        Serial.println(F("l### - brightness"));
        Serial.println("@ - power save mode");
        Serial.println("U - begin auto-update");
        return;

      case 'U':
        this->auto_updater.start();
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

  void broadcast_state() {
    this->node->sendCommand(COMMAND_UPDATE, &this->current_state, sizeof(TubeStates));
  }

  void broadcast_options() {
    this->node->sendCommand(COMMAND_OPTIONS, &this->options, sizeof(this->options));
  }

  virtual void onCommand(CommandId command, void *data) {
    switch (command) {
      case COMMAND_RESET:
        // TODO
        return;
  
      case COMMAND_OPTIONS:
        memcpy(&this->options, data, sizeof(this->options));
        this->load_options(this->options);
        Serial.printf("[debug=%d  bri=%d]",
          this->options.debugging,
          this->options.brightness
        );
        return;

      case COMMAND_UPDATE: {
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
    }
  
    Serial.printf("UNKNOWN COMMAND %02X", command);
  }

};
