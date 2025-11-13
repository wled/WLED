# SwiftLED Package Specifications

**Comprehensive specifications for all 32 packages across 5 waves.**

Each package specification includes:
- Public API surface
- Dependencies
- Porting notes from WLED
- Example implementations
- Testing requirements

---

## Wave 1: Foundation (5 packages - ALL PARALLEL)

### Package: LEDCore

**Purpose**: Core protocols and type aliases

**Dependencies**: None

**Public API**:
```swift
/// Core effect protocol
public protocol LEDEffect {
    var id: UUID { get }
    var name: String { get }
    var metadata: EffectMetadata { get }
    func render(segment: Segment, time: TimeInterval) -> TimeInterval
}

/// Segment protocol - what effects manipulate
public protocol Segment {
    var length: Int { get }
    var speed: Double { get } // 0.0-1.0
    var intensity: Double { get } // 0.0-1.0
    var colors: [LEDColor] { get set }

    func setPixel(_ index: Int, color: LEDColor)
    func getPixel(_ index: Int) -> LEDColor
    func fill(color: LEDColor)
    func blur(amount: Double)
    func fade(amount: UInt8)
}

/// Hardware controller protocol
public protocol LEDHardwareController {
    func sendFrame(_ pixels: [LEDColor]) async throws
    var maxRefreshRate: Double { get }
    var isConnected: Bool { get }
}

/// Effect registry protocol
public protocol EffectRegistry {
    func register(_ effect: LEDEffect)
    func get(_ id: UUID) -> LEDEffect?
    func all() -> [LEDEffect]
}

/// Type alias for effect function
public typealias EffectFunction = (Segment, TimeInterval) -> TimeInterval
```

**Testing**: Protocol conformance tests, mock implementations

**Estimated complexity**: Low (protocols only)

---

### Package: LEDModels

**Purpose**: Data structures and models

**Dependencies**: LEDCore

**Public API**:
```swift
/// Effect metadata
public struct EffectMetadata: Codable, Hashable {
    public var displayName: String
    public var category: EffectCategory
    public var parameters: [EffectParameter]
    public var supportsAudio: Bool
    public var supports2D: Bool
    public var previewImageName: String?
    public var tags: [String]
}

/// Effect categories
public enum EffectCategory: String, Codable, CaseIterable {
    case classic
    case particle
    case noise
    case animation
    case music
    case strobe
    case utility
}

/// Effect parameter definition
public struct EffectParameter: Codable, Hashable {
    public var name: String
    public var displayName: String
    public var range: ClosedRange<Double>
    public var defaultValue: Double
    public var stepSize: Double
    public var unit: ParameterUnit?
}

public enum ParameterUnit: String, Codable {
    case milliseconds
    case percent
    case degrees
    case pixels
}

/// Segment configuration
public struct SegmentConfig: Codable {
    public var start: Int
    public var end: Int
    public var speed: Double
    public var intensity: Double
    public var colors: [LEDColor]
    public var reverse: Bool
    public var mirror: Bool
}

/// Device configuration
public struct LEDDeviceConfig: Codable {
    public var name: String
    public var ledCount: Int
    public var hardwareType: HardwareType
    public var ipAddress: String?
    public var segments: [SegmentConfig]
}

public enum HardwareType: String, Codable {
    case wled
    case e131
    case artNet
    case bluetooth
    case simulator
}
```

**Testing**: Codable conformance, equality, hashable

**Estimated complexity**: Low (data models only)

---

### Package: LEDPrimitives

**Purpose**: Basic color and pixel types

**Dependencies**: None

**Public API**:
```swift
/// Core color type (mirrors WLED's uint32_t color)
public struct LEDColor: Hashable, Codable {
    public var red: UInt8
    public var green: UInt8
    public var blue: UInt8
    public var white: UInt8? // For RGBW strips

    // Initializers
    public init(red: UInt8, green: UInt8, blue: UInt8, white: UInt8? = nil)
    public init(hue: Double, saturation: Double, brightness: Double)
    public init(hex: UInt32)
    public init(kelvin: Int) // Color temperature

    // Conversions
    public func toRGB() -> (UInt8, UInt8, UInt8)
    public func toHSV() -> (Double, Double, Double)
    public func toHex() -> UInt32

    // Common colors
    public static let black: LEDColor
    public static let white: LEDColor
    public static let red: LEDColor
    public static let green: LEDColor
    public static let blue: LEDColor
    public static let yellow: LEDColor
    public static let cyan: LEDColor
    public static let magenta: LEDColor
    public static let orange: LEDColor
    public static let purple: LEDColor
}

/// Pixel with position
public struct Pixel {
    public var index: Int
    public var color: LEDColor
}

/// 2D coordinate (for matrix effects)
public struct LEDCoordinate {
    public var x: Int
    public var y: Int
}
```

**WLED mapping**:
- `RGBW32(r,g,b,w)` → `LEDColor.init(red:green:blue:white:)`
- `RED`, `GREEN`, `BLUE` macros → static constants

**Testing**: Color conversion accuracy, HSV↔RGB roundtrip

**Estimated complexity**: Medium (color math)

---

### Package: LEDMath

