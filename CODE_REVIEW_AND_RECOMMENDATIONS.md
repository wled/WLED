# ArkLights Code Review & Recommendations
## Multi-Perspective Team Review

**Date:** 2024  
**Reviewers:** UX Designer, Senior Developer, Product Manager, Embedded Systems Engineer

---

## Executive Summary

Your ArkLights system is well-architected with solid motion detection capabilities. The codebase shows good structure with motion features (blinkers, park mode, impact detection) already implemented. However, there are **critical missing features** for your OneWheel use case, and several UX improvements that would significantly enhance usability.

**Key Findings:**
- ✅ **Strengths:** Clean code structure, good motion detection foundation, comprehensive UI
- ⚠️ **Gaps:** Missing braking detection, no direction-based headlight switching, limited effect separation
- 🎯 **Priority:** Implement direction-based lighting and braking detection first

---

## 1. UX PERSPECTIVE REVIEW

### Current UX Strengths
1. **Comprehensive Web UI** - Good tabbed interface with clear sections
2. **Motion Status Indicators** - Real-time feedback on blinker/park mode status
3. **Calibration System** - Step-by-step calibration with visual feedback
4. **Preset System** - Quick access to common configurations

### Critical UX Issues

#### 🔴 **HIGH PRIORITY - Missing Core Features**

**1. No Direction-Based Headlight Switching**
- **Problem:** User wants headlight to switch sides based on forward/backward direction
- **Current State:** No implementation found for this feature
- **Impact:** Core functionality missing for OneWheel use case
- **User Expectation:** When moving forward, front LEDs act as headlight; when moving backward, rear LEDs act as headlight

**2. No Braking Detection**
- **Problem:** No deceleration/braking detection implemented
- **Current State:** Only impact detection exists (high G-force)
- **Impact:** Missing safety feature - taillight should brighten/change on braking
- **User Expectation:** Taillight should indicate braking (brighten, flash, or change color)

**3. Limited Effect Separation**
- **Problem:** User wants "effects only on taillight" and "running headlight" options
- **Current State:** Effects apply to both headlight and taillight together
- **Impact:** Can't achieve desired lighting patterns
- **User Expectation:** 
  - Option to run effects only on taillight while headlight stays solid/running
  - Option for "running lights" effect on headlight based on direction

#### 🟡 **MEDIUM PRIORITY - UX Improvements**

**4. Motion Control Settings Scattered**
- **Problem:** Motion settings spread across multiple sections
- **Recommendation:** Create dedicated "Motion & Safety" section with:
  - Motion enable/disable master switch (already exists)
  - Blinker settings grouped together
  - Braking settings (new)
  - Park mode settings grouped together
  - Impact detection settings

**5. No Visual Preview of Effects**
- **Problem:** Users can't preview effects before applying
- **Recommendation:** Add "Test" buttons next to each effect selector
- **Benefit:** Reduces trial-and-error, improves confidence

**6. Calibration UX Could Be Better**
- **Current:** Step-by-step with "Next Step" button
- **Issue:** No visual indication of which orientation is correct
- **Recommendation:** 
  - Add real-time visual feedback showing current orientation
  - Show "✓ Good" when orientation is stable
  - Add countdown timer for each step

**7. Status Information Not Prominent**
- **Problem:** Motion status buried in Settings page
- **Recommendation:** Add status bar at top of Main Controls showing:
  - Current direction (Forward/Backward/Stopped)
  - Blinker status (Left/Right/Off)
  - Park mode status
  - Braking status (when implemented)

**8. No Quick Actions**
- **Problem:** Common actions require multiple clicks
- **Recommendation:** Add quick action buttons:
  - "Emergency Flash" - Flash all lights white
  - "Test All LEDs" - Cycle through colors
  - "Reset to Defaults" - One-click reset

#### 🟢 **LOW PRIORITY - Nice to Have**

**9. Effect Descriptions Missing**
- **Recommendation:** Add tooltips or descriptions for each effect
- **Example:** "Breath: Gentle pulsing effect, good for park mode"

**10. No Preset Customization**
- **Problem:** Can't customize presets
- **Recommendation:** Allow users to save current settings as custom preset

**11. No Effect Speed Preview**
- **Problem:** Speed slider doesn't show what "64" means
- **Recommendation:** Add labels: "Slow" (0-85), "Medium" (86-170), "Fast" (171-255)

---

## 2. CODE QUALITY PERSPECTIVE

