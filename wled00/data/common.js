var d=document;
var loc = false, locip, locproto = "http:";

function H(pg="")   { window.open("https://kno.wled.ge/"+pg); }
function GH()       { window.open("https://github.com/wled-dev/WLED"); }
function gId(c)     { return d.getElementById(c); } // getElementById
function cE(e)      { return d.createElement(e); } // createElement
function gEBCN(c)   { return d.getElementsByClassName(c); } // getElementsByClassName
function gN(s)      { return d.getElementsByName(s)[0]; } // getElementsByName
function isE(o)     { return Object.keys(o).length === 0; } // isEmpty
function isO(i)     { return (i && typeof i === 'object' && !Array.isArray(i)); } // isObject
function isN(n)     { return !isNaN(parseFloat(n)) && isFinite(n); } // isNumber
// https://stackoverflow.com/questions/3885817/how-do-i-check-that-a-number-is-float-or-integer
function isF(n)     { return n === +n && n !== (n|0); } // isFloat
function isI(n)     { return n === +n && n === (n|0); } // isInteger
function toggle(el) { gId(el).classList.toggle("hide"); let n = gId('No'+el); if (n) n.classList.toggle("hide"); }
function tooltip(cont=null) {
	d.querySelectorAll((cont?cont+" ":"")+"[title]").forEach((element)=>{
		element.addEventListener("pointerover", ()=>{
			// save title
			element.setAttribute("data-title", element.getAttribute("title"));
			const tooltip = d.createElement("span");
			tooltip.className = "tooltip";
			tooltip.textContent = element.getAttribute("title");

			// prevent default title popup
			element.removeAttribute("title");

			let { top, left, width } = element.getBoundingClientRect();

			d.body.appendChild(tooltip);

			const { offsetHeight, offsetWidth } = tooltip;

			const offset = element.classList.contains("sliderwrap") ? 4 : 10;
			top -= offsetHeight + offset;
			left += (width - offsetWidth) / 2;

			tooltip.style.top = top + "px";
			tooltip.style.left = left + "px";
			tooltip.classList.add("visible");
		});

		element.addEventListener("pointerout", ()=>{
			d.querySelectorAll('.tooltip').forEach((tooltip)=>{
				tooltip.classList.remove("visible");
				d.body.removeChild(tooltip);
			});
			// restore title
			element.setAttribute("title", element.getAttribute("data-title"));
		});
	});
};
// https://www.educative.io/edpresso/how-to-dynamically-load-a-js-file-in-javascript
function loadJS(FILE_URL, async = true, preGetV = undefined, postGetV = undefined) {
	let scE = d.createElement("script");
	scE.setAttribute("src", FILE_URL);
	scE.setAttribute("type", "text/javascript");
	scE.setAttribute("async", async);
	d.body.appendChild(scE);
	// success event 
	scE.addEventListener("load", () => {
		//console.log("File loaded");
		if (preGetV) preGetV();
		GetV();
		if (postGetV) postGetV();
	});
	// error event
	scE.addEventListener("error", (ev) => {
		console.log("Error on loading file", ev);
		alert("Loading of configuration script failed.\nIncomplete page data!");
	});
}
function getLoc() {
	let l = window.location;
	if (l.protocol == "file:") {
		loc = true;
		locip = localStorage.getItem('locIp');
		if (!locip) {
			locip = prompt("File Mode. Please enter WLED IP!");
			localStorage.setItem('locIp', locip);
		}
	} else {
		// detect reverse proxy
		let path = l.pathname;
		let paths = path.slice(1,path.endsWith('/')?-1:undefined).split("/");
		if (paths.length > 1) paths.pop(); // remove subpage (or "settings")
		if (paths.length > 0 && paths[paths.length-1]=="settings") paths.pop(); // remove "settings"
		if (paths.length > 1) {
			locproto = l.protocol;
			loc = true;
			locip = l.hostname + (l.port ? ":" + l.port : "") + "/" + paths.join('/');
		}
	}
}
function getURL(path) { return (loc ? locproto + "//" + locip : "") + path; }
function B()          { window.open(getURL("/settings"),"_self"); }
var timeout;
function showToast(text, error = false) {
	var x = gId("toast");
	if (!x) return;
	x.innerHTML = text;
	x.className = error ? "error":"show";
	clearTimeout(timeout);
	x.style.animation = 'none';
	timeout = setTimeout(function(){ x.className = x.className.replace("show", ""); }, 2900);
}
function uploadFile(fileObj, name) {
	var req = new XMLHttpRequest();
	req.addEventListener('load', function(){showToast(this.responseText,this.status >= 400)});
	req.addEventListener('error', function(e){showToast(e.stack,true);});
	req.open("POST", "/upload");
	var formData = new FormData();
	formData.append("data", fileObj.files[0], name);
	req.send(formData);
	fileObj.value = '';
	return false;
}
// connect to WebSocket, use parent WS or open new
function connectWs(onOpen) {
	let ws;
	try {
		ws = top.window.ws;
		if (ws && ws.readyState === WebSocket.OPEN) {
			if (onOpen) onOpen();
			return ws;
		}
	} catch (e) {}
	let l = window.location;
	let pathn = l.pathname;
	let paths = pathn.slice(1, pathn.endsWith('/') ? -1 : undefined).split("/");
	let url = l.origin.replace("http", "ws");
	if (paths.length > 1) {
		url += "/" + paths[0]; // add base path for reverse proxy
	}
	ws = new WebSocket(url + "/ws");
	ws.binaryType = "arraybuffer";
	if (onOpen) { ws.onopen = onOpen; }
	return ws;
}

// send led colors to ESP using WebSocket and DDP protocol (RGB)
// ws: WebSocket object
// start: start pixel index
// len: number of pixels to send
// colors: Uint8Array with RGB values (3*len bytes)
function sendDDP(ws, start, len, colors) {
	let maxDDPpx = 480; // must fit into one WebSocket frame of 1450 bytes, DDP header is 10 bytes -> 480 RGB pixels
	if (!ws || ws.readyState !== WebSocket.OPEN) return false;
	// send in chunks of maxDDPpx
	for (let i = 0; i < len; i += maxDDPpx) {
		let cnt = Math.min(maxDDPpx, len - i);
		let off = (start + i) * 3;
		let dLen = cnt * 3;
		let cOff = i * 3;
		let pkt = new Uint8Array(10 + dLen);
		pkt[0] = 0x41; pkt[1] = 0x01; pkt[2] = 0x01; pkt[3] = 0;
		pkt[4] = (off >> 24) & 255;
		pkt[5] = (off >> 16) & 255;
		pkt[6] = (off >> 8) & 255;
		pkt[7] = off & 255;
		pkt[8] = (dLen >> 8) & 255;
		pkt[9] = dLen & 255;
		pkt.set(colors.subarray(cOff, cOff + dLen), 10);
		try {
			ws.send(pkt.buffer);
		} catch (e) {
			console.error(e);
			return false;
		}
	}
	return true;
}
