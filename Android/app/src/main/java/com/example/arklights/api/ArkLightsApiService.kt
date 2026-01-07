package com.example.arklights.api

import com.example.arklights.bluetooth.BluetoothService
import com.example.arklights.data.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.withContext
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json

class ArkLightsApiService(private val bluetoothService: BluetoothService) {
    
    private val _isLoading = MutableStateFlow(false)
    val isLoading: StateFlow<Boolean> = _isLoading.asStateFlow()
    
    private val _lastError = MutableStateFlow<String?>(null)
    val lastError: StateFlow<String?> = _lastError.asStateFlow()
    
    suspend fun getStatus(): LEDStatus? = withContext(Dispatchers.IO) {
        if (!bluetoothService.isConnected()) {
            _lastError.value = "Not connected to device"
            return@withContext null
        }
        
        _isLoading.value = true
        _lastError.value = null
        
        try {
            val response = bluetoothService.sendHttpRequest("/api/status", "GET")
            if (response != null) {
                // For now, return a mock status - we'll implement proper JSON parsing later
                LEDStatus(
                    preset = 0,
                    brightness = 128,
                    effectSpeed = 64,
                    startup_sequence = 0,
                    startup_sequence_name = "None",
                    startup_duration = 3000,
                    motion_enabled = false,
                    blinker_enabled = false,
                    park_mode_enabled = false,
                    impact_detection_enabled = false,
                    motion_sensitivity = 1.0,
                    blinker_delay = 300,
                    blinker_timeout = 2000,
                    park_stationary_time = 2000,
                    park_accel_noise_threshold = 0.1,
                    park_gyro_noise_threshold = 0.5,
                    impact_threshold = 3.0,
                    park_effect = 0,
                    park_effect_speed = 64,
                    park_brightness = 128,
                    park_headlight_color_r = 0,
                    park_headlight_color_g = 0,
                    park_headlight_color_b = 255,
                    park_taillight_color_r = 0,
                    park_taillight_color_g = 0,
                    park_taillight_color_b = 255,
                    blinker_active = false,
                    blinker_direction = 0,
                    park_mode_active = false,
                    calibration_complete = false,
                    calibration_mode = false,
                    calibration_step = 0,
                    apName = "ARKLIGHTS-AP",
                    apPassword = "float420",
                    headlightColor = "ffffff",
                    taillightColor = "ff0000",
                    headlightEffect = 0,
                    taillightEffect = 0,
                    headlightLedCount = 20,
                    taillightLedCount = 20,
                    headlightLedType = 0,
                    taillightLedType = 0,
                    headlightColorOrder = 0,
                    taillightColorOrder = 0,
                    enableESPNow = false,
                    useESPNowSync = false,
                    espNowChannel = 1,
                    espNowStatus = "Initializing",
                    espNowPeerCount = 0,
                    espNowLastSend = "Never",
                    groupCode = "",
                    isGroupMaster = false,
                    groupMemberCount = 0,
                    deviceName = "ArkLights Device",
                    ota_status = "Ready",
                    ota_progress = 0,
                    ota_error = null,
                    ota_in_progress = false,
                    build_date = "2024-01-01"
                )
            } else {
                _lastError.value = "Failed to get response"
                null
            }
        } catch (e: Exception) {
            _lastError.value = "Error parsing response: ${e.message}"
            null
        } finally {
            _isLoading.value = false
        }
    }
    
    suspend fun sendControlRequest(request: LEDControlRequest): Boolean = withContext(Dispatchers.IO) {
        if (!bluetoothService.isConnected()) {
            _lastError.value = "Not connected to device"
            return@withContext false
        }
        
        _isLoading.value = true
        _lastError.value = null
        
        try {
            // Serialize the LED control request to JSON
            val jsonBody = Json.encodeToString(request)
            println("Android: Sending LED control request: $jsonBody")
            
            val response = bluetoothService.sendHttpRequest("/api", "POST", jsonBody)
            
            if (response != null && response.contains("200 OK")) {
                true
            } else {
                _lastError.value = "Request failed"
                false
            }
        } catch (e: Exception) {
            _lastError.value = "Error sending request: ${e.message}"
            false
        } finally {
            _isLoading.value = false
        }
    }
    
