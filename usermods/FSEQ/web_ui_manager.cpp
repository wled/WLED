#include "web_ui_manager.h"
#include "usermod_fseq.h"

uint16_t FSEQ_refreshFileIndexCache();
bool FSEQ_getFileNameByIndex(uint16_t index, String &outName);
void FSEQ_invalidateFileIndexCache();

struct UploadContext {
  File* file;
  String path;
  bool error;
  int statusCode;
  const char* message;

  UploadContext()
    : file(nullptr),
      path(),
      error(false),
      statusCode(500),
      message("Failed to open file for writing") {}
};

static bool isUnsafeSdPath(const String& path) {
  if (path.length() == 0) return true;
  if (path.indexOf("..") >= 0) return true;
  if (path.indexOf('\\') >= 0) return true;
  return false;
}

static String normalizeSdPath(String path) {
  path.trim();
  if (!path.startsWith("/")) path = "/" + path;
  return path;
}

static const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>WLED FSEQ SD Manager</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<meta name="theme-color" content="#111111">

<style>
:root {
  --bg: #111;
  --card: #1b1b1b;
  --accent: #ff9800;
  --danger: #ff3b3b;
  --text: #eee;
  --text-dim: #aaa;
  --radius: 10px;
}

body { margin:0; background:var(--bg); color:var(--text); font-family:Arial, Helvetica, sans-serif; }

header {
  display:flex; align-items:center; padding:12px;
  background:#000; border-bottom:1px solid #222;
}

header h1 {
  flex:1; font-size:18px; margin:0;
  text-align:center; letter-spacing:1px;
}

.back-btn {
  background:none; border:1px solid var(--accent);
  color:var(--accent); padding:6px 12px;
  border-radius:var(--radius); cursor:pointer;
}

