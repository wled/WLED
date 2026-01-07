package com.example.arklights.viewmodel

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.example.arklights.api.ArkLightsApiService
import com.example.arklights.bluetooth.BluetoothService
import com.example.arklights.data.*
import com.example.arklights.data.ConnectionState
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import android.bluetooth.BluetoothDevice

class ArkLightsViewModel(
    private val bluetoothService: BluetoothService,
    private val apiService: ArkLightsApiService
) : ViewModel() {
    
    // Connection state
    val connectionState = bluetoothService.connectionState
    val discoveredDevices = bluetoothService.discoveredDevices
    val errorMessage = bluetoothService.errorMessage
    
    // API state
    val isLoading = apiService.isLoading
    val lastError = apiService.lastError
    
    // Device status
    private val _deviceStatus = MutableStateFlow<LEDStatus?>(null)
    val deviceStatus: StateFlow<LEDStatus?> = _deviceStatus.asStateFlow()
    
    // UI state
    private val _currentPage = MutableStateFlow("main")
    val currentPage: StateFlow<String> = _currentPage.asStateFlow()
    
    private val _selectedDevice = MutableStateFlow<BluetoothDevice?>(null)
    val selectedDevice: StateFlow<BluetoothDevice?> = _selectedDevice.asStateFlow()
    
    init {
        // Auto-refresh status when connected
        viewModelScope.launch {
            connectionState.collect { state ->
                if (state == ConnectionState.CONNECTED) {
                    refreshStatus()
                    // Start periodic status updates
                    startStatusUpdates()
                }
            }
        }
    }
    
    fun setCurrentPage(page: String) {
        _currentPage.value = page
    }
    
    fun selectDevice(device: BluetoothDevice) {
        _selectedDevice.value = device
    }
    
    suspend fun startDiscovery(): Boolean {
        return bluetoothService.startDiscovery()
    }
    
    suspend fun stopDiscovery(): Boolean {
        return bluetoothService.stopDiscovery()
    }
    
    suspend fun getPairedDevices(): List<BluetoothDevice> {
        return bluetoothService.getPairedDevices()
    }
    
    suspend fun connectToDevice(device: BluetoothDevice): Boolean {
        return bluetoothService.connectToDevice(device)
    }
    
    suspend fun disconnect(): Boolean {
        return bluetoothService.disconnect()
    }
    
    suspend fun refreshStatus() {
        val status = apiService.getStatus()
        if (status != null) {
            _deviceStatus.value = status
        }
    }
    
    private fun startStatusUpdates() {
        viewModelScope.launch {
            while (connectionState.value == ConnectionState.CONNECTED) {
                refreshStatus()
                kotlinx.coroutines.delay(5000) // Update every 5 seconds
            }
        }
    }
    
    // LED Control Methods
    suspend fun setPreset(preset: Int) {
        apiService.setPreset(preset)
        refreshStatus()
    }
    
    suspend fun setBrightness(brightness: Int) {
        apiService.setBrightness(brightness)
        refreshStatus()
    }
    
    suspend fun setEffectSpeed(speed: Int) {
        apiService.setEffectSpeed(speed)
        refreshStatus()
    }
    
    suspend fun setHeadlightColor(color: String) {
        apiService.setHeadlightColor(color)
        refreshStatus()
    }
    
    suspend fun setTaillightColor(color: String) {
        apiService.setTaillightColor(color)
        refreshStatus()
    }
    
    suspend fun setHeadlightEffect(effect: Int) {
        apiService.setHeadlightEffect(effect)
        refreshStatus()
    }
    
    suspend fun setTaillightEffect(effect: Int) {
        apiService.setTaillightEffect(effect)
        refreshStatus()
    }
    
    // Motion Control Methods
    suspend fun setMotionEnabled(enabled: Boolean) {
        apiService.setMotionEnabled(enabled)
        refreshStatus()
    }
    
    suspend fun setBlinkerEnabled(enabled: Boolean) {
        apiService.setBlinkerEnabled(enabled)
        refreshStatus()
    }
    
    suspend fun setParkModeEnabled(enabled: Boolean) {
        apiService.setParkModeEnabled(enabled)
        refreshStatus()
    }
    
    suspend fun setImpactDetectionEnabled(enabled: Boolean) {
        apiService.setImpactDetectionEnabled(enabled)
        refreshStatus()
    }
    
    suspend fun setMotionSensitivity(sensitivity: Double) {
        apiService.setMotionSensitivity(sensitivity)
        refreshStatus()
    }
    
    suspend fun setBlinkerDelay(delay: Int) {
        apiService.setBlinkerDelay(delay)
        refreshStatus()
    }
    
    suspend fun setBlinkerTimeout(timeout: Int) {
        apiService.setBlinkerTimeout(timeout)
        refreshStatus()
    }
    
    suspend fun setParkStationaryTime(time: Int) {
        apiService.setParkStationaryTime(time)
        refreshStatus()
    }
    
    suspend fun setParkAccelNoiseThreshold(threshold: Double) {
        apiService.setParkAccelNoiseThreshold(threshold)
        refreshStatus()
    }
    
    suspend fun setParkGyroNoiseThreshold(threshold: Double) {
        apiService.setParkGyroNoiseThreshold(threshold)
        refreshStatus()
    }
    
    suspend fun setImpactThreshold(threshold: Double) {
        apiService.setImpactThreshold(threshold)
        refreshStatus()
    }
    
    // Park Mode Settings
    suspend fun setParkEffect(effect: Int) {
        apiService.setParkEffect(effect)
        refreshStatus()
    }
    
    suspend fun setParkEffectSpeed(speed: Int) {
        apiService.setParkEffectSpeed(speed)
        refreshStatus()
    }
    
    suspend fun setParkBrightness(brightness: Int) {
        apiService.setParkBrightness(brightness)
        refreshStatus()
    }
    
    suspend fun setParkHeadlightColor(r: Int, g: Int, b: Int) {
        apiService.setParkHeadlightColor(r, g, b)
        refreshStatus()
    }
    
    suspend fun setParkTaillightColor(r: Int, g: Int, b: Int) {
        apiService.setParkTaillightColor(r, g, b)
        refreshStatus()
    }
    
    // Calibration Methods
    suspend fun startCalibration() {
        apiService.startCalibration()
        refreshStatus()
    }
    
    suspend fun nextCalibrationStep() {
        apiService.nextCalibrationStep()
        refreshStatus()
    }
    
    suspend fun resetCalibration() {
        apiService.resetCalibration()
        refreshStatus()
    }
    
    // Test Methods
    suspend fun testStartupSequence() {
        apiService.testStartupSequence()
    }
    
    suspend fun testParkMode() {
        apiService.testParkMode()
    }
    
    suspend fun testLEDs() {
        apiService.testLEDs()
    }
    
    // Startup Sequence Settings
    suspend fun setStartupSequence(sequence: Int) {
        apiService.setStartupSequence(sequence)
        refreshStatus()
    }
    
    suspend fun setStartupDuration(duration: Int) {
        apiService.setStartupDuration(duration)
        refreshStatus()
    }
    
    // WiFi Configuration
    suspend fun setAPName(name: String) {
        apiService.setAPName(name)
        refreshStatus()
    }
    
    suspend fun setAPPassword(password: String) {
        apiService.setAPPassword(password)
        refreshStatus()
    }
    
    suspend fun applyWiFiConfig(name: String, password: String) {
        apiService.applyWiFiConfig(name, password)
        refreshStatus()
    }
    
    // ESPNow Configuration
    suspend fun setESPNowEnabled(enabled: Boolean) {
        apiService.setESPNowEnabled(enabled)
        refreshStatus()
    }
    
    suspend fun setESPNowSync(enabled: Boolean) {
        apiService.setESPNowSync(enabled)
        refreshStatus()
    }
    
    suspend fun setESPNowChannel(channel: Int) {
        apiService.setESPNowChannel(channel)
        refreshStatus()
    }
    
    // Group Management
    suspend fun setDeviceName(name: String) {
        apiService.setDeviceName(name)
        refreshStatus()
    }
    
    suspend fun createGroup(code: String) {
        apiService.createGroup(code)
        refreshStatus()
    }
    
    suspend fun joinGroup(code: String) {
        apiService.joinGroup(code)
        refreshStatus()
    }
    
    suspend fun leaveGroup() {
        apiService.leaveGroup()
        refreshStatus()
    }
    
    suspend fun allowGroupJoin() {
        apiService.allowGroupJoin()
        refreshStatus()
    }
    
    suspend fun blockGroupJoin() {
        apiService.blockGroupJoin()
        refreshStatus()
    }
    
    // LED Configuration
    suspend fun updateLEDConfig(config: LEDConfigRequest) {
        apiService.updateLEDConfig(config)
        refreshStatus()
    }
    
    // Utility methods
    fun hexToRgb(hex: String): Triple<Int, Int, Int> {
        val cleanHex = hex.replace("#", "")
        val r = cleanHex.substring(0, 2).toInt(16)
        val g = cleanHex.substring(2, 4).toInt(16)
        val b = cleanHex.substring(4, 6).toInt(16)
        return Triple(r, g, b)
    }
    
    fun rgbToHex(r: Int, g: Int, b: Int): String {
        return String.format("#%02X%02X%02X", r, g, b)
    }
}
