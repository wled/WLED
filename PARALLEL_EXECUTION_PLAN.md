# SwiftLED Parallel Execution Plan
## How to Use 8 Sonnet Agents for Maximum Throughput

**Budget**: $700 in credits
**Timeline**: 3 weeks (vs 12 weeks sequential)
**Strategy**: Launch 8 parallel agents per wave

---

## Quick Start

### Prerequisites
1. Read `SWIFT_LED_ARCHITECTURE.md` (overall design)
2. Read `SWIFT_LED_PACKAGES.md` (package specs)
3. Have GitHub repo set up: `SwiftLED` Swift Package
4. Budget $700 = ~$90/wave × 5 waves + $250 buffer

### Wave Execution Pattern
```
1. Launch all agents in SINGLE MESSAGE (parallel execution!)
2. Wait for all agents to complete
3. Review PRs
4. Merge all packages
5. Verify wave builds successfully
6. Launch next wave
```

---

## Wave 0: Project Setup (30 minutes, $5)

**Single agent (use haiku for cost savings)**

### Task:
```xml
<invoke name="Task">
  <parameter name="subagent_type">general-purpose</parameter>
  <parameter name="model">haiku</parameter>
  <parameter name="description">Setup SwiftLED project</parameter>
  <parameter name="prompt">
Create a new Swift Package for the SwiftLED project.

Steps:
1. Create `Package.swift` with targets for all 32 packages (define structure, don't implement)
2. Create directory structure:
   - Sources/PackageName/ for each package
   - Tests/PackageNameTests/ for each package
3. Create README.md with project overview
4. Create .gitignore for Swift
5. Create LICENSE (MIT or choose appropriate)

Refer to SWIFT_LED_ARCHITECTURE.md for package list and dependencies.

Output structure:
```
SwiftLED/
├── Package.swift
├── README.md
├── LICENSE
├── .gitignore
├── SWIFT_LED_ARCHITECTURE.md
├── SWIFT_LED_PACKAGES.md
├── Sources/
│   ├── LEDCore/
│   ├── LEDModels/
│   ├── ... (30 more)
└── Tests/
    ├── LEDCoreTests/
    ├── LEDModelsTests/
    └── ... (30 more)
```

Return confirmation when complete.
  </parameter>
</invoke>
```

**Expected output**: Project structure ready for Wave 1

---

## Wave 1: Foundation (1 day, $100)

**5 packages - ALL PARALLEL**

### Launch Command (SINGLE MESSAGE!):

