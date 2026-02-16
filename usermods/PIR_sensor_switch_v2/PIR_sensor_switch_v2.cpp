/*
 * Rawframe
 * Usermod for PIR sensor motion detection.
 *
 * This usermod handles PIR sensor states and triggers actions (presets) based on motion.
 * It supports multiple PIR sensors and multiple actions, with configurable linking.
 * 
 * Features:
 * - Multiple PIR sensors
 * - Multiple Actions (On/Off presets, delays)
 * - Flexible linking between PIRs and Actions
 * - Web UI for configuration and status
 * - API for external control
 */

#include "wled.h"

// ---------- Configuration defaults ----------
#ifndef PIR_SENSOR_PIN
  #ifdef ARDUINO_ARCH_ESP32
    #define PIR_SENSOR_PIN 23
  #else
    #define PIR_SENSOR_PIN 13
  #endif
#endif

#ifndef PIR_SENSOR_MAX
  #define PIR_SENSOR_MAX 2
#endif

#ifndef ACTION_MAX
  #define ACTION_MAX 2
#endif

#ifndef PRESET_FIFO_SIZE
  #define PRESET_FIFO_SIZE 16
#endif

#ifndef PRESET_SPACING_MS
  #define PRESET_SPACING_MS 5
#endif

static const char _name[] PROGMEM = "MotionDetection";

class MotionDetectionUsermod : public Usermod {
private:
  bool initDone = false;
  unsigned long lastLoop = 0;

  // PIR config/runtime
  bool   pirEnabled[PIR_SENSOR_MAX] = { true };
  int8_t pirPins[PIR_SENSOR_MAX]    = { PIR_SENSOR_PIN };
  bool   pirState[PIR_SENSOR_MAX]   = { LOW };

  // Per-PIR links to actions (initialized at runtime to match ACTION_MAX)
  // OPTIMIZATION: Use bitmask instead of 2D bool array to save RAM
  uint8_t pirActions[PIR_SENSOR_MAX];

  // Action profile (presets/timers/contributors). Enabled flag removed from struct usage.
  struct ActionProfile {
    uint8_t onPreset;
    uint8_t offPreset;
    uint32_t offDelayMs;
    bool contrib[PIR_SENSOR_MAX];
    uint8_t activeCount;
    unsigned long offStartMs;
    ActionProfile() {
      onPreset = 0;
      offPreset = 0;
      offDelayMs = 600 * 1000UL;
      activeCount = 0;
      offStartMs = 0;
      for (int i = 0; i < PIR_SENSOR_MAX; i++) contrib[i] = false;
    }
  } actions[ACTION_MAX];

  // Authoritative simple boolean array for enabled state (PIR-like)
  bool actionEnabled[ACTION_MAX] = { true };

  // Preset FIFO
  uint8_t presetFifo[PRESET_FIFO_SIZE];
  uint8_t fifoHead = 0;
  uint8_t fifoTail = 0;
  unsigned long lastPresetApplyMs = 0;



  inline bool fifoEmpty() { return fifoHead == fifoTail; }
  inline bool fifoFull()  { return ((fifoHead + 1) % PRESET_FIFO_SIZE) == fifoTail; }

  inline void enqueuePreset(uint8_t preset) {
    if (preset == 0) return;
    // OPTIMIZATION: Prevent flooding by ignoring duplicate sequential requests
    if (!fifoEmpty() && presetFifo[(fifoHead + PRESET_FIFO_SIZE - 1) % PRESET_FIFO_SIZE] == preset) return;
    
    if (fifoFull()) fifoTail = (fifoTail + 1) % PRESET_FIFO_SIZE; // drop oldest
    presetFifo[fifoHead] = preset;
    fifoHead = (fifoHead + 1) % PRESET_FIFO_SIZE;
  }

  void processPresetFifo() {
    if (fifoEmpty()) return;
    unsigned long now = millis();
    if (now - lastPresetApplyMs < PRESET_SPACING_MS) return;
    uint8_t preset = presetFifo[fifoTail];
    fifoTail = (fifoTail + 1) % PRESET_FIFO_SIZE;
    lastPresetApplyMs = now;
    applyPreset(preset, CALL_MODE_BUTTON_PRESET);
  }

public:
  MotionDetectionUsermod() {}
  ~MotionDetectionUsermod() {}