### Architecture Strengths
1. **Clean Separation:** Motion control, effects, and communication well-separated
2. **Good State Management:** Clear state variables for motion features
3. **Priority System:** Good effect priority system (Park > Blinker > Normal)
4. **Calibration System:** Well-designed orientation-independent calibration

### Code Quality Issues

#### 🔴 **HIGH PRIORITY**

**1. Missing Direction Detection Implementation**
```cpp
// Found in legacy code but not in main.cpp:
// - getCalibratedForwardAccel() exists
// - Calibration has forwardAxis/forwardSign
// - But no direction-based lighting logic
```
- **Recommendation:** Implement direction detection using calibrated forward acceleration
- **Location:** Add to `updateMotionControl()` or new `processDirectionDetection()`

**2. No Braking Detection**
- **Current:** Only `processImpactDetection()` exists
- **Needed:** `processBrakingDetection()` function
- **Logic:** Detect negative forward acceleration (deceleration)
- **Threshold:** Configurable deceleration threshold (e.g., -0.5G)

**3. Effect Separation Not Implemented**
- **Current:** Effects apply globally
- **Needed:** Per-segment effect control
- **Implementation:** Add flags like `headlightEffectOnly`, `taillightEffectOnly`

#### 🟡 **MEDIUM PRIORITY**

**4. Magic Numbers**
- **Issue:** Hard-coded values throughout (e.g., `500` for blink interval)
- **Recommendation:** Move to constants or configurable settings
```cpp
const uint16_t BLINK_INTERVAL_MS = 500; // Currently hard-coded
```

**5. Effect State Management**
- **Issue:** Effect state variables scattered
- **Recommendation:** Create `EffectState` struct to group related state

**6. Error Handling**
- **Issue:** Limited error handling for IMU failures
- **Recommendation:** Add IMU health checks and graceful degradation

**7. Memory Management**
- **Good:** Dynamic LED array allocation
- **Issue:** No bounds checking on LED count inputs
- **Recommendation:** Add validation in `updateLEDConfig()`

#### 🟢 **LOW PRIORITY**

**8. Code Documentation**
- **Good:** Some comments exist
- **Recommendation:** Add function-level documentation for public APIs

**9. Testing**
- **Issue:** No unit tests visible
- **Recommendation:** Add test framework for motion detection logic

---

## 3. FEATURE COMPLETENESS PERSPECTIVE

### ✅ Implemented Features
- [x] Basic LED control (headlight/taillight)
- [x] Multiple effects (20+ effects)
- [x] Motion-based blinkers
- [x] Park mode detection
- [x] Impact detection
- [x] Calibration system
- [x] ESP-NOW sync
- [x] Web UI
- [x] Preset system
- [x] Brightness control
- [x] Effect speed control

### ❌ Missing Critical Features

**1. Direction-Based Headlight Switching** ⭐⭐⭐
- **Priority:** CRITICAL
- **Complexity:** Medium
- **Dependencies:** Calibration system (already exists)
- **Estimated Effort:** 4-6 hours

**2. Braking Detection** ⭐⭐⭐
- **Priority:** CRITICAL
- **Complexity:** Low-Medium
- **Dependencies:** Calibration system
- **Estimated Effort:** 3-4 hours

**3. Effect Separation (Taillight Only / Headlight Only)** ⭐⭐
- **Priority:** HIGH
- **Complexity:** Low
- **Dependencies:** None
- **Estimated Effort:** 2-3 hours

**4. Running Lights Effect Based on Direction** ⭐⭐
- **Priority:** HIGH
- **Complexity:** Medium
- **Dependencies:** Direction detection
- **Estimated Effort:** 3-4 hours

### 🟡 Nice-to-Have Features

**5. Speed-Based Brightness**
- Auto-adjust brightness based on speed
- **Effort:** 2-3 hours

**6. Turn Signal Auto-Cancel**
- Automatically cancel after turn completes
- **Effort:** 1-2 hours (partially exists)

**7. Night Mode Auto-Detection**
- Detect ambient light and auto-switch to night mode
- **Effort:** 4-6 hours (requires light sensor)

**8. Effect Presets Per Mode**
- Different effects for forward vs backward
- **Effort:** 2-3 hours

---

## 4. UI/UX RECOMMENDATIONS

### Main Controls Page Improvements

