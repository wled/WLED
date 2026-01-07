package com.example.arklights.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.example.arklights.viewmodel.ArkLightsViewModel
import com.example.arklights.ui.components.*
import kotlinx.coroutines.launch

@Composable
fun MainControlScreen(
    currentPage: String,
    onPageChange: (String) -> Unit,
    viewModel: ArkLightsViewModel
) {
    Column(
        modifier = Modifier.fillMaxSize()
    ) {
        // Navigation Tabs
        TabRow(
            selectedTabIndex = when (currentPage) {
                "main" -> 0
                "settings" -> 1
                else -> 0
            }
        ) {
            Tab(
                selected = currentPage == "main",
                onClick = { onPageChange("main") },
                text = { Text("Main Controls") }
            )
            Tab(
                selected = currentPage == "settings",
                onClick = { onPageChange("settings") },
                text = { Text("Settings") }
            )
        }
        
        // Page Content
        Box(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
        ) {
            when (currentPage) {
                "main" -> MainControlsPage(viewModel = viewModel)
                "settings" -> SettingsPage(viewModel = viewModel)
            }
        }
    }
}

@Composable
fun MainControlsPage(
    viewModel: ArkLightsViewModel
) {
    val deviceStatus by viewModel.deviceStatus.collectAsState()
    val scope = rememberCoroutineScope()
    
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // Presets Section
        PresetsSection(
            onPresetSelected = { preset ->
                scope.launch {
                    viewModel.setPreset(preset)
                }
            }
        )
        
        // Headlight Section
        HeadlightSection(
            deviceStatus = deviceStatus,
            onColorChange = { color ->
                scope.launch {
                    viewModel.setHeadlightColor(color)
                }
            },
            onEffectChange = { effect ->
                scope.launch {
                    viewModel.setHeadlightEffect(effect)
                }
            }
        )
        
        // Taillight Section
        TaillightSection(
            deviceStatus = deviceStatus,
            onColorChange = { color ->
                scope.launch {
                    viewModel.setTaillightColor(color)
                }
            },
            onEffectChange = { effect ->
                scope.launch {
                    viewModel.setTaillightEffect(effect)
                }
            }
        )
        
        // Brightness Section
        BrightnessSection(
            deviceStatus = deviceStatus,
            onBrightnessChange = { brightness ->
                scope.launch {
                    viewModel.setBrightness(brightness)
                }
            }
        )
        
        // Effect Speed Section
        EffectSpeedSection(
            deviceStatus = deviceStatus,
            onSpeedChange = { speed ->
                scope.launch {
                    viewModel.setEffectSpeed(speed)
                }
            }
        )
        
        // Motion Control Section
        MotionControlSection(
            deviceStatus = deviceStatus,
            onMotionEnabled = { enabled ->
                scope.launch {
                    viewModel.setMotionEnabled(enabled)
                }
            },
            onBlinkerEnabled = { enabled ->
                scope.launch {
                    viewModel.setBlinkerEnabled(enabled)
                }
            },
            onParkModeEnabled = { enabled ->
                scope.launch {
                    viewModel.setParkModeEnabled(enabled)
                }
            },
            onImpactDetectionEnabled = { enabled ->
                scope.launch {
                    viewModel.setImpactDetectionEnabled(enabled)
                }
            },
            onMotionSensitivityChange = { sensitivity ->
                scope.launch {
                    viewModel.setMotionSensitivity(sensitivity)
                }
            }
        )
        
        // Status Section
        StatusSection(deviceStatus = deviceStatus)
        
        // Disconnect Button
        Button(
            onClick = {
                scope.launch {
                    viewModel.disconnect()
                }
            },
            colors = ButtonDefaults.buttonColors(
                containerColor = MaterialTheme.colorScheme.error
            ),
            modifier = Modifier.fillMaxWidth()
        ) {
            Text("Disconnect")
        }
    }
}

