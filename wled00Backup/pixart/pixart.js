//Start up code
//if (window.location.protocol == "file:") {
//  let locip = prompt("File Mode. Please enter WLED IP!");
//  gId('curlUrl').value = locip;
//} else
//
//Start up code
let devMode =  false; //Remove
gurl.value = location.host;

const urlParams = new URLSearchParams(window.location.search);
if (gurl.value.length < 1){
  gurl.value = "Missing_Host";
}

function gen(){
  //Generate image if enough info is in place
  //Is host non empty
  //Is image loaded
  //is scale > 0
  if (((szX.value > 0 && szY.value > 0) || szDiv.style.display == 'none') && gurl.value.length > 0 && prw.style.display != 'none'){
    //regenerate
    let base64Image = prw.src;
    if (isValidBase64Gif(base64Image)) {
      im.src = base64Image;
      getPixelRGBValues(base64Image);
      imcn.style.display = "block";
      bcn.style.display = "";
    } else {
      let imageInfo = '<p><b>WARNING!</b> File does not appear to be a valid image</p>';
      imin.innerHTML = imageInfo;
      imin.style.display = "block";
      imcn.style.display = "none";
      JLD.value = '';
      if (devMode) console.log("The string '" + base64Image + "' is not a valid base64 image.");
    }
  }
  
  if(gurl.value.length > 0){
    gId("sSg").setAttribute("fill", accentColor);
  } else{
    gId("sSg").setAttribute("fill", accentTextColor);
    let ts = tSg;
    ts.style.display = "none";
    ts.innerHTML = "";
    sID.style.display = "flex";
  }
}


// Code for copying the generated string to clipboard

cjb.addEventListener('click', async () => {
  let JSONled = JLD;
  JSONled.select();
  try {
    await navigator.clipboard.writeText(JSONled.value);
  } catch (err) {
    try {
      await d.execCommand("copy");
    } catch (err) {
      console.error('Failed to copy text: ', err);
    }
  }
});

// Event listeners =======================

lSS.addEventListener("change", gen);
szY.addEventListener("change", gen);
szX.addEventListener("change", gen);
cFS.addEventListener("change", gen);
aS.addEventListener("change", gen);
brgh.addEventListener("change", gen);
cLN.addEventListener("change", gen);
haIDe.addEventListener("change", gen);
haUe.addEventListener("change", gen);
haNe.addEventListener("change", gen);
gurl.addEventListener("change", gen);
sID.addEventListener("change", gen);
prw.addEventListener("load", gen);
//gId("convertbutton").addEventListener("click", gen);

tSg.addEventListener("change", () => {
  sop = tSg.options[tSg.selectedIndex];
  szX.value = sop.dataset.x;
  szY.value = sop.dataset.y;
  gen();
});

gId("sendJSONledbutton").addEventListener('click', async () => {
  if (window.location.protocol === "https:") {
    alert('Will only be available when served over http (or WLED is run over https)');
  } else {
    postPixels();
  }
});

brgh.oninput = () => {
  brgV.textContent = brgh.value;
  let perc = parseInt(brgh.value)*100/255;
  var val = `linear-gradient(90deg, #bbb ${perc}%, #333 ${perc}%)`;
  brgh.style.backgroundImage = val;
}

cLN.oninput = () => {
  let cln = cLN;
  cLV.textContent = cln.value;
  let perc = parseInt(cln.value)*100/512;
  var val = `linear-gradient(90deg, #bbb ${perc}%, #333 ${perc}%)`;
  cln.style.backgroundImage = val;
}

frm.addEventListener("change", () => {
  for (var i = 0; i < hideableRows.length; i++) {
    hideableRows[i].classList.toggle("hide", frm.value !== "ha");
    gen();
  }
});

async function postPixels() {
  let ss = gId("sendSvgP");
  ss.setAttribute("fill", prsCol);
  let er = false;
  for (let i of httpArray) {
    try {
      if (devMode) console.log(i);
      if (devMode) console.log(i.length);
      const response = await fetch('http://'+gId('curlUrl').value+'/json/state', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
          //'Content-Type': 'text/html; charset=UTF-8'
        },
        body: i
      });
      const data = await response.json();
      if (devMode) console.log(data);
    } catch (error) {
      console.error(error);
      er = true;
    }
  }
  if(er){
    //Something went wrong
    ss.setAttribute("fill", redColor);
    setTimeout(function(){ 
      ss.setAttribute("fill", accentTextColor);
    }, 1000);
  } else {
    // A, OK
    ss.setAttribute("fill", greenColor);
    setTimeout(function(){ 
      ss.setAttribute("fill", accentColor);
    }, 1000);
  }
}

//File uploader code
const dropZone = gId('drop-zone');
const filePicker = gId('file-picker');
const preview = prw;

// Listen for dragenter, dragover, and drop events
dropZone.addEventListener('dragenter', dragEnter);
dropZone.addEventListener('dragover', dragOver);
dropZone.addEventListener('drop', dropped);
dropZone.addEventListener('click', zoneClicked);