```xml
<!-- Agent 1: LEDCore -->
<invoke name="Task">
  <parameter name="subagent_type">general-purpose</parameter>
  <parameter name="model">sonnet</parameter>
  <parameter name="description">Implement LEDCore package</parameter>
  <parameter name="prompt">
You are implementing the LEDCore package for SwiftLED.

Context:
- Read SWIFT_LED_ARCHITECTURE.md for overall design
- Read SWIFT_LED_PACKAGES.md, find "Package: LEDCore" section for your spec

Your task:
1. Implement all protocols defined in spec:
   - LEDEffect protocol
   - Segment protocol
   - LEDHardwareController protocol
   - EffectRegistry protocol
   - EffectFunction typealias

2. Create file: Sources/LEDCore/LEDCore.swift

3. Add inline DocC documentation for all public APIs

4. Create comprehensive tests in Tests/LEDCoreTests/LEDCoreTests.swift:
   - Protocol conformance tests
   - Mock implementations
   - 80%+ coverage

5. Commit and push to branch: `wave1/led-core`

6. Create PR with title: "[Wave 1] Implement LEDCore"

Return: PR URL and any issues encountered.
  </parameter>
</invoke>

<!-- Agent 2: LEDModels -->
<invoke name="Task">
  <parameter name="subagent_type">general-purpose</parameter>
  <parameter name="model">sonnet</parameter>
  <parameter name="description">Implement LEDModels package</parameter>
  <parameter name="prompt">
You are implementing the LEDModels package for SwiftLED.

Context:
- Read SWIFT_LED_ARCHITECTURE.md for overall design
- Read SWIFT_LED_PACKAGES.md, find "Package: LEDModels" section for your spec

Dependencies: LEDCore (assume it exists, don't implement it)

Your task:
1. Implement all data structures from spec:
   - EffectMetadata
   - EffectCategory
   - EffectParameter
   - ParameterUnit
   - SegmentConfig
   - LEDDeviceConfig
   - HardwareType

2. Create file: Sources/LEDModels/LEDModels.swift

3. Ensure all types are:
   - Codable
   - Hashable (where applicable)
   - Have sensible defaults

4. Add DocC documentation

5. Create tests in Tests/LEDModelsTests/:
   - Codable round-trip tests
   - Equality tests
   - Default value tests
   - 80%+ coverage

6. Commit and push to branch: `wave1/led-models`

7. Create PR: "[Wave 1] Implement LEDModels"

Return: PR URL and any issues encountered.
  </parameter>
</invoke>

<!-- Agent 3: LEDPrimitives -->
<invoke name="Task">
  <parameter name="subagent_type">general-purpose</parameter>
  <parameter name="model">sonnet</parameter>
  <parameter name="description">Implement LEDPrimitives package</parameter>
  <parameter name="prompt">
You are implementing the LEDPrimitives package for SwiftLED.

Context:
- Read SWIFT_LED_ARCHITECTURE.md
- Read SWIFT_LED_PACKAGES.md, find "Package: LEDPrimitives" section

Your task:
1. Implement LEDColor struct with:
   - RGB/RGBW storage (UInt8)
   - HSV initializer with conversion
   - Hex initializer (0xRRGGBB)
   - Kelvin/color temperature initializer
   - toRGB(), toHSV(), toHex() conversions
   - Common color constants (red, green, blue, etc.)

2. Implement Pixel struct

3. Implement LEDCoordinate struct (for 2D)

4. Create file: Sources/LEDPrimitives/LEDPrimitives.swift

5. CRITICAL: Color conversion accuracy is essential!
   - HSV to RGB must match standard algorithms
   - Test against known values

6. Tests in Tests/LEDPrimitivesTests/:
   - Color conversion accuracy
   - HSV ↔ RGB round-trip (within 1% error)
   - Hex conversion
   - Kelvin conversion
   - 80%+ coverage

7. Commit to branch: `wave1/led-primitives`

8. Create PR: "[Wave 1] Implement LEDPrimitives"

Return: PR URL and any color conversion issues.
  </parameter>
</invoke>

<!-- Agent 4: LEDMath -->
<invoke name="Task">
  <parameter name="subagent_type">general-purpose</parameter>
  <parameter name="model">sonnet</parameter>
  <parameter name="description">Implement LEDMath package</parameter>
  <parameter name="prompt">
You are implementing the LEDMath package for SwiftLED.

Context:
- Read SWIFT_LED_ARCHITECTURE.md
- Read SWIFT_LED_PACKAGES.md, find "Package: LEDMath" section
- Reference WLED source: /home/user/wled/wled00/colors.cpp for algorithms
- Reference FastLED for math functions

Dependencies: LEDPrimitives

Your task:
1. Implement color blending functions:
   - blend(_ a, _ b, ratio: Double) -> LEDColor
   - blend(_ a, _ b, amount: UInt8) -> LEDColor

2. Implement color wheel (rainbow hue generator)

3. Port FastLED math functions:
   - sin8, cos8, sin16, cos16
   - beatsin8, beatsin16
   - scale8, scale16
   - qadd8 (saturating add)

4. Implement easing functions:
   - ease8InOutQuad
   - ease8InOutCubic

5. Implement gamma correction

6. Random functions:
   - random8, random16

7. Create file: Sources/LEDMath/LEDMath.swift

8. CRITICAL: Math accuracy matters!
   - Compare output with WLED reference values
   - Port algorithms exactly (don't approximate)

9. Tests in Tests/LEDMathTests/:
   - Test against known values from WLED
   - Blending smoothness
   - Trigonometric accuracy
   - 80%+ coverage

10. Commit to branch: `wave1/led-math`

11. Create PR: "[Wave 1] Implement LEDMath"

Return: PR URL and any accuracy issues vs WLED.
  </parameter>
</invoke>

<!-- Agent 5: LEDTiming -->
<invoke name="Task">
  <parameter name="subagent_type">general-purpose</parameter>
  <parameter name="model">sonnet</parameter>
  <parameter name="description">Implement LEDTiming package</parameter>
  <parameter name="prompt">
You are implementing the LEDTiming package for SwiftLED.

Context:
- Read SWIFT_LED_ARCHITECTURE.md
- Read SWIFT_LED_PACKAGES.md, find "Package: LEDTiming" section

Your task:
1. Implement FrameTimer class:
   - Schedules callbacks at target FPS
   - Measures actual FPS
   - Adjusts for drift

2. Implement time utilities:
   - millis() -> UInt32 (milliseconds since start)
   - micros() -> UInt32 (microseconds since start)

3. Implement FPSCalculator:
   - Moving average FPS calculation
   - Recent frame times tracking

4. Implement Timebase:
   - Tracks effect start time
   - Provides elapsed time
   - Reset capability

5. Create file: Sources/LEDTiming/LEDTiming.swift

6. CRITICAL: Timing must be accurate!
   - Use CACurrentMediaTime() for precision
   - Test frame rate consistency

7. Tests in Tests/LEDTimingTests/:
   - Frame rate accuracy (within 5%)
   - Timer consistency
   - FPS calculation accuracy
   - 80%+ coverage

8. Commit to branch: `wave1/led-timing`

9. Create PR: "[Wave 1] Implement LEDTiming"

Return: PR URL and any timing accuracy issues.
  </parameter>
</invoke>
```