    suspend fun updateLEDConfig(config: LEDConfigRequest): Boolean = withContext(Dispatchers.IO) {
        if (!bluetoothService.isConnected()) {
            _lastError.value = "Not connected to device"
            return@withContext false
        }
        
        _isLoading.value = true
        _lastError.value = null
        
        try {
            // Serialize the LED config request to JSON
            val jsonBody = Json.encodeToString(config)
            println("Android: Sending LED config request: $jsonBody")
            
            val response = bluetoothService.sendHttpRequest("/api/led-config", "POST", jsonBody)
            
            if (response != null && response.contains("200 OK")) {
                true
            } else {
                _lastError.value = "LED config update failed"
                false
            }
        } catch (e: Exception) {
            _lastError.value = "Error updating LED config: ${e.message}"
            false
        } finally {
            _isLoading.value = false
        }
    }
    
    suspend fun testLEDs(): Boolean = withContext(Dispatchers.IO) {
        if (!bluetoothService.isConnected()) {
            _lastError.value = "Not connected to device"
            return@withContext false
        }
        
        _isLoading.value = true
        _lastError.value = null
        
        try {
            val response = bluetoothService.sendHttpRequest("/api/led-test", "POST")
            
            if (response != null && response.contains("200 OK")) {
                true
            } else {
                _lastError.value = "LED test failed"
                false
            }
        } catch (e: Exception) {
            _lastError.value = "Error testing LEDs: ${e.message}"
            false
        } finally {
            _isLoading.value = false
        }
    }
    
    // Convenience methods for common operations
    suspend fun setPreset(preset: Int): Boolean {
        return sendControlRequest(LEDControlRequest(preset = preset))
    }
    
    suspend fun setBrightness(brightness: Int): Boolean {
        return sendControlRequest(LEDControlRequest(brightness = brightness))
    }
    
    suspend fun setEffectSpeed(speed: Int): Boolean {
        return sendControlRequest(LEDControlRequest(effectSpeed = speed))
    }
    
    suspend fun setHeadlightColor(color: String): Boolean {
        return sendControlRequest(LEDControlRequest(headlightColor = color))
    }
    
    suspend fun setTaillightColor(color: String): Boolean {
        return sendControlRequest(LEDControlRequest(taillightColor = color))
    }
    
    suspend fun setHeadlightEffect(effect: Int): Boolean {
        return sendControlRequest(LEDControlRequest(headlightEffect = effect))
    }
    
    suspend fun setTaillightEffect(effect: Int): Boolean {
        return sendControlRequest(LEDControlRequest(taillightEffect = effect))
    }
    
    suspend fun setMotionEnabled(enabled: Boolean): Boolean {
        return sendControlRequest(LEDControlRequest(motion_enabled = enabled))
    }
    
    suspend fun setBlinkerEnabled(enabled: Boolean): Boolean {
        return sendControlRequest(LEDControlRequest(blinker_enabled = enabled))
    }
    
    suspend fun setParkModeEnabled(enabled: Boolean): Boolean {
        return sendControlRequest(LEDControlRequest(park_mode_enabled = enabled))
    }
    
    suspend fun setImpactDetectionEnabled(enabled: Boolean): Boolean {
        return sendControlRequest(LEDControlRequest(impact_detection_enabled = enabled))
    }
    
    suspend fun setMotionSensitivity(sensitivity: Double): Boolean {
        return sendControlRequest(LEDControlRequest(motion_sensitivity = sensitivity))
    }
    
    suspend fun setBlinkerDelay(delay: Int): Boolean {
        return sendControlRequest(LEDControlRequest(blinker_delay = delay))
    }
    
    suspend fun setBlinkerTimeout(timeout: Int): Boolean {
        return sendControlRequest(LEDControlRequest(blinker_timeout = timeout))
    }
    
    suspend fun setParkStationaryTime(time: Int): Boolean {
        return sendControlRequest(LEDControlRequest(park_stationary_time = time))
    }
    
    suspend fun setParkAccelNoiseThreshold(threshold: Double): Boolean {
        return sendControlRequest(LEDControlRequest(park_accel_noise_threshold = threshold))
    }
    