// Listen for change event on file picker
filePicker.addEventListener('change', filePicked);

// Handle zone click
function zoneClicked(e) {
  e.preventDefault();
  //this.classList.add('drag-over');
  //alert('Hej');
  filePicker.click();
}

// Handle dragenter
function dragEnter(e) {
  e.preventDefault();
  this.classList.add('drag-over');
}

// Handle dragover
function dragOver(e) {
  e.preventDefault();
}

// Handle drop
function dropped(e) {
  e.preventDefault();
  this.classList.remove('drag-over');

  // Get the dropped file
  const file = e.dataTransfer.files[0];
  updatePreview(file)
}

// Handle file picked
function filePicked(e) {
  // Get the picked file
  const file = e.target.files[0];
  updatePreview(file)
}

// Update the preview image
function updatePreview(file) {
  // Use FileReader to read the file
  const reader = new FileReader();
  reader.onload = () => {
    // Update the preview image
    preview.src = reader.result;
    //gId("submitConvertDiv").style.display = "";
    prw.style.display = "";
  };
  reader.readAsDataURL(file);
}

function isValidBase64Gif(string) {
  // Use a regular expression to check that the string is a valid base64 string
  /*
  const base64gifPattern = /^data:image\/gif;base64,([A-Za-z0-9+/:]{4})*([A-Za-z0-9+/:]{3}=|[A-Za-z0-9+/:]{2}==)?$/;
  const base64pngPattern = /^data:image\/png;base64,([A-Za-z0-9+/:]{4})*([A-Za-z0-9+/:]{3}=|[A-Za-z0-9+/:]{2}==)?$/;
  const base64jpgPattern = /^data:image\/jpg;base64,([A-Za-z0-9+/:]{4})*([A-Za-z0-9+/:]{3}=|[A-Za-z0-9+/:]{2}==)?$/;
  const base64webpPattern = /^data:image\/webp;base64,([A-Za-z0-9+/:]{4})*([A-Za-z0-9+/:]{3}=|[A-Za-z0-9+/:]{2}==)?$/;
  */
  //REMOVED, Any image appear to work as long as it can be drawn to the canvas. Leaving code in for future use, possibly
  if (1==1 || base64gifPattern.test(string) || base64pngPattern.test(string) || base64jpgPattern.test(string) || base64webpPattern.test(string)) {
    return true;
  } else {
    //Not OK
    return false;
  }
}

var hideableRows = d.querySelectorAll(".ha-hide");
for (var i = 0; i < hideableRows.length; i++) {
  hideableRows[i].classList.add("hide");
}
frm.addEventListener("change", () => {
  for (var i = 0; i < hideableRows.length; i++) {
    hideableRows[i].classList.toggle("hide", frm.value !== "ha");
  }
});

function switchScale() {
  //let scalePath = gId("scaleDiv").children[1].children[0]
  let scaleTogglePath = scDiv.children[0].children[0]
  let color = scaleTogglePath.getAttribute("fill");
  let d = '';
  if (color === accentColor) {
    color = accentTextColor;
    d = scaleToggleOffd;
    szDiv.style.display = "none";
    // Set values to actual XY of image, if possible
  } else {
    color = accentColor;
    d = scaleToggleOnd;
    szDiv.style.display = "";
  }
  //scalePath.setAttribute("fill", color);
  scaleTogglePath.setAttribute("fill", color);
  scaleTogglePath.setAttribute("d", d);
  gen();
}

function generateSegmentOptions(array) {
  //This function is prepared for a name property on each segment for easier selection
  //Currently the name is generated generically based on index
  tSg.innerHTML = "";
  for (var i = 0; i < array.length; i++) {
    var option = cE("option");
    option.value = array[i].value;
    option.text = array[i].text;
    option.dataset.x = array[i].x;
    option.dataset.y = array[i].y;
    tSg.appendChild(option);
    if(i === 0) {
      option.selected = true;
      szX.value = option.dataset.x;
      szY.value = option.dataset.y;
    }
  }
}

// Get segments from device
async function getSegments() {
  cv = gurl.value;
  if (cv.length > 0 ){
    try {
      var arr = [];
      const response = await fetch('http://'+cv+'/json/state');
      const json = await response.json();
      let ids = json.seg.map(sg => ({id: sg.id, n: sg.n, xs: sg.start, xe: sg.stop, ys: sg.startY, ye: sg.stopY}));
      for (var i = 0; i < ids.length; i++) {
        arr.push({
            value: ids[i]["id"],
            text: ids[i]["n"] + ' (index: ' + ids[i]["id"] + ')',
            x: ids[i]["xe"] - ids[i]["xs"],
            y: ids[i]["ye"] - ids[i]["ys"]
        });
      }
      generateSegmentOptions(arr);
      tSg.style.display = "flex";
      sID.style.display = "none";
      gId("sSg").setAttribute("fill", greenColor);
      setTimeout(function(){ 
        gId("sSg").setAttribute("fill", accentColor);
      }, 1000);

    } catch (error) {
      console.error(error);
      gId("sSg").setAttribute("fill", redColor);
      setTimeout(function(){ 
        gId("sSg").setAttribute("fill", accentColor);
      }, 1000);
      tSg.style.display = "none";
      sID.style.display = "flex";
    }
  } else{
    gId("sSg").setAttribute("fill", redColor);
    setTimeout(function(){ 
      gId("sSg").setAttribute("fill", accentTextColor);
    }, 1000);
    tSg.style.display = "none";
    sID.style.display = "flex";
  }
}