**IMPORTANT**: Send ALL 5 agent tasks in a SINGLE MESSAGE for parallel execution!

### After Wave 1 Completion:
1. Review all 5 PRs
2. Merge all PRs
3. Verify `swift build` succeeds
4. Verify `swift test` passes
5. Tag release: `v0.1.0-wave1`

---

## Wave 2: Core Systems (2 days, $200)

**8 packages - ALL PARALLEL**

### Launch Command (SINGLE MESSAGE with 8 Tasks!):

```xml
<!-- Agent 1: LEDEffectDSL -->
<invoke name="Task">
  <parameter name="subagent_type">general-purpose</parameter>
  <parameter name="model">sonnet</parameter>
  <parameter name="description">Implement LEDEffectDSL</parameter>
  <parameter name="prompt">
You are implementing the LEDEffectDSL package for SwiftLED.

Context:
- Read SWIFT_LED_ARCHITECTURE.md
- Read SWIFT_LED_PACKAGES.md, find "Package: LEDEffectDSL" section
- This is the MOST IMPORTANT package - the Swift DSL for creating effects!

Dependencies: LEDCore, LEDModels, LEDPrimitives (assume they exist)

Your task:
1. Implement @resultBuilder EffectBuilder:
   - buildBlock for combining operations
   - buildEither for conditionals
   - buildOptional for optional operations

2. Implement Effect() function that takes @EffectBuilder closure

3. Implement EffectOperation protocol and concrete types:
   - ForEachPixel (pixel-wise operations)
   - Fill (solid color)
   - Gradient (color gradient)
   - Blur (blur filter)
   - Fade (fade amount)
   - FrameDelay (timing)
   - Animate (animated transitions)

4. Implement @resultBuilder ColorBuilder for color expressions

5. Create file: Sources/LEDEffectDSL/LEDEffectDSL.swift

6. Example usage test (must compile and work):
```swift
let rainbow = Effect {
    ForEachPixel { index, time in
        LEDColor(hue: (Double(index) + time) / 100.0, saturation: 1.0, brightness: 1.0)
    }
    FrameDelay(20)
}
```

7. Tests in Tests/LEDEffectDSLTests/:
   - DSL syntax variations
   - Nested builders
   - Conditional operations
   - Type safety
   - 80%+ coverage

8. Commit to branch: `wave2/led-effect-dsl`

9. Create PR: "[Wave 2] Implement LEDEffectDSL"

Return: PR URL and example effects created.
  </parameter>
</invoke>

<!-- Agent 2: LEDEffectRuntime -->
<invoke name="Task">
  <parameter name="subagent_type">general-purpose</parameter>
  <parameter name="model">sonnet</parameter>
  <parameter name="description">Implement LEDEffectRuntime</parameter>
  <parameter name="prompt">
You are implementing the LEDEffectRuntime package for SwiftLED.

Context:
- Read SWIFT_LED_ARCHITECTURE.md
- Read SWIFT_LED_PACKAGES.md, find "Package: LEDEffectRuntime" section
- Reference WLED: /home/user/wled/wled00/wled.cpp service() function

Dependencies: LEDCore, LEDModels, LEDTiming, LEDPrimitives

Your task:
1. Implement EffectRuntime class:
   - Effect execution loop
   - Frame scheduling based on effect's return value
   - Start/stop/pause/resume controls
   - FPS monitoring

2. Implement RuntimeEffect wrapper:
   - Wraps LEDEffect with timing state
   - Tracks last frame time
   - Handles frame rendering

3. Implement EffectPerformanceMonitor:
   - Average frame time
   - Dropped frame detection
   - Performance recommendations

4. Create file: Sources/LEDEffectRuntime/LEDEffectRuntime.swift

5. CRITICAL: Timing must be precise!
   - Effects return delay until next frame
   - Schedule must honor that delay
   - No drift accumulation

6. Tests in Tests/LEDEffectRuntimeTests/:
   - Frame scheduling accuracy
   - Effect switching
   - Pause/resume behavior
   - Performance monitoring
   - 80%+ coverage

7. Commit to branch: `wave2/led-effect-runtime`

8. Create PR: "[Wave 2] Implement LEDEffectRuntime"

Return: PR URL and any timing issues.
  </parameter>
</invoke>

<!-- Agent 3: LEDSegments -->
<invoke name="Task">
  <parameter name="subagent_type">general-purpose</parameter>
  <parameter name="model">sonnet</parameter>
  <parameter name="description">Implement LEDSegments</parameter>
  <parameter name="prompt">
You are implementing the LEDSegments package for SwiftLED.

Context:
- Read SWIFT_LED_ARCHITECTURE.md
- Read SWIFT_LED_PACKAGES.md, find "Package: LEDSegments" section
- Reference WLED: /home/user/wled/wled00/FX.h Segment class

Dependencies: LEDCore, LEDModels, LEDPrimitives, LEDMath

Your task:
1. Implement LEDSegment class (conforms to Segment protocol):
   - Pixel storage (array of LEDColor)
   - Pixel manipulation: setPixel, getPixel, fill
   - Effects: blur, fade, mirror, reverse, rotate
   - 2D support: width, height, setPixelXY

2. Implement SegmentManager:
   - Manage multiple segments
   - Add/remove segments
   - Query segments

3. Create file: Sources/LEDSegments/LEDSegments.swift

4. CRITICAL: Pixel operations must be fast!
   - Use UnsafeMutableBufferPointer for bulk operations
   - Avoid unnecessary copies

5. Tests in Tests/LEDSegmentsTests/:
   - Pixel manipulation accuracy
   - Bounds checking
   - 2D coordinate mapping
   - Blur/fade algorithms
   - 80%+ coverage

6. Commit to branch: `wave2/led-segments`

7. Create PR: "[Wave 2] Implement LEDSegments"

Return: PR URL and any performance concerns.
  </parameter>
</invoke>

<!-- Agent 4: LEDColorEngine -->
<invoke name="Task">
  <parameter name="subagent_type">general-purpose</parameter>
  <parameter name="model">sonnet</parameter>
  <parameter name="description">Implement LEDColorEngine</parameter>
  <parameter name="prompt">
You are implementing the LEDColorEngine package for SwiftLED.

Context:
- Read SWIFT_LED_ARCHITECTURE.md
- Read SWIFT_LED_PACKAGES.md, find "Package: LEDColorEngine" section
- Reference WLED: /home/user/wled/wled00/palettes.h for palette data

Dependencies: LEDPrimitives, LEDMath

Your task:
1. Implement Palette struct:
   - Color array storage
   - colorAt(position:blend:) method
   - Blending between palette colors

2. Define 10 essential palettes (more later):
   - rainbow
   - rainbowStripe
   - cloudColors
   - lavaColors
   - oceanColors
   - forestColors
   - partyColors
   - heatColors
   - iceColors
   - sunsetColors

3. Implement GradientGenerator:
   - linear() for linear gradients
   - radial() for radial gradients

4. Implement ColorTransition:
   - Animated color transitions
   - AsyncStream of colors
   - Configurable curves

5. Create file: Sources/LEDColorEngine/LEDColorEngine.swift

6. CRITICAL: Palette colors should match WLED!
   - Port palette data exactly from palettes.h
   - Test visual similarity

7. Tests in Tests/LEDColorEngineTests/:
   - Palette color accuracy
   - Blending smoothness
   - Gradient generation
   - 80%+ coverage

8. Commit to branch: `wave2/led-color-engine`

9. Create PR: "[Wave 2] Implement LEDColorEngine"

Return: PR URL and palette comparison with WLED.
  </parameter>
</invoke>

<!-- Agent 5: LEDBuffers -->
<invoke name="Task">
  <parameter name="subagent_type">general-purpose</parameter>
  <parameter name="model">sonnet</parameter>
  <parameter name="description">Implement LEDBuffers</parameter>
  <parameter name="prompt">
You are implementing the LEDBuffers package for SwiftLED.

Context:
- Read SWIFT_LED_ARCHITECTURE.md
- Read SWIFT_LED_PACKAGES.md, find "Package: LEDBuffers" section

Dependencies: LEDPrimitives

Your task:
1. Implement PixelBuffer class:
   - Efficient pixel storage
   - Subscript access
   - Bulk operations (fill, copy)
   - Unsafe buffer access for performance

2. Implement DoubleBuffer:
   - Front/back buffer swapping
   - Smooth transitions

3. Implement BufferPool:
   - Reusable buffer pool
   - Acquire/release pattern
   - Memory efficiency

4. Create file: Sources/LEDBuffers/LEDBuffers.swift

5. CRITICAL: Performance is key!
   - Minimize allocations
   - Use copy-on-write where appropriate
   - Profile memory usage

6. Tests in Tests/LEDBuffersTests/:
   - Buffer operations correctness
   - Double buffering behavior
   - Buffer pool reuse
   - Memory leak tests
   - 80%+ coverage

7. Commit to branch: `wave2/led-buffers`

8. Create PR: "[Wave 2] Implement LEDBuffers"

Return: PR URL and memory profiling results.
  </parameter>
</invoke>

<!-- Agent 6: LEDAnimationCurves -->
<invoke name="Task">
  <parameter name="subagent_type">general-purpose</parameter>
  <parameter name="model">sonnet</parameter>
  <parameter name="description">Implement LEDAnimationCurves</parameter>
  <parameter name="prompt">
You are implementing the LEDAnimationCurves package for SwiftLED.

Context:
- Read SWIFT_LED_ARCHITECTURE.md
- Read SWIFT_LED_PACKAGES.md, find "Package: LEDAnimationCurves" section

Dependencies: None

Your task:
1. Implement AnimationCurve protocol

2. Implement standard curves:
   - LinearCurve
   - EaseInCurve (cubic)
   - EaseOutCurve (cubic)
   - EaseInOutCurve (cubic)
   - SineCurve
   - BezierCurve (configurable control points)
   - BounceCurve
   - ElasticCurve

3. Implement CurveComposer:
   - chain() for sequential curves
   - reverse() for reversed curves

4. Create file: Sources/LEDAnimationCurves/LEDAnimationCurves.swift

5. Tests in Tests/LEDAnimationCurvesTests/:
   - Curve accuracy at known points
   - Edge cases (0.0, 1.0, 0.5)
   - Curve composition
   - 80%+ coverage

6. Commit to branch: `wave2/led-animation-curves`

7. Create PR: "[Wave 2] Implement LEDAnimationCurves"

Return: PR URL and curve visualizations (if possible).
  </parameter>
</invoke>

<!-- Agent 7: LEDEffectRegistry -->
<invoke name="Task">
  <parameter name="subagent_type">general-purpose</parameter>
  <parameter name="model">sonnet</parameter>
  <parameter name="description">Implement LEDEffectRegistry</parameter>
  <parameter name="prompt">
You are implementing the LEDEffectRegistry package for SwiftLED.

Context:
- Read SWIFT_LED_ARCHITECTURE.md
- Read SWIFT_LED_PACKAGES.md, find "Package: LEDEffectRegistry" section
- Reference WLED: /home/user/wled/wled00/FX.cpp setupEffectData() function

Dependencies: LEDCore, LEDModels

Your task:
1. Implement LEDEffectRegistryImpl class (conforms to EffectRegistry protocol):
   - Thread-safe effect storage (use actor or locks)
   - register/unregister effects
   - get effect by ID
   - all() effects
   - filter() by category
   - search() by name

2. Implement EffectLoader:
   - load() effect from file/URL
   - save() effect to file
   - JSON serialization

3. Create file: Sources/LEDEffectRegistry/LEDEffectRegistry.swift

4. CRITICAL: Thread safety!
   - Multiple threads may access registry
   - Use Swift actor or DispatchQueue

5. Tests in Tests/LEDEffectRegistryTests/:
   - Registration/unregistration
   - Lookup correctness
   - Filtering and search
   - Thread safety tests
   - 80%+ coverage

6. Commit to branch: `wave2/led-effect-registry`

7. Create PR: "[Wave 2] Implement LEDEffectRegistry"

Return: PR URL and thread safety approach used.
  </parameter>
</invoke>

<!-- Agent 8: LEDParameters -->
<invoke name="Task">
  <parameter name="subagent_type">general-purpose</parameter>
  <parameter name="model">sonnet</parameter>
  <parameter name="description">Implement LEDParameters</parameter>
  <parameter name="prompt">
You are implementing the LEDParameters package for SwiftLED.

Context:
- Read SWIFT_LED_ARCHITECTURE.md
- Read SWIFT_LED_PACKAGES.md, find "Package: LEDParameters" section

Dependencies: LEDModels

Your task:
1. Implement ParameterValues class:
   - Dictionary-based storage
   - Subscript access by parameter name
   - reset() to defaults
   - Observable (for UI binding)

2. Implement ParameterBinding class:
   - ObservableObject for SwiftUI
   - @Published value
   - bind() to keyPath

3. Implement ParameterValidator:
   - validate() value against parameter spec
   - clamp() value to range

4. Create file: Sources/LEDParameters/LEDParameters.swift

5. Tests in Tests/LEDParametersTests/:
   - Value storage/retrieval
   - Validation logic
   - Clamping correctness
   - Binding behavior
   - 80%+ coverage

6. Commit to branch: `wave2/led-parameters`

7. Create PR: "[Wave 2] Implement LEDParameters"

Return: PR URL and any binding issues.
  </parameter>
</invoke>
```