  /*
   * readFromConfig() is called prior to setup() to read configuration from cfg.json
   * You can use it to initialize variables, sensors or similar.
   */
  bool readFromConfig(JsonObject &root) override {
    if (!root.containsKey(FPSTR(_name))) return false;
    JsonObject top = root[FPSTR(_name)];

    // PIRs
    char key[16]; // OPTIMIZATION: Buffer for snprintf to avoid String allocation
    for (int i = 0; i < PIR_SENSOR_MAX; i++) {
      snprintf(key, sizeof(key), "PIR %d", i + 1);
      if (!top.containsKey(key)) continue;
      JsonObject p = top[key];
      if (!p.isNull()) {
        if (p.containsKey("pin")) pirPins[i] = p["pin"] | pirPins[i];
        if (p.containsKey("enabled")) pirEnabled[i] = p["enabled"].as<bool>(); // FIX: Explicit cast
        for (int a = 0; a < ACTION_MAX; a++) {
          snprintf(key, sizeof(key), "Action %d", a + 1);
          if (p.containsKey(key)) {
             bool linked = p[key].as<bool>(); // FIX: Explicit cast for Link Action checkboxes
             if (linked) pirActions[i] |= (1 << a);
             else        pirActions[i] &= ~(1 << a);
          }
        }
      }
    }

    // Actions: read into struct fields and read enabled into boolean array
    for (int a = 0; a < ACTION_MAX; a++) {
      snprintf(key, sizeof(key), "Action %d", a + 1);
      if (!top.containsKey(key)) continue;
      JsonObject ap = top[key];
      if (!ap.isNull()) {
        if (ap.containsKey("onPreset")) actions[a].onPreset = ap["onPreset"] | actions[a].onPreset;
        if (ap.containsKey("offPreset")) actions[a].offPreset = ap["offPreset"] | actions[a].offPreset;
        if (ap.containsKey("offSec")) {
          uint32_t s = ap["offSec"] | (actions[a].offDelayMs / 1000UL);
          if (s > 4294967) s = 4294967;  // Max ~49.7 days, prevents overflow when multiplied by 1000
          actions[a].offDelayMs = s * 1000UL;
        }
        if (ap.containsKey("enabled")) {
          actionEnabled[a] = ap["enabled"].as<bool>(); // FIX: Explicit cast
        }
      }
    }

    return true;
  }

  /*
   * setup() is called once at boot. WiFi is not yet connected at this point.
   * readFromConfig() is called prior to setup()
   */
  void setup() override {
    // allocate pins and initialize states
    for (int i = 0; i < PIR_SENSOR_MAX; i++) {
      pirState[i] = LOW;
      if (pirPins[i] < 0) { pirEnabled[i] = false; continue; }
      if (PinManager::allocatePin(pirPins[i], false, PinOwner::UM_PIR)) {
        #ifdef ESP8266
          pinMode(pirPins[i], pirPins[i] == 16 ? INPUT_PULLDOWN_16 : INPUT_PULLUP);
        #else
          pinMode(pirPins[i], INPUT_PULLDOWN);
        #endif
        pirState[i] = digitalRead(pirPins[i]);
      } else {
        DEBUG_PRINT(F("PIR pin allocation failed: "));
        DEBUG_PRINTLN(pirPins[i]);
        pirPins[i] = -1;
        pirEnabled[i] = false;
      }
    }

    // initialize action runtime state (pirActions loaded from config, not overwritten here!)
    for (int a = 0; a < ACTION_MAX; a++) {
      // actionEnabled[a] = true; // REMOVED: Do not overwrite config/state!
      actions[a].activeCount = 0;
      actions[a].offStartMs = 0;
      for (int i = 0; i < PIR_SENSOR_MAX; i++) actions[a].contrib[i] = false;
    }

    fifoHead = fifoTail = 0;
    lastPresetApplyMs = 0;

    initDone = true;
  }

