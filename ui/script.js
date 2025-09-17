// ArkLights PEV Control - JavaScript Functions

function setPreset(preset) {
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ preset: preset })
    }).then(() => updateStatus());
}

function setBrightness(value) {
    document.getElementById('brightnessValue').textContent = value;
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ brightness: parseInt(value) })
    });
}

function setEffectSpeed(value) {
    document.getElementById('speedValue').textContent = value;
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ effectSpeed: parseInt(value) })
    });
}

function setStartupSequence(sequence) {
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ startup_sequence: parseInt(sequence) })
    });
}

function setStartupDuration(duration) {
    document.getElementById('startupDurationValue').textContent = duration;
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ startup_duration: parseInt(duration) })
    });
}

function testStartupSequence() {
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ testStartup: true })
    }).then(() => {
        console.log('Startup sequence test started');
    });
}

// Motion Control Functions
function setMotionEnabled(enabled) {
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ motion_enabled: enabled })
    });
}

function setBlinkerEnabled(enabled) {
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ blinker_enabled: enabled })
    });
}

function setParkModeEnabled(enabled) {
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ park_mode_enabled: enabled })
    });
}

function setImpactDetectionEnabled(enabled) {
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ impact_detection_enabled: enabled })
    });
}

function setMotionSensitivity(value) {
    document.getElementById('motionSensitivityValue').textContent = value;
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ motion_sensitivity: parseFloat(value) })
    });
}

function setBlinkerDelay(value) {
    document.getElementById('blinkerDelayValue').textContent = value;
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ blinker_delay: parseInt(value) })
    });
}

function setBlinkerTimeout(value) {
    document.getElementById('blinkerTimeoutValue').textContent = value;
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ blinker_timeout: parseInt(value) })
    });
}

function setParkDetectionAngle(value) {
    document.getElementById('parkDetectionAngleValue').textContent = value;
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ park_detection_angle: parseInt(value) })
    });
}

function setParkStationaryTime(value) {
    document.getElementById('parkStationaryTimeValue').textContent = value;
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ park_stationary_time: parseInt(value) })
    });
}

function setParkAccelNoiseThreshold(value) {
    document.getElementById('parkAccelNoiseThresholdValue').textContent = value;
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ park_accel_noise_threshold: parseFloat(value) })
    });
}

function setParkGyroNoiseThreshold(value) {
    document.getElementById('parkGyroNoiseThresholdValue').textContent = value;
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ park_gyro_noise_threshold: parseFloat(value) })
    });
}

function setImpactThreshold(value) {
    document.getElementById('impactThresholdValue').textContent = value;
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ impact_threshold: parseFloat(value) })
    });
}

// Park Mode Settings Functions
function setParkEffect(effect) {
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ park_effect: parseInt(effect) })
    });
}

function setParkEffectSpeed(speed) {
    document.getElementById('parkEffectSpeedValue').textContent = speed;
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ park_effect_speed: parseInt(speed) })
    });
}

function setParkBrightness(brightness) {
    document.getElementById('parkBrightnessValue').textContent = brightness;
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ park_brightness: parseInt(brightness) })
    });
}

function setParkHeadlightColor(color) {
    const rgb = hexToRgb(color);
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            park_headlight_color_r: rgb.r,
            park_headlight_color_g: rgb.g,
            park_headlight_color_b: rgb.b
        })
    });
}

function setParkTaillightColor(color) {
    const rgb = hexToRgb(color);
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            park_taillight_color_r: rgb.r,
            park_taillight_color_g: rgb.g,
            park_taillight_color_b: rgb.b
        })
    });
}

function testParkMode() {
    // Temporarily activate park mode for testing
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ testParkMode: true })
    });
}

// Utility function to convert hex to RGB
function hexToRgb(hex) {
    const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? {
        r: parseInt(result[1], 16),
        g: parseInt(result[2], 16),
        b: parseInt(result[3], 16)
    } : {r: 0, g: 0, b: 0};
}

// Utility function to convert RGB to hex
function rgbToHex(r, g, b) {
    return "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
}