**Send ALL 8 tasks in SINGLE MESSAGE!**

### After Wave 2 Completion:
1. Review all 8 PRs
2. Merge all PRs
3. Verify `swift build` succeeds
4. Verify `swift test` passes
5. Tag release: `v0.2.0-wave2`

---

## Wave 3: Effects & Communication (3 days, $180)

**6 packages - ALL PARALLEL**

### Launch Command (SINGLE MESSAGE with 6 Tasks!):

```xml
<!-- Agent 1: LEDEffectLibrary -->
<invoke name="Task">
  <parameter name="subagent_type">general-purpose</parameter>
  <parameter name="model">sonnet</parameter>
  <parameter name="description">Port 20 WLED effects</parameter>
  <parameter name="prompt">
You are implementing the LEDEffectLibrary package for SwiftLED.

Context:
- Read SWIFT_LED_ARCHITECTURE.md
- Read SWIFT_LED_PACKAGES.md, find "Package: LEDEffectLibrary" section
- Reference WLED effects: /home/user/wled/wled00/FX.cpp

Dependencies: All Wave 1-2 packages

Your task:
Port these 20 core effects from WLED to Swift:

1. SolidEffect (mode_static)
2. BlinkEffect (mode_blink)
3. BreatheEffect (mode_breath)
4. ColorWipeEffect (mode_color_wipe)
5. RainbowEffect (mode_rainbow)
6. RainbowCycleEffect (mode_rainbow_cycle)
7. ScanEffect (mode_scan) - Larson scanner
8. TheaterChaseEffect (mode_theater_chase)
9. TwinkleEffect (mode_twinkle)
10. FadeEffect (mode_fade)
11. RunningLightsEffect (mode_running_lights)
12. FireEffect (mode_fire)
13. ColorLoopEffect (mode_colorloop)
14. SparkleEffect (mode_sparkle)
15. StrobeEffect (mode_strobe)
16. DissolveEffect (mode_dissolve)
17. MeteorEffect (mode_meteor)
18. DynamicEffect (mode_dynamic)
19. DualScanEffect (mode_dual_scan)
20. ColorSweepEffect (mode_color_sweep)

For each effect:
- Port algorithm from WLED C++ to Swift
- Implement LEDEffect protocol
- Add proper metadata (category, parameters)
- Add inline documentation
- Keep visual appearance identical to WLED

Create files:
- Sources/LEDEffectLibrary/Effects/ (one file per effect)
- Sources/LEDEffectLibrary/LEDEffectLibrary.swift (registration)

Tests in Tests/LEDEffectLibraryTests/:
- Each effect renders without errors
- Visual tests (if possible)
- Parameter handling
- 80%+ coverage

Commit to branch: `wave3/led-effect-library`

Create PR: "[Wave 3] Port 20 WLED Effects"

CRITICAL: This is the LARGEST package! Take your time. Accuracy matters more than speed.

Return: PR URL, list of effects ported, and any porting challenges.
  </parameter>
</invoke>

<!-- Agent 2: LEDNetworking -->
<invoke name="Task">
  <parameter name="subagent_type">general-purpose</parameter>
  <parameter name="model">sonnet</parameter>
  <parameter name="description">Implement WLED API client</parameter>
  <parameter name="prompt">
You are implementing the LEDNetworking package for SwiftLED.

Context:
- Read SWIFT_LED_ARCHITECTURE.md
- Read SWIFT_LED_PACKAGES.md, find "Package: LEDNetworking" section
- Reference WLED JSON API: https://kno.wled.ge/interfaces/json-api/

Dependencies: LEDModels, LEDCore, LEDPrimitives

Your task:
1. Implement WLEDClient class (conforms to LEDHardwareController):
   - JSON API client using URLSession
   - getState() - GET /json/state
   - setState() - POST /json/state
   - setEffect() - shortcut method
   - setBrightness() - shortcut method
   - sendFrame() - real-time UDP or HTTP

2. Implement data models:
   - WLEDState (Codable)
   - WLEDSegment (Codable)
   - WLEDInfo (Codable)

3. Implement WLEDDiscovery:
   - mDNS-based discovery
   - discoveredDevices AsyncStream

4. Create file: Sources/LEDNetworking/LEDNetworking.swift

5. CRITICAL: WLED API compatibility!
   - Test against real WLED device (or emulator)
   - Handle API version differences

6. Tests in Tests/LEDNetworkingTests/:
   - Mock HTTP server tests
   - Codable conformance
   - Error handling
   - 80%+ coverage

7. Commit to branch: `wave3/led-networking`

8. Create PR: "[Wave 3] Implement WLED API Client"

Return: PR URL and WLED API version tested against.
  </parameter>
</invoke>

<!-- Agent 3: LEDProtocols -->
<invoke name="Task">
  <parameter name="subagent_type">general-purpose</parameter>
  <parameter name="model">sonnet</parameter>
  <parameter name="description">Implement E1.31/Art-Net/DDP</parameter>
  <parameter name="prompt">
You are implementing the LEDProtocols package for SwiftLED.

Context:
- Read SWIFT_LED_ARCHITECTURE.md
- Read SWIFT_LED_PACKAGES.md, find "Package: LEDProtocols" section
- Reference protocol specs:
  - E1.31 (sACN): ANSI E1.31-2018
  - Art-Net: https://art-net.org.uk/
  - DDP: http://www.3waylabs.com/ddp/

Dependencies: LEDPrimitives, LEDCore

Your task:
1. Implement E131Controller (conforms to LEDHardwareController):
   - Build E1.31 packets (universe, priority, data)
   - Send via UDP
   - Support multiple universes

2. Implement ArtNetController:
   - Build Art-Net packets
   - Support multiple universes
   - Device polling

3. Implement DDPController:
   - Build DDP packets
   - Push mode support

4. Create packet builders:
   - E131Packet struct
   - ArtNetPacket struct
   - DDPPacket struct

5. Create files:
   - Sources/LEDProtocols/E131.swift
   - Sources/LEDProtocols/ArtNet.swift
   - Sources/LEDProtocols/DDP.swift

6. CRITICAL: Binary protocol correctness!
   - Test packet format with Wireshark
   - Validate against protocol specs

7. Tests in Tests/LEDProtocolsTests/:
   - Packet format validation
   - Data encoding correctness
   - Multi-universe handling
   - 80%+ coverage

8. Commit to branch: `wave3/led-protocols`

9. Create PR: "[Wave 3] Implement E1.31/Art-Net/DDP"

Return: PR URL and protocol compliance verification.
  </parameter>
</invoke>

<!-- Agent 4: LEDDiscovery -->
<invoke name="Task">
  <parameter name="subagent_type">general-purpose</parameter>
  <parameter name="model">sonnet</parameter>
  <parameter name="description">Implement device discovery</parameter>
  <parameter name="prompt">
You are implementing the LEDDiscovery package for SwiftLED.

Context:
- Read SWIFT_LED_ARCHITECTURE.md
- Read SWIFT_LED_PACKAGES.md, find "Package: LEDDiscovery" section

Dependencies: None

Your task:
1. Implement LEDDeviceDiscovery class:
   - NetServiceBrowser for Bonjour/mDNS
   - startDiscovering() / stopDiscovering()
   - onDeviceFound callback
   - onDeviceLost callback

2. Service types to discover:
   - _wled._tcp (WLED devices)
   - _e131._udp (E1.31 receivers)
   - _artnet._udp (Art-Net devices)

3. Implement DiscoveredDevice struct:
   - name, host, port
   - device type detection

4. Create file: Sources/LEDDiscovery/LEDDiscovery.swift

5. Tests in Tests/LEDDiscoveryTests/:
   - Mock NetService tests
   - Device type detection
   - Callback behavior
   - 80%+ coverage

6. Commit to branch: `wave3/led-discovery`

7. Create PR: "[Wave 3] Implement Device Discovery"

Return: PR URL and discovered device types.
  </parameter>
</invoke>

<!-- Agent 5: LEDBluetooth -->
<invoke name="Task">
  <parameter name="subagent_type">general-purpose</parameter>
  <parameter name="model">sonnet</parameter>
  <parameter name="description">Implement BLE controller</parameter>
  <parameter name="prompt">
You are implementing the LEDBluetooth package for SwiftLED.

Context:
- Read SWIFT_LED_ARCHITECTURE.md
- Read SWIFT_LED_PACKAGES.md, find "Package: LEDBluetooth" section

Dependencies: LEDCore, LEDPrimitives

Your task:
1. Implement BLELEDController class:
   - CBCentralManager for BLE scanning
   - scan() for devices
   - connect(to:) async method
   - disconnect() method
   - sendFrame() sends pixels over BLE characteristic

2. Define BLE service/characteristic UUIDs:
   - LED service UUID
   - Pixel data characteristic UUID

3. Create file: Sources/LEDBluetooth/LEDBluetooth.swift

4. CRITICAL: BLE has lower bandwidth!
   - Chunk large pixel arrays
   - Handle MTU limitations (typically 512 bytes)

5. Tests in Tests/LEDBluetoothTests/:
   - Mock CBPeripheral tests
   - Data chunking correctness
   - Connection state management
   - 80%+ coverage

6. Commit to branch: `wave3/led-bluetooth`

7. Create PR: "[Wave 3] Implement BLE Controller"

Return: PR URL and BLE bandwidth considerations.
  </parameter>
</invoke>

<!-- Agent 6: LEDSimulator -->
<invoke name="Task">
  <parameter name="subagent_type">general-purpose</parameter>
  <parameter name="model">sonnet</parameter>
  <parameter name="description">Implement LED simulator</parameter>
  <parameter name="prompt">
You are implementing the LEDSimulator package for SwiftLED.

Context:
- Read SWIFT_LED_ARCHITECTURE.md
- Read SWIFT_LED_PACKAGES.md, find "Package: LEDSimulator" section

Dependencies: LEDCore, LEDPrimitives, LEDBuffers

Your task:
1. Implement VirtualLEDStrip class (conforms to LEDHardwareController):
   - In-memory pixel storage
   - sendFrame() updates pixels
   - onPixelsChanged callback for UI updates
   - maxRefreshRate = 60fps

2. Implement LEDSimulator class:
   - Manages multiple virtual strips
   - addStrip() / removeStrip()
   - allStrips() query

3. Create file: Sources/LEDSimulator/LEDSimulator.swift

4. Tests in Tests/LEDSimulatorTests/:
   - Pixel update correctness
   - Callback invocation
   - Multi-strip management
   - 80%+ coverage

5. Commit to branch: `wave3/led-simulator`

6. Create PR: "[Wave 3] Implement LED Simulator"

Return: PR URL.
  </parameter>
</invoke>
```

