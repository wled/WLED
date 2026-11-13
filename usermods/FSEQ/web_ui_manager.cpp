#include "web_ui_manager.h"
#include "fseq_player.h"
#include "sd_manager.h"
#include "usermod_fseq.h"

static const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>Unified UI</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0, minimum-scale=1">
  <meta name="theme-color" content="#222222">
  <style>
    body { margin: 0; background: #000; color: #eee; font-family: sans-serif; }
    nav { display: flex; }
    nav button {
      flex: 1; padding: 10px; border: none; cursor: pointer;
      background: #222; color: #ccc; font-size: 1rem;
    }
    nav button:hover { background: #333; color: #fff; }
    nav button.active { background: #444; color: #fff; }
    .btn {
      display: inline-block; color: #0f0; border: 1px solid #0f0;
      background: #111; padding: 6px 12px; text-decoration: none;
      margin: 4px; border-radius: 4px; cursor: pointer;
    }
    .btn:hover { background: #0f0; color: #000; }
    /* Stop buttons will have a red background */
    .btn-stop { background: #f00; color: #000; }
    .tab-content { display: none; padding: 20px; }
    .tab-content.active { display: block; }
    h1 { margin-top: 0; }
    ul { list-style: none; margin: 0; padding: 0; }
    li { margin-bottom: 8px; }
    .btn-delete { color: #f00; border-color: #f00; }
    .btn-delete:hover { background: #f00; color: #000; }
  </style>
  <script>
    // --- Functions for SD and FSEQ operations ---
    function showTab(tabName) {
      document.getElementById('tabSd').classList.remove('active');
      document.getElementById('tabFseq').classList.remove('active');
      if(tabName === 'sd') {
        document.getElementById('tabSd').classList.add('active');
        document.getElementById('tabSdBtn').classList.add('active');
        document.getElementById('tabFseqBtn').classList.remove('active');
      } else {
        document.getElementById('tabFseq').classList.add('active');
        document.getElementById('tabFseqBtn').classList.add('active');
        document.getElementById('tabSdBtn').classList.remove('active');
      }
    }
    
    function loadSDList() {
      fetch('/api/sd/list')
        .then(res => res.json())
        .then(files => {
          const ul = document.getElementById('sdList');
          ul.innerHTML = '';
          files.forEach(f => {
            const li = document.createElement('li');
            li.innerHTML = `${f.name} (${f.size} KB)
              <a href="#" class="btn btn-delete" onclick="deleteFile('${f.name}')">Delete</a>`;
            ul.appendChild(li);
          });
        })
        .catch(err => console.log(err));
    }
    
    function loadFseqList() {
      fetch('/api/fseq/list')
        .then(res => res.json())
        .then(files => {
          const ul = document.getElementById('fseqList');
          ul.innerHTML = '';
          files.forEach(f => {
            const li = document.createElement('li');
            li.innerHTML = `
              ${f.name}
              <button class="btn" id="btn_normal_${f.name}" onclick="toggleFseq('${f.name}')">Play</button>
              <button class="btn" id="btn_loop_${f.name}" onclick="toggleFseqLoop('${f.name}')">Play Loop</button>
            `;
            ul.appendChild(li);
          });
        })
        .catch(err => console.log(err));
    }
    
    function deleteFile(name) {
      if(!confirm("Delete " + name + "?")) return;
      fetch('/api/sd/delete?path=' + encodeURIComponent(name))
        .then(res => res.text())
        .then(msg => {
          document.getElementById('uploadStatus').textContent = msg;
          loadSDList();
          loadFseqList(); // Update FSEQ list as well
        })
        .catch(err => {
          document.getElementById('uploadStatus').textContent = "Delete failed";
        });
    }
    
    function toggleFseq(fname) {
      let btn = document.getElementById('btn_normal_' + fname);
      if(btn.innerText === 'Play') {
        fetch('/api/fseq/start?file=' + encodeURIComponent(fname))
          .then(res => res.text())
          .then(_ => { 
            btn.innerText = 'Stop';
            btn.classList.add('btn-stop');
          });
      } else {
        fetch('/api/fseq/stop')
          .then(res => res.text())
          .then(_ => { 
            btn.innerText = 'Play';
            btn.classList.remove('btn-stop');
          });
      }
    }
    
    function toggleFseqLoop(fname) {
      let btn = document.getElementById('btn_loop_' + fname);
      if(btn.innerText === 'Play Loop') {
        fetch('/api/fseq/startloop?file=' + encodeURIComponent(fname))
          .then(res => res.text())
          .then(_ => { 
            btn.innerText = 'Stop';
            btn.classList.add('btn-stop');
          });
      } else {
        fetch('/api/fseq/stop')
          .then(res => res.text())
          .then(_ => { 
            btn.innerText = 'Play Loop';
            btn.classList.remove('btn-stop');
          });
      }
    }
    
    function checkFseqStatus() {
      fetch('/api/fseq/status')
        .then(res => res.json())
        .then(status => {
          if (!status.playing) {
            document.querySelectorAll("button[id^='btn_normal_']").forEach(btn => {
              if (btn.innerText === "Stop") {
                btn.innerText = "Play";
                btn.classList.remove("btn-stop");
              }
            });
            document.querySelectorAll("button[id^='btn_loop_']").forEach(btn => {
              if (btn.innerText === "Stop") {
                btn.innerText = "Play Loop";
                btn.classList.remove("btn-stop");
              }
            });
          }
        })
        .catch(err => console.log(err));
    }
    
    document.addEventListener('DOMContentLoaded', () => {
      loadSDList();
      loadFseqList();
      
      // Handle file upload form submission
      document.getElementById('uploadForm').addEventListener('submit', function(e) {
        e.preventDefault();
        let formData = new FormData(this);
        fetch('/api/sd/upload', {
          method: 'POST',
          body: formData
        })
        .then(res => res.text())
        .then(msg => {
          document.getElementById('uploadStatus').textContent = msg;
          loadSDList();
          loadFseqList(); // Update FSEQ list as well
        })
        .catch(err => {
          document.getElementById('uploadStatus').textContent = "Upload failed";
        });
      });
      
      // Start polling for FSEQ status every 500 ms
      setInterval(checkFseqStatus, 500);
    });
  </script>
</head>
<body>
  <!-- Navigation Bar: SD Files and FSEQ -->
  <nav>
    <button id="tabSdBtn" class="active" onclick="showTab('sd')">SD Files</button>
    <button id="tabFseqBtn" onclick="showTab('fseq')">FSEQ</button>
  </nav>
  
  <!-- Tab Content -->
  <div id="tabSd" class="tab-content active">
    <h1>SD Files</h1>
    <ul id="sdList"></ul>
    <h2>Upload File</h2>
    <form id="uploadForm">
      <input type="file" name="upload"><br><br>
      <button class="btn" type="submit">Upload</button>
    </form>
    <div id="uploadStatus"></div>
  </div>
  
  <div id="tabFseq" class="tab-content">
    <h1>FSEQ Files</h1>
    <ul id="fseqList"></ul>
  </div>
</body>
</html>
)rawliteral";

void WebUIManager::registerEndpoints() {

  // Main UI page (navigation, SD and FSEQ tabs)
  server.on("/fsequi", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", PAGE_HTML);
  });

  // API - List SD files (size in KB)
  server.on("/api/sd/list", HTTP_GET, [](AsyncWebServerRequest *request) {
    File root = SD_ADAPTER.open("/");
    String json = "[";
    if (root && root.isDirectory()) {
      bool first = true;
      File file = root.openNextFile();
      while (file) {
        if (!first)
          json += ",";
        first = false;
        float sizeKB = file.size() / 1024.0;
        json += "{";
        json += "\"name\":\"" + String(file.name()) + "\",";
        json += "\"size\":" + String(sizeKB, 2);
        json += "}";
        file.close();
        file = root.openNextFile();
      }
    }
    root.close();
    json += "]";
    request->send(200, "application/json", json);
  });

  // API - List FSEQ files
  server.on("/api/fseq/list", HTTP_GET, [](AsyncWebServerRequest *request) {
    File root = SD_ADAPTER.open("/");
    String json = "[";
    if (root && root.isDirectory()) {
      bool first = true;
      File file = root.openNextFile();
      while (file) {
        String name = file.name();
        if (name.endsWith(".fseq") || name.endsWith(".FSEQ")) {
          if (!first)
            json += ",";
          first = false;
          json += "{";
          json += "\"name\":\"" + name + "\"";
          json += "}";
        }
        file.close();
        file = root.openNextFile();
      }
    }
    root.close();
    json += "]";
    request->send(200, "application/json", json);
  });

  // API - File Upload
  server.on(
      "/api/sd/upload", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Upload complete");
      },
      [](AsyncWebServerRequest *request, String filename, size_t index,
         uint8_t *data, size_t len, bool final) {
        static File uploadFile;
        if (index == 0) {
          if (!filename.startsWith("/"))
            filename = "/" + filename;
          uploadFile = SD_ADAPTER.open(filename.c_str(), FILE_WRITE);
        }
        if (uploadFile) {
          uploadFile.write(data, len);
          if (final)
            uploadFile.close();
        }
      });

  // API - File Delete
  server.on("/api/sd/delete", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasArg("path")) {
      request->send(400, "text/plain", "Missing path");
      return;
    }
    String path = request->arg("path");
    if (!path.startsWith("/"))
      path = "/" + path;
    bool res = SD_ADAPTER.remove(path.c_str());
    request->send(200, "text/plain", res ? "File deleted" : "Delete failed");
  });

  // API - Start FSEQ (normal playback)
  server.on("/api/fseq/start", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasArg("file")) {
      request->send(400, "text/plain", "Missing file param");
      return;
    }
    String filepath = request->arg("file");
    if (!filepath.startsWith("/"))
      filepath = "/" + filepath;
    FSEQPlayer::loadRecording(filepath.c_str(), 0, uint16_t(-1), 0.0f);
    request->send(200, "text/plain", "FSEQ started");
  });

  // API - Start FSEQ in loop mode
  server.on(
      "/api/fseq/startloop", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->hasArg("file")) {
          request->send(400, "text/plain", "Missing file param");
          return;
        }
        String filepath = request->arg("file");
        if (!filepath.startsWith("/"))
          filepath = "/" + filepath;
        // Passing 1.0f enables loop mode in loadRecording()
        FSEQPlayer::loadRecording(filepath.c_str(), 0, uint16_t(-1), 1.0f);
        request->send(200, "text/plain", "FSEQ loop started");
      });

  // API - Stop FSEQ
  server.on("/api/fseq/stop", HTTP_GET, [](AsyncWebServerRequest *request) {
    FSEQPlayer::clearLastPlayback();
    if (realtimeOverride == REALTIME_OVERRIDE_ONCE)
      realtimeOverride = REALTIME_OVERRIDE_NONE;
    if (realtimeMode)
      exitRealtime();
    else {
      realtimeMode = REALTIME_MODE_INACTIVE;
      strip.trigger();
    }
    request->send(200, "text/plain", "FSEQ stopped");
  });

  // API - FSEQ Status
  server.on("/api/fseq/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool playing = FSEQPlayer::isPlaying();
    String json = "{\"playing\":";
    json += (playing ? "true" : "false");
    json += "}";
    request->send(200, "application/json", json);
  });
}