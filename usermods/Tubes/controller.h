#pragma once

#include "beats.h"

#include "pattern.h"
#include "palettes.h"
#include "effects.h"
#include "global_state.h"
#include "radio.h"

const static uint8_t DEFAULT_MASTER_BRIGHTNESS = 200;

const static CommandId COMMAND_HELLO = 0x00;
const static CommandId COMMAND_OPTIONS = 0x10;
const static CommandId COMMAND_UPDATE = 0x20;
const static CommandId COMMAND_NEXT = 0x30;
const static CommandId COMMAND_RESET = 0xF0;

const static CommandId COMMAND_BRIGHTNESS = 0x80;
const static CommandId COMMAND_FIREWORK = 0x90;


typedef struct {
  bool debugging;
  uint8_t brightness;
} ControllerOptions;

#define NEXT_PATTERN_TIME 53000
#define NEXT_PALETTE_TIME 27000

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
    const static int FRAMES_PER_SECOND = 300;  // how often we animate, in frames per second
    const static int REFRESH_PERIOD = 1000 / FRAMES_PER_SECOND;  // how often we animate, in milliseconds

    uint8_t num_leds;
    VirtualStrip *vstrips[NUM_VSTRIPS];
    uint8_t next_vstrip = 0;
    bool isMaster = false;
    
    Timer graphicsTimer;
    Timer updateTimer;

#ifdef USELCD
    Lcd *lcd;