**Send ALL 6 tasks in SINGLE MESSAGE!**

### After Wave 3 Completion:
1. Review all 6 PRs
2. Merge all PRs
3. Test with real WLED device (if available)
4. Tag release: `v0.3.0-wave3`

---

## Wave 4: Apple Integration (2 days, $160)

**8 packages - ALL PARALLEL**

### Launch Command (SINGLE MESSAGE with 8 Tasks!):

Due to length constraints, here's the abbreviated version:

```xml
<!-- Agent 1: LEDAppIntents -->
<!-- Agent 2: LEDWidgetKit -->
<!-- Agent 3: LEDWatchKit -->
<!-- Agent 4: LEDLiveActivity -->
<!-- Agent 5: LEDFocusFilter -->
<!-- Agent 6: LEDSiriTips -->
<!-- Agent 7: LEDSpotlight -->
<!-- Agent 8: LEDHandoff -->
```

Each agent gets:
- Package spec from SWIFT_LED_PACKAGES.md
- Dependencies on previous waves
- Platform-specific requirements (iOS 17+, watchOS 10+, etc.)
- Test requirements

**Note**: These are smaller packages, mostly Apple framework integrations.

---

## Wave 5: UI & Preview (2 days, $100)

**5 packages - SOME PARALLEL**

### Phase 1: Parallel (3 agents)

