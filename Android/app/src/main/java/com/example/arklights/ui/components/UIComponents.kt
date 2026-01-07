package com.example.arklights.ui.components

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.selection.selectable
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.example.arklights.data.*
import androidx.compose.material3.ExperimentalMaterial3Api

@Composable
fun PresetsSection(
    onPresetSelected: (Int) -> Unit
) {
    Card(
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(
            modifier = Modifier.padding(16.dp)
        ) {
            Text(
                text = "Presets",
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.Bold
            )
            
            Spacer(modifier = Modifier.height(8.dp))
            
            LazyRow(
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                items(Presets.presetNames.toList()) { (id, name) ->
                    Button(
                        onClick = { onPresetSelected(id) },
                        colors = ButtonDefaults.buttonColors(
                            containerColor = MaterialTheme.colorScheme.primary
                        )
                    ) {
                        Text(name)
                    }
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun HeadlightSection(
    deviceStatus: LEDStatus?,
    onColorChange: (String) -> Unit,
    onEffectChange: (Int) -> Unit
) {
    Card(
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(
            modifier = Modifier.padding(16.dp)
        ) {
            Text(
                text = "Headlight",
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.Bold
            )
            
            Spacer(modifier = Modifier.height(12.dp))
            
            // Color Picker
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = "Color:",
                    modifier = Modifier.weight(1f)
                )
                
                // Simple color picker using buttons
                LazyRow(
                    horizontalArrangement = Arrangement.spacedBy(4.dp)
                ) {
                    items(listOf("#FFFFFF", "#FF0000", "#00FF00", "#0000FF", "#FFFF00", "#FF00FF", "#00FFFF")) { color ->
                        Button(
                            onClick = { onColorChange(color) },
                            modifier = Modifier.size(40.dp),
                            colors = ButtonDefaults.buttonColors(
                                containerColor = androidx.compose.ui.graphics.Color(
                                    android.graphics.Color.parseColor(color)
                                )
                            )
                        ) {}
                    }
                }
            }
            
            Spacer(modifier = Modifier.height(12.dp))
            
            // Effect Selector
            Text(
                text = "Effect:",
                style = MaterialTheme.typography.bodyMedium
            )
            
            Spacer(modifier = Modifier.height(4.dp))
            
            var expanded by remember { mutableStateOf(false) }
            var selectedEffect by remember { mutableStateOf(deviceStatus?.headlightEffect ?: 0) }
            
            ExposedDropdownMenuBox(
                expanded = expanded,
                onExpandedChange = { expanded = !expanded }
            ) {
                OutlinedTextField(
                    value = LEDEffects.effectNames[selectedEffect] ?: "Unknown",
                    onValueChange = {},
                    readOnly = true,
                    trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = expanded) },
                    modifier = Modifier.fillMaxWidth()
                )
                
                ExposedDropdownMenu(
                    expanded = expanded,
                    onDismissRequest = { expanded = false }
                ) {
                    LEDEffects.effectNames.forEach { (id, name) ->
                        DropdownMenuItem(
                            text = { Text(name) },
                            onClick = {
                                selectedEffect = id
                                expanded = false
                                onEffectChange(id)
                            }
                        )
                    }
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun TaillightSection(
    deviceStatus: LEDStatus?,
    onColorChange: (String) -> Unit,
    onEffectChange: (Int) -> Unit
) {
    Card(
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(
            modifier = Modifier.padding(16.dp)
        ) {
            Text(
                text = "Taillight",
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.Bold
            )
            
            Spacer(modifier = Modifier.height(12.dp))
            
            // Color Picker
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = "Color:",
                    modifier = Modifier.weight(1f)
                )
                
                // Simple color picker using buttons
                LazyRow(
                    horizontalArrangement = Arrangement.spacedBy(4.dp)
                ) {
                    items(listOf("#FF0000", "#FFFFFF", "#00FF00", "#0000FF", "#FFFF00", "#FF00FF", "#00FFFF")) { color ->
                        Button(
                            onClick = { onColorChange(color) },
                            modifier = Modifier.size(40.dp),
                            colors = ButtonDefaults.buttonColors(
                                containerColor = androidx.compose.ui.graphics.Color(
                                    android.graphics.Color.parseColor(color)
                                )
                            )
                        ) {}
                    }
                }
            }
            
            Spacer(modifier = Modifier.height(12.dp))
            
            // Effect Selector
            Text(
                text = "Effect:",
                style = MaterialTheme.typography.bodyMedium
            )
            
            Spacer(modifier = Modifier.height(4.dp))
            
            var expanded by remember { mutableStateOf(false) }
            var selectedEffect by remember { mutableStateOf(deviceStatus?.taillightEffect ?: 0) }
            
            ExposedDropdownMenuBox(
                expanded = expanded,
                onExpandedChange = { expanded = !expanded }
            ) {
                OutlinedTextField(
                    value = LEDEffects.effectNames[selectedEffect] ?: "Unknown",
                    onValueChange = {},
                    readOnly = true,
                    trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = expanded) },
                    modifier = Modifier.fillMaxWidth()
                )
                
                ExposedDropdownMenu(
                    expanded = expanded,
                    onDismissRequest = { expanded = false }
                ) {
                    LEDEffects.effectNames.forEach { (id, name) ->
                        DropdownMenuItem(
                            text = { Text(name) },
                            onClick = {
                                selectedEffect = id
                                expanded = false
                                onEffectChange(id)
                            }
                        )
                    }
                }
            }
        }
    }
}

@Composable
fun BrightnessSection(
    deviceStatus: LEDStatus?,
    onBrightnessChange: (Int) -> Unit
) {
    Card(
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(
            modifier = Modifier.padding(16.dp)
        ) {
            Text(
                text = "Brightness",
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.Bold
            )
            
            Spacer(modifier = Modifier.height(8.dp))
            
            var brightness by remember { mutableStateOf(deviceStatus?.brightness ?: 128) }
            
            Text(
                text = "Global Brightness: $brightness",
                style = MaterialTheme.typography.bodyMedium
            )
            
            Slider(
                value = brightness.toFloat(),
                onValueChange = { newValue ->
                    brightness = newValue.toInt()
                    onBrightnessChange(brightness)
                },
                valueRange = 0f..255f,
                modifier = Modifier.fillMaxWidth()
            )
        }
    }
}

@Composable
fun EffectSpeedSection(
    deviceStatus: LEDStatus?,
    onSpeedChange: (Int) -> Unit
) {
    Card(
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(
            modifier = Modifier.padding(16.dp)
        ) {
            Text(
                text = "Effect Speed",
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.Bold
            )
            
            Spacer(modifier = Modifier.height(8.dp))
            
            var speed by remember { mutableStateOf(deviceStatus?.effectSpeed ?: 64) }
            
            Text(
                text = "Effect Speed: $speed",
                style = MaterialTheme.typography.bodyMedium
            )
            
            Text(
                text = "Higher values = faster effects",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
            
            Slider(
                value = speed.toFloat(),
                onValueChange = { newValue ->
                    speed = newValue.toInt()
                    onSpeedChange(speed)
                },
                valueRange = 0f..255f,
                modifier = Modifier.fillMaxWidth()
            )
        }
    }
}

@Composable
fun MotionControlSection(
    deviceStatus: LEDStatus?,
    onMotionEnabled: (Boolean) -> Unit,
    onBlinkerEnabled: (Boolean) -> Unit,
    onParkModeEnabled: (Boolean) -> Unit,
    onImpactDetectionEnabled: (Boolean) -> Unit,
    onMotionSensitivityChange: (Double) -> Unit
) {
    Card(
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(
            modifier = Modifier.padding(16.dp)
        ) {
            Text(
                text = "Motion Control",
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.Bold
            )
            
            Spacer(modifier = Modifier.height(12.dp))
            
            // Motion Enabled
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text("Enable Motion Control")
                    Text(
                        text = "Master switch for all motion features",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                Switch(
                    checked = deviceStatus?.motion_enabled ?: false,
                    onCheckedChange = onMotionEnabled
                )
            }
            
            Spacer(modifier = Modifier.height(8.dp))
            
            // Blinker Enabled
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text("Auto Blinkers")
                    Text(
                        text = "Automatic turn signals based on lean angle",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                Switch(
                    checked = deviceStatus?.blinker_enabled ?: false,
                    onCheckedChange = onBlinkerEnabled
                )
            }
            
            Spacer(modifier = Modifier.height(8.dp))
            
            // Park Mode Enabled
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text("Park Mode")
                    Text(
                        text = "Special effects when stationary and tilted",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                Switch(
                    checked = deviceStatus?.park_mode_enabled ?: false,
                    onCheckedChange = onParkModeEnabled
                )
            }
            
            Spacer(modifier = Modifier.height(8.dp))
            
            // Impact Detection Enabled
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text("Impact Detection")
                    Text(
                        text = "Flash lights on sudden acceleration changes",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                Switch(
                    checked = deviceStatus?.impact_detection_enabled ?: false,
                    onCheckedChange = onImpactDetectionEnabled
                )
            }
            
            Spacer(modifier = Modifier.height(12.dp))
            
            // Motion Sensitivity
            var sensitivity by remember { mutableStateOf(deviceStatus?.motion_sensitivity ?: 1.0) }
            
            Text(
                text = "Motion Sensitivity: ${String.format("%.1f", sensitivity)}",
                style = MaterialTheme.typography.bodyMedium
            )
            
            Text(
                text = "Higher values = more sensitive to motion",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
            
            Slider(
                value = sensitivity.toFloat(),
                onValueChange = { newValue ->
                    sensitivity = newValue.toDouble()
                    onMotionSensitivityChange(sensitivity)
                },
                valueRange = 0.5f..2.0f,
                steps = 14, // 0.1 increments
                modifier = Modifier.fillMaxWidth()
            )
        }
    }
}

@Composable
fun StatusSection(
    deviceStatus: LEDStatus?
) {
    Card(
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(
            modifier = Modifier.padding(16.dp)
        ) {
            Text(
                text = "Status",
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.Bold
            )
            
            Spacer(modifier = Modifier.height(8.dp))
            
            if (deviceStatus != null) {
                Text(
                    text = "Preset: ${Presets.presetNames[deviceStatus.preset] ?: "Unknown"}",
                    style = MaterialTheme.typography.bodyMedium
                )
                Text(
                    text = "Brightness: ${deviceStatus.brightness}",
                    style = MaterialTheme.typography.bodyMedium
                )
                Text(
                    text = "Effect Speed: ${deviceStatus.effectSpeed}",
                    style = MaterialTheme.typography.bodyMedium
                )
                Text(
                    text = "Startup: ${deviceStatus.startup_sequence_name} (${deviceStatus.startup_duration}ms)",
                    style = MaterialTheme.typography.bodyMedium
                )
                Text(
                    text = "Motion: ${if (deviceStatus.motion_enabled) "Enabled" else "Disabled"}",
                    style = MaterialTheme.typography.bodyMedium
                )
                Text(
                    text = "Blinker: ${if (deviceStatus.blinker_active) {
                        if (deviceStatus.blinker_direction > 0) "Right" else "Left"
                    } else "Inactive"}",
                    style = MaterialTheme.typography.bodyMedium
                )
                Text(
                    text = "Park Mode: ${if (deviceStatus.park_mode_active) "Active" else "Inactive"}",
                    style = MaterialTheme.typography.bodyMedium
                )
                Text(
                    text = "Calibration: ${if (deviceStatus.calibration_complete) "Complete" else "Not calibrated"}",
                    style = MaterialTheme.typography.bodyMedium
                )
                Text(
                    text = "WiFi AP: ${deviceStatus.apName}",
                    style = MaterialTheme.typography.bodyMedium
                )
                Text(
                    text = "Headlight: Effect ${deviceStatus.headlightEffect}, Color #${deviceStatus.headlightColor}",
                    style = MaterialTheme.typography.bodyMedium
                )
                Text(
                    text = "Taillight: Effect ${deviceStatus.taillightEffect}, Color #${deviceStatus.taillightColor}",
                    style = MaterialTheme.typography.bodyMedium
                )
            } else {
                Text(
                    text = "Loading...",
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
        }
    }
}

// Placeholder components for settings page
@Composable
fun CalibrationSection(
    deviceStatus: LEDStatus?,
    onStartCalibration: () -> Unit,
    onNextStep: () -> Unit,
    onResetCalibration: () -> Unit
) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text("Calibration", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
            Spacer(modifier = Modifier.height(8.dp))
            Button(onClick = onStartCalibration) { Text("Start Calibration") }
            Button(onClick = onNextStep) { Text("Next Step") }
            Button(onClick = onResetCalibration) { Text("Reset Calibration") }
        }
    }
}

@Composable
fun MotionStatusSection(deviceStatus: LEDStatus?) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text("Motion Status", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
            Spacer(modifier = Modifier.height(8.dp))
            Text("Blinker: ${if (deviceStatus?.blinker_active == true) "Active" else "Inactive"}")
            Text("Park Mode: ${if (deviceStatus?.park_mode_active == true) "Active" else "Inactive"}")
            Text("Calibration: ${if (deviceStatus?.calibration_complete == true) "Complete" else "Not calibrated"}")
        }
    }
}

