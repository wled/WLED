# SwiftLED Architecture
## Swift-Native LED Effect System with Runtime Scripting

**Inspired by:** WLED (C++ embedded LED controller)
**Target:** iOS/macOS with App Intents & Apple Intelligence
**Key Innovation:** Swift DSL for runtime effect creation/modification

---

## Core Principles

### 1. Effect Function Signature
```swift
// Mirrors WLED's `uint16_t (*mode_ptr)()`
typealias EffectFunction = (Segment, TimeInterval) -> TimeInterval

// Example:
func rainbowEffect(segment: Segment, time: TimeInterval) -> TimeInterval {
    for i in 0..<segment.length {
        let hue = (Double(i) * 10.0 + time) / 360.0
        segment.setPixel(i, color: Color(hue: hue, saturation: 1.0, brightness: 1.0))
    }
    return 0.020 // 20ms next frame delay
}
```

### 2. Swift DSL for Runtime Creation
```swift
// User or AI agent creates effects at runtime
let pulseEffect = Effect {
    ForEachPixel { index, time in
        let brightness = sin(time * speed) * 0.5 + 0.5
        Color.white.withBrightness(brightness)
    }
    FrameDelay(16) // 60fps
}

// Preview on phone, then publish to hardware
```

### 3. Three Effect Types
1. **Native Swift Effects** - Compiled, fastest performance
2. **DSL Effects** - Runtime-created via Swift resultBuilder
3. **Scripted Effects** - JavaScript via JavaScriptCore (future)

---

## Package Architecture

### Dependency Waves

```
Wave 1: Foundation (5 packages - parallel after Wave 0)
    LEDCore          Protocol definitions
    LEDModels        Data structures
    LEDMath          Color math & utilities
    LEDPrimitives    Basic color/pixel types
    LEDTiming        Frame timing & scheduling

Wave 2: Core Systems (8 packages - PARALLEL!)
    LEDEffectDSL     Swift resultBuilder for effects
    LEDEffectRuntime Effect execution engine
    LEDSegments      Segment management
    LEDColorEngine   Color blending & palettes
    LEDBuffers       Pixel buffer management
    LEDAnimationCurves Easing functions
    LEDEffectRegistry Effect registration
    LEDParameters    Effect parameter system

Wave 3: Effects & Communication (6 packages - PARALLEL!)
    LEDEffectLibrary Port of 20 core WLED effects
    LEDNetworking    WLED JSON API client
    LEDProtocols     E1.31/Art-Net/DDP
    LEDDiscovery     Device discovery (Bonjour)
    LEDBluetooth     BLE communication
    LEDSimulator     On-device LED simulation

Wave 4: Apple Integration (8 packages - PARALLEL!)
    LEDAppIntents    Siri & Shortcuts
    LEDWidgetKit     Home screen widgets
    LEDWatchKit      Apple Watch glue
    LEDLiveActivity  Dynamic Island
    LEDFocusFilter   Focus mode integration
    LEDSiriTips      Siri suggestions
    LEDSpotlight     Spotlight indexing
    LEDHandoff       Continuity support

Wave 5: UI & Preview (5 packages - needs Wave 1-4)
    LEDEffectEditor  Effect creation UI
    LEDPreviewView   Real-time preview
    LEDLibraryView   Effect browser
    LEDParameterUI   Parameter controls
    LEDApp           Main application

Total: 32 packages across 5 waves
```

---

## Effect System Design

### Core Protocol (LEDCore)

```swift
/// Core effect protocol - mirrors WLED's mode_ptr
public protocol LEDEffect {
    var id: UUID { get }
    var name: String { get }
    var metadata: EffectMetadata { get }

    /// Render one frame. Returns delay until next frame (seconds)
    func render(segment: Segment, time: TimeInterval) -> TimeInterval
}

/// Effect metadata for UI
public struct EffectMetadata {
    var displayName: String
    var category: EffectCategory
    var parameters: [EffectParameter]
    var supportsAudio: Bool
    var supports2D: Bool
    var previewImage: String?
}

/// Segment protocol - what effects manipulate
public protocol Segment {
    var length: Int { get }
    var speed: Double { get } // 0.0-1.0
    var intensity: Double { get } // 0.0-1.0
    var colors: [Color] { get }

    func setPixel(_ index: Int, color: Color)
    func getPixel(_ index: Int) -> Color
    func fill(color: Color)
    func blur(amount: Double)
}

/// Color primitives
public struct Color {
    var red: Double
    var green: Double
    var blue: Double
    var white: Double? // For RGBW strips

    init(hue: Double, saturation: Double, brightness: Double)
    init(red: Double, green: Double, blue: Double)
    static func blend(_ a: Color, _ b: Color, ratio: Double) -> Color
}
```

