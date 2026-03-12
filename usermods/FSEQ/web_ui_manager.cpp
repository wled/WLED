#include "web_ui_manager.h"
#include "fseq_player.h"
#include "usermod_fseq.h"

struct UploadContext {
  File* file;
  bool error;
};

static const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>WLED FSEQ UI</title>
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

nav { display:flex; background:#000; border-bottom:1px solid #222; }

nav button {
  flex:1; padding:12px; background:none; border:none;
  color:var(--text-dim); font-size:14px; cursor:pointer;
}

nav button.active {
  color:var(--accent);
  border-bottom:2px solid var(--accent);
}

.tab-content { display:none; padding:15px; }
.tab-content.active { display:block; }

.card {
  background:var(--card); padding:15px;
  margin-bottom:15px; border-radius:var(--radius);
  box-shadow:0 0 10px #000;
}

ul { list-style:none; padding:0; margin:0; }

li {
  padding:8px 0; border-bottom:1px solid #222;
  font-size:14px;
}

.btn {
  display:inline-block; padding:6px 12px;
  margin:4px 4px 0 0; border-radius:var(--radius);
  border:1px solid var(--accent);
  background:transparent; color:var(--accent);
  cursor:pointer; font-size:13px;
}

.btn:hover { background:var(--accent); color:#000; }

.btn-stop {
  border-color:var(--danger);
  color:var(--danger);
}

.btn-stop:hover { background:var(--danger); color:#000; }

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
</style>

<script>
function goBack(){ window.location.href="/"; }

function showTab(tab){
  tabSd.classList.remove('active');
  tabFseq.classList.remove('active');
  tabSdBtn.classList.remove('active');
  tabFseqBtn.classList.remove('active');

  if(tab==="sd"){ tabSd.classList.add('active'); tabSdBtn.classList.add('active'); }
  else { tabFseq.classList.add('active'); tabFseqBtn.classList.add('active'); }
}

/* ---------------- SD LIST ---------------- */

function loadSDList(){
  fetch('/api/sd/list')
    .then(r=>r.json())
    .then(data=>{
      const ul=document.getElementById('sdList');
      ul.innerHTML='';

      data.files.forEach(f=>{
        const li=document.createElement('li');
        li.textContent=f.name+" ("+f.size+" KB) ";

        const btn=document.createElement('button');
        btn.className="btn";
        btn.textContent="Delete";

        btn.addEventListener("click",()=>{
          deleteFile(f.name);
        });

        li.appendChild(btn);
        ul.appendChild(li);
      });

      updateStorage(data.usedKB,data.totalKB);
    });
}

function updateStorage(used,total){
  if(!total||total===0)return;
  let percent=(used/total)*100;
  storageBar.style.width=percent+"%";
  storageText.innerText=
    "Used: "+used+" KB / "+total+" KB ("+percent.toFixed(1)+"%)";
}

/* ---------------- FSEQ LIST ---------------- */

function loadFseqList(){
  fetch('/api/fseq/list')
    .then(r=>r.json())
    .then(files=>{
      const ul=document.getElementById('fseqList');
      ul.innerHTML='';

      files.forEach(f=>{
        const li=document.createElement('li');

        const nameDiv=document.createElement('div');
        nameDiv.textContent=f.name;
        nameDiv.style.marginBottom="6px";

        const playBtn=document.createElement('button');
        playBtn.className="btn";
        playBtn.textContent="Play";

        const loopBtn=document.createElement('button');
        loopBtn.className="btn";
        loopBtn.textContent="Loop";

        playBtn.dataset.type = "play";
        loopBtn.dataset.type = "loop";

        playBtn.addEventListener("click",()=>{
          toggleNormal(f.name,playBtn,loopBtn);
        });

        loopBtn.addEventListener("click",()=>{
          toggleLoop(f.name,playBtn,loopBtn);
        });

        li.appendChild(nameDiv);
        li.appendChild(playBtn);
        li.appendChild(loopBtn);

        ul.appendChild(li);
      });
    });
}

function resetAllFseqButtons(){
  document.querySelectorAll("#fseqList button").forEach(btn=>{
    btn.classList.remove("btn-stop");
    btn.textContent = btn.dataset.type === "loop" ? "Loop" : "Play";
    btn.dataset.state = "";
  });
}

function toggleNormal(name, playBtn, loopBtn) {

  const isPlaying = playBtn.dataset.state === "playing";

  if (!isPlaying) {

    fetch('/api/fseq/start', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:'file='+encodeURIComponent(name)})
      .then(r => {
        if (r.ok) {
          resetAllFseqButtons();
          playBtn.dataset.state = "playing";
          playBtn.textContent = "Stop";
          playBtn.classList.add("btn-stop");
        }
      });

  } else {

    fetch('/api/fseq/stop', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}})
    resetAllFseqButtons();
  }
}