#### Add Status Bar (Top of Page)
```html
<div class="status-bar">
  <div class="status-item">
    <span class="label">Direction:</span>
    <span id="directionStatus" class="value">Forward</span>
  </div>
  <div class="status-item">
    <span class="label">Blinker:</span>
    <span id="blinkerStatus" class="value">Off</span>
  </div>
  <div class="status-item">
    <span class="label">Park Mode:</span>
    <span id="parkStatus" class="value">Inactive</span>
  </div>
  <div class="status-item">
    <span class="label">Braking:</span>
    <span id="brakingStatus" class="value">No</span>
  </div>
</div>
```

#### Add Lighting Mode Selector
```html
<div class="section">
  <h2>Lighting Mode</h2>
  <div class="control-group">
    <label>Mode:</label>
    <select id="lightingMode" onchange="setLightingMode(this.value)">
      <option value="normal">Normal (Effects on Both)</option>
      <option value="taillight-only">Effects on Taillight Only</option>
      <option value="headlight-only">Effects on Headlight Only</option>
      <option value="direction-based">Direction-Based (Auto Switch)</option>
      <option value="running-lights">Running Lights Mode</option>
    </select>
    <small>Control how effects are applied to headlight and taillight</small>
  </div>
</div>
```

#### Add Braking Settings Section
```html
<div class="section">
  <h2>🛑 Braking Detection</h2>
  <div class="control-group">
    <label>
      <input type="checkbox" id="brakingEnabled" onchange="setBrakingEnabled(this.checked)">
      Enable Braking Detection
    </label>
    <small>Automatically detect braking and adjust taillight</small>
  </div>
  
  <div class="control-group">
    <label>Braking Threshold: <span id="brakingThresholdValue">-0.5</span>G</label>
    <input type="range" id="brakingThreshold" min="-2.0" max="-0.1" step="0.1" 
           oninput="setBrakingThreshold(this.value)">
    <small>Deceleration threshold to trigger braking effect</small>
  </div>
  
  <div class="control-group">
    <label>Braking Effect:</label>
    <select id="brakingEffect" onchange="setBrakingEffect(this.value)">
      <option value="brighten">Brighten Taillight</option>
      <option value="flash">Flash Taillight</option>
      <option value="red">Turn Red</option>
      <option value="pulse">Pulse Effect</option>
    </select>
  </div>
  
  <div class="control-group">
    <label>Braking Brightness: <span id="brakingBrightnessValue">255</span></label>
    <input type="range" id="brakingBrightness" min="128" max="255" value="255" 
           oninput="setBrakingBrightness(this.value)">
  </div>
</div>
```

#### Improve Effect Selectors
```html
<div class="control-group">
  <label>Effect:</label>
  <div class="effect-selector">
    <select id="headlightEffect" onchange="setHeadlightEffect(this.value)">
      <!-- options -->
    </select>
    <button onclick="testEffect('headlight', this.value)" class="test-btn">Test</button>
    <span class="effect-description" id="headlightEffectDesc">Solid color, no animation</span>
  </div>
</div>
```

### Settings Page Improvements

#### Reorganize Motion Settings
- Create "Motion & Safety" tab with subsections:
  - **Direction & Braking**
    - Direction detection settings
    - Braking detection settings
  - **Turn Signals**
    - Blinker settings (already exist)
  - **Park Mode**
    - Park mode settings (already exist)
  - **Impact Detection**
    - Impact settings (already exist)

#### Add Visual Calibration Guide
- Show 3D orientation diagram
- Real-time orientation indicator
- Progress indicators for each step

---

## 5. PRIORITIZED TASK LIST

### 🔴 PHASE 1: CRITICAL FEATURES (Week 1)

#### Task 1.1: Direction Detection & Headlight Switching ⭐⭐⭐
**Priority:** CRITICAL  
**Effort:** 4-6 hours  
**Dependencies:** None (calibration exists)

**Implementation:**
1. Add direction detection function using `getCalibratedForwardAccel()`
2. Track current direction (Forward/Backward/Stopped)
3. Add `directionBasedLighting` flag
4. Implement headlight/taillight role swapping based on direction
5. Add UI controls for direction-based mode

**Code Changes:**
```cpp
// Add to motion state
bool directionBasedLighting = false;
bool isMovingForward = true;  // Track current direction
float forwardAccelThreshold = 0.3;  // G-force threshold

// Add function
void processDirectionDetection(MotionData& data) {
    if (!directionBasedLighting) return;
    
    float forwardAccel = getCalibratedForwardAccel(data);
    bool newDirection = forwardAccel > forwardAccelThreshold;
    
    if (newDirection != isMovingForward) {
        isMovingForward = newDirection;
        // Swap headlight/taillight roles
        swapLightRoles();
    }
}
```

