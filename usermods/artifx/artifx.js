
var isCEEditor = false;

function toggleCEEditor(name, segID) {
    if (isInfo) toggleInfo();
    if (isNodes) toggleNodes();
    isCEEditor = !isCEEditor;
    if (isCEEditor) populateCEEditor(name, segID);
    d.getElementById('ceEditor').style.transform = (isCEEditor) ? "translateY(0px)":"translateY(100%)";
}

function loadLogFile(name, attempt) {
    var ceLogArea = d.getElementById("ceLogArea");
    fetchAndExecute((loc?`http://${locip}`:'.') + "/", name, null, function(parms,logtext)
    {
      if (logtext == "") {
        if (attempt < 10) {
          ceLogArea.value = ("...........").substring(0, attempt + 1);
          setTimeout(() =>
          {
            loadLogFile(name, attempt + 1);
          }, 1000);
        }
        else
          ceLogArea.value = "log not found after 10 seconds";
      }
      else
        ceLogArea.value = logtext;
    }, function(parms,error){
      showToast(error);
      console.log(error);
    });
}

function uploadFileWithText(name, text)
{
  var req = new XMLHttpRequest();
  req.addEventListener('load', function(){showToast(this.responseText,this.status >= 400)});
  req.addEventListener('error', function(e){showToast(e.stack,true);});
  req.open("POST", "/upload");
  var formData = new FormData();

  var blob = new Blob([text], {type : 'application/text'});
  var fileOfBlob = new File([blob], name);
  formData.append("upload", fileOfBlob);

  req.send(formData);
}

function saveCE(name, segID) {
    showToast("Saving " + name + ".wled");

    var ceProgramArea = d.getElementById("ceProgramArea");

    uploadFileWithText("/" + name + ".wled", ceProgramArea.value);

    var obj = {"seg": {"id": segID, "reset": true}};
    requestJson(obj);

    var ceLogArea = d.getElementById("ceLogArea");
    ceLogArea.value = ".";
    setTimeout(() =>
    {
        loadLogFile(name + ".log", 1);
    }, 1000);
}

function populateCEEditor(name, segID)
{
  fetchAndExecute((loc?`http://${locip}`:'.') + "/", name + ".wled", null, function(parms,text)
  {
    var cn=`ARTI-FX Editor<br>
            <i>${name}.wled</i><br>
            <textarea class="ceTextarea" id="ceProgramArea">${text}</textarea><br>
            <button class="btn infobtn" onclick="toggleCEEditor()">Close</button>
            <button class="btn infobtn" onclick="saveCE('${name}', ${segID})">Save and Run</button><br>
            <button class="btn infobtn" onclick="downloadGHFile('CE','${name}.wled')">Download ${name}.wled</button>
            <button class="btn infobtn" onclick="loadCETemplate('${name}')">Load template</button><br>
            <button class="btn infobtn" onclick="downloadGHFile('CE','wledv033.json',true,true)">Download wled json</button>
            <button class="btn infobtn" onclick="downloadGHFile('CE','presets.json',true,true)">Download presets.json</button><br>
            <button class="btn infobtn" onclick="location.href='https://github.com/MoonModules/WLED-Effects/tree/master/ARTIFX/wled'" type="button">ARTI-FX Library</button>
            <button class="btn infobtn btn-xs" onclick="location.href='https://mm.kno.wled.ge/moonmodules/arti-fx'" type="button">?</button><br>
            <br><i>Compile and Run Log</i><br>
            <textarea class="ceTextarea" id="ceLogArea"></textarea><br>
            <i>Run log > 3 seconds is send to Serial Output.</i><br>
            <a href="#" onclick="downloadGHFile('HBB','presets.json',true,true);return false;" title="Download HBaas Base presets">🥚</a>
            <a href="#" onclick="downloadGHFile('HBE','presets.json',true,true);return false;" title="Download HBaas Effects presets">🥚</a>
            <a href="#" onclick="downloadGHFile('LM','presets.json',true,true);return false;" title="Download Ledmap presets">🥚</a>`;

    d.getElementById('kceEditor').innerHTML = cn;

    var ceLogArea = d.getElementById("ceLogArea");
    ceLogArea.value = ".";
    loadLogFile(name + ".log", 1);

  }, function(parms,error){
    showToast(error);
    console.log(error);
  });
}

function downloadGHFile(url, name, save=false, warn=false) { //Githubfile
    if (url == "CE") url = "https://raw.githubusercontent.com/MoonModules/WLED-Effects/master/ARTIFX/wled/";
    if (url == "HBB") url = "https://raw.githubusercontent.com/MoonModules/WLED-Effects/master/Presets/HB_PresetPack210808_32x32_16seg/Base%20pack/";
    if (url == "HBE") url = "https://raw.githubusercontent.com/MoonModules/WLED-Effects/master/Presets/HB_PresetPack210808_32x32_16seg/Effects%20pack/";
    if (url == "LM") url = "https://raw.githubusercontent.com/MoonModules/WLED-Effects/master/Ledmaps/";

    fetchAndExecute(url, name, null, function(parms,text) {
        if (save) {
            if (warn && !confirm('Are you sure to download/overwrite ' + name + '?'))
              return;
            uploadFileWithText("/" + name, text);
        }
        else
        {
          var ceProgramArea = d.getElementById("ceProgramArea");
          ceProgramArea.value = text;
        }
    }, function(parms,error){
      showToast(error);
      console.log(url + name,error);
    });

    return;
  
    var request = new XMLHttpRequest();
    request.onload = function() {
      if (name == "wledv033.json" || name == "presets.json") {
          if (!confirm('Are you sure to download ' + name + '?'))
            return;
          uploadFileWithText("/" + name, request.response);
      }
      else
      {
        var ceProgramArea = d.getElementById("ceProgramArea");
        ceProgramArea.value = request.response;
      }
    }
    request.open("GET", url);
    request.send();
  }
  
function loadCETemplate(name) {
    var ceProgramArea = d.getElementById("ceProgramArea");
    ceProgramArea.value = `/*
    ARTIFX Template
  */
  program ${name}
  {
    function renderFrame()
    {
      setPixelColor(counter, colorFromPalette(counter, counter))
    }
  }`;
  
}  