function startCalibration() {
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ startCalibration: true })
    })
    .then(response => response.json())
    .then(data => {
        console.log('Calibration started:', data);
        // Show progress UI
        document.getElementById('calibrationProgress').style.display = 'block';
        document.getElementById('startCalibrationBtn').style.display = 'none';
        document.getElementById('nextCalibrationBtn').style.display = 'inline-block';
        document.getElementById('calibrationStatusText').textContent = 'In Progress';
        // Update status to get current calibration state
        updateStatus();
    })
    .catch(error => {
        console.error('Error starting calibration:', error);
    });
}

function nextCalibrationStep() {
    // Disable button temporarily to prevent double-clicks
    const nextBtn = document.getElementById('nextCalibrationBtn');
    nextBtn.disabled = true;
    nextBtn.textContent = 'Capturing...';
    
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ nextCalibrationStep: true })
    })
    .then(response => response.json())
    .then(data => {
        console.log('Next calibration step sent:', data);
        // Re-enable button
        nextBtn.disabled = false;
        nextBtn.textContent = 'Next Step';
        // Add small delay to ensure backend has updated the step
        setTimeout(() => {
            updateStatus();
        }, 100);
    })
    .catch(error => {
        console.error('Error sending next calibration step:', error);
        // Re-enable button on error
        nextBtn.disabled = false;
        nextBtn.textContent = 'Next Step';
    });
}

function resetCalibration() {
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ resetCalibration: true })
    })
    .then(response => response.json())
    .then(data => {
        console.log('Calibration reset:', data);
        // Reset UI
        document.getElementById('calibrationProgress').style.display = 'none';
        document.getElementById('startCalibrationBtn').style.display = 'inline-block';
        document.getElementById('nextCalibrationBtn').style.display = 'none';
        document.getElementById('calibrationStatusText').textContent = 'Not calibrated';
        document.getElementById('calibrationProgressBar').style.width = '0%';
        // Update status to get current state
        updateStatus();
    })
    .catch(error => {
        console.error('Error resetting calibration:', error);
    });
}

// OTA Update Functions
function handleFileSelect(input) {
    const file = input.files[0];
    const button = document.getElementById('startOTAButton');
    
    if (file) {
        if (file.name.endsWith('.bin')) {
            button.disabled = false;
            button.textContent = `Upload & Install (${(file.size / 1024 / 1024).toFixed(1)}MB)`;
        } else {
            alert('Please select a .bin file');
            input.value = '';
            button.disabled = true;
            button.textContent = 'Upload & Install';
        }
    } else {
        button.disabled = true;
        button.textContent = 'Upload & Install';
    }
}

function startOTAUpdate() {
    const fileInput = document.getElementById('otaFileInput');
    const file = fileInput.files[0];
    
    if (!file) {
        alert('Please select a firmware file first');
        return;
    }
    
    if (!confirm('This will restart the device. Continue?')) {
        return;
    }
    
    // Show progress UI
    document.getElementById('otaProgress').style.display = 'block';
    document.getElementById('startOTAButton').disabled = true;
    document.getElementById('otaProgressText').textContent = 'Uploading file...';
    document.getElementById('otaStatusText').textContent = 'Status: Uploading';
    
    // Upload file
    const formData = new FormData();
    formData.append('firmware', file);
    
    fetch('/api/ota-upload', {
        method: 'POST',
        body: formData
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            if (data.message && data.message.includes('restarting')) {
                // Update completed successfully
                document.getElementById('otaProgressText').textContent = '✅ Update complete! Device is restarting...';
                document.getElementById('otaStatusText').textContent = 'Status: Complete - Restarting';
                document.getElementById('otaProgressBar').style.width = '100%';
                document.getElementById('otaProgressBar').style.background = '#4CAF50';
                
                // Show success message
                setTimeout(() => {
                    alert('✅ Firmware update completed successfully! The device is restarting with the new firmware.');
                }, 1000);
                
                // Redirect to the page after a delay to see the new firmware
                setTimeout(() => {
                    window.location.reload();
                }, 5000);
            } else {
                document.getElementById('otaProgressText').textContent = 'Installing firmware...';
                document.getElementById('otaStatusText').textContent = 'Status: Installing';
            }
        } else {
            alert('Upload failed: ' + (data.error || 'Unknown error'));
            document.getElementById('otaProgress').style.display = 'none';
            document.getElementById('startOTAButton').disabled = false;
        }
    })
    .catch(error => {
        alert('Upload error: ' + error);
        document.getElementById('otaProgress').style.display = 'none';
        document.getElementById('startOTAButton').disabled = false;
    });
}

