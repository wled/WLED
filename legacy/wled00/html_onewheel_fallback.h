#ifndef HTML_ONEWHEEL_FALLBACK_H
#define HTML_ONEWHEEL_FALLBACK_H

// Minimal OneWheel UI fallback - embedded in flash memory
// This is only used if the filesystem version doesn't exist
static const char PAGE_onewheel_fallback[] PROGMEM = R"html(
<!DOCTYPE html>
<html>
<head>
    <title>OneWheel LED Controller</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #1a1a2e; color: white; }
        .control { margin: 15px 0; padding: 15px; background: rgba(255,255,255,0.1); border-radius: 8px; }
        button { padding: 10px 20px; margin: 5px; background: #667eea; color: white; border: none; border-radius: 5px; cursor: pointer; }
        button:hover { background: #764ba2; }
        input[type="color"] { width: 50px; height: 40px; margin: 0 10px; }
        input[type="range"] { width: 200px; margin: 0 10px; }
    </style>
</head>
<body>
    <h1>🛹 OneWheel LED Controller</h1>
    <p><em>Fallback UI - Update filesystem for full interface</em></p>
    
    <div class="control">
        <h3>Power Control</h3>
        <button onclick="setPower(true)">POWER ON</button>
        <button onclick="setPower(false)">POWER OFF</button>
    </div>

    <div class="control">
        <h3>Color Control</h3>
        <input type="color" id="colorPicker" onchange="setColor(this.value)">
        <button onclick="setEffect('solid')">Solid</button>
        <button onclick="setEffect('rainbow')">Rainbow</button>
    </div>

    <div class="control">
        <h3>Brightness</h3>
        <input type="range" id="brightnessSlider" min="0" max="255" value="128" onchange="setBrightness(this.value)">
        <span id="brightnessValue">128</span>
    </div>

    <div class="control">
        <h3>Effects</h3>
        <button onclick="setEffect('breathe')">Breathe</button>
        <button onclick="setEffect('twinkle')">Twinkle</button>
        <button onclick="setEffect('chase')">Chase</button>
        <button onclick="setEffect('pulse')">Pulse</button>
    </div>

    <div class="control">
        <button onclick="window.location.href='./?sliders'">Advanced WLED</button>
        <button onclick="window.location.href='./settings'">Settings</button>
    </div>

    <script>
        function setPower(state) {
            fetch('/json/state', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({on: state}) });
        }
        function setColor(color) {
            const hex = color.substring(1);
            const r = parseInt(hex.substring(0,2), 16);
            const g = parseInt(hex.substring(2,4), 16);
            const b = parseInt(hex.substring(4,6), 16);
            fetch('/json/state', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({seg: [{col: [[r, g, b]]}]}) });
        }
        function setBrightness(value) {
            document.getElementById('brightnessValue').textContent = value;
            fetch('/json/state', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({bri: parseInt(value)}) });
        }
        function setEffect(effect) {
            const effectMap = {'solid': 0, 'rainbow': 1, 'breathe': 2, 'twinkle': 3, 'chase': 4, 'pulse': 5};
            fetch('/json/state', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({seg: [{fx: effectMap[effect] || 0}]}) });
        }
    </script>
</body>
</html>
)html";

const size_t PAGE_onewheel_fallback_L = sizeof(PAGE_onewheel_fallback) - 1;

#endif // HTML_ONEWHEEL_FALLBACK_H