//Initial population of segment selection
function generateSegmentArray(noOfSegments) {
  var arr = [];
  for (var i = 0; i < noOfSegments; i++) {
    arr.push({
      value: i,
      text: "Segment index " + i
    });
  }
  return arr;
}

var segmentData = generateSegmentArray(10);

generateSegmentOptions(segmentData);

seDiv.innerHTML =
'<svg id=getSegmentsSVG style="width:36px;height:36px;cursor:pointer" viewBox="0 0 24 24" onclick="getSegments()"><path id=sSg fill="currentColor" d="M6.5 20Q4.22 20 2.61 18.43 1 16.85 1 14.58 1 12.63 2.17 11.1 3.35 9.57 5.25 9.15 5.68 7.35 7.38 5.73 9.07 4.1 11 4.1 11.83 4.1 12.41 4.69 13 5.28 13 6.1V12.15L14.6 10.6L16 12L12 16L8 12L9.4 10.6L11 12.15V6.1Q9.1 6.45 8.05 7.94 7 9.43 7 11H6.5Q5.05 11 4.03 12.03 3 13.05 3 14.5 3 15.95 4.03 17 5.05 18 6.5 18H18.5Q19.55 18 20.27 17.27 21 16.55 21 15.5 21 14.45 20.27 13.73 19.55 13 18.5 13H17V11Q17 9.8 16.45 8.76 15.9 7.73 15 7V4.68Q16.85 5.55 17.93 7.26 19 9 19 11 20.73 11.2 21.86 12.5 23 13.78 23 15.5 23 17.38 21.69 18.69 20.38 20 18.5 20M12 11.05Z" /></svg>'
/*gId("convertbutton").innerHTML = 
'<svg style="width:36px;height:36px" viewBox="0 0 24 24"><path fill="currentColor" d="M12,6V9L16,5L12,1V4A8,8 0 0,0 4,12C4,13.57 4.46,15.03 5.24,16.26L6.7,14.8C6.25,13.97 6,13 6,12A6,6 0 0,1 12,6M18.76,7.74L17.3,9.2C17.74,10.04 18,11 18,12A6,6 0 0,1 12,18V15L8,19L12,23V20A8,8 0 0,0 20,12C20,10.43 19.54,8.97 18.76,7.74Z" /> </svg>&nbsp; Convert to WLED JSON '; 
*/
cjb.innerHTML = 
'<svg class="svg-icon" style="width:36px;height:36px" viewBox="0 0 24 24"> <path fill="currentColor" d="M19,21H8V7H19M19,5H8A2,2 0 0,0 6,7V21A2,2 0 0,0 8,23H19A2,2 0 0,0 21,21V7A2,2 0 0,0 19,5M16,1H4A2,2 0 0,0 2,3V17H4V3H16V1Z" /> </svg>&nbsp; Copy to clipboard'; 
gId("sendJSONledbutton").innerHTML = 
'<svg class="svg-icon" style="width:36px;height:36px" viewBox="0 0 24 24"> <path id=sendSvgP fill="currentColor" d="M6.5 20Q4.22 20 2.61 18.43 1 16.85 1 14.58 1 12.63 2.17 11.1 3.35 9.57 5.25 9.15 5.88 6.85 7.75 5.43 9.63 4 12 4 14.93 4 16.96 6.04 19 8.07 19 11 20.73 11.2 21.86 12.5 23 13.78 23 15.5 23 17.38 21.69 18.69 20.38 20 18.5 20H13Q12.18 20 11.59 19.41 11 18.83 11 18V12.85L9.4 14.4L8 13L12 9L16 13L14.6 14.4L13 12.85V18H18.5Q19.55 18 20.27 17.27 21 16.55 21 15.5 21 14.45 20.27 13.73 19.55 13 18.5 13H17V11Q17 8.93 15.54 7.46 14.08 6 12 6 9.93 6 8.46 7.46 7 8.93 7 11H6.5Q5.05 11 4.03 12.03 3 13.05 3 14.5 3 15.95 4.03 17 5.05 18 6.5 18H9V20M12 13Z" /> </svg>&nbsp; Send to device';

//After everything is loaded, check if we have a possible IP/host

if(gurl.value.length > 0){
  // Needs to be addressed directly here so the object actually exists
  gId("sSg").setAttribute("fill", accentColor);
}