  /*
   * loop() is called continuously. Here you can check for events, read sensors, etc.
   */
  void loop() override {
    if (!initDone) return;

    processPresetFifo();

    // poll PIRs ~5Hz
    if (millis() - lastLoop < 200) return;
    lastLoop = millis();

    for (int i = 0; i < PIR_SENSOR_MAX; i++) {
      if (!pirEnabled[i] || pirPins[i] < 0) continue;

      bool pin = digitalRead(pirPins[i]);
      if (pin == pirState[i]) continue;
      pirState[i] = pin;

      if (pin == HIGH) {
        // rising edge: mark contributor and manage action
        for (int a = 0; a < ACTION_MAX; a++) {
          if (!actionEnabled[a]) continue;    // use simple boolean gating (PIR-like)
          if (!((pirActions[i] >> a) & 0x01)) continue; // OPTIMIZATION: Bitmask check

          if (!actions[a].contrib[i]) {
            actions[a].contrib[i] = true;
            actions[a].activeCount++;
          }

          // if action just became active, enqueue onPreset; always clear offStartMs
          if (actions[a].activeCount == 1) {
            // OPTIMIZATION: Removed redundant offStartMs = 0
            if (actions[a].onPreset) enqueuePreset(actions[a].onPreset);
          } else {
            // OPTIMIZATION: Removed redundant offStartMs = 0
          }
        }
      } else {
        // falling edge: remove contributor, maybe start unified off timer
        for (int a = 0; a < ACTION_MAX; a++) {
          if (!actionEnabled[a]) continue;
          if (!((pirActions[i] >> a) & 0x01)) continue; // OPTIMIZATION: Bitmask check

          if (actions[a].contrib[i]) {
            actions[a].contrib[i] = false;
            if (actions[a].activeCount > 0) actions[a].activeCount--;
            if (actions[a].activeCount == 0) {
              actions[a].offStartMs = millis();
            }
          }
        }
      }
    }

    // check unified off timers
    unsigned long now = millis();
    for (int a = 0; a < ACTION_MAX; a++) {
      if (!actionEnabled[a]) continue;
      if (actions[a].activeCount == 0) {
        if (actions[a].offStartMs > 0 && now - actions[a].offStartMs > actions[a].offDelayMs) {
          actions[a].offStartMs = 0;
          if (actions[a].offPreset) enqueuePreset(actions[a].offPreset);
        }
      } else {
        // OPTIMIZATION: Removed redundant offStartMs = 0
      }
    }

    processPresetFifo();
  }

  /*
   * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
   * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
   */
  void addToJsonInfo(JsonObject &root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    char buf[64]; // OPTIMIZATION: Buffer for snprintf
    // PIR blocks
    for (int i = 0; i < PIR_SENSOR_MAX; i++) {
      snprintf(buf, sizeof(buf), "PIR %d", i+1);
      JsonArray infoArr = user.createNestedArray(buf);

      if (pirPins[i] < 0) {
        infoArr.add("⛔ PIR not configured");
        continue;
      }

      String uiDomString;
      uiDomString  = F("<button class=\"btn btn-xs\" onclick=\"requestJson({");
      uiDomString += FPSTR(_name);
      uiDomString += F(":{pir"); uiDomString += String(i);
      uiDomString += F(":");
      uiDomString += (pirEnabled[i] ? F("false") : F("true"));
      uiDomString += F("}})\">");
      uiDomString += (pirEnabled[i] ? F("<i class=\"icons on\">&#xe325;</i>") : F("<i class=\"icons off\">&#xe08f;</i>"));
      uiDomString += F("</button>");

      infoArr.add(pirEnabled[i] ? (pirState[i] ? "● motion" : "○ idle") : "⛔ disabled");
      infoArr.add(uiDomString);

      String linked = "";
      bool any = false;
      for (int a = 0; a < ACTION_MAX; a++) {
        if ((pirActions[i] >> a) & 0x01) { // OPTIMIZATION: Bitmask check
          if (any) linked += ", ";
          linked += String(a+1);
          any = true;
        }
      }
      infoArr.add(any ? String("🔗 actions: ") + linked : "🔗 no actions linked");
    }

    // Action blocks: use PIR-like keys and boolean array
    unsigned long now = millis();
    for (int a = 0; a < ACTION_MAX; a++) {
      snprintf(buf, sizeof(buf), "Action %d", a+1);
      JsonArray infoArr = user.createNestedArray(buf);

      bool en = actionEnabled[a];

      // status first
      String status;
      if (!en) {
        status = "⛔ disabled";
      } else {
        bool active = actions[a].activeCount > 0;
        uint32_t remain = 0;
        // Calculate remaining time, avoiding underflow if timer already elapsed
        if (!active && actions[a].offStartMs > 0) {
          unsigned long elapsed = now - actions[a].offStartMs;
          if (elapsed < actions[a].offDelayMs) {
            remain = (actions[a].offDelayMs - elapsed) / 1000;
          }
        }
        if (active) status = "⏱ active";
        else if (remain > 0) status = String("⏱ off in ") + String(remain) + "s";
        else status = "○ inactive";
      }
      infoArr.add(status); // MUST be first

      // button second (PIR-style key "actionN")
      String uiDomString;
      uiDomString  = F("<button class=\"btn btn-xs\" onclick=\"requestJson({");
      uiDomString += FPSTR(_name);
      uiDomString += F(":{action"); uiDomString += String(a);
      uiDomString += F(":");
      uiDomString += (en ? F("false") : F("true"));
      uiDomString += F("}})\">");
      uiDomString += (en ? F("<i class=\"icons on\">&#xe325;</i>") : F("<i class=\"icons off\">&#xe08f;</i>"));
      uiDomString += F("</button>");
      infoArr.add(uiDomString); // MUST be second

      // details
      String onP = actions[a].onPreset ? String(actions[a].onPreset) : String("none");
      String offP = actions[a].offPreset ? String(actions[a].offPreset) : String("none");
      infoArr.add(String("On: ") + onP + String("  Off: ") + offP);
      infoArr.add(String("Delay: ") + String(actions[a].offDelayMs / 1000UL) + "s");
    }


  }