#endif
    LEDs *led_strip;
    BeatController *beats;
    Radio *radio;
    Effects *effects;

    ControllerOptions options;
    char key_buffer[20] = {0};

    Energy energy=LowEnergy;
    TubeState current_state;
    TubeState next_state;

  PatternController(uint8_t num_leds, BeatController *beats, Radio *radio) {
    this->num_leds = num_leds;
#ifdef USELCD
    this->lcd = new Lcd();
#endif
    this->led_strip = new LEDs(num_leds);
    this->beats = beats;
    this->radio = radio;
    this->effects = new Effects();

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
    this->isMaster = isMaster;
    this->options.debugging = false;
    this->options.brightness = DEFAULT_MASTER_BRIGHTNESS;

#ifdef USELCD
    this->lcd->setup();
#endif
    this->set_next_pattern(0);
    this->set_next_palette(0);
    this->set_next_effect(0);
    this->next_state.pattern_phrase = 0;
    this->next_state.palette_phrase = 0;
    this->next_state.effect_phrase = 0;
    Serial.println(F("Patterns: ok"));

    this->radio->setup(this->isMaster);
    this->radio->sendCommand(COMMAND_HELLO);

    this->updateTimer.start(RADIO_SENDPERIOD); // Ready to send an update as soon as we're able to
  }

  void update()
  {
    this->read_keys();

    // Update patterns to the beat
    this->update_beat();

    uint16_t phrase = this->current_state.beat_frame >> 12;
    if (phrase >= this->next_state.pattern_phrase) {
      this->load_pattern(this->next_state);
      this->next_state.pattern_phrase = phrase + this->set_next_pattern(phrase);
    }
    if (phrase >= this->next_state.palette_phrase) {
      this->load_palette(this->next_state);
      this->next_state.palette_phrase = phrase + this->set_next_palette(phrase);
    }
    if (phrase >= this->next_state.effect_phrase) {
      this->load_effect(this->next_state);
      this->next_state.effect_phrase = phrase + this->set_next_effect(phrase);
    }

    // Update current status
    if (this->updateTimer.ended()) {
      this->send_update();
    }

    this->radio->receiveCommands(this);

    if (this->graphicsTimer.every(REFRESH_PERIOD)) {
      this->updateGraphics();
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
      this->energy = LowEnergy;
  }
  
  void send_update() {
    this->current_state.print();
    Serial.print(F(" "));

    this->radio->sendCommand(COMMAND_UPDATE, &this->current_state, sizeof(this->current_state));
    this->updateTimer.snooze(RADIO_SENDPERIOD);

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
    this->radio->sendCommand(COMMAND_NEXT, &this->next_state, sizeof(this->next_state));
    Serial.println();    
  }

  void background_changed() {
    this->update_background();
    this->current_state.print();
    Serial.println();
  }

  void load_pattern(TubeState &tube_state) {
    if (this->current_state.pattern_id == tube_state.pattern_id 
        && this->current_state.pattern_sync_id == tube_state.pattern_sync_id)
      return;

    this->current_state.pattern_phrase = tube_state.pattern_phrase;
    this->current_state.pattern_id = tube_state.pattern_id % gPatternCount;
    this->current_state.pattern_sync_id = tube_state.pattern_sync_id;

    Serial.print(F("Change pattern "));
    this->background_changed();
  }

  uint16_t set_next_pattern(uint16_t phrase) {
    uint8_t pattern_id = random8(gPatternCount);
    PatternDef def = gPatterns[pattern_id];
    if (def.control.energy > this->energy) {
      pattern_id = 0;
      def = gPatterns[0];
    }

    this->next_state.pattern_id = pattern_id;
    this->next_state.pattern_sync_id = this->randomSyncMode();

    switch (def.control.duration) {
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
    this->current_state.palette_id = palette_id % gGradientPaletteCount;
    
    Serial.print(F("Change palette"));
    this->background_changed();
  }

  uint16_t set_next_palette(uint16_t phrase) {
    this->next_state.palette_id = random8(gGradientPaletteCount);
    return random8(4,40);
  }

  void load_effect(TubeState &tube_state) {
    if (this->current_state.effect_params.effect == tube_state.effect_params.effect && 
        this->current_state.effect_params.pen == tube_state.effect_params.pen && 
        this->current_state.effect_params.chance == tube_state.effect_params.chance)
      return;

    this->current_state.palette_id = tube_state.palette_id % gGradientPaletteCount;
    this->_load_effect(tube_state.effect_params);
  }

  void _load_effect(EffectParameters params) {
    this->current_state.effect_params = params;
  
    Serial.print(F("Change effect "));
    this->current_state.print();
    Serial.println();
    
    this->effects->load(this->current_state.effect_params);
  }

  uint16_t set_next_effect(uint16_t phrase) {
    uint8_t effect_num = random8(gEffectCount);

    EffectDef def = gEffects[effect_num];
    if (def.control.energy > this->energy)
      def = gEffects[0];

    this->next_state.effect_params = def.params;

    switch (def.control.duration) {
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
    background.palette = gGradientPalettes[this->current_state.palette_id];
    background.sync = (SyncMode)this->current_state.pattern_sync_id;

    // re-use virtual strips to prevent heap fragmentation
    for (uint8_t i = 0; i < NUM_VSTRIPS; i++) {
      this->vstrips[i]->fadeOut();
    }
    this->vstrips[this->next_vstrip]->load(background);
    this->next_vstrip = (this->next_vstrip + 1) % NUM_VSTRIPS; 
  }

  void optionsChanged() {
    if (this->isMaster) {
      this->radio->sendCommand(COMMAND_OPTIONS, &options, sizeof(options));
    }
  }

  void setBrightness(uint8_t brightness) {
    Serial.print(F("brightness "));
    Serial.println(brightness);

    this->options.brightness = brightness;
    this->optionsChanged();
  }

  void setDebugging(bool debugging) {
    Serial.print(F("debugging "));
    Serial.println(debugging);

    this->options.debugging = debugging;
    this->optionsChanged();
  }
  
  SyncMode randomSyncMode() {
    uint8_t r = random8(128);
    if (r < 40)
      return SinDrift;
    if (r < 65)
      return Pulse;
    if (r < 72)
      return Swing;
    if (r < 84)
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

    VirtualStrip *first_strip = NULL;
    for (uint8_t i=0; i < NUM_VSTRIPS; i++) {
      VirtualStrip *vstrip = this->vstrips[i];
      if (vstrip->fade == Dead)
        continue;

      // Remember the first strip
      if (first_strip == NULL)
        first_strip = vstrip;
     
      vstrip->update(beat_frame, beat_pulse);
      vstrip->blend(this->led_strip->leds, this->led_strip->num_leds, this->options.brightness, vstrip == first_strip);
    }

    this->effects->update(first_strip, beat_frame, (BeatPulse)beat_pulse);
    this->effects->draw(this->led_strip->leds, this->num_leds);    
  }

  virtual void acknowledge() {
    addFlash();
  }

  virtual void onCommand(uint8_t fromId, CommandId command, void *data) {
    if (fromId) {
      Serial.print(F("From "));
      Serial.print(fromId);
      Serial.print(F(": "));
    }
    
    switch (command) {
      case COMMAND_FIREWORK:
        Serial.print(F("fireworks"));
        this->acknowledge();
        return;
  
      case COMMAND_RESET:
        Serial.print(F("reset"));
        return;
  
      case COMMAND_BRIGHTNESS: {
        uint8_t *bright = (uint8_t *)data;
        this->setBrightness(*bright);
        return;
      }
  
      case COMMAND_HELLO:
        Serial.print(F("hello"));
        this->updateTimer.stop();
        return;
  
      case COMMAND_OPTIONS: {
        Serial.print(F("options"));
        memcpy(&this->options, data, sizeof(this->options));
        return;
      }

      case COMMAND_NEXT: {
        Serial.print(F(" next "));

        memcpy(&this->next_state, data, sizeof(TubeState));
        this->next_state.print();
        Serial.print(F(" (obeying)"));
        return;
      }
  
      case COMMAND_UPDATE: {
        Serial.print(F(" update "));

        TubeState state;
        memcpy(&state, data, sizeof(TubeState));
        state.print();
        Serial.print(F(" (obeying)"));
  
        // Catch up to this state
        this->load_pattern(state);
        this->load_palette(state);
        this->load_effect(state);
        this->beats->sync(state.bpm, state.beat_frame);
        return;
      }
    }
  
    Serial.print(F("UNKNOWN "));
    Serial.print(command, HEX);
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
    uint8_t b;
    accum88 arg = this->parse_number(command+1);
    
    switch (command[0]) {
      case 'f':
        this->radio->sendCommandFrom(255, COMMAND_FIREWORK, NULL, 0);
        this->onCommand(0, COMMAND_FIREWORK, NULL);
        Serial.println();
        break;

      case 'i':
        this->radio->mesh_node.reset(arg >> 8);
        break;

      case 'd':
        this->setDebugging(!this->options.debugging);
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
        this->update_next();
        return;        
        
      case 'm':
        this->next_state.pattern_phrase = 0;
        this->next_state.pattern_id = this->current_state.pattern_id;
        this->next_state.pattern_sync_id = arg >> 8;
        this->update_next();
        return;
        
      case 'c':
        this->next_state.palette_phrase = 0;
        this->next_state.palette_id = arg >> 8;
        this->update_next();
        return;
        
      case 'e':
        this->next_state.effect_phrase = 0;
        this->next_state.effect_params = gEffects[(arg >> 8) % gEffectCount].params;
        this->update_next();
        return;

      case '%':
        this->next_state.effect_phrase = 0;
        this->next_state.effect_params = this->current_state.effect_params;
        this->next_state.effect_params.chance = arg;
        this->update_next();
        return;

      case 'h':
        // Pretend to receive a HELLO
        this->onCommand(0, COMMAND_HELLO, NULL);
        Serial.println();
        return;

      case 'g':
        for (int i=0; i< 10; i++)
          addGlitter();
        break;

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
    }
  }

  void force_next() {
    uint16_t phrase = this->current_state.beat_frame >> 12;
    uint16_t next_phrase = min(this->next_state.pattern_phrase, min(this->next_state.palette_phrase, this->next_state.effect_phrase)) - phrase;
    this->next_state.pattern_phrase -= next_phrase;
    this->next_state.palette_phrase -= next_phrase;
    this->next_state.effect_phrase -= next_phrase;
    this->update_next();
  }

  void update_next() {
    this->radio->sendCommand(COMMAND_NEXT, &this->next_state, sizeof(this->next_state));
  }

};



// What's interesting?
// c53 - clouds
// m4 - swing drift