.back-btn:hover { background:var(--accent); color:#000; }

.card {
  background:var(--card); padding:15px;
  margin:15px; border-radius:var(--radius);
  box-shadow:0 0 10px #000;
}

ul { list-style:none; padding:0; margin:0; }

li {
  padding:10px 0; border-bottom:1px solid #222;
  font-size:14px;
}

.code {
  display:inline-block;
  min-width:52px;
  color:var(--accent);
  font-weight:bold;
}

.meta {
  color:var(--text-dim);
  font-size:12px;
}

.btn {
  display:inline-block; padding:6px 12px;
  margin:4px 4px 0 0; border-radius:var(--radius);
  border:1px solid var(--accent);
  background:transparent; color:var(--accent);
  cursor:pointer; font-size:13px;
}

.btn:hover { background:var(--accent); color:#000; }

.progress-container {
  width:100%; background:#222;
  border-radius:var(--radius);
  margin-top:10px; overflow:hidden; height:12px;
}

.progress-bar {
  height:100%; width:0%;
  background:var(--accent);
  transition:width 0.2s;
}

.storage-bar {
  height:12px; background:var(--danger);
  width:0%; transition:width 0.3s;
}

.status-text, .storage-text {
  font-size:13px; margin-top:8px;
  color:var(--text-dim);
}

.info {
  font-size:13px; color:var(--text-dim);
  margin-top:8px; line-height:1.5;
}
</style>

<script>
function goBack(){ window.location.href="/"; }

function loadSDList(){
  fetch('/api/sd/list')
    .then(r=>r.json())
    .then(data=>{
      const fseqList=document.getElementById('fseqList');
      const otherList=document.getElementById('otherList');
      fseqList.innerHTML='';
      otherList.innerHTML='';

      if (Array.isArray(data.fseqFiles) && data.fseqFiles.length) {
        data.fseqFiles.forEach(f=>{
          const li=document.createElement('li');
          const title=document.createElement('div');
          const code=document.createElement('span');
          code.className='code';
          code.textContent='#'+f.index;
          title.appendChild(code);
          title.appendChild(document.createTextNode(' '+f.name));
          li.appendChild(title);

          const meta=document.createElement('div');
          meta.className='meta';
          meta.textContent='Use this index on the FSEQ Player effect slider.';
          li.appendChild(meta);

          const btn=document.createElement('button');
          btn.className='btn';
          btn.textContent='Delete';
          btn.addEventListener('click',()=>deleteFile(f.name));
          li.appendChild(btn);
          fseqList.appendChild(li);
        });
      } else {
        const li=document.createElement('li');
        li.textContent='No FSEQ files found on the SD card.';
        fseqList.appendChild(li);
      }

      if (Array.isArray(data.otherFiles) && data.otherFiles.length) {
        data.otherFiles.forEach(f=>{
          const li=document.createElement('li');
          li.textContent=f.name+' ('+f.size.toFixed(1)+' KB) ';

          const btn=document.createElement('button');
          btn.className='btn';
          btn.textContent='Delete';
          btn.addEventListener('click',()=>deleteFile(f.name));

          li.appendChild(btn);
          otherList.appendChild(li);
        });
      } else {
        const li=document.createElement('li');
        li.textContent='No additional files found on the SD card.';
        otherList.appendChild(li);
      }

      updateStorage(data.usedKB,data.totalKB);
    });
}

function updateStorage(used,total){
  if(!total||total===0)return;
  let percent=(used/total)*100;
  storageBar.style.width=percent+'%';
  storageText.innerText=
    'Used: '+used.toFixed(1)+' KB / '+total.toFixed(1)+' KB ('+percent.toFixed(1)+'%)';
}

function deleteFile(name){
  if(!confirm('Delete '+name+'?'))return;
  fetch('/api/sd/delete', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:'path='+encodeURIComponent(name)})
    .then(()=>{ loadSDList(); });
}

function uploadFile(){
  let fileInput=document.getElementById('fileInput');
  if(!fileInput.files.length)return;

  let formData=new FormData();
  formData.append('upload',fileInput.files[0]);

  let xhr=new XMLHttpRequest();
  xhr.open('POST','/api/sd/upload',true);

  xhr.upload.onprogress=function(e){
    if(e.lengthComputable){
      let percent=(e.loaded/e.total)*100;
      progressBar.style.width=percent+'%';
      statusText.innerText=Math.round(percent)+'%';
    }
  };

  xhr.onload=function(){
    statusText.innerText='Upload complete';
    loadSDList();
    setTimeout(()=>{
      progressBar.style.width='0%';
      statusText.innerText='';
    },2000);
  };

  xhr.send(formData);
}

document.addEventListener('DOMContentLoaded',()=>{
  loadSDList();
});
</script>
</head>

<body>

<header>
  <button class="back-btn" onclick="goBack()">&#9664; Back</button>
  <h1>FSEQ SD Manager</h1>
  <div style="width:60px;"></div>
</header>

<div class="card">
  <h3>SD Storage</h3>
  <div class="progress-container">
    <div class="storage-bar" id="storageBar"></div>
  </div>
  <div class="storage-text" id="storageText"></div>
</div>

<div class="card">
  <h3>FSEQ Files</h3>
  <p class="info">The list below is the indexed order used by the <b>FSEQ Player</b> effect slider.</p>
  <ul id="fseqList"></ul>
</div>

<div class="card">
  <h3>Other SD Files</h3>
  <ul id="otherList"></ul>
</div>

<div class="card">
  <h3>Upload File</h3>
  <input type="file" id="fileInput"><br><br>
  <button class="btn" onclick="uploadFile()">Upload</button>
  <div class="progress-container">
    <div class="progress-bar" id="progressBar"></div>
  </div>
  <div class="status-text" id="statusText"></div>
</div>

<div class="card">
  <h3>How it works</h3>
  <p class="info">
    1. Upload one or more <b>.fseq</b> files to the SD card.<br>
    2. Open the WLED effects UI and select <b>FSEQ Player</b>.<br>
    3. Use the <b>Index</b> slider to select one of the numbered FSEQ files shown above.<br>
    4. Enable <b>Loop</b> if the sequence should repeat continuously.<br>
    5. Save the state as a preset if you want the same sequence to be restored on boot.<br>
    6. When FPP takes control, FPP temporarily overrides the local effect selection until its control timeout expires.
  </p>
</div>

</body>
</html>
)rawliteral";