  /*
   * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
   * Values in the state object may be modified by connected clients
   */
  void readFromJsonState(JsonObject &root) override {
    // accept nested under usermod name or flat keys
    JsonObject um = root.containsKey(FPSTR(_name)) ? root[FPSTR(_name)] : root;
    bool anyChange = false;

    // PIR keys
    for (int i = 0; i < PIR_SENSOR_MAX; i++) {
      String k = "pir" + String(i);
      if (!um.containsKey(k)) continue;
      bool en = um[k].is<bool>() ? um[k].as<bool>() : (um[k].as<int>() != 0);
      if (pirEnabled[i] != en) {
        pirEnabled[i] = en;
        anyChange = true;
        if (!en) {
          for (int a = 0; a < ACTION_MAX; a++) {
            if (actions[a].contrib[i]) {
              actions[a].contrib[i] = false;
              if (actions[a].activeCount > 0) actions[a].activeCount--;
            }
          }
        }
      }
    }

    // Action keys: accept actionN, actionN_on, actionN_off, 1-based actionN
    // OPTIMIZATION: Use pointer arithmetic to parse keys without creating Strings
    for (JsonPair p : um) {
      const char* key = p.key().c_str();
      if (strncmp(key, "action", 6) != 0) continue;

      // extract numeric suffix
      const char* ptr = key + 6; // after "action"
      if (!isdigit(*ptr)) continue;
      
      int idx = 0;
      while (isdigit(*ptr)) {
        idx = idx * 10 + (*ptr - '0');
        ptr++;
      }

      // tail after digits (may be "_on", "_off", or empty)
      const char* tail = ptr;
      if (*tail == '_') tail++; // skip underscore

      // map 1-based keys (action1 -> index 0) if needed
      int mappedIdx = -1;
      if (idx >= 0 && idx < ACTION_MAX && *tail == '\0') {
        mappedIdx = idx; // action0 style
      } else if (idx >= 1 && idx <= ACTION_MAX && *tail == '\0') {
        mappedIdx = idx - 1; // action1 style -> index 0
      } else if (idx >= 0 && idx < ACTION_MAX) {
        mappedIdx = idx; // action0_on style
      } else {
        continue;
      }

      // determine desired enabled state
      bool en = false;
      bool found = false;

      if (*tail != '\0') {
        if (strcasecmp(tail, "on") == 0 || strcasecmp(tail, "enable") == 0) {
          en = true;
          found = true;
        } else if (strcasecmp(tail, "off") == 0 || strcasecmp(tail, "disable") == 0) {
          en = false;
          found = true;
        } else {
          // fallback: read value if present
          en = um[key].is<bool>() ? um[key].as<bool>() : (um[key].as<int>() != 0);
          found = true;
        }
      } else {
        // no tail: use the provided value (true/false or numeric)
        en = um[key].is<bool>() ? um[key].as<bool>() : (um[key].as<int>() != 0);
        found = true;
      }

      if (!found) continue;
      if (mappedIdx < 0 || mappedIdx >= ACTION_MAX) continue;

      // set simple boolean and clear runtime contributor state when toggling
      if (actionEnabled[mappedIdx] != en) {
        actionEnabled[mappedIdx] = en;
        anyChange = true;
        // when disabling, clear contributors and active counts
        if (!en) {
          for (int i = 0; i < PIR_SENSOR_MAX; i++) {
            if (actions[mappedIdx].contrib[i]) {
              actions[mappedIdx].contrib[i] = false;
            }
          }
          actions[mappedIdx].activeCount = 0;
          actions[mappedIdx].offStartMs = 0;
        } else {
          // enabling: ensure timers cleared
          actions[mappedIdx].offStartMs = 0;
        }
      }
    }

    if (anyChange) stateUpdated(CALL_MODE_BUTTON);
  }