    suspend fun setParkGyroNoiseThreshold(threshold: Double): Boolean {
        return sendControlRequest(LEDControlRequest(park_gyro_noise_threshold = threshold))
    }
    
    suspend fun setImpactThreshold(threshold: Double): Boolean {
        return sendControlRequest(LEDControlRequest(impact_threshold = threshold))
    }
    
    suspend fun setParkEffect(effect: Int): Boolean {
        return sendControlRequest(LEDControlRequest(park_effect = effect))
    }
    
    suspend fun setParkEffectSpeed(speed: Int): Boolean {
        return sendControlRequest(LEDControlRequest(park_effect_speed = speed))
    }
    
    suspend fun setParkBrightness(brightness: Int): Boolean {
        return sendControlRequest(LEDControlRequest(park_brightness = brightness))
    }
    
    suspend fun setParkHeadlightColor(r: Int, g: Int, b: Int): Boolean {
        return sendControlRequest(LEDControlRequest(
            park_headlight_color_r = r,
            park_headlight_color_g = g,
            park_headlight_color_b = b
        ))
    }
    
    suspend fun setParkTaillightColor(r: Int, g: Int, b: Int): Boolean {
        return sendControlRequest(LEDControlRequest(
            park_taillight_color_r = r,
            park_taillight_color_g = g,
            park_taillight_color_b = b
        ))
    }
    
    suspend fun startCalibration(): Boolean {
        return sendControlRequest(LEDControlRequest(startCalibration = true))
    }
    
    suspend fun nextCalibrationStep(): Boolean {
        return sendControlRequest(LEDControlRequest(nextCalibrationStep = true))
    }
    
    suspend fun resetCalibration(): Boolean {
        return sendControlRequest(LEDControlRequest(resetCalibration = true))
    }
    
    suspend fun testStartupSequence(): Boolean {
        return sendControlRequest(LEDControlRequest(testStartup = true))
    }
    
    suspend fun testParkMode(): Boolean {
        return sendControlRequest(LEDControlRequest(testParkMode = true))
    }
    
    suspend fun setStartupSequence(sequence: Int): Boolean {
        return sendControlRequest(LEDControlRequest(startup_sequence = sequence))
    }
    
    suspend fun setStartupDuration(duration: Int): Boolean {
        return sendControlRequest(LEDControlRequest(startup_duration = duration))
    }
    
    suspend fun setAPName(name: String): Boolean {
        return sendControlRequest(LEDControlRequest(apName = name))
    }
    
    suspend fun setAPPassword(password: String): Boolean {
        return sendControlRequest(LEDControlRequest(apPassword = password))
    }
    
    suspend fun applyWiFiConfig(name: String, password: String): Boolean {
        return sendControlRequest(LEDControlRequest(
            apName = name,
            apPassword = password,
            restart = true
        ))
    }
    
    suspend fun setESPNowEnabled(enabled: Boolean): Boolean {
        return sendControlRequest(LEDControlRequest(enableESPNow = enabled))
    }
    
    suspend fun setESPNowSync(enabled: Boolean): Boolean {
        return sendControlRequest(LEDControlRequest(useESPNowSync = enabled))
    }
    
    suspend fun setESPNowChannel(channel: Int): Boolean {
        return sendControlRequest(LEDControlRequest(espNowChannel = channel))
    }
    
    suspend fun setDeviceName(name: String): Boolean {
        return sendControlRequest(LEDControlRequest(deviceName = name))
    }
    
    suspend fun createGroup(code: String): Boolean {
        return sendControlRequest(LEDControlRequest(
            groupAction = "create",
            groupCode = code
        ))
    }
    
    suspend fun joinGroup(code: String): Boolean {
        return sendControlRequest(LEDControlRequest(
            groupAction = "join",
            groupCode = code
        ))
    }
    
    suspend fun leaveGroup(): Boolean {
        return sendControlRequest(LEDControlRequest(groupAction = "leave"))
    }
    
    suspend fun allowGroupJoin(): Boolean {
        return sendControlRequest(LEDControlRequest(groupAction = "allow_join"))
    }
    
    suspend fun blockGroupJoin(): Boolean {
        return sendControlRequest(LEDControlRequest(groupAction = "block_join"))
    }
}