@Composable
fun SettingsPage(
    viewModel: ArkLightsViewModel
) {
    val deviceStatus by viewModel.deviceStatus.collectAsState()
    val scope = rememberCoroutineScope()
    
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        Text(
            text = "Settings",
            style = MaterialTheme.typography.headlineMedium,
            fontWeight = FontWeight.Bold
        )
        
        // Calibration Section
        CalibrationSection(
            deviceStatus = deviceStatus,
            onStartCalibration = {
                scope.launch {
                    viewModel.startCalibration()
                }
            },
            onNextStep = {
                scope.launch {
                    viewModel.nextCalibrationStep()
                }
            },
            onResetCalibration = {
                scope.launch {
                    viewModel.resetCalibration()
                }
            }
        )
        
        // Motion Status
        MotionStatusSection(deviceStatus = deviceStatus)
        
        // Startup Sequence Section
        StartupSequenceSection(
            deviceStatus = deviceStatus,
            onSequenceChange = { sequence ->
                scope.launch {
                    viewModel.setStartupSequence(sequence)
                }
            },
            onDurationChange = { duration ->
                scope.launch {
                    viewModel.setStartupDuration(duration)
                }
            },
            onTestStartup = {
                scope.launch {
                    viewModel.testStartupSequence()
                }
            }
        )
        
        // Advanced Motion Control Section
        AdvancedMotionControlSection(
            deviceStatus = deviceStatus,
            onBlinkerDelayChange = { delay ->
                scope.launch {
                    viewModel.setBlinkerDelay(delay)
                }
            },
            onBlinkerTimeoutChange = { timeout ->
                scope.launch {
                    viewModel.setBlinkerTimeout(timeout)
                }
            },
            onParkStationaryTimeChange = { time ->
                scope.launch {
                    viewModel.setParkStationaryTime(time)
                }
            },
            onParkAccelNoiseThresholdChange = { threshold ->
                scope.launch {
                    viewModel.setParkAccelNoiseThreshold(threshold)
                }
            },
            onParkGyroNoiseThresholdChange = { threshold ->
                scope.launch {
                    viewModel.setParkGyroNoiseThreshold(threshold)
                }
            },
            onImpactThresholdChange = { threshold ->
                scope.launch {
                    viewModel.setImpactThreshold(threshold)
                }
            }
        )
        
        // Park Mode Settings Section
        ParkModeSettingsSection(
            deviceStatus = deviceStatus,
            onParkEffectChange = { effect ->
                scope.launch {
                    viewModel.setParkEffect(effect)
                }
            },
            onParkEffectSpeedChange = { speed ->
                scope.launch {
                    viewModel.setParkEffectSpeed(speed)
                }
            },
            onParkBrightnessChange = { brightness ->
                scope.launch {
                    viewModel.setParkBrightness(brightness)
                }
            },
            onParkHeadlightColorChange = { r, g, b ->
                scope.launch {
                    viewModel.setParkHeadlightColor(r, g, b)
                }
            },
            onParkTaillightColorChange = { r, g, b ->
                scope.launch {
                    viewModel.setParkTaillightColor(r, g, b)
                }
            },
            onTestParkMode = {
                scope.launch {
                    viewModel.testParkMode()
                }
            }
        )
        
        // WiFi Configuration Section
        WiFiConfigurationSection(
            deviceStatus = deviceStatus,
            onAPNameChange = { name ->
                scope.launch {
                    viewModel.setAPName(name)
                }
            },
            onAPPasswordChange = { password ->
                scope.launch {
                    viewModel.setAPPassword(password)
                }
            },
            onApplyWiFiConfig = { name, password ->
                scope.launch {
                    viewModel.applyWiFiConfig(name, password)
                }
            }
        )
        
        // ESPNow Configuration Section
        ESPNowConfigurationSection(
            deviceStatus = deviceStatus,
            onESPNowEnabled = { enabled ->
                scope.launch {
                    viewModel.setESPNowEnabled(enabled)
                }
            },
            onESPNowSync = { enabled ->
                scope.launch {
                    viewModel.setESPNowSync(enabled)
                }
            },
            onESPNowChannelChange = { channel ->
                scope.launch {
                    viewModel.setESPNowChannel(channel)
                }
            }
        )
        
        // Group Management Section
        GroupManagementSection(
            deviceStatus = deviceStatus,
            onDeviceNameChange = { name ->
                scope.launch {
                    viewModel.setDeviceName(name)
                }
            },
            onCreateGroup = { code ->
                scope.launch {
                    viewModel.createGroup(code)
                }
            },
            onJoinGroup = { code ->
                scope.launch {
                    viewModel.joinGroup(code)
                }
            },
            onLeaveGroup = {
                scope.launch {
                    viewModel.leaveGroup()
                }
            },
            onAllowGroupJoin = {
                scope.launch {
                    viewModel.allowGroupJoin()
                }
            },
            onBlockGroupJoin = {
                scope.launch {
                    viewModel.blockGroupJoin()
                }
            }
        )
        
        // LED Configuration Section
        LEDConfigurationSection(
            deviceStatus = deviceStatus,
            onLEDConfigUpdate = { config ->
                scope.launch {
                    viewModel.updateLEDConfig(config)
                }
            },
            onTestLEDs = {
                scope.launch {
                    viewModel.testLEDs()
                }
            }
        )
    }
}