### Swift DSL (LEDEffectDSL)

```swift
/// ResultBuilder for declarative effect creation
@resultBuilder
public struct EffectBuilder {
    static func buildBlock(_ components: EffectOperation...) -> LEDEffect {
        DSLEffect(operations: components)
    }
}

/// DSL operations
public protocol EffectOperation {}

public struct ForEachPixel: EffectOperation {
    let body: (Int, TimeInterval) -> Color
    init(@ColorBuilder _ body: @escaping (Int, TimeInterval) -> Color)
}

public struct Fill: EffectOperation {
    let color: Color
}

public struct Blur: EffectOperation {
    let amount: Double
}

public struct FrameDelay: EffectOperation {
    let milliseconds: Int
}

// Example usage:
let effect = Effect {
    ForEachPixel { index, time in
        Color(hue: (Double(index) + time) / 100.0, saturation: 1.0, brightness: 1.0)
    }
    Blur(amount: 0.3)
    FrameDelay(20)
}
```

### Effect Runtime (LEDEffectRuntime)

```swift
/// Executes effects at correct frame rate
public class EffectRuntime {
    private var currentEffect: LEDEffect?
    private var segments: [Segment]
    private var startTime: TimeInterval
    private var timer: Timer?

    public func setEffect(_ effect: LEDEffect, on segments: [Segment]) {
        self.currentEffect = effect
        self.segments = segments
        startEffect()
    }

    private func startEffect() {
        timer?.invalidate()
        startTime = CACurrentMediaTime()
        renderFrame()
    }

    private func renderFrame() {
        guard let effect = currentEffect else { return }

        let currentTime = CACurrentMediaTime() - startTime

        // Render effect on all segments
        for segment in segments {
            let nextFrameDelay = effect.render(segment: segment, time: currentTime)

            // Schedule next frame
            timer = Timer.scheduledTimer(withTimeInterval: nextFrameDelay, repeats: false) { [weak self] _ in
                self?.renderFrame()
            }
        }

        // Send to hardware
        sendToHardware()
    }
}
```

---

## Example: Porting WLED Effect to Swift

### Original WLED C++ (mode_breath)
```cpp
uint16_t mode_breath(void) {
  unsigned var = 0;
  unsigned counter = (strip.now * ((SEGMENT.speed >> 3) +10)) & 0xFFFFU;
  counter = (counter >> 2) + (counter >> 4);
  if (counter < 16384) {
    if (counter > 8192) counter = 8192 - (counter - 8192);
    var = sin16_t(counter) / 103;
  }

  uint8_t lum = 30 + var;
  for (unsigned i = 0; i < SEGLEN; i++) {
    SEGMENT.setPixelColor(i, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0), lum));
  }

  return FRAMETIME;
}
```

### Swift Native Implementation
```swift
struct BreathEffect: LEDEffect {
    let id = UUID()
    let name = "Breathe"
    let metadata = EffectMetadata(
        displayName: "Breathe",
        category: .classic,
        parameters: [
            EffectParameter(name: "Speed", range: 0...255, default: 128)
        ],
        supportsAudio: false,
        supports2D: false
    )

    func render(segment: Segment, time: TimeInterval) -> TimeInterval {
        let speedFactor = (segment.speed * 0.03125) + 10.0
        let counter = UInt16((time * speedFactor * 1000.0).truncatingRemainder(dividingBy: 65536.0))
        let adjustedCounter = (counter >> 2) + (counter >> 4)

        var variance: Double = 0
        if adjustedCounter < 16384 {
            let c = adjustedCounter > 8192 ? 8192 - (adjustedCounter - 8192) : adjustedCounter
            variance = sin(Double(c) / 16384.0 * .pi) * 23170.0 / 103.0
        }

        let luminosity = (30.0 + variance) / 255.0
        let baseColor = segment.colors[0]
        let bgColor = segment.colors[1]

        for i in 0..<segment.length {
            let blendedColor = Color.blend(bgColor, baseColor, ratio: luminosity)
            segment.setPixel(i, color: blendedColor)
        }

        return 0.042 // ~24fps like WLED FRAMETIME
    }
}
```