```xml
<!-- Agent 1: LEDEffectEditor -->
<!-- Agent 2: LEDLibraryView -->
<!-- Agent 3: LEDParameterUI -->
```

### Phase 2: After Phase 1 (2 agents)

```xml
<!-- Agent 4: LEDPreviewView (needs LEDSimulator from Wave 3) -->
<!-- Agent 5: LEDApp (needs ALL previous) -->
```

---

## Cost Breakdown

| Wave | Packages | Model | Est. Cost |
|------|----------|-------|-----------|
| Wave 0 | 1 | haiku | $5 |
| Wave 1 | 5 | sonnet | $100 |
| Wave 2 | 8 | sonnet | $200 |
| Wave 3 | 6 | sonnet | $180 |
| Wave 4 | 8 | sonnet | $160 |
| Wave 5 | 5 | sonnet | $100 |
| **Total** | **33** | - | **$745** |

**Buffer**: $45 over budget, but within range!

**Optimization tips**:
- Use haiku for simple packages (data models, protocols)
- Use sonnet for complex logic (DSL, runtime, effects)
- Reuse agents between waves (no extra cost)

---

## Timeline

| Wave | Duration | Cumulative |
|------|----------|------------|
| Wave 0 | 30 min | 30 min |
| Wave 1 | 1 day | 1.5 days |
| Wave 2 | 2 days | 3.5 days |
| Wave 3 | 3 days | 6.5 days |
| Wave 4 | 2 days | 8.5 days |
| Wave 5 | 2 days | 10.5 days |
| **Total** | **~2 weeks** | **2 weeks** |

