#include "wled.h"

/*
 * genPresets usermod
 *
 * Serves a self-contained page at /genpresets that auto-generates a
 * presets.json covering every installed effect with sensible defaults,
 * plus playlists grouped by dimensionality.
 *
 * A link to the page is added to the Info tab in the main UI.
 */

// The HTML page is stored in flash to save RAM.
static const char genPresets_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Generate Presets - WLED</title>
<style>
  body{font-family:sans-serif;background:#1a1a2e;color:#eee;margin:0;padding:1em;}
  h1{color:#c8a0ff;margin-top:0;}
  button{background:#4a3080;color:#fff;border:none;border-radius:6px;padding:.6em 1.2em;cursor:pointer;font-size:1em;margin:.3em .3em .3em 0;}
  button:hover{background:#6a50a0;}
  button:disabled{opacity:.4;cursor:not-allowed;}
  textarea{width:100%;box-sizing:border-box;background:#111;color:#8f8;font-family:monospace;font-size:.85em;border:1px solid #444;border-radius:4px;padding:.5em;margin-top:.8em;}
  #status{margin:.5em 0;min-height:1.4em;color:#fa0;}
  a{color:#a080ff;}
</style>
</head>
<body>
<h1>Generate Presets</h1>
<p>Generates a complete <code>presets.json</code> with one preset per effect and playlists
grouped by dimensionality. <strong>This will overwrite your current presets.</strong></p>
<p><a href="/">&larr; Back to WLED</a></p>
<button id="btnGen" onclick="generate()">Generate presets</button>
<button id="btnSave" onclick="savePresets()" disabled>Save to device</button>
<div id="status"></div>
<textarea id="out" rows="20" placeholder="Generated JSON will appear here..." readonly></textarea>
<script>
var generated = '';

function setStatus(msg, err) {
  var el = document.getElementById('status');
  el.textContent = msg;
  el.style.color = err ? '#f66' : '#fa0';
}

async function generate() {
  document.getElementById('btnGen').disabled = true;
  document.getElementById('btnSave').disabled = true;
  document.getElementById('out').value = '';
  generated = '';
  setStatus('Fetching effect list...');

  var jsonResp, fxdataResp;
  try {
    [jsonResp, fxdataResp] = await Promise.all([
      fetch('/json').then(function(r){ return r.json(); }),
      fetch('/json/fxdata').then(function(r){ return r.json(); })
    ]);
  } catch(e) {
    setStatus('Fetch failed: ' + e, true);
    document.getElementById('btnGen').disabled = false;
    return;
  }

  var effects = jsonResp.effects || [];
  // /json/effects gives names only; build id-indexed array
  // /json gives { effects: ["name0","name1",...] } where index == id
  var fxdata = fxdataResp; // array indexed by fx id

  var result = '';
  var sep = '{';
  var playlistData = {};
  var seq = 230;

  function addToPlaylist(m, id, ql) {
    if (!playlistData[m]) playlistData[m] = { ps:[], dur:[], trans:[], ql:undefined };
    playlistData[m].ps.push(id);
    playlistData[m].dur.push(300);
    playlistData[m].trans.push(0);
    if (ql) playlistData[m].ql = ql;
  }

  setStatus('Generating...');

  for (var id = 0; id < effects.length; id++) {
    var name = effects[id];
    if (name.indexOf('RSVD') >= 0) continue;
    if (!Array.isArray(fxdata) || fxdata.length <= id) continue;

    var fd = fxdata[id];
    var eP = (fd === '') ? [] : fd.split(';');
    var m = (eP.length < 4 || eP[3] === '') ? '1' : eP[3];

    var defaultString = '';
    if (eP.length > 4) {
      var defs = (eP[4] === '') ? [] : eP[4].split(',');
      for (var di = 0; di < defs.length; di++) {
        var d = defs[di];
        if (!d) continue;
        var kv = d.split('=');
        defaultString += ',"' + kv[0] + '":' + kv[1];
      }
    }

    var stdDefaults = [
      ['sx',128],['ix',128],['c1',128],['c2',128],['c3',16],
      ['o1',0],['o2',0],['o3',0],['pal',11]
    ];
    for (var si = 0; si < stdDefaults.length; si++) {
      var k = stdDefaults[si][0], v = stdDefaults[si][1];
      if (defaultString.indexOf(k) < 0) defaultString += ',"' + k + '":' + v;
    }

    if (defaultString.indexOf('m12') < 0 && m.indexOf('1') >= 0 && m.indexOf('1.5') < 0 && m.indexOf('12') < 0) {
      defaultString += ',"rev":true,"mi":true,"rY":true,"mY":true,"m12":2';
    } else {
      var mirrorKeys = ['rev','mi','rY','mY'];
      for (var mi2 = 0; mi2 < mirrorKeys.length; mi2++) {
        if (defaultString.indexOf(mirrorKeys[mi2]) < 0)
          defaultString += ',"' + mirrorKeys[mi2] + '":false';
      }
    }

    result += sep + '"' + id + '":{"n":"' + name.replace(/"/g,'\\"') + '","mainseg":0,"seg":[{"id":0,"fx":' + id + defaultString + '}]}';
    sep = '\n,';

    if (m.length <= 3) addToPlaylist(m, id, m);
    else addToPlaylist(m, id);
    addToPlaylist('All', id, 'ALL');
    if (name.indexOf('Y\uD83D\uDCA1') === 0) addToPlaylist('AnimARTrix', id, 'AM');
    if (name.indexOf('PS ') === 0) addToPlaylist('Particle System', id, 'PS');
    if (m.indexOf('1') >= 0) addToPlaylist('All 1D', id, '1D');
    if (m.indexOf('2') >= 0) addToPlaylist('All 2D', id, '2D');

    seq = Math.max(seq, id + 1);
  }

  for (var pm in playlistData) {
    var pl = playlistData[pm];
    var ql = pl.ql || seq;
    result += '\n,"' + seq + '":{"n":"' + pm + ' Playlist","ql":"' + ql + '","on":true,"playlist":{"ps":[' + pl.ps.join() + '],"dur":[' + pl.dur.join() + '],"transition":[' + pl.trans.join() + '],"repeat":0,"end":0,"r":1}}';
    seq++;
  }

  result += '}';

  generated = result;
  document.getElementById('out').value = result;
  document.getElementById('btnSave').disabled = false;
  document.getElementById('btnGen').disabled = false;
  setStatus('Done. Review the JSON below, then click "Save to device" to apply.');
}

async function savePresets() {
  if (!generated) return;
  if (!confirm('Overwrite presets.json on the device?')) return;

  document.getElementById('btnSave').disabled = true;
  setStatus('Uploading...');

  var blob = new Blob([generated], {type:'application/text'});
  var file = new File([blob], '/presets.json');
  var form = new FormData();
  form.append('upload', file);

  try {
    var resp = await fetch('/upload', {method:'POST', body:form});
    var txt = await resp.text();
    setStatus(resp.ok ? 'Saved! ' + txt : 'Error: ' + txt, !resp.ok);
  } catch(e) {
    setStatus('Upload failed: ' + e, true);
  }
  document.getElementById('btnSave').disabled = false;
}
</script>
</body>
</html>
)=====";


class GenPresetsUsermod : public Usermod {
  public:

    void setup() override {
      server.on("/genpresets", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", genPresets_html);
      });
    }

    void loop() override {}

    void addToJsonInfo(JsonObject& root) override {
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      JsonArray arr = user.createNestedArray(F(""));
      arr.add(F("<a class=\"btn sml\" href=\"/genpresets\" target=\"_blank\">Generate Presets</a>"));
    }

    uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }
};

static GenPresetsUsermod genPresets_instance;
REGISTER_USERMOD(genPresets_instance);
