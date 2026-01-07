# Quick Implementation Guide
## Critical Features for OneWheel Use Case

This guide provides copy-paste ready code for the three most critical missing features.

---

## 1. Direction Detection & Headlight Switching

### Step 1: Add State Variables

**Location:** `src/main.cpp` around line 280 (with other motion state variables)

```cpp
// Direction detection
bool directionBasedLighting = false;
bool isMovingForward = true;
float forwardAccelThreshold = 0.3;  // G-force threshold for direction change
unsigned long lastDirectionChange = 0;
const unsigned long DIRECTION_CHANGE_DEBOUNCE = 500;  // ms
```

### Step 2: Add Direction Detection Function

**Location:** `src/main.cpp` after `processImpactDetection()` (around line 2110)

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

### Step 3: Call from updateMotionControl()

**Location:** `src/main.cpp` in `updateMotionControl()` function (around line 1994)

Add this line after the calibration check:
```cpp
void updateMotionControl() {
    if (!motionEnabled) return;
    
    MotionData data = getMotionData();
    
    // Handle calibration mode...
    if (calibrationMode) {
        // ... existing code ...
        return;
    }
    
    // ADD THIS LINE:
    if (directionBasedLighting) {
        processDirectionDetection(data);
    }
    
    // Process motion features
    if (blinkerEnabled) {
        processBlinkers(data);
    }
    // ... rest of existing code ...
}
```

### Step 4: Modify updateEffects() for Direction-Based Lighting

**Location:** `src/main.cpp` in `updateEffects()` function (around line 800)

Modify the effect application logic:
```cpp
void updateEffects() {
    // ... existing timing and priority checks ...
    
    // Priority 3: Normal effects
    FastLED.setBrightness(globalBrightness);
    
    // Apply effects based on direction if enabled
    if (directionBasedLighting) {
        // When moving forward: headlight = front, taillight = back
        // When moving backward: headlight = back, taillight = front
        CRGB* frontLights = isMovingForward ? headlight : taillight;
        CRGB* backLights = isMovingForward ? taillight : headlight;
        uint8_t frontCount = isMovingForward ? headlightLedCount : taillightLedCount;
        uint8_t backCount = isMovingForward ? taillightLedCount : headlightLedCount;
        
        // Apply headlight effect to front lights
        switch (headlightEffect) {
            case FX_SOLID:
                fillSolidWithColorOrder(frontLights, frontCount, headlightColor, 
                                       isMovingForward ? headlightLedType : taillightLedType,
                                       isMovingForward ? headlightColorOrder : taillightColorOrder);
                break;
            // ... add other effects ...
        }
        
        // Apply taillight effect to back lights
        switch (taillightEffect) {
            case FX_SOLID:
                fillSolidWithColorOrder(backLights, backCount, taillightColor,
                                       isMovingForward ? taillightLedType : headlightLedType,
                                       isMovingForward ? taillightColorOrder : headlightColorOrder);
                break;
            // ... add other effects ...
        }
    } else {
        // Normal mode - existing behavior
        // ... existing effect code ...
    }
    
    FastLED.show();
}
```

### Step 5: Add API Endpoint

**Location:** `src/main.cpp` in API handler (find where other settings are handled)

```cpp
// In your API handler, add:
if (doc.containsKey("direction_based_lighting")) {
    directionBasedLighting = doc["direction_based_lighting"] | false;
    Serial.printf("Direction-based lighting: %s\n", directionBasedLighting ? "enabled" : "disabled");
}

if (doc.containsKey("forward_accel_threshold")) {
    forwardAccelThreshold = doc["forward_accel_threshold"] | 0.3;
    Serial.printf("Forward acceleration threshold: %.2fG\n", forwardAccelThreshold);
}
```

### Step 6: Add UI Control

**File:** `ui/index.html` - Add after Motion Control section (around line 155)