void WebUIManager::registerEndpoints() {

  server.on("/fsequi", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", PAGE_HTML);
  });

  server.on("/api/sd/list", HTTP_GET, [](AsyncWebServerRequest *request) {
    File root = SD_ADAPTER.open("/");

    uint64_t totalBytes = SD_ADAPTER.totalBytes();
    uint64_t usedBytes  = SD_ADAPTER.usedBytes();

    DynamicJsonDocument doc(12288);
    JsonObject rootObj = doc.to<JsonObject>();
    JsonArray fseqFiles = rootObj.createNestedArray("fseqFiles");
    JsonArray otherFiles = rootObj.createNestedArray("otherFiles");

    const uint16_t fseqCount = FSEQ_refreshFileIndexCache();
    for (uint16_t i = 0; i < fseqCount; i++) {
      String fileName;
      if (!FSEQ_getFileNameByIndex(i, fileName)) continue;
      JsonObject obj = fseqFiles.createNestedObject();
      obj["index"] = i;
      obj["name"] = fileName;
    }

    if (root && root.isDirectory()) {
      File file = root.openNextFile();
      while (file) {
        if (!file.isDirectory()) {
          String name = file.name();
          if (!name.startsWith("/")) name = "/" + name;
          if (!(name.endsWith(".fseq") || name.endsWith(".FSEQ"))) {
            JsonObject obj = otherFiles.createNestedObject();
            obj["name"] = name;
            obj["size"] = (float)file.size() / 1024.0f;
          }
        }

        file.close();
        file = root.openNextFile();
      }
    }

    root.close();

    rootObj["usedKB"]  = (float)usedBytes / 1024.0f;
    rootObj["totalKB"] = (float)totalBytes / 1024.0f;

    String output;
    serializeJson(doc, output);

    if (doc.overflowed()) {
      request->send(507, "text/plain", "JSON buffer too small; file list may be truncated");
      return;
    }

    request->send(200, "application/json", output);
  });

  server.on("/api/fseq/list", HTTP_GET, [](AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(4096);
    JsonArray files = doc.to<JsonArray>();

    const uint16_t fseqCount = FSEQ_refreshFileIndexCache();
    for (uint16_t i = 0; i < fseqCount; i++) {
      String fileName;
      if (!FSEQ_getFileNameByIndex(i, fileName)) continue;
      JsonObject obj = files.createNestedObject();
      obj["index"] = i;
      obj["name"] = fileName;
    }

    String output;
    serializeJson(doc, output);

    if (doc.overflowed()) {
      request->send(507, "text/plain", "JSON buffer too small; file list may be truncated");
      return;
    }

    request->send(200, "application/json", output);
  });

  server.on(
    "/api/sd/upload", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      UploadContext* ctx = static_cast<UploadContext*>(request->_tempObject);

      if (!ctx || ctx->error || !ctx->file || !*(ctx->file)) {
        const int statusCode = ctx ? ctx->statusCode : 500;
        const char* message = ctx ? ctx->message : "Failed to open file for writing";
        request->send(statusCode, "text/plain", message);
      } else {
        request->send(200, "text/plain", "Upload complete");
      }

      if (ctx) {
        if (!ctx->error) FSEQ_invalidateFileIndexCache();
        if (ctx->file) {
          if (*(ctx->file)) ctx->file->close();
          delete ctx->file;
          ctx->file = nullptr;
        }

        if (ctx->error && !ctx->path.isEmpty()) {
          SD_ADAPTER.remove(ctx->path.c_str());
        }

        delete ctx;
        request->_tempObject = nullptr;
      }
    },
    [](AsyncWebServerRequest *request, String filename, size_t index,
       uint8_t *data, size_t len, bool final) {
      UploadContext* ctx = static_cast<UploadContext*>(request->_tempObject);

      if (index == 0) {
        ctx = new UploadContext();

        if (isUnsafeSdPath(filename)) {
          ctx->error = true;
          ctx->statusCode = 400;
          ctx->message = "Invalid path";
          request->_tempObject = ctx;
          return;
        }

        filename = normalizeSdPath(filename);

        if (filename == "/") {
          ctx->error = true;
          ctx->statusCode = 400;
          ctx->message = "Invalid filename";
          request->_tempObject = ctx;
          return;
        }

        ctx->path = filename;

        if (SD_ADAPTER.exists(filename.c_str())) SD_ADAPTER.remove(filename.c_str());
        ctx->file = new File(SD_ADAPTER.open(filename.c_str(), FILE_WRITE));

        if (!ctx->file || !*(ctx->file)) {
          ctx->error = true;
          ctx->statusCode = 500;
          ctx->message = "Failed to open file for writing";
        }

        request->_tempObject = ctx;
      }

      ctx = static_cast<UploadContext*>(request->_tempObject);

      if (!ctx || ctx->error || !ctx->file || !*(ctx->file))
        return;

      const size_t written = ctx->file->write(data, len);
      if (written != len) {
        ctx->error = true;
        ctx->statusCode = 500;
        ctx->message = "Failed to write upload data";
      }
    }
  );

  server.on("/api/sd/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasArg("path")) {
      request->send(400, "text/plain", "Missing path");
      return;
    }

    String path = request->arg("path");

    if (isUnsafeSdPath(path)) {
      request->send(400, "text/plain", "Invalid path");
      return;
    }

    path = normalizeSdPath(path);

    if (path == "/") {
      request->send(400, "text/plain", "Invalid path");
      return;
    }

    if (!SD_ADAPTER.exists(path.c_str())) {
      request->send(404, "text/plain", "File not found");
      return;
    }

    bool res = SD_ADAPTER.remove(path.c_str());
    if (res) FSEQ_invalidateFileIndexCache();
    request->send(res ? 200 : 500, "text/plain", res ? "File deleted" : "Delete failed");
  });
}