**Sequential would take**: ~12 weeks (6x slower!)

---

## Monitoring Progress

### Per-Agent Tracking
```
Wave 1:
✅ Agent 1: LEDCore - PR #1 merged
✅ Agent 2: LEDModels - PR #2 merged
🔄 Agent 3: LEDPrimitives - PR #3 in review
⏳ Agent 4: LEDMath - working
⏳ Agent 5: LEDTiming - working
```

### Wave Completion Criteria
- [ ] All PRs submitted
- [ ] All PRs reviewed and approved
- [ ] All PRs merged
- [ ] `swift build` succeeds
- [ ] `swift test` passes (80%+ coverage)
- [ ] No merge conflicts
- [ ] Tagged release

---

## Troubleshooting

### Agent gets stuck
- Check dependencies are actually available (previous wave merged?)
- Simplify prompt (too much context?)
- Break into smaller sub-tasks

### Test failures
- Agent may have misunderstood spec
- Provide WLED reference code snippet
- Ask agent to compare with WLED output

### Merge conflicts
- Shouldn't happen if packages are independent!
- Review dependency graph
- Merge order: foundation → core → effects → UI

### Budget overrun
- Switch to haiku for simple packages
- Reduce parallel agents (6 instead of 8)
- Extend timeline, reduce costs

---

## Success Metrics

**After Wave 3 (MVP)**:
- [ ] Can create effect in DSL
- [ ] Preview shows effect on iOS
- [ ] Control real WLED device
- [ ] 20 effects ported and working

**After Wave 5 (Full Release)**:
- [ ] "Hey Siri, set lights to rainbow" works
- [ ] Widgets show current effect
- [ ] Watch app controls lighting
- [ ] Effect editor creates custom effects
- [ ] Share effects via AirDrop

---

## Next Steps

1. **Review this plan** - any questions?
2. **Setup GitHub repo** - `SwiftLED` Swift Package
3. **Run Wave 0** - project structure
4. **Launch Wave 1** - 5 parallel agents!

**Let's ship this in 2 weeks!** 🚀

---

*Estimated completion: 2-3 weeks with $700 budget*
*Sequential: 12 weeks with $800 budget*
*Speedup: 4-6x faster, slightly cheaper!*