```html
<div class="control-group">
    <label>
        <input type="checkbox" id="directionBasedLighting" onchange="setDirectionBasedLighting(this.checked)">
        Direction-Based Lighting
    </label>
    <small>Automatically switch headlight/taillight based on movement direction</small>
</div>

<div class="control-group">
    <label>Direction Threshold: <span id="forwardAccelThresholdValue">0.3</span>G</label>
    <input type="range" id="forwardAccelThreshold" min="0.1" max="1.0" step="0.1" 
           oninput="setForwardAccelThreshold(this.value)">
    <small>Acceleration threshold for direction change detection</small>
</div>
```

**File:** `ui/script.js` - Add functions

```javascript
function setDirectionBasedLighting(enabled) {
    fetch('/api/control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ direction_based_lighting: enabled })
    })
    .then(response => response.json())
    .then(data => {
        console.log('Direction-based lighting:', enabled);
        updateStatus();
    });
}

function setForwardAccelThreshold(value) {
    document.getElementById('forwardAccelThresholdValue').textContent = value;
    fetch('/api/control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ forward_accel_threshold: parseFloat(value) })
    });
}
```

---

## 2. Braking Detection

### Step 1: Add State Variables

**Location:** `src/main.cpp` around line 280

```cpp
// Braking detection
bool brakingEnabled = true;
bool brakingActive = false;
float brakingThreshold = -0.5;  // G-force deceleration threshold
uint8_t brakingEffect = 0;  // 0=brighten, 1=flash, 2=red, 3=pulse
uint8_t brakingBrightness = 255;
unsigned long brakingStartTime = 0;
unsigned long lastBrakingFlash = 0;
bool brakingFlashState = false;
```

### Step 2: Add Braking Detection Function

**Location:** `src/main.cpp` after `processDirectionDetection()` (around line 2140)

```cpp
void processBrakingDetection(MotionData& data) {
    if (!brakingEnabled || !motionEnabled) return;
    
    float forwardAccel = calibration.valid ? getCalibratedForwardAccel(data) : data.accelX;
    
    // Only detect braking when moving forward (negative acceleration = deceleration)
    bool isBraking = isMovingForward && forwardAccel < brakingThreshold;
    
    unsigned long currentTime = millis();
    
    if (isBraking && !brakingActive) {
        brakingActive = true;
        brakingStartTime = currentTime;
        Serial.printf("🛑 Braking detected! Deceleration: %.2fG\n", forwardAccel);
    } else if (!isBraking && brakingActive) {
        brakingActive = false;
        Serial.println("🛑 Braking ended");
    }
}

void showBrakingEffect() {
    if (!brakingActive) return;
    
    CRGB* targetLights = isMovingForward ? taillight : headlight;
    uint8_t targetCount = isMovingForward ? taillightLedCount : headlightLedCount;
    
    unsigned long currentTime = millis();
    
    switch (brakingEffect) {
        case 0: // Brighten
            // Increase brightness of taillight
            FastLED.setBrightness(brakingBrightness);
            fillSolidWithColorOrder(targetLights, targetCount, taillightColor,
                                   isMovingForward ? taillightLedType : headlightLedType,
                                   isMovingForward ? taillightColorOrder : headlightColorOrder);
            break;
            
        case 1: // Flash
            // Flash taillight
            if (currentTime - lastBrakingFlash > 200) {  // 200ms flash interval
                brakingFlashState = !brakingFlashState;
                lastBrakingFlash = currentTime;
            }
            if (brakingFlashState) {
                FastLED.setBrightness(brakingBrightness);
                fillSolidWithColorOrder(targetLights, targetCount, CRGB::Red,
                                       isMovingForward ? taillightLedType : headlightLedType,
                                       isMovingForward ? taillightColorOrder : headlightColorOrder);
            } else {
                fillSolidWithColorOrder(targetLights, targetCount, CRGB::Black,
                                       isMovingForward ? taillightLedType : headlightLedType,
                                       isMovingForward ? taillightColorOrder : headlightColorOrder);
            }
            break;
            
        case 2: // Red
            // Turn taillight bright red
            FastLED.setBrightness(brakingBrightness);
            fillSolidWithColorOrder(targetLights, targetCount, CRGB::Red,
                                   isMovingForward ? taillightLedType : headlightLedType,
                                   isMovingForward ? taillightColorOrder : headlightColorOrder);
            break;
            
        case 3: // Pulse
            // Pulse effect on taillight
            uint8_t pulseBrightness = brakingBrightness / 2 + 
                (brakingBrightness / 2) * sin((currentTime - brakingStartTime) / 100.0);
            FastLED.setBrightness(pulseBrightness);
            fillSolidWithColorOrder(targetLights, targetCount, CRGB::Red,
                                   isMovingForward ? taillightLedType : headlightLedType,
                                   isMovingForward ? taillightColorOrder : headlightColorOrder);
            break;
    }
}
```