  /*
   * addToConfig() can be used to add custom persistent settings to the cfg.json file in the "um" (usermod) object.
   * It will be called by WLED when settings are actually saved (for example, LED settings are saved)
   */
  void addToConfig(JsonObject &root) override {
    JsonObject top = root.createNestedObject(FPSTR(_name));

    char key[16]; // OPTIMIZATION: Buffer for snprintf
    // PIRs
    for (int i = 0; i < PIR_SENSOR_MAX; i++) {
      snprintf(key, sizeof(key), "PIR %d", i + 1);
      JsonObject p = top.createNestedObject(key);
      p["pin"] = pirPins[i];
      p["enabled"] = pirEnabled[i];
      for (int a = 0; a < ACTION_MAX; a++) {
        snprintf(key, sizeof(key), "Action %d", a + 1);
        p[key] = (bool)((pirActions[i] >> a) & 0x01); // OPTIMIZATION: Bitmask check
      }
    }

    // Actions
    for (int a = 0; a < ACTION_MAX; a++) {
      snprintf(key, sizeof(key), "Action %d", a + 1);
      JsonObject ap = top.createNestedObject(key);
      ap["enabled"] = actionEnabled[a];
      ap["onPreset"] = actions[a].onPreset;
      ap["offPreset"] = actions[a].offPreset;
      ap["offSec"] = actions[a].offDelayMs / 1000UL;
    }
  }