@Composable
fun StartupSequenceSection(
    deviceStatus: LEDStatus?,
    onSequenceChange: (Int) -> Unit,
    onDurationChange: (Int) -> Unit,
    onTestStartup: () -> Unit
) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text("Startup Sequence", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
            Spacer(modifier = Modifier.height(8.dp))
            Button(onClick = onTestStartup) { Text("Test Startup Sequence") }
        }
    }
}

@Composable
fun AdvancedMotionControlSection(
    deviceStatus: LEDStatus?,
    onBlinkerDelayChange: (Int) -> Unit,
    onBlinkerTimeoutChange: (Int) -> Unit,
    onParkStationaryTimeChange: (Int) -> Unit,
    onParkAccelNoiseThresholdChange: (Double) -> Unit,
    onParkGyroNoiseThresholdChange: (Double) -> Unit,
    onImpactThresholdChange: (Double) -> Unit
) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text("Advanced Motion Control", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
            Spacer(modifier = Modifier.height(8.dp))
            Text("Advanced motion control settings...")
        }
    }
}

@Composable
fun ParkModeSettingsSection(
    deviceStatus: LEDStatus?,
    onParkEffectChange: (Int) -> Unit,
    onParkEffectSpeedChange: (Int) -> Unit,
    onParkBrightnessChange: (Int) -> Unit,
    onParkHeadlightColorChange: (Int, Int, Int) -> Unit,
    onParkTaillightColorChange: (Int, Int, Int) -> Unit,
    onTestParkMode: () -> Unit
) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text("Park Mode Settings", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
            Spacer(modifier = Modifier.height(8.dp))
            Button(onClick = onTestParkMode) { Text("Test Park Mode") }
        }
    }
}

