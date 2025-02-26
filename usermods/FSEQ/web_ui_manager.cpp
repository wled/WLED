#include "web_ui_manager.h"
#include "sd_manager.h"
#include "fseq_player.h"

// Registrácia všetkých web endpointov
void WebUIManager::registerEndpoints() {
  // Hlavná stránka SD & FSEQ manažéra
  server.on("/sd/ui", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<html><head><meta charset='utf-8'><title>SD & FSEQ Manager</title>";
    html += "<style>";
    html += "body { font-family: sans-serif; font-size: 24px; color: #00FF00; background-color: #000; margin: 0; padding: 20px; }";
    html += "h1 { margin-top: 0; }";
    html += "ul { list-style: none; padding: 0; margin: 0 0 20px 0; }";
    html += "li { margin-bottom: 10px; }";
    html += "a, button { display: inline-block; font-size: 24px; color: #00FF00; border: 2px solid #00FF00; background-color: transparent; padding: 10px 20px; margin: 5px; text-decoration: none; }";
    html += "a:hover, button:hover { background-color: #00FF00; color: #000; }";
    html += "</style></head><body>";
    html += "<h1>SD & FSEQ Manager</h1>";
    html += "<ul>";
    html += "<li><a href='/sd/list'>SD Files</a></li>";
    html += "<li><a href='/fseq/list'>FSEQ Files</a></li>";
    html += "</ul>";
    html += "<a href='/'>BACK</a>";
    html += "</body></html>";
    request->send(200, "text/html", html);
  });

  // Výpis súborov na SD karte s tlačidlom Delete a možnosťou uploadu
  server.on("/sd/list", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<html><head><meta charset='utf-8'><title>SD Card Files</title>";
    html += "<style>";
    html += "body { font-family: sans-serif; font-size: 24px; color: #00FF00; background-color: #000; margin: 0; padding: 20px; }";
    html += "h1 { margin-top: 0; }";
    html += "ul { list-style: none; margin: 0; padding: 0; }";
    html += "li { margin-bottom: 10px; }";
    html += "a, button { display: inline-block; font-size: 24px; color: #00FF00; border: 2px solid #00FF00; background-color: transparent; padding: 10px 20px; margin: 5px; text-decoration: none; }";
    html += "a:hover, button:hover { background-color: #00FF00; color: #000; }";
    html += ".deleteLink { border-color: #FF0000; color: #FF0000; }";
    html += ".deleteLink:hover { background-color: #FF0000; color: #000; }";
    html += ".backLink { border: 2px solid #00FF00; padding: 10px 20px; }";
    html += "</style></head><body>";
    html += "<h1>SD Card Files</h1><ul>";
    
    SDManager sd;
    File root = SD_ADAPTER.open("/");
    if(root && root.isDirectory()){
      File file = root.openNextFile();
      while(file){
        String name = file.name();
        html += "<li>" + name + " (" + String(file.size()) + " bytes) ";
        // Namiesto priameho linku voláme funkciu deleteFile()
        html += "<a href='#' class='deleteLink' onclick=\"deleteFile('" + name + "')\">Delete</a></li>";
        file.close();
        file = root.openNextFile();
      }
    } else {
      html += "<li>Failed to open directory: /</li>";
    }
    root.close();
    
    html += "</ul>";
    html += "<h2>Upload File</h2>";
    html += "<form id='uploadForm' enctype='multipart/form-data'>";
    html += "Select file: <input type='file' name='upload'><br><br>";
    html += "<input type='submit' value='Upload'>";
    html += "</form>";
    html += "<div id='uploadStatus'></div>";
    html += "<p><a href='/sd/ui' class='backLink'>BACK</a></p>";
    html += "<script>";
    html += "document.getElementById('uploadForm').addEventListener('submit', function(e) {";
    html += "  e.preventDefault();";
    html += "  var formData = new FormData(this);";
    html += "  document.getElementById('uploadStatus').innerText = 'Uploading...';";
    html += "  fetch('/sd/upload', { method: 'POST', body: formData })";
    html += "    .then(response => response.text())";
    html += "    .then(data => {";
    html += "      document.getElementById('uploadStatus').innerText = data;";
    html += "      setTimeout(function() { location.reload(); }, 1000);";
    html += "    })";
    html += "    .catch(err => {";
    html += "      document.getElementById('uploadStatus').innerText = 'Upload failed';";
    html += "    });";
    html += "});";
    // Upravená funkcia pre mazanie súborov, ktorá spracováva požiadavku asynchrónne
    html += "function deleteFile(filename) {";
    html += "  if (!confirm('Are you sure you want to delete ' + filename + '?')) return;";
    html += "  fetch('/sd/delete?path=' + encodeURIComponent(filename))";
    html += "    .then(response => response.text())";
    html += "    .then(data => {";
    html += "       alert(data);";
    html += "       setTimeout(function() { location.reload(); }, 1000);";
    html += "    })";
    html += "    .catch(err => { alert('Delete failed'); });";
    html += "}";
    html += "</script>";
    html += "</body></html>";
    request->send(200, "text/html", html);
  });

  // Endpoint for uploadind files
  server.on("/sd/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Upload complete");
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    static File uploadFile;
    String path = filename;
    if (!filename.startsWith("/")) path = "/" + filename;
    if(index == 0) {
      DEBUG_PRINTF("[SD] Starting upload for file: %s\n", path.c_str());
      uploadFile = SD_ADAPTER.open(path.c_str(), FILE_WRITE);
      if (!uploadFile) {
        DEBUG_PRINTF("[SD] Failed to open file for writing: %s\n", path.c_str());
      }
    }
    if(uploadFile) {
      size_t written = uploadFile.write(data, len);
      DEBUG_PRINTF("[SD] Writing %d bytes to file: %s (written: %d bytes)\n", len, path.c_str(), written);
    }
    if(final) {
      if(uploadFile) {
        uploadFile.close();
        DEBUG_PRINTF("[SD] Upload complete and file closed: %s\n", path.c_str());
      }
    }
  });

  // Endpoint for deleting files
  server.on("/sd/delete", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasArg("path")) {
      request->send(400, "text/plain", "Missing 'path' parameter");
      return;
    }
    String path = request->arg("path");
    if (!path.startsWith("/")) path = "/" + path;
    bool res = SD_ADAPTER.remove(path.c_str());
    String msg = res ? "File deleted" : "Delete failed";
    request->send(200, "text/plain", msg);
  });
  
  //  FSEQ list
  server.on("/fseq/list", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<html><head><meta charset='utf-8'><title>FSEQ Files</title>";
    html += "<style>";
    html += "body { font-family: sans-serif; font-size: 24px; color: #00FF00; background-color: #000; margin: 0; padding: 20px; }";
    html += "h1 { margin-top: 0; }";
    html += "ul { list-style: none; margin: 0; padding: 0; }";
    html += "li { margin-bottom: 10px; }";
    html += "a, button { display: inline-block; font-size: 24px; color: #00FF00; border: 2px solid #00FF00; background-color: transparent; padding: 10px 20px; margin: 5px; text-decoration: none; }";
    html += "a:hover, button:hover { background-color: #00FF00; color: #000; }";
    html += "</style></head><body>";
    html += "<h1>FSEQ Files</h1><ul>";
    
    File root = SD_ADAPTER.open("/");
    if(root && root.isDirectory()){
      File file = root.openNextFile();
      while(file){
        String name = file.name();
        if(name.endsWith(".fseq") || name.endsWith(".FSEQ")){
          html += "<li>" + name + " ";
          html += "<button id='btn_" + name + "' onclick=\"toggleFseq('" + name + "')\">Play</button>";
          html += "</li>";
        }
        file.close();
        file = root.openNextFile();
      }
    }
    root.close();
    html += "</ul>";
    html += "<p><a href='/sd/ui'>BACK</a></p>";
    html += "<script>";
    html += "function toggleFseq(file){";
    html += "  var btn = document.getElementById('btn_' + file);";
    html += "  if(btn.innerText === 'Play'){";
    html += "    fetch('/fseq/start?file=' + encodeURIComponent(file))";
    html += "      .then(response => response.text())";
    html += "      .then(data => { btn.innerText = 'Stop'; });";
    html += "  } else {";
    html += "    fetch('/fseq/stop?file=' + encodeURIComponent(file))";
    html += "      .then(response => response.text())";
    html += "      .then(data => { btn.innerText = 'Play'; });";
    html += "  }";
    html += "}";
    html += "</script>";
    html += "</body></html>";
    request->send(200, "text/html", html);
  });
  
  // Endpoint for playing FSEQ
  server.on("/fseq/start", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasArg("file")) {
      request->send(400, "text/plain", "Missing 'file' parameter");
      return;
    }
    String filepath = request->arg("file");
    if (!filepath.startsWith("/")) filepath = "/" + filepath;
    FSEQPlayer::loadRecording(filepath.c_str(), 0, uint16_t(-1), 0.0f);
    request->send(200, "text/plain", "FSEQ started: " + filepath);
  });
  
  // Endpoint for stop playing FSEQ 
  server.on("/fseq/stop", HTTP_GET, [](AsyncWebServerRequest *request) {
    FSEQPlayer::clearLastPlayback();
    realtimeLock(10, REALTIME_MODE_INACTIVE);
    request->send(200, "text/plain", "FSEQ stopped");
  });
}