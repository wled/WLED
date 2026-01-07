package com.example.arklights.data

import kotlinx.serialization.Serializable

// LED Effect IDs (matching the C++ definitions)
object LEDEffects {
    const val SOLID = 0
    const val BREATH = 1
    const val RAINBOW = 2
    const val CHASE = 3
    const val BLINK_RAINBOW = 4
    const val TWINKLE = 5
    const val FIRE = 6
    const val METEOR = 7
    const val WAVE = 8
    const val COMET = 9
    const val CANDLE = 10
    const val STATIC_RAINBOW = 11
    const val KNIGHT_RIDER = 12
    const val POLICE = 13
    const val STROBE = 14
    const val LARSON_SCANNER = 15
    const val COLOR_WIPE = 16
    const val THEATER_CHASE = 17
    const val RUNNING_LIGHTS = 18
    const val COLOR_SWEEP = 19
    
    val effectNames = mapOf(
        SOLID to "Solid",
        BREATH to "Breath",
        RAINBOW to "Rainbow",
        CHASE to "Chase",
        BLINK_RAINBOW to "Blink Rainbow",
        TWINKLE to "Twinkle",
        FIRE to "Fire",
        METEOR to "Meteor",
        WAVE to "Wave",
        COMET to "Comet",
        CANDLE to "Candle",
        STATIC_RAINBOW to "Static Rainbow",
        KNIGHT_RIDER to "Knight Rider",
        POLICE to "Police",
        STROBE to "Strobe",
        LARSON_SCANNER to "Larson Scanner",
        COLOR_WIPE to "Color Wipe",
        THEATER_CHASE to "Theater Chase",
        RUNNING_LIGHTS to "Running Lights",
        COLOR_SWEEP to "Color Sweep"
    )
}

// Preset IDs
object Presets {
    const val STANDARD = 0
    const val NIGHT = 1
    const val PARTY = 2
    const val STEALTH = 3
    
    val presetNames = mapOf(
        STANDARD to "Standard",
        NIGHT to "Night",
        PARTY to "Party",
        STEALTH to "Stealth"
    )
}

// Startup Sequence IDs
object StartupSequences {
    const val NONE = 0
    const val POWER_ON = 1
    const val SCAN = 2
    const val WAVE = 3
    const val RACE = 4
    const val CUSTOM = 5
    
    val sequenceNames = mapOf(
        NONE to "None",
        POWER_ON to "Power On",
        SCAN to "Scanner",
        WAVE to "Wave",
        RACE to "Race",
        CUSTOM to "Custom"
    )
}

// LED Types
object LEDTypes {
    const val SK6812_RGBW = 0
    const val SK6812_RGB = 1
    const val WS2812B_RGB = 2
    const val APA102_RGB = 3
    const val LPD8806_RGB = 4
    
    val typeNames = mapOf(
        SK6812_RGBW to "SK6812 (RGBW)",
        SK6812_RGB to "SK6812 (RGB)",
        WS2812B_RGB to "WS2812B (RGB)",
        APA102_RGB to "APA102 (RGB)",
        LPD8806_RGB to "LPD8806 (RGB)"
    )
}

// Color Orders
object ColorOrders {
    const val RGB = 0
    const val GRB = 1
    const val BGR = 2
    
    val orderNames = mapOf(
        RGB to "RGB",
        GRB to "GRB",
        BGR to "BGR"
    )
}