#### Task 1.2: Braking Detection ⭐⭐⭐
**Priority:** CRITICAL  
**Effort:** 3-4 hours  
**Dependencies:** Direction detection (for forward/backward context)

**Implementation:**
1. Add braking detection function
2. Detect deceleration using calibrated forward acceleration
3. Implement braking effects (brighten, flash, etc.)
4. Add UI controls for braking settings

**Code Changes:**
```cpp
// Add to motion state
bool brakingEnabled = true;
bool brakingActive = false;
float brakingThreshold = -0.5;  // G-force deceleration threshold
uint8_t brakingEffect = 0;  // 0=brighten, 1=flash, 2=red, 3=pulse
uint8_t brakingBrightness = 255;

void processBrakingDetection(MotionData& data) {
    if (!brakingEnabled) return;
    
    float forwardAccel = getCalibratedForwardAccel(data);
    bool isBraking = forwardAccel < brakingThreshold && isMovingForward;
    
    if (isBraking != brakingActive) {
        brakingActive = isBraking;
        if (brakingActive) {
            showBrakingEffect();
        }
    }
}
```

#### Task 1.3: Effect Separation (Taillight Only / Headlight Only) ⭐⭐
**Priority:** HIGH  
**Effort:** 2-3 hours  
**Dependencies:** None

**Implementation:**
1. Add lighting mode selector
2. Add flags: `headlightEffectOnly`, `taillightEffectOnly`
3. Modify `updateEffects()` to respect mode
4. Add UI controls

**Code Changes:**
```cpp
// Add lighting mode enum
enum LightingMode {
    MODE_NORMAL,           // Effects on both
    MODE_TAILLIGHT_ONLY,   // Effects only on taillight
    MODE_HEADLIGHT_ONLY,   // Effects only on headlight
    MODE_DIRECTION_BASED,  // Direction-based switching
    MODE_RUNNING_LIGHTS    // Running lights mode
};

LightingMode currentLightingMode = MODE_NORMAL;

// Modify updateEffects()
void updateEffects() {
    // ... existing priority checks ...
    
    // Apply effects based on mode
    switch (currentLightingMode) {
        case MODE_NORMAL:
            // Apply to both (existing behavior)
            break;
        case MODE_TAILLIGHT_ONLY:
            // Apply effect only to taillight, headlight stays solid
            break;
        case MODE_HEADLIGHT_ONLY:
            // Apply effect only to headlight, taillight stays solid
            break;
        // ... etc
    }
}
```

### 🟡 PHASE 2: UX IMPROVEMENTS (Week 2)

#### Task 2.1: Status Bar on Main Page
**Priority:** MEDIUM  
**Effort:** 2-3 hours

#### Task 2.2: Effect Test Buttons
**Priority:** MEDIUM  
**Effort:** 2-3 hours

#### Task 2.3: Reorganize Motion Settings
**Priority:** MEDIUM  
**Effort:** 3-4 hours

#### Task 2.4: Visual Calibration Guide
**Priority:** LOW  
**Effort:** 4-6 hours

### 🟢 PHASE 3: ENHANCEMENTS (Week 3+)

#### Task 3.1: Running Lights Effect
**Priority:** MEDIUM  
**Effort:** 3-4 hours  
**Dependencies:** Direction detection

#### Task 3.2: Speed-Based Brightness
**Priority:** LOW  
**Effort:** 2-3 hours

#### Task 3.3: Effect Descriptions & Tooltips
**Priority:** LOW  
**Effort:** 2-3 hours

---

## 6. IMPLEMENTATION ROADMAP

### Week 1: Core Features
- [ ] Day 1-2: Direction detection & headlight switching
- [ ] Day 3-4: Braking detection
- [ ] Day 5: Effect separation modes
- [ ] Day 6-7: Testing & bug fixes

### Week 2: UX Polish
- [ ] Day 1-2: Status bar & quick actions
- [ ] Day 3-4: Settings reorganization
- [ ] Day 5: Effect test buttons
- [ ] Day 6-7: UI testing & refinement

### Week 3: Enhancements
- [ ] Running lights effect
- [ ] Additional polish
- [ ] Documentation updates

---

## 7. SPECIFIC CODE RECOMMENDATIONS

### Add Direction Detection

**File:** `src/main.cpp`