function toggleLoop(name, playBtn, loopBtn) {

  const isLooping = loopBtn.dataset.state === "playing";

  if (!isLooping) {

    fetch('/api/fseq/startloop', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:'file='+encodeURIComponent(name)})
      .then(r => {
        if (r.ok) {
          resetAllFseqButtons();
          playBtn.dataset.state = "looping";
          playBtn.textContent = "Stop";
          playBtn.classList.add("btn-stop");
        }
      });

  } else {

    fetch('/api/fseq/stop', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}})
    resetAllFseqButtons();
  }
}

function checkFseqStatus(){
  fetch('/api/fseq/status')
    .then(r=>r.json())
    .then(status=>{

      if(!status.playing){
        resetAllFseqButtons();
        return;
      }

      document.querySelectorAll("#fseqList li").forEach(li=>{
        const name = li.querySelector("div").textContent;

        if(name === status.file){

          const btns = li.querySelectorAll("button");

          btns.forEach(btn=>{
            btn.classList.add("btn-stop");
            btn.textContent = "Stop";
            btn.dataset.state = "playing";
          });
        }
      });

    });
}

/* ---------------- DELETE ---------------- */

function deleteFile(name){
  if(!confirm("Delete "+name+"?"))return;
  fetch('/api/sd/delete', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:'path='+encodeURIComponent(name)})
    .then(()=>{ loadSDList(); loadFseqList(); });
}

/* ---------------- UPLOAD ---------------- */

function uploadFile(){
  let fileInput=document.getElementById("fileInput");
  if(!fileInput.files.length)return;

  let formData=new FormData();
  formData.append("upload",fileInput.files[0]);

  let xhr=new XMLHttpRequest();
  xhr.open("POST","/api/sd/upload",true);

  xhr.upload.onprogress=function(e){
    if(e.lengthComputable){
      let percent=(e.loaded/e.total)*100;
      progressBar.style.width=percent+"%";
      statusText.innerText=Math.round(percent)+"%";
    }
  };

  xhr.onload=function(){
    statusText.innerText="Upload complete";
    loadSDList();
    loadFseqList();
    setTimeout(()=>{
      progressBar.style.width="0%";
      statusText.innerText="";
    },2000);
  };

  xhr.send(formData);
}

document.addEventListener("DOMContentLoaded",()=>{
  loadSDList();
  loadFseqList();
  setInterval(checkFseqStatus,1000);
});
</script>
</head>

<body>

<header>
  <button class="back-btn" onclick="goBack()">◀ Back</button>
  <h1>FSEQ UI</h1>
  <div style="width:60px;"></div>
</header>

<nav>
  <button id="tabSdBtn" class="active" onclick="showTab('sd')">SD Files</button>
  <button id="tabFseqBtn" onclick="showTab('fseq')">FSEQ</button>
</nav>

<div id="tabSd" class="tab-content active">

  <div class="card">
    <h3>SD Storage</h3>
    <div class="progress-container">
      <div class="storage-bar" id="storageBar"></div>
    </div>
    <div class="storage-text" id="storageText"></div>
  </div>

  <div class="card">
    <h3>SD Files</h3>
    <ul id="sdList"></ul>
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

</div>

<div id="tabFseq" class="tab-content">
  <div class="card">
    <h3>FSEQ Files</h3>
    <ul id="fseqList"></ul>
  </div>
</div>

</body>
</html>
)rawliteral";