### Swift DSL Version (User/AI Created)
```swift
let breatheEffect = Effect {
    Animate(curve: .sine, duration: 2.0) { phase in
        let brightness = phase * 0.7 + 0.3 // 30-100% brightness
        Fill(color: .palette.withBrightness(brightness))
    }
    FrameDelay(42)
}
```

---

## Apple Intelligence Integration

### Natural Language → Effect
```swift
import AppIntents

struct CreateEffectIntent: AppIntent {
    static var title: LocalizedStringResource = "Create LED Effect"

    @Parameter(title: "Description")
    var description: String

    func perform() async throws -> some IntentResult & ReturnsValue<LEDEffect> {
        // Use Apple's NL parsing or LLM API
        let parsed = try await NaturalLanguageParser.parse(description)

        // Generate DSL effect from parsed parameters
        let effect = EffectGenerator.create(from: parsed)

        // Preview on device
        await EffectPreview.show(effect)

        return .result(value: effect)
    }
}

// User: "Hey Siri, make my lights pulse blue slowly"
// → Creates pulse effect with blue color and slow speed
```

### Effect Preview System
```swift
/// Real-time preview on iOS device
class EffectPreview: ObservableObject {
    @Published var pixels: [Color] = []
    private let simulator: LEDSimulator
    private let runtime: EffectRuntime

    func preview(_ effect: LEDEffect, ledCount: Int = 150) {
        let virtualSegment = VirtualSegment(length: ledCount)
        runtime.setEffect(effect, on: [virtualSegment])

        // Update UI at 60fps
        virtualSegment.onPixelsChanged = { [weak self] pixels in
            DispatchQueue.main.async {
                self?.pixels = pixels
            }
        }
    }
}

// SwiftUI View
struct EffectPreviewView: View {
    @StateObject var preview = EffectPreview()
    let effect: LEDEffect

    var body: some View {
        Canvas { context, size in
            let pixelWidth = size.width / CGFloat(preview.pixels.count)
            for (index, color) in preview.pixels.enumerated() {
                let rect = CGRect(
                    x: CGFloat(index) * pixelWidth,
                    y: 0,
                    width: pixelWidth,
                    height: size.height
                )
                context.fill(Path(rect), with: .color(Color(color)))
            }
        }
        .frame(height: 100)
        .onAppear {
            preview.preview(effect)
        }
    }
}
```

---

## Hardware Communication Strategy

### Multi-Protocol Support
```swift
/// Abstraction over different hardware protocols
protocol LEDHardwareController {
    func sendFrame(_ pixels: [Color]) async throws
    var maxRefreshRate: Double { get }
}

/// WLED JSON API
class WLEDController: LEDHardwareController {
    let baseURL: URL

    func sendFrame(_ pixels: [Color]) async throws {
        // Send via WLED real-time API
        let json = ["seg": [["col": pixels.map { $0.toRGB() }]]]
        try await URLSession.shared.upload(json: json, to: baseURL.appendingPathComponent("/json/state"))
    }

    var maxRefreshRate: Double { 60.0 }
}

/// E1.31 (sACN) Protocol
class E131Controller: LEDHardwareController {
    let universeID: UInt16
    let socket: UDPSocket

    func sendFrame(_ pixels: [Color]) async throws {
        let packet = E131Packet(universe: universeID, pixels: pixels)
        try await socket.send(packet.data)
    }

    var maxRefreshRate: Double { 120.0 } // E1.31 can go faster
}

/// Art-Net Protocol
class ArtNetController: LEDHardwareController {
    // Similar to E1.31
}

/// Bluetooth LE (for local ESP32)
class BLEController: LEDHardwareController, CBCentralManagerDelegate {
    // Direct BLE connection
}
```

---

## Performance Considerations

### Frame Rate Management
```swift
/// Adaptive frame rate based on complexity
class AdaptiveFrameRateManager {
    private var recentFrameTimes: [TimeInterval] = []
    private let targetFPS: Double = 60.0

    func recordFrameTime(_ duration: TimeInterval) {
        recentFrameTimes.append(duration)
        if recentFrameTimes.count > 10 {
            recentFrameTimes.removeFirst()
        }
    }

    var recommendedFrameDelay: TimeInterval {
        let avgFrameTime = recentFrameTimes.reduce(0, +) / Double(recentFrameTimes.count)
        let targetFrameTime = 1.0 / targetFPS

        // If we're too slow, suggest longer delay
        if avgFrameTime > targetFrameTime * 1.2 {
            return targetFrameTime * 1.5
        }

        return targetFrameTime
    }
}
```