// API Request Models
@Serializable
data class LEDControlRequest(
    val preset: Int? = null,
    val brightness: Int? = null,
    val effectSpeed: Int? = null,
    val headlightColor: String? = null,
    val taillightColor: String? = null,
    val headlightEffect: Int? = null,
    val taillightEffect: Int? = null,
    val startup_sequence: Int? = null,
    val startup_duration: Int? = null,
    val testStartup: Boolean? = null,
    val testParkMode: Boolean? = null,
    
    // Motion Control
    val motion_enabled: Boolean? = null,
    val blinker_enabled: Boolean? = null,
    val park_mode_enabled: Boolean? = null,
    val impact_detection_enabled: Boolean? = null,
    val motion_sensitivity: Double? = null,
    val blinker_delay: Int? = null,
    val blinker_timeout: Int? = null,
    val park_stationary_time: Int? = null,
    val park_accel_noise_threshold: Double? = null,
    val park_gyro_noise_threshold: Double? = null,
    val impact_threshold: Double? = null,
    
    // Park Mode Settings
    val park_effect: Int? = null,
    val park_effect_speed: Int? = null,
    val park_brightness: Int? = null,
    val park_headlight_color_r: Int? = null,
    val park_headlight_color_g: Int? = null,
    val park_headlight_color_b: Int? = null,
    val park_taillight_color_r: Int? = null,
    val park_taillight_color_g: Int? = null,
    val park_taillight_color_b: Int? = null,
    
    // Calibration
    val startCalibration: Boolean? = null,
    val nextCalibrationStep: Boolean? = null,
    val resetCalibration: Boolean? = null,
    
    // WiFi Configuration
    val apName: String? = null,
    val apPassword: String? = null,
    val restart: Boolean? = null,
    
    // ESPNow Configuration
    val enableESPNow: Boolean? = null,
    val useESPNowSync: Boolean? = null,
    val espNowChannel: Int? = null,
    
    // Group Management
    val deviceName: String? = null,
    val groupAction: String? = null,
    val groupCode: String? = null
)

@Serializable
data class LEDConfigRequest(
    val headlightLedCount: Int,
    val taillightLedCount: Int,
    val headlightLedType: Int,
    val taillightLedType: Int,
    val headlightColorOrder: Int,
    val taillightColorOrder: Int
)

// API Response Models
@Serializable
data class LEDStatus(
    val preset: Int,
    val brightness: Int,
    val effectSpeed: Int,
    val startup_sequence: Int,
    val startup_sequence_name: String,
    val startup_duration: Int,
    val motion_enabled: Boolean,
    val blinker_enabled: Boolean,
    val park_mode_enabled: Boolean,
    val impact_detection_enabled: Boolean,
    val motion_sensitivity: Double,
    val blinker_delay: Int,
    val blinker_timeout: Int,
    val park_stationary_time: Int,
    val park_accel_noise_threshold: Double,
    val park_gyro_noise_threshold: Double,
    val impact_threshold: Double,
    val park_effect: Int,
    val park_effect_speed: Int,
    val park_brightness: Int,
    val park_headlight_color_r: Int,
    val park_headlight_color_g: Int,
    val park_headlight_color_b: Int,
    val park_taillight_color_r: Int,
    val park_taillight_color_g: Int,
    val park_taillight_color_b: Int,
    val blinker_active: Boolean,
    val blinker_direction: Int,
    val park_mode_active: Boolean,
    val calibration_complete: Boolean,
    val calibration_mode: Boolean,
    val calibration_step: Int,
    val apName: String,
    val apPassword: String,
    val headlightColor: String,
    val taillightColor: String,
    val headlightEffect: Int,
    val taillightEffect: Int,
    val headlightLedCount: Int,
    val taillightLedCount: Int,
    val headlightLedType: Int,
    val taillightLedType: Int,
    val headlightColorOrder: Int,
    val taillightColorOrder: Int,
    val enableESPNow: Boolean,
    val useESPNowSync: Boolean,
    val espNowChannel: Int,
    val espNowStatus: String,
    val espNowPeerCount: Int,
    val espNowLastSend: String,
    val groupCode: String,
    val isGroupMaster: Boolean,
    val groupMemberCount: Int,
    val deviceName: String,
    val ota_status: String,
    val ota_progress: Int,
    val ota_error: String?,
    val ota_in_progress: Boolean,
    val build_date: String
)

@Serializable
data class ApiResponse(
    val success: Boolean,
    val message: String? = null,
    val error: String? = null
)

// Bluetooth Device Model
data class BluetoothDevice(
    val name: String,
    val address: String,
    val isConnected: Boolean = false
)

// Connection State
enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR
}