### Step 3: Call from updateMotionControl()

**Location:** `src/main.cpp` in `updateMotionControl()` function

Add after direction detection:
```cpp
if (brakingEnabled) {
    processBrakingDetection(data);
}
```

### Step 4: Update Effect Priority

**Location:** `src/main.cpp` in `updateEffects()` function (around line 810)

Modify priority system:
```cpp
void updateEffects() {
    // ... existing timing checks ...
    
    // Priority 1: Park mode (highest)
    if (parkModeActive) {
        showParkEffect();
        return;
    }
    
    // Priority 2: Braking (high priority - but don't return, allow headlight effects)
    if (brakingActive) {
        showBrakingEffect();
        // Continue to update headlight normally
    }
    
    // Priority 3: Blinker effects
    if (blinkerActive) {
        showBlinkerEffect(blinkerDirection);
        return;
    }
    
    // Priority 4: Normal effects
    // ... existing effect code ...
}
```

### Step 5: Add API Endpoints

**Location:** `src/main.cpp` in API handler

```cpp
if (doc.containsKey("braking_enabled")) {
    brakingEnabled = doc["braking_enabled"] | true;
}

if (doc.containsKey("braking_threshold")) {
    brakingThreshold = doc["braking_threshold"] | -0.5;
}

if (doc.containsKey("braking_effect")) {
    brakingEffect = doc["braking_effect"] | 0;
}

if (doc.containsKey("braking_brightness")) {
    brakingBrightness = doc["braking_brightness"] | 255;
}
```

### Step 6: Add UI Controls

**File:** `ui/index.html` - Add to Motion Control section

```html
<div class="control-group">
    <label>
        <input type="checkbox" id="brakingEnabled" onchange="setBrakingEnabled(this.checked)">
        Braking Detection
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
        <option value="0">Brighten</option>
        <option value="1">Flash</option>
        <option value="2">Red</option>
        <option value="3">Pulse</option>
    </select>
</div>

<div class="control-group">
    <label>Braking Brightness: <span id="brakingBrightnessValue">255</span></label>
    <input type="range" id="brakingBrightness" min="128" max="255" value="255" 
           oninput="setBrakingBrightness(this.value)">
</div>
```

**File:** `ui/script.js` - Add functions

```javascript
function setBrakingEnabled(enabled) {
    fetch('/api/control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ braking_enabled: enabled })
    });
}

function setBrakingThreshold(value) {
    document.getElementById('brakingThresholdValue').textContent = value;
    fetch('/api/control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ braking_threshold: parseFloat(value) })
    });
}

function setBrakingEffect(value) {
    fetch('/api/control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ braking_effect: parseInt(value) })
    });
}

function setBrakingBrightness(value) {
    document.getElementById('brakingBrightnessValue').textContent = value;
    fetch('/api/control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ braking_brightness: parseInt(value) })
    });
}
```

---

## 3. Effect Separation (Taillight Only / Headlight Only)

### Step 1: Add Lighting Mode Enum

**Location:** `src/main.cpp` near effect definitions (around line 200)