**Add to motion state (around line 280):**
```cpp
// Direction detection
bool directionBasedLighting = false;
bool isMovingForward = true;
float forwardAccelThreshold = 0.3;  // G-force threshold for direction change
unsigned long lastDirectionChange = 0;
const unsigned long DIRECTION_CHANGE_DEBOUNCE = 500;  // ms
```

**Add function (after processImpactDetection, around line 2110):**
```cpp
void processDirectionDetection(MotionData& data) {
    if (!directionBasedLighting || !motionEnabled) return;
    
    float forwardAccel = calibration.valid ? getCalibratedForwardAccel(data) : data.accelX;
    
    // Use hysteresis to prevent rapid switching
    bool newDirection = forwardAccel > forwardAccelThreshold;
    bool oldDirection = forwardAccel < -forwardAccelThreshold;
    
    unsigned long currentTime = millis();
    
    if (newDirection && !isMovingForward && 
        (currentTime - lastDirectionChange > DIRECTION_CHANGE_DEBOUNCE)) {
        isMovingForward = true;
        lastDirectionChange = currentTime;
        Serial.println("🔄 Direction: Forward");
    } else if (oldDirection && isMovingForward && 
               (currentTime - lastDirectionChange > DIRECTION_CHANGE_DEBOUNCE)) {
        isMovingForward = false;
        lastDirectionChange = currentTime;
        Serial.println("🔄 Direction: Backward");
    }
}
```

**Add to updateMotionControl() (around line 1994):**
```cpp
void updateMotionControl() {
    if (!motionEnabled) return;
    
    MotionData data = getMotionData();
    
    // ... existing calibration check ...
    
    // Process motion features
    if (directionBasedLighting) {
        processDirectionDetection(data);
    }
    
    if (blinkerEnabled) {
        processBlinkers(data);
    }
    
    // ... rest of existing code ...
}
```

### Add Braking Detection

**Add to motion state:**
```cpp
// Braking detection
bool brakingEnabled = true;
bool brakingActive = false;
float brakingThreshold = -0.5;  // G-force deceleration
uint8_t brakingEffect = 0;  // 0=brighten, 1=flash, 2=red, 3=pulse
uint8_t brakingBrightness = 255;
unsigned long brakingStartTime = 0;
```

**Add function:**
```cpp
void processBrakingDetection(MotionData& data) {
    if (!brakingEnabled || !motionEnabled) return;
    
    float forwardAccel = calibration.valid ? getCalibratedForwardAccel(data) : data.accelX;
    
    // Only detect braking when moving forward
    bool isBraking = isMovingForward && forwardAccel < brakingThreshold;
    
    if (isBraking && !brakingActive) {
        brakingActive = true;
        brakingStartTime = millis();
        Serial.println("🛑 Braking detected!");
        showBrakingEffect();
    } else if (!isBraking && brakingActive) {
        brakingActive = false;
        Serial.println("🛑 Braking ended");
    }
}

void showBrakingEffect() {
    switch (brakingEffect) {
        case 0: // Brighten
            // Increase taillight brightness
            break;
        case 1: // Flash
            // Flash taillight
            break;
        case 2: // Red
            // Turn taillight bright red
            break;
        case 3: // Pulse
            // Pulse effect on taillight
            break;
    }
}
```

### Modify Effect Priority System

**Update updateEffects() priority (around line 810):**
```cpp
void updateEffects() {
    // ... existing timing checks ...
    
    // Priority 1: Park mode (highest)
    if (parkModeActive) {
        showParkEffect();
        return;
    }
    
    // Priority 2: Braking (high priority)
    if (brakingActive) {
        showBrakingEffect();
        // Don't return - allow normal effects to continue on headlight
    }
    
    // Priority 3: Blinker effects
    if (blinkerActive) {
        showBlinkerEffect(blinkerDirection);
        return;
    }
    
    // Priority 4: Normal effects (with mode-based separation)
    FastLED.setBrightness(globalBrightness);
    
    // Apply effects based on lighting mode
    if (currentLightingMode == MODE_NORMAL || 
        currentLightingMode == MODE_HEADLIGHT_ONLY) {
        // Update headlight effect
    }
    
    if (currentLightingMode == MODE_NORMAL || 
        currentLightingMode == MODE_TAILLIGHT_ONLY) {
        // Update taillight effect
    }
    
    // ... rest of effect logic ...
}
```

---

## 8. UI IMPLEMENTATION GUIDE

### Add Status Bar

**File:** `ui/index.html`