function setAPName(name) {
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ apName: name })
    });
}

function setAPPassword(password) {
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ apPassword: password })
    });
}

function applyWiFiConfig() {
    const name = document.getElementById('apName').value;
    const password = document.getElementById('apPassword').value;
    
    if (name.length < 1 || name.length > 32) {
        alert('AP Name must be 1-32 characters');
        return;
    }
    
    if (password.length < 8 || password.length > 63) {
        alert('Password must be 8-63 characters');
        return;
    }
    
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ 
            apName: name,
            apPassword: password,
            restart: true 
        })
    }).then(() => {
        alert('WiFi settings saved! Device will restart in 3 seconds...');
        setTimeout(() => {
            window.location.reload();
        }, 3000);
    });
}

function setHeadlightColor(color) {
    const hex = color.replace('#', '');
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ headlightColor: hex })
    });
}

function setTaillightColor(color) {
    const hex = color.replace('#', '');
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ taillightColor: hex })
    });
}

function setHeadlightEffect(effect) {
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ headlightEffect: parseInt(effect) })
    });
}

function setTaillightEffect(effect) {
    fetch('/api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ taillightEffect: parseInt(effect) })
    });
}

function updateStatus() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            document.getElementById('status').innerHTML = 
                `Preset: ${data.preset}<br>` +
                `Brightness: ${data.brightness}<br>` +
                `Effect Speed: ${data.effectSpeed}<br>` +
                `Startup: ${data.startup_sequence_name} (${data.startup_duration}ms)<br>` +
                `Motion: ${data.motion_enabled ? 'Enabled' : 'Disabled'}<br>` +
                `Blinker: ${data.blinker_active ? (data.blinker_direction > 0 ? 'Right' : 'Left') : 'Inactive'}<br>` +
                `Park Mode: ${data.park_mode_active ? 'Active' : 'Inactive'}<br>` +
                `Calibration: ${data.calibration_complete ? 'Complete' : 'Not calibrated'}<br>` +
                `WiFi AP: ${data.apName}<br>` +
                `Headlight: Effect ${data.headlightEffect}, Color #${data.headlightColor}<br>` +
                `Taillight: Effect ${data.taillightEffect}, Color #${data.taillightColor}<br>` +
                `Headlight Config: ${data.headlightLedCount} LEDs, Type ${data.headlightLedType}, Order ${data.headlightColorOrder}<br>` +
                `Taillight Config: ${data.taillightLedCount} LEDs, Type ${data.taillightLedType}, Order ${data.taillightColorOrder}`;

            // Update UI elements
            // Update elements that exist, with null checks
            const brightnessEl = document.getElementById('brightness');
            if (brightnessEl) brightnessEl.value = data.brightness;
            const brightnessValueEl = document.getElementById('brightnessValue');
            if (brightnessValueEl) brightnessValueEl.textContent = data.brightness;
            
            const effectSpeedEl = document.getElementById('effectSpeed');
            if (effectSpeedEl) effectSpeedEl.value = data.effectSpeed;
            const speedValueEl = document.getElementById('speedValue');
            if (speedValueEl) speedValueEl.textContent = data.effectSpeed;
            
            const startupSequenceEl = document.getElementById('startupSequence');
            if (startupSequenceEl) startupSequenceEl.value = data.startup_sequence;
            
            const startupDurationEl = document.getElementById('startupDuration');
            if (startupDurationEl) startupDurationEl.value = data.startup_duration;
            const startupDurationValueEl = document.getElementById('startupDurationValue');
            if (startupDurationValueEl) startupDurationValueEl.textContent = data.startup_duration;
            
            // Update motion control UI (with null checks)
            const motionEnabledEl = document.getElementById('motionEnabled');
            if (motionEnabledEl) motionEnabledEl.checked = data.motion_enabled;
            const blinkerEnabledEl = document.getElementById('blinkerEnabled');
            if (blinkerEnabledEl) blinkerEnabledEl.checked = data.blinker_enabled;
            const parkModeEnabledEl = document.getElementById('parkModeEnabled');
            if (parkModeEnabledEl) parkModeEnabledEl.checked = data.park_mode_enabled;
            const impactDetectionEnabledEl = document.getElementById('impactDetectionEnabled');
            if (impactDetectionEnabledEl) impactDetectionEnabledEl.checked = data.impact_detection_enabled;
            
            const motionSensitivityEl = document.getElementById('motionSensitivity');
            if (motionSensitivityEl) motionSensitivityEl.value = data.motion_sensitivity;
            const motionSensitivityValueEl = document.getElementById('motionSensitivityValue');
            if (motionSensitivityValueEl) motionSensitivityValueEl.textContent = data.motion_sensitivity;
            
            const blinkerDelayEl = document.getElementById('blinkerDelay');
            if (blinkerDelayEl) blinkerDelayEl.value = data.blinker_delay;
            const blinkerDelayValueEl = document.getElementById('blinkerDelayValue');
            if (blinkerDelayValueEl) blinkerDelayValueEl.textContent = data.blinker_delay;
            
            const blinkerTimeoutEl = document.getElementById('blinkerTimeout');
            if (blinkerTimeoutEl) blinkerTimeoutEl.value = data.blinker_timeout;
            const blinkerTimeoutValueEl = document.getElementById('blinkerTimeoutValue');
            if (blinkerTimeoutValueEl) blinkerTimeoutValueEl.textContent = data.blinker_timeout;
            
            const parkDetectionAngleEl = document.getElementById('parkDetectionAngle');
            if (parkDetectionAngleEl) parkDetectionAngleEl.value = data.park_detection_angle;
            const parkDetectionAngleValueEl = document.getElementById('parkDetectionAngleValue');
            if (parkDetectionAngleValueEl) parkDetectionAngleValueEl.textContent = data.park_detection_angle;
            
            const parkStationaryTimeEl = document.getElementById('parkStationaryTime');
            if (parkStationaryTimeEl) parkStationaryTimeEl.value = data.park_stationary_time;
            const parkStationaryTimeValueEl = document.getElementById('parkStationaryTimeValue');
            if (parkStationaryTimeValueEl) parkStationaryTimeValueEl.textContent = data.park_stationary_time;
            
            const parkAccelNoiseThresholdEl = document.getElementById('parkAccelNoiseThreshold');
            if (parkAccelNoiseThresholdEl) parkAccelNoiseThresholdEl.value = data.park_accel_noise_threshold;
            const parkAccelNoiseThresholdValueEl = document.getElementById('parkAccelNoiseThresholdValue');
            if (parkAccelNoiseThresholdValueEl) parkAccelNoiseThresholdValueEl.textContent = data.park_accel_noise_threshold;
            
            const parkGyroNoiseThresholdEl = document.getElementById('parkGyroNoiseThreshold');
            if (parkGyroNoiseThresholdEl) parkGyroNoiseThresholdEl.value = data.park_gyro_noise_threshold;
            const parkGyroNoiseThresholdValueEl = document.getElementById('parkGyroNoiseThresholdValue');
            if (parkGyroNoiseThresholdValueEl) parkGyroNoiseThresholdValueEl.textContent = data.park_gyro_noise_threshold;
            
            const impactThresholdEl = document.getElementById('impactThreshold');
            if (impactThresholdEl) impactThresholdEl.value = data.impact_threshold;
            const impactThresholdValueEl = document.getElementById('impactThresholdValue');
            if (impactThresholdValueEl) impactThresholdValueEl.textContent = data.impact_threshold;
            
            // Update park mode settings (with null checks)
            const parkEffectEl = document.getElementById('parkEffect');
            if (parkEffectEl) parkEffectEl.value = data.park_effect;
            
            const parkEffectSpeedEl = document.getElementById('parkEffectSpeed');
            if (parkEffectSpeedEl) parkEffectSpeedEl.value = data.park_effect_speed;
            const parkEffectSpeedValueEl = document.getElementById('parkEffectSpeedValue');
            if (parkEffectSpeedValueEl) parkEffectSpeedValueEl.textContent = data.park_effect_speed;
            
            const parkBrightnessEl = document.getElementById('parkBrightness');
            if (parkBrightnessEl) parkBrightnessEl.value = data.park_brightness;
            const parkBrightnessValueEl = document.getElementById('parkBrightnessValue');
            if (parkBrightnessValueEl) parkBrightnessValueEl.textContent = data.park_brightness;
            
            const parkHeadlightColorEl = document.getElementById('parkHeadlightColor');
            if (parkHeadlightColorEl) parkHeadlightColorEl.value = rgbToHex(data.park_headlight_color_r, data.park_headlight_color_g, data.park_headlight_color_b);
            
            const parkTaillightColorEl = document.getElementById('parkTaillightColor');
            if (parkTaillightColorEl) parkTaillightColorEl.value = rgbToHex(data.park_taillight_color_r, data.park_taillight_color_g, data.park_taillight_color_b);
            
            // Update OTA status (with null checks)
            const otaStatusDisplayEl = document.getElementById('otaStatusDisplay');
            if (otaStatusDisplayEl) otaStatusDisplayEl.textContent = data.ota_status;
            const otaProgressDisplayEl = document.getElementById('otaProgressDisplay');
            if (otaProgressDisplayEl) otaProgressDisplayEl.textContent = data.ota_progress + '%';
            const otaErrorDisplayEl = document.getElementById('otaErrorDisplay');
            if (otaErrorDisplayEl) otaErrorDisplayEl.textContent = data.ota_error || 'None';
            
            // Update OTA progress bar (with null checks)
            if (data.ota_in_progress) {
                const otaProgressEl = document.getElementById('otaProgress');
                if (otaProgressEl) otaProgressEl.style.display = 'block';
                const otaProgressBarEl = document.getElementById('otaProgressBar');
                if (otaProgressBarEl) otaProgressBarEl.style.width = data.ota_progress + '%';
                const otaProgressTextEl = document.getElementById('otaProgressText');
                if (otaProgressTextEl) otaProgressTextEl.textContent = `${data.ota_status}... ${data.ota_progress}%`;
                const otaStatusTextEl = document.getElementById('otaStatusText');
                if (otaStatusTextEl) otaStatusTextEl.textContent = `Status: ${data.ota_status}`;
                const startOTAButtonEl = document.getElementById('startOTAButton');
                if (startOTAButtonEl) startOTAButtonEl.disabled = true;
            } else {
                const otaProgressEl = document.getElementById('otaProgress');
                if (otaProgressEl) otaProgressEl.style.display = 'none';
                const startOTAButtonEl = document.getElementById('startOTAButton');
                if (startOTAButtonEl) startOTAButtonEl.disabled = false;
            }
            
            // Update firmware version info (with null checks)
            const buildDateEl = document.getElementById('buildDate');
            if (buildDateEl) buildDateEl.textContent = data.build_date || 'Unknown';
            
            // Update motion status (with null checks)
            const blinkerStatusEl = document.getElementById('blinkerStatus');
            if (blinkerStatusEl) blinkerStatusEl.textContent = data.blinker_active ? 
                (data.blinker_direction > 0 ? 'Right' : 'Left') : 'Inactive';
            const parkModeStatusEl = document.getElementById('parkModeStatus');
            if (parkModeStatusEl) parkModeStatusEl.textContent = data.park_mode_active ? 'Active' : 'Inactive';
            const calibrationStatusDisplayEl = document.getElementById('calibrationStatusDisplay');
            if (calibrationStatusDisplayEl) calibrationStatusDisplayEl.textContent = data.calibration_complete ? 'Complete' : 'Not calibrated';
            
            // Update calibration UI (with null checks)
            if (data.calibration_mode) {
                console.log('Calibration mode active, step:', data.calibration_step, 'complete:', data.calibration_complete);
                
                const calibrationProgressEl = document.getElementById('calibrationProgress');
                if (calibrationProgressEl) calibrationProgressEl.style.display = 'block';
                const startCalibrationBtnEl = document.getElementById('startCalibrationBtn');
                if (startCalibrationBtnEl) startCalibrationBtnEl.style.display = 'none';
                const nextCalibrationBtnEl = document.getElementById('nextCalibrationBtn');
                if (nextCalibrationBtnEl) nextCalibrationBtnEl.style.display = 'inline-block';
                const calibrationStatusTextEl = document.getElementById('calibrationStatusText');
                if (calibrationStatusTextEl) calibrationStatusTextEl.textContent = 'In Progress';
                
                // Update progress bar - calibration_step represents current step (0-4)
                // Progress should show how much is completed
                const currentStep = data.calibration_step; // 0-4
                const progress = ((currentStep + 1) / 5) * 100; // 20%, 40%, 60%, 80%, 100%
                const calibrationProgressBarEl = document.getElementById('calibrationProgressBar');
                if (calibrationProgressBarEl) calibrationProgressBarEl.style.width = progress + '%';
                
                // Update step text - show current step being worked on
                const stepTexts = [
                    'Hold device LEVEL',
                    'Tilt FORWARD', 
                    'Tilt BACKWARD',
                    'Tilt LEFT',
                    'Tilt RIGHT'
                ];
                const currentStepNumber = data.calibration_step + 1; // 1-5
                const stepIndex = Math.min(data.calibration_step, 4); // Ensure we don't go out of bounds
                const stepDescription = stepTexts[stepIndex] || 'Calibrating...';
                
                // Show current step with clear instructions
                const calibrationStepTextEl = document.getElementById('calibrationStepText');
                if (calibrationStepTextEl) calibrationStepTextEl.textContent = `Step ${currentStepNumber}/5: ${stepDescription}`;
                
                console.log('Updated UI - Step:', currentStepNumber, 'Progress:', progress + '%', 'Text:', stepDescription);
                
                // Auto-refresh UI every 2 seconds during calibration
                if (!window.calibrationRefreshInterval) {
                    window.calibrationRefreshInterval = setInterval(updateStatus, 2000);
                }
            } else {
                const calibrationProgressEl = document.getElementById('calibrationProgress');
                if (calibrationProgressEl) calibrationProgressEl.style.display = 'none';
                const startCalibrationBtnEl = document.getElementById('startCalibrationBtn');
                if (startCalibrationBtnEl) startCalibrationBtnEl.style.display = 'inline-block';
                const nextCalibrationBtnEl = document.getElementById('nextCalibrationBtn');
                if (nextCalibrationBtnEl) nextCalibrationBtnEl.style.display = 'none';
                const calibrationStatusTextEl = document.getElementById('calibrationStatusText');
                if (calibrationStatusTextEl) calibrationStatusTextEl.textContent = data.calibration_complete ? 'Complete' : 'Not calibrated';
                
                // Clear auto-refresh when calibration is done
                if (window.calibrationRefreshInterval) {
                    clearInterval(window.calibrationRefreshInterval);
                    window.calibrationRefreshInterval = null;
                }
            }
            
            // Update remaining elements (with null checks)
            const apNameEl = document.getElementById('apName');
            if (apNameEl) apNameEl.value = data.apName;
            const apPasswordEl = document.getElementById('apPassword');
            if (apPasswordEl) apPasswordEl.value = data.apPassword;
            const headlightColorEl = document.getElementById('headlightColor');
            if (headlightColorEl) {
                // Ensure color is in proper #rrggbb format
                let headlightColor = data.headlightColor;
                if (headlightColor.length === 4) {
                    // Convert #rgb to #rrggbb
                    headlightColor = '#' + headlightColor[1] + headlightColor[1] + 
                                   headlightColor[2] + headlightColor[2] + 
                                   headlightColor[3] + headlightColor[3];
                } else if (headlightColor.length === 3) {
                    // Convert rgb to #rrggbb
                    headlightColor = '#' + headlightColor[0] + headlightColor[0] + 
                                   headlightColor[1] + headlightColor[1] + 
                                   headlightColor[2] + headlightColor[2];
                } else if (!headlightColor.startsWith('#')) {
                    headlightColor = '#' + headlightColor;
                }
                headlightColorEl.value = headlightColor;
            }
            const taillightColorEl = document.getElementById('taillightColor');
            if (taillightColorEl) {
                // Ensure color is in proper #rrggbb format
                let taillightColor = data.taillightColor;
                if (taillightColor.length === 4) {
                    // Convert #rgb to #rrggbb
                    taillightColor = '#' + taillightColor[1] + taillightColor[1] + 
                                   taillightColor[2] + taillightColor[2] + 
                                   taillightColor[3] + taillightColor[3];
                } else if (taillightColor.length === 3) {
                    // Convert rgb to #rrggbb
                    taillightColor = '#' + taillightColor[0] + taillightColor[0] + 
                                   taillightColor[1] + taillightColor[1] + 
                                   taillightColor[2] + taillightColor[2];
                } else if (!taillightColor.startsWith('#')) {
                    taillightColor = '#' + taillightColor;
                }
                taillightColorEl.value = taillightColor;
            }
            const headlightEffectEl = document.getElementById('headlightEffect');
            if (headlightEffectEl) headlightEffectEl.value = data.headlightEffect;
            const taillightEffectEl = document.getElementById('taillightEffect');
            if (taillightEffectEl) taillightEffectEl.value = data.taillightEffect;
            
            // Update LED configuration elements (with null checks)
            const headlightLedCountEl = document.getElementById('headlightLedCount');
            if (headlightLedCountEl) headlightLedCountEl.value = data.headlightLedCount;
            const taillightLedCountEl = document.getElementById('taillightLedCount');
            if (taillightLedCountEl) taillightLedCountEl.value = data.taillightLedCount;
            const headlightLedTypeEl = document.getElementById('headlightLedType');
            if (headlightLedTypeEl) headlightLedTypeEl.value = data.headlightLedType;
            const taillightLedTypeEl = document.getElementById('taillightLedType');
            if (taillightLedTypeEl) taillightLedTypeEl.value = data.taillightLedType;
            const headlightColorOrderEl = document.getElementById('headlightColorOrder');
            if (headlightColorOrderEl) headlightColorOrderEl.value = data.headlightColorOrder;
            const taillightColorOrderEl = document.getElementById('taillightColorOrder');
            if (taillightColorOrderEl) taillightColorOrderEl.value = data.taillightColorOrder;
        });
}

function updateLEDConfig() {
    const config = {
        headlightLedCount: parseInt(document.getElementById('headlightLedCount').value),
        taillightLedCount: parseInt(document.getElementById('taillightLedCount').value),
        headlightLedType: parseInt(document.getElementById('headlightLedType').value),
        taillightLedType: parseInt(document.getElementById('taillightLedType').value),
        headlightColorOrder: parseInt(document.getElementById('headlightColorOrder').value),
        taillightColorOrder: parseInt(document.getElementById('taillightColorOrder').value)
    };
    
    fetch('/api/led-config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(config)
    }).then(() => {
        console.log('LED configuration updated');
        updateStatus();
    });
}

function testLEDs() {
    fetch('/api/led-test', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' }
    }).then(() => {
        console.log('LED test completed');
    });
}

function saveLEDConfig() {
    updateLEDConfig();
    alert('LED configuration saved!');
}

// Update status on page load
updateStatus();

// Auto-refresh status every 5 seconds
setInterval(updateStatus, 5000);

// Cleanup calibration refresh interval when page is unloaded
window.addEventListener('beforeunload', function() {
    if (window.calibrationRefreshInterval) {
        clearInterval(window.calibrationRefreshInterval);
        window.calibrationRefreshInterval = null;
    }
});
