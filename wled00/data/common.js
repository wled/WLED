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
// sequential loading of external resources (JS or CSS) with retry, calls init() when done
function loadResources(files, init) {
	let i = 0;
	const loadNext = () => {
		if (i >= files.length) {
			if (init) {
				d.documentElement.style.visibility = 'visible'; // make page visible after all files are loaded if it was hidden (prevent ugly display)
				d.readyState === 'complete' ? init() : window.addEventListener('load', init);
			}
			return;
		}
		const file = files[i++];
		const isCSS = file.endsWith('.css');
		const el = d.createElement(isCSS ? 'link' : 'script');
		if (isCSS) {
			el.rel = 'stylesheet';
			el.href = file;
			const st = d.head.querySelector('style');
			if (st) d.head.insertBefore(el, st); // insert before any <style> to allow overrides
			else d.head.appendChild(el);
		} else {
			el.src = file;
			d.head.appendChild(el);
		}
		el.onload = () => {	loadNext(); };
		el.onerror = () => {
			i--; // load this file again
			setTimeout(loadNext, 100);
		};
	};
	loadNext();
}
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
// connect to WebSocket, use parent WS or open new, callback function gets passed the new WS object
function connectWs(onOpen) {
	let ws;
	try {	ws = top.window.ws;} catch (e) {}
	// reuse if open
	if (ws && ws.readyState === WebSocket.OPEN) {
		if (onOpen) onOpen(ws);
	} else {
		// create new ws connection
		getLoc(); // ensure globals are up to date
		let url = loc ? getURL('/ws').replace("http", "ws")
									: "ws://" + window.location.hostname + "/ws";
		ws = new WebSocket(url);
		ws.binaryType = "arraybuffer";
		if (onOpen) ws.onopen = () => onOpen(ws);
	}
	return ws;
}

// send LED colors to ESP using WebSocket and DDP protocol (RGB)
// ws: WebSocket object
// start: start pixel index
// len: number of pixels to send
// colors: Uint8Array with RGB values (3*len bytes)
function sendDDP(ws, start, len, colors) {
	if (!colors || colors.length < len * 3) return false; // not enough color data
	let maxDDPpx = 472; // must fit into one WebSocket frame of 1428 bytes, DDP header is 10+1 bytes -> 472 RGB pixels
	//let maxDDPpx = 172; // ESP8266: must fit into one WebSocket frame of 528 bytes -> 172 RGB pixels TODO: add support for ESP8266?
	if (!ws || ws.readyState !== WebSocket.OPEN) return false;
	// send in chunks of maxDDPpx
	for (let i = 0; i < len; i += maxDDPpx) {
		let cnt = Math.min(maxDDPpx, len - i);
		let off = (start + i) * 3; // DDP pixel offset in bytes
		let dLen = cnt * 3;
		let cOff = i * 3; // offset in color buffer
		let pkt = new Uint8Array(11 + dLen); // DDP header is 10 bytes, plus 1 byte for WLED websocket protocol indicator
		pkt[0] = 0x02; // DDP protocol indicator for WLED websocket. Note: below DDP protocol bytes are offset by 1
		pkt[1] = 0x40; // flags: 0x40 = no push, 0x41 = push (i.e. render), note: this is DDP protocol byte 0
		pkt[2] = 0x00; // reserved
		pkt[3] = 0x01; // 1 = RGB (currently only supported mode)
		pkt[4] = 0x01; // destination id (not used but 0x01 is default output)
		pkt[5] = (off >> 24) & 255; // DDP protocol 4-7 is offset
		pkt[6] = (off >> 16) & 255;
		pkt[7] = (off >> 8) & 255;
		pkt[8] = off & 255;
		pkt[9] = (dLen >> 8) & 255; // DDP protocol 8-9 is data length
		pkt[10] = dLen & 255;
		pkt.set(colors.subarray(cOff, cOff + dLen), 11);
		if(i + cnt >= len) {
			pkt[1] = 0x41;  //if this is last packet, set the "push" flag to render the frame
		}
		try {
			ws.send(pkt.buffer);
		} catch (e) {
			console.error(e);
			return false;
		}
	}
	return true;
}