Add after line 13 (after build date):
```html
<!-- Status Bar -->
<div class="status-bar" style="display: flex; justify-content: space-around; padding: 10px; background: rgba(0,0,0,0.3); border-radius: 8px; margin: 10px 0;">
  <div class="status-item">
    <span style="font-weight: bold;">Direction:</span>
    <span id="directionStatus" style="color: #4CAF50;">Forward</span>
  </div>
  <div class="status-item">
    <span style="font-weight: bold;">Blinker:</span>
    <span id="blinkerStatusBar" style="color: #FFC107;">Off</span>
  </div>
  <div class="status-item">
    <span style="font-weight: bold;">Park:</span>
    <span id="parkStatusBar" style="color: #2196F3;">Inactive</span>
  </div>
  <div class="status-item">
    <span style="font-weight: bold;">Braking:</span>
    <span id="brakingStatusBar" style="color: #f44336;">No</span>
  </div>
</div>
```

**File:** `ui/script.js`

Add to `updateStatus()` function:
```javascript
// Update status bar
fetch('/api/status')
  .then(response => response.json())
  .then(data => {
    document.getElementById('directionStatus').textContent = 
      data.isMovingForward ? 'Forward' : 'Backward';
    document.getElementById('blinkerStatusBar').textContent = 
      data.blinkerDirection > 0 ? 'Right' : 
      data.blinkerDirection < 0 ? 'Left' : 'Off';
    document.getElementById('parkStatusBar').textContent = 
      data.parkModeActive ? 'Active' : 'Inactive';
    document.getElementById('brakingStatusBar').textContent = 
      data.brakingActive ? 'Yes' : 'No';
  });
```

### Add Lighting Mode Selector

**File:** `ui/index.html`

Add new section after line 112 (after Effect Speed):
```html
<div class="section">
  <h2>Lighting Mode</h2>
  <div class="control-group">
    <label>Mode:</label>
    <select id="lightingMode" onchange="setLightingMode(this.value)">
      <option value="0">Normal (Effects on Both)</option>
      <option value="1">Effects on Taillight Only</option>
      <option value="2">Effects on Headlight Only</option>
      <option value="3">Direction-Based (Auto Switch)</option>
      <option value="4">Running Lights Mode</option>
    </select>
    <small>Control how effects are applied to headlight and taillight</small>
  </div>
</div>
```

**File:** `ui/script.js`

Add function:
```javascript
function setLightingMode(mode) {
  fetch('/api/control', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ lighting_mode: parseInt(mode) })
  })
  .then(response => response.json())
  .then(data => {
    console.log('Lighting mode set:', mode);
    updateStatus();
  });
}
```

---

## 9. TESTING RECOMMENDATIONS

### Unit Tests Needed
1. **Direction Detection Logic**
   - Test forward/backward detection
   - Test threshold boundaries
   - Test debouncing

2. **Braking Detection**
   - Test deceleration thresholds
   - Test forward/backward context
   - Test effect triggering

3. **Effect Separation**
   - Test each lighting mode
   - Test mode switching
   - Test effect application

### Integration Tests
1. **Motion Feature Interactions**
   - Test braking + blinker priority
   - Test direction change + park mode
   - Test all motion features together

2. **UI Functionality**
   - Test all new UI controls
   - Test status updates
   - Test mode switching

### Field Testing
1. **OneWheel Testing**
   - Test direction detection while riding
   - Test braking detection
   - Test blinker accuracy
   - Test park mode activation

---

## 10. SUMMARY & NEXT STEPS

### Immediate Actions (This Week)
1. ✅ **Implement direction detection** - Core feature for your use case
2. ✅ **Implement braking detection** - Safety feature
3. ✅ **Add effect separation modes** - Enables your desired lighting patterns

### Short Term (Next 2 Weeks)
4. Add status bar to main page
5. Reorganize motion settings
6. Add effect test buttons
7. Implement running lights effect

### Long Term (Next Month)
8. Visual calibration guide
9. Speed-based brightness
10. Additional polish and refinements

---

## Questions for Clarification

1. **Direction Detection:**
   - Should headlight/taillight swap instantly or fade?
   - What threshold for direction change? (0.3G default)

2. **Braking Detection:**
   - Preferred braking effect? (brighten, flash, red, pulse)
   - Should braking override other effects?

3. **Running Lights:**
   - What pattern? (Knight Rider style? Sequential?)
   - Speed based on actual speed or fixed?

4. **Effect Separation:**
   - When in "taillight only" mode, what should headlight show? (Solid white? Off?)

---

**Review Complete**  
*Ready for implementation discussion and prioritization*