@Composable
fun WiFiConfigurationSection(
    deviceStatus: LEDStatus?,
    onAPNameChange: (String) -> Unit,
    onAPPasswordChange: (String) -> Unit,
    onApplyWiFiConfig: (String, String) -> Unit
) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text("WiFi Configuration", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
            Spacer(modifier = Modifier.height(8.dp))
            Text("WiFi configuration settings...")
        }
    }
}

@Composable
fun ESPNowConfigurationSection(
    deviceStatus: LEDStatus?,
    onESPNowEnabled: (Boolean) -> Unit,
    onESPNowSync: (Boolean) -> Unit,
    onESPNowChannelChange: (Int) -> Unit
) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text("ESPNow Configuration", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
            Spacer(modifier = Modifier.height(8.dp))
            Text("ESPNow configuration settings...")
        }
    }
}

@Composable
fun GroupManagementSection(
    deviceStatus: LEDStatus?,
    onDeviceNameChange: (String) -> Unit,
    onCreateGroup: (String) -> Unit,
    onJoinGroup: (String) -> Unit,
    onLeaveGroup: () -> Unit,
    onAllowGroupJoin: () -> Unit,
    onBlockGroupJoin: () -> Unit
) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text("Group Management", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
            Spacer(modifier = Modifier.height(8.dp))
            Text("Group management settings...")
        }
    }
}

@Composable
fun LEDConfigurationSection(
    deviceStatus: LEDStatus?,
    onLEDConfigUpdate: (LEDConfigRequest) -> Unit,
    onTestLEDs: () -> Unit
) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text("LED Configuration", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
            Spacer(modifier = Modifier.height(8.dp))
            Button(onClick = onTestLEDs) { Text("Test LEDs") }
        }
    }
}