```cpp
// Lighting modes
enum LightingMode {
    MODE_NORMAL = 0,           // Effects on both
    MODE_TAILLIGHT_ONLY = 1,   // Effects only on taillight
    MODE_HEADLIGHT_ONLY = 2,   // Effects only on headlight
    MODE_DIRECTION_BASED = 3,  // Direction-based switching
    MODE_RUNNING_LIGHTS = 4    // Running lights mode
};

LightingMode currentLightingMode = MODE_NORMAL;
```

### Step 2: Modify updateEffects()

**Location:** `src/main.cpp` in `updateEffects()` function

Replace the normal effects section with:
```cpp
// Priority 4: Normal effects (with mode-based separation)
FastLED.setBrightness(globalBrightness);

switch (currentLightingMode) {
    case MODE_NORMAL:
        // Apply effects to both (existing behavior)
        if (headlightUpdate) {
            // Apply headlight effect
            switch (headlightEffect) {
                // ... existing headlight effect code ...
            }
        }
        if (taillightUpdate) {
            // Apply taillight effect
            switch (taillightEffect) {
                // ... existing taillight effect code ...
            }
        }
        break;
        
    case MODE_TAILLIGHT_ONLY:
        // Effects only on taillight, headlight stays solid
        if (headlightUpdate) {
            fillSolidWithColorOrder(headlight, headlightLedCount, headlightColor, 
                                   headlightLedType, headlightColorOrder);
        }
        if (taillightUpdate) {
            // Apply taillight effect
            switch (taillightEffect) {
                // ... existing taillight effect code ...
            }
        }
        break;
        
    case MODE_HEADLIGHT_ONLY:
        // Effects only on headlight, taillight stays solid
        if (headlightUpdate) {
            // Apply headlight effect
            switch (headlightEffect) {
                // ... existing headlight effect code ...
            }
        }
        if (taillightUpdate) {
            fillSolidWithColorOrder(taillight, taillightLedCount, taillightColor,
                                   taillightLedType, taillightColorOrder);
        }
        break;
        
    case MODE_DIRECTION_BASED:
        // Already handled by direction detection logic
        // (Use the code from Direction Detection section)
        break;
        
    case MODE_RUNNING_LIGHTS:
        // Implement running lights effect
        // (See Running Lights section in main review doc)
        break;
}

FastLED.show();
```

### Step 3: Add API Endpoint

**Location:** `src/main.cpp` in API handler

```cpp
if (doc.containsKey("lighting_mode")) {
    uint8_t mode = doc["lighting_mode"] | 0;
    if (mode <= MODE_RUNNING_LIGHTS) {
        currentLightingMode = (LightingMode)mode;
        Serial.printf("Lighting mode set to: %d\n", mode);
    }
}
```

### Step 4: Add UI Control

**File:** `ui/index.html` - Add new section after Effect Speed

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

**File:** `ui/script.js` - Add function

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

## Testing Checklist

After implementing each feature:

- [ ] Direction detection works when tilting board forward/backward
- [ ] Headlight/taillight swap correctly based on direction
- [ ] Braking detection triggers on deceleration
- [ ] Braking effect displays correctly
- [ ] Effect separation modes work (taillight only, headlight only)
- [ ] UI controls update correctly
- [ ] Settings persist after reboot
- [ ] No conflicts with existing motion features (blinkers, park mode)

---

## Quick Test Commands

Add these to your serial command handler for quick testing:

```cpp
// In your serial command handler
else if (command.startsWith("dir")) {
    directionBasedLighting = !directionBasedLighting;
    Serial.printf("Direction-based lighting: %s\n", directionBasedLighting ? "ON" : "OFF");
}
else if (command.startsWith("brake")) {
    brakingEnabled = !brakingEnabled;
    Serial.printf("Braking detection: %s\n", brakingEnabled ? "ON" : "OFF");
}
else if (command.startsWith("mode")) {
    uint8_t mode = command.substring(4).toInt();
    if (mode <= MODE_RUNNING_LIGHTS) {
        currentLightingMode = (LightingMode)mode;
        Serial.printf("Lighting mode: %d\n", mode);
    }
}
```

---

**Ready to implement!** Start with Direction Detection, then Braking, then Effect Separation.