### Metal Acceleration (Future)
```swift
/// Use GPU for complex effects
class MetalEffectRenderer {
    let device: MTLDevice
    let commandQueue: MTLCommandQueue

    func render(effect: MetalAcceleratedEffect, segment: Segment) {
        // Use Metal shaders for parallel pixel computation
        // 100x faster for complex math-heavy effects
    }
}
```

---

## Development Waves

### Wave 0: Project Setup (1 agent - haiku)
- Create Swift Package
- Setup basic structure
- Create README

### Wave 1: Foundation (5 agents - parallel)
Each agent implements one package independently:
1. **Agent 1**: LEDCore (protocols)
2. **Agent 2**: LEDModels (data structures)
3. **Agent 3**: LEDMath (color math)
4. **Agent 4**: LEDPrimitives (basic types)
5. **Agent 5**: LEDTiming (frame timing)

**Dependencies**: None! All parallel.

### Wave 2: Core Systems (8 agents - parallel)
1. **Agent 1**: LEDEffectDSL (resultBuilder)
2. **Agent 2**: LEDEffectRuntime (execution engine)
3. **Agent 3**: LEDSegments (segment management)
4. **Agent 4**: LEDColorEngine (blending/palettes)
5. **Agent 5**: LEDBuffers (pixel buffers)
6. **Agent 6**: LEDAnimationCurves (easing)
7. **Agent 7**: LEDEffectRegistry (registration)
8. **Agent 8**: LEDParameters (parameter system)

**Dependencies**: Wave 1 only. All Wave 2 packages are independent!

### Wave 3: Effects & Communication (6 agents - parallel)
1. **Agent 1**: LEDEffectLibrary (port 20 effects)
2. **Agent 2**: LEDNetworking (WLED API)
3. **Agent 3**: LEDProtocols (E1.31/Art-Net)
4. **Agent 4**: LEDDiscovery (Bonjour)
5. **Agent 5**: LEDBluetooth (BLE)
6. **Agent 6**: LEDSimulator (on-device sim)

**Dependencies**: Waves 1-2. All Wave 3 packages are independent!

### Wave 4: Apple Integration (8 agents - parallel)
1. **Agent 1**: LEDAppIntents
2. **Agent 2**: LEDWidgetKit
3. **Agent 3**: LEDWatchKit
4. **Agent 4**: LEDLiveActivity
5. **Agent 5**: LEDFocusFilter
6. **Agent 6**: LEDSiriTips
7. **Agent 7**: LEDSpotlight
8. **Agent 8**: LEDHandoff

**Dependencies**: Waves 1-3. All Wave 4 packages are independent!

### Wave 5: UI (5 agents - some parallel)
1. **Agent 1**: LEDEffectEditor (needs LEDEffectDSL)
2. **Agent 2**: LEDPreviewView (needs LEDSimulator)
3. **Agent 3**: LEDLibraryView (needs LEDEffectRegistry)
4. **Agent 4**: LEDParameterUI (needs LEDParameters)
5. **Agent 5**: LEDApp (needs all above)

**Dependencies**: All previous waves. UI packages partially parallel.

---

## Timeline Estimate

**Sequential development**: ~12 weeks
**Parallel development (8 agents)**: ~3 weeks
**Speedup**: 4x faster

**Credit usage estimate**:
- Wave 1: 5 agents × $20 = $100
- Wave 2: 8 agents × $25 = $200
- Wave 3: 6 agents × $30 = $180
- Wave 4: 8 agents × $20 = $160
- Wave 5: 5 agents × $15 = $75
- **Total**: ~$715

Perfect for your $700 budget!

---

## Success Criteria

**Phase 1 (MVP - Waves 1-3):**
- [ ] 20 core effects ported and working
- [ ] Swift DSL for creating effects
- [ ] Preview on iOS device
- [ ] Control WLED hardware via JSON API

**Phase 2 (Apple Integration - Wave 4):**
- [ ] "Hey Siri, set lights to rainbow" works
- [ ] Widgets show current effect
- [ ] Watch app controls
- [ ] Shortcuts automation

**Phase 3 (Advanced - Wave 5):**
- [ ] Effect editor UI
- [ ] Share effects via AirDrop
- [ ] Natural language effect creation
- [ ] Apple Intelligence integration

---

## Next Steps

1. **Create project repository**
2. **Setup Package.swift structure**
3. **Launch Wave 1 (5 parallel agents)**
4. **Iterate through waves**
5. **Ship MVP in ~3 weeks!**

**Let's fucking go!** 🚀🎨💡
