var d=document;
var loc = false, locip, locproto = "http:";

function H(pg="")   { window.open("https://kno.wled.ge/"+pg); }
function GH()       { window.open("https://github.com/Aircoookie/WLED"); }
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
function toggle(el) { gId(el).classList.toggle("hide"); gId('No'+el).classList.toggle("hide"); }
function tooltip(cont=null) {
	d.querySelectorAll((cont?cont+" ":"")+"[title]").forEach((element)=>{
		element.addEventListener("mouseover", ()=>{
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

		element.addEventListener("mouseout", ()=>{
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