  /*
   * appendConfigData() is called when user enters usermod settings page
   * it may add additional metadata for certain entry fields (adding drop down is possible)
   */
  void appendConfigData() override {
    // Indicator to show the mod is active (will be updated by JS)
    oappend(F("addInfo('MotionDetection:PIR 1:enabled',1,'<b id=\"um-status\" style=\"display:none;\"></b>');"));

    // Inject CSS via JS
    // Scoped CSS to #pir-root
    // Removed border from .um-g to prevent double lines between blocks
    // Added border-top and border-bottom to #pir-root for the only desired lines
    // OPTIMIZATION: Combined strings
    oappend(F("var s=document.createElement('style');s.innerHTML='#pir-root{border-top:1px solid #444;border-bottom:1px solid #444;padding:10px 0;margin:10px 0} #pir-root .um-g{display:flex;flex-wrap:wrap;justify-content:center;gap:10px;margin:4px 0;padding:10px;border-radius:5px;background:#222} #pir-root .um-h{width:100%;text-align:center;font-weight:bold;margin-bottom:5px;color:#fca} #pir-root .um-i{display:flex;flex-direction:column;align-items:center;background:#333;padding:5px;border-radius:4px;min-width:80px;border:1px solid #555} #pir-root .um-i label{font-size:0.8em;margin-bottom:2px;color:#aaa} #pir-root .um-i input[type=\"checkbox\"]{margin:5px} #pir-root .um-i input[type=\"number\"],#pir-root .um-i select{width:70px;text-align:center} #pir-root hr{display:none !important} #pir-root br{display:none}';document.head.appendChild(s);"));

    // Inject Layout Logic
    // OPTIMIZATION: Combined strings
    oappend(F("function aUM(){"
      "var st=document.getElementById('um-status');"
      "try {"
        "var count=0;"
        // Fix Main Heading
        "const headers = document.querySelectorAll('h3');"
        "let header = null;"
        "headers.forEach(h=>{ if(h.textContent.trim()==='MotionDetection'){ header=h; h.textContent='Motion Detection'; h.style.marginTop='20px'; h.style.color=''; h.style.textDecoration='none'; } });"

        // Create Root Container
        "if(header && !document.getElementById('pir-root')){"
          "const root = document.createElement('div'); root.id='pir-root';"
          "header.parentNode.insertBefore(root, header);"
          "root.appendChild(header);"
          "let next = root.nextSibling;"
          "while(next && next.tagName!=='H3' && next.tagName!=='BUTTON' && !(next.tagName==='DIV' && next.className==='overlay')){"
            "let curr = next; next = next.nextSibling; root.appendChild(curr);"
          "}"
        "}"
        
        // Selector 'f'
        "const f=(k)=>{var e=document.getElementsByName(`MotionDetection:${k}`); if(e.length===0)return null; for(var i=0;i<e.length;i++){if(e[i].type!=='hidden')return e[i];} return e[0];};"
        
        // Helper 'getStart'
        "const getStart=(e)=>{let c=e; if(c.previousSibling&&c.previousSibling.type==='hidden')c=c.previousSibling; if(c.previousSibling&&c.previousSibling.nodeType===3&&c.previousSibling.textContent.trim().length>0)c=c.previousSibling; return c;};"

        // Helper w: FIX: Robustly find hidden inputs by searching backwards
        "const w=(e,l)=>{if(!e)return null;const d=document.createElement('div');d.className='um-i';"
          "let curr=e.previousSibling; let lblNode=null;"
          "if(curr&&curr.type==='hidden') curr=curr.previousSibling;"
          "while(curr&&curr.nodeType===3&&!curr.textContent.trim()) curr=curr.previousSibling;"
          "if(curr&&curr.nodeType===3) lblNode=curr;"
          "if(lblNode) lblNode.remove();"
          "const lbl=document.createElement('label');lbl.textContent=l||(lblNode?lblNode.textContent.trim():'');if(lbl.textContent)d.appendChild(lbl);"
          // FIX: Search backwards for the hidden input with the same name
          "let sib = e.previousSibling;"
          "while(sib) {"
            "if(sib.type === 'hidden' && sib.name === e.name) {"
               "d.appendChild(sib); break;"
            "}"
            "if(sib.tagName === 'DIV' || sib.tagName === 'H3' || sib.tagName === 'BUTTON') break;"
            "sib = sib.previousSibling;"
          "}"
          "d.appendChild(e);return d;};"
        
        // Loop for Actions
        "for(let i=1;i<=8;i++){"
          "const en=f(`Action ${i}:enabled`); if(!en) continue;"
          "const g=document.createElement('div');g.className='um-g';g.innerHTML=`<div class='um-h'>Action ${i}</div>`;"
          "const s=f(`Action ${i}:offSec`); const on=f(`Action ${i}:onPreset`); const off=f(`Action ${i}:offPreset`);"
          "const anchor = en || on;"
          "if(anchor) anchor.parentNode.insertBefore(g, getStart(anchor));"
          "if(en){count++; g.appendChild(w(en, 'Enabled'));}"
          "if(s){count++; g.appendChild(w(s, 'Off Delay (s)'));}"
          "if(on){count++; g.appendChild(w(on, 'On Preset'));}"
          "if(off){count++; g.appendChild(w(off, 'Off Preset'));}"
        "}"

        // Loop for PIRs
        "for(let i=1;i<=8;i++){"
          "const en=f(`PIR ${i}:enabled`); if(!en) continue;"
          "const g=document.createElement('div');g.className='um-g';g.innerHTML=`<div class='um-h'>PIR Sensor ${i}</div>`;"
          "const p=f(`PIR ${i}:pin`);"
          "const anchor = en || p;"
          "if(anchor) anchor.parentNode.insertBefore(g, getStart(anchor));"
          "if(en){count++; g.appendChild(w(en, 'Enabled'));}"
          "if(p){count++; g.appendChild(w(p, 'Pin'));}"
          "for(let a=1;a<=8;a++){const l=f(`PIR ${i}:Action ${a}`);if(l){count++; g.appendChild(w(l, `Link Action ${a}`));}}"
        "}"

        // AGGRESSIVE CLEANUP
        "const root = document.getElementById('pir-root');"
        "if(root){"
          "const children = Array.from(root.children);"
          "children.forEach(c => {"
            "if(c.tagName!=='H3' && c.tagName!=='BUTTON' && !c.classList.contains('um-g')){ c.remove(); }"
          "});"
          "let n = root.firstChild; while(n){ let next=n.nextSibling; if(n.nodeType===3){ n.remove(); } n=next; }"
          "let prev = root.previousElementSibling; if(prev && prev.tagName==='HR') prev.remove();"
        "}"
        
      "} catch(e) {"
        "if(st){st.innerHTML='Error: '+e.message; st.style.display='block'; st.style.color='red';}"
      "}"
    "} setTimeout(aUM,1000);"));
  }

  /*
   * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
   */
  uint16_t getId() override { return USERMOD_ID_PIRSWITCH; }
};

// Register instance
static MotionDetectionUsermod motionDetectionUsermod;
REGISTER_USERMOD(motionDetectionUsermod);