**Purpose**: Color math and utility functions (port of WLED's color utilities)

**Dependencies**: LEDPrimitives

**Public API**:
```swift
/// Color blending
public func blend(_ a: LEDColor, _ b: LEDColor, ratio: Double) -> LEDColor
public func blend(_ a: LEDColor, _ b: LEDColor, amount: UInt8) -> LEDColor

/// Color wheel (rainbow hue)
public func colorWheel(position: UInt8) -> LEDColor

/// Gamma correction
public func gammaCorrect(_ color: LEDColor, gamma: Double = 2.8) -> LEDColor

/// Color from palette
public func colorFromPalette(_ palette: Palette, position: UInt8, brightness: UInt8) -> LEDColor

/// FastLED-style functions
public func sin8(_ theta: UInt8) -> UInt8
public func cos8(_ theta: UInt8) -> UInt8
public func sin16(_ theta: UInt16) -> Int16
public func cos16(_ theta: UInt16) -> Int16
public func beatsin8(bpm: UInt16, min: UInt8, max: UInt8, timebase: TimeInterval) -> UInt8
public func beatsin16(bpm: UInt16, min: Int16, max: Int16, timebase: TimeInterval) -> Int16

/// Scaling functions
public func scale8(_ value: UInt8, _ scale: UInt8) -> UInt8
public func scale16(_ value: UInt16, _ scale: UInt16) -> UInt16
public func qadd8(_ a: UInt8, _ b: UInt8) -> UInt8 // Saturating add

/// Easing functions
public func ease8InOutQuad(_ t: UInt8) -> UInt8
public func ease8InOutCubic(_ t: UInt8) -> UInt8

/// Random functions
public func random8() -> UInt8
public func random8(max: UInt8) -> UInt8
public func random16() -> UInt16
```

**WLED mapping**:
- Port functions from `wled00/colors.cpp`
- Port FastLED math from `FastLED.h`

**Testing**: Accuracy tests against WLED reference values

**Estimated complexity**: High (lots of math functions to port)

---

### Package: LEDTiming

**Purpose**: Frame timing and scheduling

**Dependencies**: None

**Public API**:
```swift
/// Frame timing manager
public class FrameTimer {
    public var targetFPS: Double
    public var actualFPS: Double { get }

    public func scheduleNextFrame(delay: TimeInterval, handler: @escaping () -> Void)
    public func start()
    public func stop()
}

/// Time utilities
public func millis() -> UInt32 // Mirrors WLED's millis()
public func micros() -> UInt32 // Mirrors WLED's micros()

/// FPS calculator
public class FPSCalculator {
    public func recordFrame(at time: TimeInterval)
    public var averageFPS: Double { get }
    public var recentFrameTimes: [TimeInterval] { get }
}

/// Animation timebase
public struct Timebase {
    public var startTime: TimeInterval
    public var currentTime: TimeInterval { get }
    public var elapsedTime: TimeInterval { get }

    public func reset()
}
```

**WLED mapping**:
- `millis()` → system time
- `FRAMETIME` → calculated frame delay
- `strip.now` → timebase

**Testing**: Frame rate accuracy, timing consistency

**Estimated complexity**: Medium (timing logic)

---

## Wave 2: Core Systems (8 packages - ALL PARALLEL)

### Package: LEDEffectDSL

**Purpose**: Swift resultBuilder for declarative effect creation

**Dependencies**: LEDCore, LEDModels, LEDPrimitives

**Public API**:
```swift
/// Main DSL entry point
public func Effect(@EffectBuilder _ builder: () -> EffectBody) -> LEDEffect

/// ResultBuilder
@resultBuilder
public struct EffectBuilder {
    static func buildBlock(_ components: EffectOperation...) -> EffectBody
    static func buildEither(first component: EffectOperation) -> EffectOperation
    static func buildEither(second component: EffectOperation) -> EffectOperation
    static func buildOptional(_ component: EffectOperation?) -> EffectOperation
}

/// Effect operations
public protocol EffectOperation {}

public struct ForEachPixel: EffectOperation {
    init(@ColorBuilder _ body: @escaping (Int, TimeInterval) -> LEDColor)
}

public struct Fill: EffectOperation {
    init(color: LEDColor)
}

public struct Gradient: EffectOperation {
    init(from: LEDColor, to: LEDColor, direction: GradientDirection)
}

public struct Blur: EffectOperation {
    init(amount: Double)
}

public struct Fade: EffectOperation {
    init(amount: UInt8)
}

public struct FrameDelay: EffectOperation {
    init(_ milliseconds: Int)
}

public struct Animate: EffectOperation {
    init(curve: AnimationCurve, duration: TimeInterval, @EffectBuilder _ body: @escaping (Double) -> EffectOperation)
}

/// Color builder
@resultBuilder
public struct ColorBuilder {
    static func buildExpression(_ color: LEDColor) -> LEDColor
}

/// Example usage:
let rainbowEffect = Effect {
    ForEachPixel { index, time in
        LEDColor(hue: (Double(index) * 10 + time) / 360.0, saturation: 1.0, brightness: 1.0)
    }
    Blur(amount: 0.2)
    FrameDelay(20)
}
```

**Testing**: DSL syntax variations, composition, nested builders

**Estimated complexity**: High (resultBuilder advanced Swift)

---

### Package: LEDEffectRuntime

**Purpose**: Effect execution engine

**Dependencies**: LEDCore, LEDModels, LEDTiming, LEDPrimitives

**Public API**:
```swift
/// Main runtime executor
public class EffectRuntime {
    public init(targetFPS: Double = 60.0)

    public func setEffect(_ effect: LEDEffect, on segments: [Segment])
    public func start()
    public func stop()
    public func pause()
    public func resume()

    public var isRunning: Bool { get }
    public var currentFPS: Double { get }
    public var currentEffect: LEDEffect? { get }

    // Callbacks
    public var onFrameRendered: ((TimeInterval) -> Void)?
    public var onError: ((Error) -> Void)?
}

/// Effect wrapper for runtime
class RuntimeEffect {
    let effect: LEDEffect
    let segments: [Segment]
    var lastFrameTime: TimeInterval

    func renderFrame(at time: TimeInterval) -> TimeInterval
}

/// Performance monitor
public class EffectPerformanceMonitor {
    public var averageFrameTime: TimeInterval { get }
    public var droppedFrames: Int { get }
    public var recommendations: [PerformanceRecommendation] { get }
}
```

**WLED mapping**:
- `WS2812FX::service()` → `EffectRuntime.start()`
- Effect function call → `effect.render()`

**Testing**: Frame timing, effect switching, pause/resume

**Estimated complexity**: High (timing-critical code)

---

### Package: LEDSegments

**Purpose**: Segment management (mirrors WLED's Segment class)

**Dependencies**: LEDCore, LEDModels, LEDPrimitives, LEDMath

**Public API**:
```swift
/// Concrete segment implementation
public class LEDSegment: Segment {
    public var length: Int
    public var speed: Double
    public var intensity: Double
    public var colors: [LEDColor]

    private var pixels: [LEDColor]
    private var startIndex: Int
    private var endIndex: Int

    public init(startIndex: Int, length: Int)

    // Segment protocol conformance
    public func setPixel(_ index: Int, color: LEDColor)
    public func getPixel(_ index: Int) -> LEDColor
    public func fill(color: LEDColor)
    public func blur(amount: Double)
    public func fade(amount: UInt8)

    // Additional methods
    public func setRange(_ range: Range<Int>, color: LEDColor)
    public func mirror()
    public func reverse()
    public func rotate(by offset: Int)

    // 2D support
    public var is2D: Bool { get }
    public var width: Int? { get }
    public var height: Int? { get }
    public func setPixelXY(_ x: Int, _ y: Int, color: LEDColor)
}

/// Segment manager
public class SegmentManager {
    private var segments: [LEDSegment]

    public func addSegment(_ segment: LEDSegment)
    public func removeSegment(at index: Int)
    public func getSegment(at index: Int) -> LEDSegment?
    public func allSegments() -> [LEDSegment]
}
```

**WLED mapping**:
- `Segment` class → `LEDSegment`
- `SEGMENT` macro → current segment reference
- `SEGLEN` → `segment.length`

**Testing**: Pixel manipulation, bounds checking, 2D support

**Estimated complexity**: High (complex pixel management)

---

### Package: LEDColorEngine

**Purpose**: Color palettes and blending (port of WLED palettes)

**Dependencies**: LEDPrimitives, LEDMath

**Public API**:
```swift
/// Color palette
public struct Palette {
    public var colors: [LEDColor]
    public var name: String

    public func colorAt(position: UInt8, blend: Bool = true) -> LEDColor

    // Standard palettes (from WLED)
    public static let rainbow: Palette
    public static let rainbowStripe: Palette
    public static let cloudColors: Palette
    public static let lavaColors: Palette
    public static let oceanColors: Palette
    public static let forestColors: Palette
    public static let partyColors: Palette
    public static let heatColors: Palette
    // ... 50+ more palettes
}

/// Gradient generator
public struct GradientGenerator {
    public static func linear(from: LEDColor, to: LEDColor, steps: Int) -> [LEDColor]
    public static func radial(center: LEDColor, edge: LEDColor, steps: Int) -> [LEDColor]
}

/// Color transitions
public class ColorTransition {
    public func transition(from: LEDColor, to: LEDColor, duration: TimeInterval, curve: AnimationCurve) -> AsyncStream<LEDColor>
}
```

**WLED mapping**:
- Port palettes from `wled00/palettes.h`
- `SEGPALETTE` → current palette
- `colorFromPalette()` → `palette.colorAt()`

**Testing**: Palette accuracy vs WLED, blending smoothness

**Estimated complexity**: Medium (palette port)

---

### Package: LEDBuffers

**Purpose**: Pixel buffer management and optimization

**Dependencies**: LEDPrimitives

**Public API**:
```swift
/// Pixel buffer
public class PixelBuffer {
    private var pixels: [LEDColor]
    public var count: Int { get }

    public init(count: Int, initialColor: LEDColor = .black)

    public subscript(index: Int) -> LEDColor { get set }

    public func fill(_ color: LEDColor)
    public func fill(_ range: Range<Int>, with color: LEDColor)
    public func copy(from source: PixelBuffer, sourceRange: Range<Int>, to destinationIndex: Int)

    // Performance optimizations
    public func withUnsafeMutableBufferPointer<R>(_ body: (inout UnsafeMutableBufferPointer<LEDColor>) throws -> R) rethrows -> R
}

/// Double buffering for smooth transitions
public class DoubleBuffer {
    private var frontBuffer: PixelBuffer
    private var backBuffer: PixelBuffer

    public func swap()
    public var currentBuffer: PixelBuffer { get }
}

/// Memory pool for reusable buffers
public class BufferPool {
    public static let shared = BufferPool()

    public func acquire(size: Int) -> PixelBuffer
    public func release(_ buffer: PixelBuffer)
}
```

**Testing**: Buffer operations, memory safety, performance

**Estimated complexity**: Medium (memory management)

---

### Package: LEDAnimationCurves

**Purpose**: Easing functions for smooth animations

**Dependencies**: None

**Public API**:
```swift
/// Animation curve protocol
public protocol AnimationCurve {
    func value(at progress: Double) -> Double // 0.0-1.0 → 0.0-1.0
}

/// Built-in curves
public struct LinearCurve: AnimationCurve
public struct EaseInCurve: AnimationCurve
public struct EaseOutCurve: AnimationCurve
public struct EaseInOutCurve: AnimationCurve
public struct SineCurve: AnimationCurve
public struct BezierCurve: AnimationCurve {
    public init(p1: CGPoint, p2: CGPoint)
}

/// Convenience extensions
extension AnimationCurve {
    public static let linear: AnimationCurve
    public static let easeIn: AnimationCurve
    public static let easeOut: AnimationCurve
    public static let easeInOut: AnimationCurve
    public static let sine: AnimationCurve
    public static let bounce: AnimationCurve
    public static let elastic: AnimationCurve
}

/// Curve composer
public struct CurveComposer {
    public static func chain(_ curves: [AnimationCurve]) -> AnimationCurve
    public static func reverse(_ curve: AnimationCurve) -> AnimationCurve
}
```

**Testing**: Curve accuracy, edge cases (0.0, 1.0)

**Estimated complexity**: Low (math functions)

---

### Package: LEDEffectRegistry

**Purpose**: Effect registration and management

**Dependencies**: LEDCore, LEDModels

**Public API**:
```swift
/// Effect registry
public class LEDEffectRegistryImpl: EffectRegistry {
    public static let shared = LEDEffectRegistryImpl()

    private var effects: [UUID: LEDEffect] = [:]

    public func register(_ effect: LEDEffect)
    public func unregister(_ id: UUID)
    public func get(_ id: UUID) -> LEDEffect?
    public func all() -> [LEDEffect]
    public func filter(by category: EffectCategory) -> [LEDEffect]
    public func search(query: String) -> [LEDEffect]

    // Built-in effects
    public func registerBuiltInEffects()
}

/// Effect loader (for user-created effects)
public class EffectLoader {
    public func load(from url: URL) throws -> LEDEffect
    public func save(_ effect: LEDEffect, to url: URL) throws
}
```

**WLED mapping**:
- `WS2812FX::addEffect()` → `registry.register()`
- `setupEffectData()` → `registerBuiltInEffects()`

**Testing**: Registration, lookup, filtering

**Estimated complexity**: Low (dictionary management)

---

### Package: LEDParameters

**Purpose**: Effect parameter system

**Dependencies**: LEDModels

**Public API**:
```swift
/// Parameter value storage
public class ParameterValues {
    private var values: [String: Double] = [:]

    public subscript(name: String) -> Double {
        get { values[name] ?? 0.0 }
        set { values[name] = newValue }
    }

    public func reset(to defaults: [EffectParameter])
}

/// Parameter binding (for UI)
public class ParameterBinding: ObservableObject {
    @Published public var value: Double

    public init(parameter: EffectParameter)
    public func bind(to keyPath: ReferenceWritableKeyPath<ParameterValues, Double>)
}

/// Parameter validation
public struct ParameterValidator {
    public static func validate(_ value: Double, against parameter: EffectParameter) -> Bool
    public static func clamp(_ value: Double, to parameter: EffectParameter) -> Double
}
```

**WLED mapping**:
- `SEGMENT.speed` → parameter value
- `SEGMENT.intensity` → parameter value
- `SEGMENT.custom1/2/3` → custom parameters

**Testing**: Value validation, clamping, binding

**Estimated complexity**: Low (data management)

---

## Wave 3: Effects & Communication (6 packages - ALL PARALLEL)

### Package: LEDEffectLibrary

**Purpose**: Port of 20 core WLED effects

**Dependencies**: All Wave 1-2 packages

**Effects to port** (priority order):
1. Solid - Static color
2. Blink - Simple blink
3. Breathe - Sine wave brightness
4. Color Wipe - Fill from one end
5. Rainbow - Hue gradient
6. Rainbow Cycle - Moving rainbow
7. Scan - Larson scanner (Cylon/KITT)
8. Theater Chase - Scrolling pattern
9. Twinkle - Random sparkles
10. Fade - Cross-fade between colors
11. Running Lights - Sine wave movement
12. Fire - Flickering fire simulation
13. Colorloop - Smooth color transitions
14. Rain - Falling drops
15. Meteor - Trailing comet
16. Sparkle - Random bright pixels
17. Strobe - Fast flashing
18. Dissolve - Random pixel fade
19. Lightning - Random flashes
20. Noise - Perlin noise patterns

**Example implementation**:
```swift
/// Breathe effect (from WLED mode_breath)
public struct BreathEffect: LEDEffect {
    public let id = UUID()
    public let name = "Breathe"
    public let metadata = EffectMetadata(
        displayName: "Breathe",
        category: .classic,
        parameters: [
            EffectParameter(name: "speed", displayName: "Speed", range: 0...255, defaultValue: 128, stepSize: 1, unit: nil)
        ],
        supportsAudio: false,
        supports2D: false
    )

    public func render(segment: Segment, time: TimeInterval) -> TimeInterval {
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
        let bgColor = segment.colors.count > 1 ? segment.colors[1] : .black

        for i in 0..<segment.length {
            let blended = blend(bgColor, baseColor, ratio: luminosity)
            segment.setPixel(i, color: blended)
        }

        return 0.042 // ~24fps like WLED FRAMETIME
    }
}
```

**Reference**: `/home/user/wled/wled00/FX.cpp` lines 123-10700

**Testing**: Visual comparison with WLED effects

**Estimated complexity**: Very High (20 effects to port)

---

### Package: LEDNetworking

**Purpose**: WLED JSON API client

**Dependencies**: LEDModels, LEDCore

**Public API**:
```swift
/// WLED API client
public class WLEDClient: LEDHardwareController {
    public let baseURL: URL

    public init(host: String, port: Int = 80)

    // State API
    public func getState() async throws -> WLEDState
    public func setState(_ state: WLEDState) async throws
    public func setEffect(_ effectID: Int) async throws
    public func setBrightness(_ brightness: UInt8) async throws
    public func setColors(_ colors: [LEDColor]) async throws

    // Real-time API
    public func sendFrame(_ pixels: [LEDColor]) async throws
    public var maxRefreshRate: Double { 60.0 }
    public var isConnected: Bool { get }

    // Info API
    public func getInfo() async throws -> WLEDInfo
    public func getEffects() async throws -> [String]
    public func getPalettes() async throws -> [String]
}

/// WLED state model
public struct WLEDState: Codable {
    public var on: Bool
    public var brightness: UInt8
    public var transition: Int
    public var segments: [WLEDSegment]
}

public struct WLEDSegment: Codable {
    public var id: Int
    public var start: Int
    public var stop: Int
    public var colors: [[UInt8]]
    public var effectID: Int
    public var speed: UInt8
    public var intensity: UInt8
}

/// Device discovery
public class WLEDDiscovery {
    public func startDiscovering()
    public func stopDiscovering()
    public var discoveredDevices: AsyncStream<WLEDDevice> { get }
}
```

**Reference**: WLED JSON API docs

**Testing**: Mock server, error handling

**Estimated complexity**: Medium (HTTP client)

---

### Package: LEDProtocols

**Purpose**: E1.31, Art-Net, DDP protocols

**Dependencies**: LEDPrimitives, LEDCore

**Public API**:
```swift
/// E1.31 (sACN) controller
public class E131Controller: LEDHardwareController {
    public init(universeID: UInt16, host: String, port: UInt16 = 5568)

    public func sendFrame(_ pixels: [LEDColor]) async throws
    public var maxRefreshRate: Double { 120.0 }
}

/// E1.31 packet builder
struct E131Packet {
    static let protocolVersion: UInt16 = 0x0001
    func build(universe: UInt16, pixels: [LEDColor], priority: UInt8 = 100) -> Data
}

/// Art-Net controller
public class ArtNetController: LEDHardwareController {
    public init(universe: UInt16, host: String, port: UInt16 = 6454)

    public func sendFrame(_ pixels: [LEDColor]) async throws
    public var maxRefreshRate: Double { 120.0 }
}

/// DDP (Display Discovery Protocol) controller
public class DDPController: LEDHardwareController {
    public init(host: String, port: UInt16 = 4048)

    public func sendFrame(_ pixels: [LEDColor]) async throws
}
```

**Reference**: E1.31, Art-Net, DDP protocol specs

**Testing**: Packet format validation

**Estimated complexity**: High (binary protocols)

---

### Package: LEDDiscovery

**Purpose**: Network device discovery (Bonjour/mDNS)

**Dependencies**: None

**Public API**:
```swift
/// Network discovery
public class LEDDeviceDiscovery: NSObject, NetServiceBrowserDelegate {
    public func startDiscovering()
    public func stopDiscovering()

    public var onDeviceFound: ((DiscoveredDevice) -> Void)?
    public var onDeviceLost: ((DiscoveredDevice) -> Void)?
}

/// Discovered device
public struct DiscoveredDevice {
    public var name: String
    public var host: String
    public var port: Int
    public var type: DeviceType

    public enum DeviceType {
        case wled
        case artNet
        case e131
        case unknown
    }
}
```

**Testing**: Mock mDNS responses

**Estimated complexity**: Medium (Bonjour APIs)

---

### Package: LEDBluetooth

**Purpose**: Bluetooth LE communication

**Dependencies**: LEDCore, LEDPrimitives

**Public API**:
```swift
/// BLE controller
public class BLELEDController: NSObject, LEDHardwareController, CBCentralManagerDelegate {
    public init()

    public func scan()
    public func connect(to device: CBPeripheral) async throws
    public func disconnect()

    public func sendFrame(_ pixels: [LEDColor]) async throws
    public var maxRefreshRate: Double { 30.0 } // BLE is slower
    public var isConnected: Bool { get }

    // BLE characteristics
    static let ledServiceUUID: CBUUID
    static let pixelCharacteristicUUID: CBUUID
}
```

**Testing**: Mock BLE peripherals

**Estimated complexity**: Medium (CoreBluetooth)

---

### Package: LEDSimulator

**Purpose**: On-device LED simulation for preview

**Dependencies**: LEDCore, LEDPrimitives, LEDBuffers

**Public API**:
```swift
/// Virtual LED strip for preview
public class VirtualLEDStrip: LEDHardwareController {
    public var pixels: [LEDColor] { get }
    public var onPixelsChanged: (([LEDColor]) -> Void)?

    public init(ledCount: Int)

    public func sendFrame(_ pixels: [LEDColor]) async throws
    public var maxRefreshRate: Double { 60.0 }
    public var isConnected: Bool { true }
}

/// Simulator for multiple strips/devices
public class LEDSimulator {
    private var virtualStrips: [VirtualLEDStrip] = []

    public func addStrip(ledCount: Int) -> VirtualLEDStrip
    public func removeStrip(_ strip: VirtualLEDStrip)
    public func allStrips() -> [VirtualLEDStrip]
}
```

**Testing**: Pixel updates, callbacks

**Estimated complexity**: Low (in-memory state)

---

## Wave 4: Apple Integration (8 packages - ALL PARALLEL)

### Package: LEDAppIntents

**Purpose**: Siri & Shortcuts integration

**Dependencies**: LEDCore, LEDModels, LEDEffectRegistry

**Public API**:
```swift
import AppIntents

/// Set effect intent
public struct SetLEDEffectIntent: AppIntent {
    public static var title: LocalizedStringResource = "Set LED Effect"
    public static var description: LocalizedStringResource = "Change the current LED effect"

    @Parameter(title: "Effect Name")
    public var effectName: String

    @Parameter(title: "Speed", default: 128)
    public var speed: Int

    public func perform() async throws -> some IntentResult {
        // Implementation
    }
}

/// Set brightness intent
public struct SetLEDBrightnessIntent: AppIntent {
    @Parameter(title: "Brightness", default: 128)
    public var brightness: Int

    public func perform() async throws -> some IntentResult
}

/// Set color intent
public struct SetLEDColorIntent: AppIntent {
    @Parameter(title: "Color")
    public var color: String // "red", "blue", "#FF0000"

    public func perform() async throws -> some IntentResult
}

/// App shortcuts provider
public struct LEDShortcutsProvider: AppShortcutsProvider {
    public static var appShortcuts: [AppShortcut] {
        AppShortcut(
            intent: SetLEDEffectIntent(),
            phrases: [
                "Set \(.applicationName) to \(\.$effectName)",
                "Change my lights to \(\.$effectName)"
            ]
        )
    }
}
```

**Testing**: Intent execution, parameter parsing

**Estimated complexity**: Medium (App Intents framework)

---

### Package: LEDWidgetKit

**Purpose**: Home screen widgets

**Dependencies**: LEDCore, LEDModels

**Public API**:
```swift
import WidgetKit
import SwiftUI

/// Widget showing current effect
public struct CurrentEffectWidget: Widget {
    public var body: some WidgetConfiguration {
        StaticConfiguration(kind: "CurrentEffect", provider: CurrentEffectProvider()) { entry in
            CurrentEffectEntryView(entry: entry)
        }
        .configurationDisplayName("Current Effect")
        .description("Shows the currently active LED effect")
    }
}

/// Timeline provider
struct CurrentEffectProvider: TimelineProvider {
    func timeline(for configuration: ConfigurationIntent, in context: Context) async -> Timeline<CurrentEffectEntry>
}

/// Widget entry
struct CurrentEffectEntry: TimelineEntry {
    let date: Date
    let effectName: String
    let colors: [LEDColor]
}

/// Widget view
struct CurrentEffectEntryView: View {
    var entry: CurrentEffectEntry

    var body: some View {
        // Visual representation
    }
}
```

**Testing**: Timeline updates, entry rendering

**Estimated complexity**: Low (WidgetKit)

---

### Package: LEDWatchKit

**Purpose**: Apple Watch companion app

**Dependencies**: LEDCore, LEDModels, LEDEffectRegistry

**Public API**:
```swift
#if os(watchOS)
import WatchKit
import SwiftUI

/// Watch app main view
public struct WatchControlView: View {
    @State private var effects: [LEDEffect] = []
    @State private var brightness: Double = 128

    public var body: some View {
        NavigationView {
            List {
                Section("Quick Effects") {
                    ForEach(effects.prefix(5), id: \.id) { effect in
                        Button(effect.name) {
                            // Set effect
                        }
                    }
                }

                Section("Controls") {
                    Slider(value: $brightness, in: 0...255)
                }
            }
        }
    }
}

/// Watch connectivity
public class WatchConnectivity: NSObject, WCSessionDelegate {
    public static let shared = WatchConnectivity()

    public func sendEffect(_ effectID: UUID)
    public func sendBrightness(_ brightness: UInt8)
}
#endif
```

**Testing**: Watch communication

**Estimated complexity**: Low (Watch UI)

---

### Package: LEDLiveActivity

**Purpose**: Dynamic Island integration

**Dependencies**: LEDModels

**Public API**:
```swift
#if canImport(ActivityKit)
import ActivityKit

/// Live activity for effect changes
public struct LEDEffectActivity: ActivityAttributes {
    public struct ContentState: Codable, Hashable {
        public var effectName: String
        public var colors: [String] // Hex colors
        public var brightness: UInt8
    }
}

/// Live activity manager
public class LEDLiveActivityManager {
    public func start(effectName: String, colors: [LEDColor]) async throws
    public func update(effectName: String, colors: [LEDColor]) async throws
    public func end() async throws
}
#endif
```

**Testing**: Activity updates

**Estimated complexity**: Low (ActivityKit)

---

### Package: LEDFocusFilter

**Purpose**: Focus mode integration

**Dependencies**: LEDCore

**Public API**:
```swift
#if canImport(AppIntents)
import AppIntents

/// Focus filter for LED control
public struct LEDFocusFilter: SetFocusFilterIntent {
    public func perform() async throws -> some IntentResult {
        // Dim lights or change effect based on focus mode
    }
}

/// Focus mode presets
public enum FocusModePreset {
    case work // Bright, steady
    case sleep // Dim, warm
    case doNotDisturb // Off
    case personal // Normal
}
#endif
```

**Testing**: Focus mode changes

**Estimated complexity**: Low

---

### Package: LEDSiriTips

**Purpose**: Siri suggestions

**Dependencies**: LEDAppIntents

**Public API**:
```swift
import Intents

/// Donation manager for Siri suggestions
public class SiriTipDonationManager {
    public func donateEffectChange(_ effect: LEDEffect)
    public func donateBrightnessChange(_ brightness: UInt8)
    public func donateColorChange(_ color: LEDColor)
}

/// Siri tip provider
public class LEDSiriTipProvider {
    public func relevantShortcuts() -> [INRelevantShortcut]
}
```

**Testing**: Donation behavior

**Estimated complexity**: Low

---

### Package: LEDSpotlight

**Purpose**: Spotlight search integration

**Dependencies**: LEDCore, LEDModels

**Public API**:
```swift
import CoreSpotlight

/// Spotlight indexer for effects
public class LEDSpotlightIndexer {
    public func indexEffects(_ effects: [LEDEffect]) async throws
    public func deleteAllItems() async throws
}

/// Searchable effect
extension LEDEffect {
    public var searchableAttributes: CSSearchableItemAttributeSet {
        let attributes = CSSearchableItemAttributeSet(contentType: .item)
        attributes.title = name
        attributes.contentDescription = metadata.displayName
        return attributes
    }
}
```

**Testing**: Search results

**Estimated complexity**: Low

---

### Package: LEDHandoff

**Purpose**: Continuity/Handoff support

**Dependencies**: LEDCore

**Public API**:
```swift
import Foundation

/// Handoff manager
public class LEDHandoffManager {
    public func advertiseCurrentEffect(_ effect: LEDEffect)
    public func continueActivity(_ userActivity: NSUserActivity) throws -> LEDEffect?
}

/// User activity types
extension NSUserActivity {
    static let ledEffectType = "com.swiftled.effectControl"
}
```

**Testing**: Activity continuation

**Estimated complexity**: Low

---

## Wave 5: UI & Preview (5 packages)

### Package: LEDEffectEditor

**Purpose**: Effect creation/editing UI

**Dependencies**: LEDEffectDSL, LEDModels, LEDCore

**Public API**:
```swift
import SwiftUI

/// Effect editor main view
public struct EffectEditorView: View {
    @State private var effectCode: String = ""
    @State private var previewEffect: LEDEffect?

    public var body: some View {
        VStack {
            CodeEditor(text: $effectCode)
            Button("Preview") {
                previewEffect = compileEffect(effectCode)
            }
        }
    }
}

/// DSL code editor
struct CodeEditor: View {
    @Binding var text: String

    var body: some View {
        TextEditor(text: $text)
            .font(.system(.body, design: .monospaced))
    }
}

/// Effect compiler (DSL → LEDEffect)
public class EffectCompiler {
    public func compile(_ code: String) throws -> LEDEffect
}
```

**Testing**: Code editing, compilation

**Estimated complexity**: High (code editing UI)

---

### Package: LEDPreviewView

**Purpose**: Real-time effect preview

**Dependencies**: LEDSimulator, LEDEffectRuntime, LEDCore

**Public API**:
```swift
import SwiftUI

/// Live preview view
public struct LEDPreviewView: View {
    @StateObject private var simulator = VirtualLEDStrip(ledCount: 150)
    @StateObject private var runtime = EffectRuntime()
    let effect: LEDEffect

    public var body: some View {
        Canvas { context, size in
            let pixelWidth = size.width / CGFloat(simulator.pixels.count)

            for (index, color) in simulator.pixels.enumerated() {
                let rect = CGRect(
                    x: CGFloat(index) * pixelWidth,
                    y: 0,
                    width: pixelWidth,
                    height: size.height
                )
                context.fill(Path(rect), with: .color(color.toSwiftUIColor()))
            }
        }
        .frame(height: 100)
        .onAppear {
            let segment = LEDSegment(startIndex: 0, length: 150)
            runtime.setEffect(effect, on: [segment])
            runtime.start()
        }
    }
}

/// Color conversion
extension LEDColor {
    func toSwiftUIColor() -> SwiftUI.Color {
        Color(red: Double(red) / 255.0, green: Double(green) / 255.0, blue: Double(blue) / 255.0)
    }
}
```

**Testing**: Preview rendering, performance

**Estimated complexity**: Medium (Canvas rendering)

---

### Package: LEDLibraryView

**Purpose**: Effect library browser

**Dependencies**: LEDEffectRegistry, LEDModels

**Public API**:
```swift
import SwiftUI

/// Effect library browser
public struct EffectLibraryView: View {
    @State private var effects: [LEDEffect] = []
    @State private var selectedCategory: EffectCategory = .classic

    public var body: some View {
        NavigationView {
            VStack {
                Picker("Category", selection: $selectedCategory) {
                    ForEach(EffectCategory.allCases, id: \.self) { category in
                        Text(category.rawValue).tag(category)
                    }
                }
                .pickerStyle(.segmented)

                List(filteredEffects) { effect in
                    EffectRow(effect: effect)
                }
            }
            .navigationTitle("Effects")
        }
    }

    var filteredEffects: [LEDEffect] {
        effects.filter { $0.metadata.category == selectedCategory }
    }
}

/// Effect list row
struct EffectRow: View {
    let effect: LEDEffect

    var body: some View {
        HStack {
            Text(effect.metadata.displayName)
            Spacer()
            ForEach(effect.metadata.colors.prefix(3), id: \.self) { color in
                Circle()
                    .fill(color.toSwiftUIColor())
                    .frame(width: 20, height: 20)
            }
        }
    }
}
```

**Testing**: Filtering, navigation

**Estimated complexity**: Low (standard SwiftUI)

---

### Package: LEDParameterUI

**Purpose**: Parameter controls

**Dependencies**: LEDParameters, LEDModels

**Public API**:
```swift
import SwiftUI

/// Parameter controls view
public struct ParameterControlsView: View {
    let parameters: [EffectParameter]
    @ObservedObject var values: ParameterValues

    public var body: some View {
        Form {
            ForEach(parameters, id: \.name) { parameter in
                ParameterSlider(parameter: parameter, value: $values[parameter.name])
            }
        }
    }
}

/// Individual parameter slider
struct ParameterSlider: View {
    let parameter: EffectParameter
    @Binding var value: Double

    var body: some View {
        VStack(alignment: .leading) {
            Text(parameter.displayName)
            HStack {
                Slider(value: $value, in: parameter.range, step: parameter.stepSize)
                Text("\(Int(value))")
                    .frame(width: 50)
                if let unit = parameter.unit {
                    Text(unit.rawValue)
                }
            }
        }
    }
}
```

**Testing**: Parameter binding, validation

**Estimated complexity**: Low (SwiftUI controls)

---

### Package: LEDApp

**Purpose**: Main application

**Dependencies**: ALL previous packages

**Public API**:
```swift
import SwiftUI

@main
struct LEDApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
        }
    }
}

/// Main content view
struct ContentView: View {
    @StateObject private var effectManager = EffectManager.shared

    var body: some View {
        TabView {
            EffectLibraryView()
                .tabItem {
                    Label("Effects", systemImage: "sparkles")
                }

            LEDPreviewView(effect: effectManager.currentEffect)
                .tabItem {
                    Label("Preview", systemImage: "eye")
                }

            EffectEditorView()
                .tabItem {
                    Label("Create", systemImage: "hammer")
                }

            SettingsView()
                .tabItem {
                    Label("Settings", systemImage: "gear")
                }
        }
    }
}

/// App-level state manager
public class EffectManager: ObservableObject {
    public static let shared = EffectManager()

    @Published public var currentEffect: LEDEffect
    @Published public var connectedDevices: [LEDDeviceConfig] = []

    private let runtime = EffectRuntime()
    private let registry = LEDEffectRegistryImpl.shared

    public func setEffect(_ effect: LEDEffect) {
        currentEffect = effect
        // Apply to hardware
    }
}
```

**Testing**: App integration, navigation

**Estimated complexity**: Medium (app composition)

---

## Testing Strategy

### Per-Package Testing
- **Unit tests**: 80%+ coverage
- **Integration tests**: Cross-package interactions
- **Performance tests**: Frame rate, memory usage

### Wave-Level Testing
- **Wave completion tests**: All packages build together
- **Dependency verification**: No circular dependencies
- **API compatibility**: Packages interoperate correctly

### System Testing
- **Visual comparison**: Effects vs WLED reference
- **Hardware testing**: Real device communication
- **Performance benchmarks**: Frame rates on real hardware

---

## Documentation Requirements

Each package must include:
1. **README.md**: Package purpose, usage examples
2. **Inline documentation**: DocC comments for all public APIs
3. **CHANGELOG.md**: Version history
4. **Examples/**: Sample code for common use cases

---

## Code Review Checklist

- [ ] Follows Swift API Design Guidelines
- [ ] All public APIs documented
- [ ] Tests pass with 80%+ coverage
- [ ] No compiler warnings
- [ ] Follows package dependency rules
- [ ] Performance acceptable (60fps for effects)
- [ ] Memory leaks checked (Instruments)

---

## Delivery Timeline

**Wave 1**: 2 days (foundation)
**Wave 2**: 4 days (core systems)
**Wave 3**: 5 days (effects & protocols)
**Wave 4**: 3 days (Apple integration)
**Wave 5**: 4 days (UI)

**Total**: ~3 weeks with 8 parallel agents

---

## Success Metrics

**MVP Success (Waves 1-3)**:
- [ ] 20 effects ported and working
- [ ] Swift DSL creates custom effects
- [ ] Preview works on iOS device
- [ ] Controls real WLED hardware

**Full Success (All Waves)**:
- [ ] All Apple platform features working
- [ ] "Hey Siri" commands functional
- [ ] Effect editor creates new effects
- [ ] Share effects via AirDrop
- [ ] Sub-30ms latency to hardware

---

*Ready to parallelize! Each package is independent within its wave.* 🚀