void WebUIManager::registerEndpoints() {

  // Main UI page (navigation, SD and FSEQ tabs)
  server.on("/fsequi", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", PAGE_HTML);
  });

  // API - List SD files (size in KB + storage info)
    server.on("/api/sd/list", HTTP_GET, [](AsyncWebServerRequest *request) {

    File root = SD_ADAPTER.open("/");

    uint64_t totalBytes = SD_ADAPTER.totalBytes();
    uint64_t usedBytes  = SD_ADAPTER.usedBytes();

    // Adjust size if needed (depends on max file count)
    DynamicJsonDocument doc(8192);

    JsonObject rootObj = doc.to<JsonObject>();
    JsonArray files = rootObj.createNestedArray("files");

    if (root && root.isDirectory()) {

      File file = root.openNextFile();
      while (file) {
	
        String name = file.name();

        JsonObject obj = files.createNestedObject();
        obj["name"] = name;
        obj["size"] = (float)file.size() / 1024.0;

        file.close();
        file = root.openNextFile();
      }
    }

    root.close();

    rootObj["usedKB"]  = (float)usedBytes / 1024.0;
    rootObj["totalKB"] = (float)totalBytes / 1024.0;

    String output;
    serializeJson(doc, output);
	  
    if (doc.overflowed()) {
      request->send(507, "text/plain", "JSON buffer too small; file list may be truncated");
      return;
      }

    request->send(200, "application/json", output);
  });


  // API - List FSEQ files
  server.on("/api/fseq/list", HTTP_GET, [](AsyncWebServerRequest *request) {

	  File root = SD_ADAPTER.open("/");

	  DynamicJsonDocument doc(4096);
	  JsonArray files = doc.to<JsonArray>();

	  if (root && root.isDirectory()) {

		File file = root.openNextFile();
		while (file) {

		  String name = file.name();

		  if (name.endsWith(".fseq") || name.endsWith(".FSEQ")) {
			JsonObject obj = files.createNestedObject();
			obj["name"] = name;
		  }

		  file.close();
		  file = root.openNextFile();
		}
	  }

	  root.close();

	  String output;
	  serializeJson(doc, output);
	  
	  if (doc.overflowed()) {
	    request->send(507, "text/plain", "JSON buffer too small; file list may be truncated");
	    return;
	  }

	  request->send(200, "application/json", output);
	});

  // API - File Upload
	server.on(
	  "/api/sd/upload", HTTP_POST,

	  // MAIN HANDLER
	  [](AsyncWebServerRequest *request) {

		UploadContext* ctx = static_cast<UploadContext*>(request->_tempObject);

		if (!ctx || ctx->error || !ctx->file || !*(ctx->file)) {
		  request->send(500, "text/plain", "Failed to open file for writing");
		} else {
		  request->send(200, "text/plain", "Upload complete");
		}

		// Cleanup
		if (ctx) {
		  if (ctx->file) {
			if (*(ctx->file)) ctx->file->close();
			delete ctx->file;
		  }
		  delete ctx;
		  request->_tempObject = nullptr;
		}
	  },

	  // UPLOAD CALLBACK
	  [](AsyncWebServerRequest *request, String filename, size_t index,
		 uint8_t *data, size_t len, bool final) {

		UploadContext* ctx;

		if (index == 0) {
		  if (!filename.startsWith("/"))
			filename = "/" + filename;

		  ctx = new UploadContext();
		  ctx->error = false;
		  ctx->file = new File(SD_ADAPTER.open(filename.c_str(), FILE_WRITE));

		  if (!*(ctx->file)) {
			ctx->error = true;
		  }

		  request->_tempObject = ctx;
		}

		ctx = static_cast<UploadContext*>(request->_tempObject);

		if (!ctx || ctx->error || !ctx->file || !*(ctx->file))
		  return;

		ctx->file->write(data, len);
		
	  }
	);

  // API - File Delete
  server.on("/api/sd/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
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
  server.on("/api/fseq/start", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasArg("file")) {
      request->send(400, "text/plain", "Missing file param");
      return;
    }
    String filepath = request->arg("file");
    if (!filepath.startsWith("/"))
      filepath = "/" + filepath;
    FSEQPlayer::loadRecording(filepath.c_str(), 0, uint16_t(-1), 0.0f, false);
    request->send(200, "text/plain", "FSEQ started");
  });

  // API - Start FSEQ in loop mode
  server.on(
      "/api/fseq/startloop", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!request->hasArg("file")) {
          request->send(400, "text/plain", "Missing file param");
          return;
        }
        String filepath = request->arg("file");
        if (!filepath.startsWith("/"))
          filepath = "/" + filepath;
        FSEQPlayer::loadRecording(filepath.c_str(), 0, uint16_t(-1), 0.0f, true);
        request->send(200, "text/plain", "FSEQ loop started");
      });

  // API - Stop FSEQ
  server.on("/api/fseq/stop", HTTP_POST, [](AsyncWebServerRequest *request) {
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

	  DynamicJsonDocument doc(512);

	  doc["playing"] = FSEQPlayer::isPlaying();
	  doc["file"]    = FSEQPlayer::getFileName();

	  String output;
	  serializeJson(doc, output);

	  request->send(200, "application/json", output);
	});
}