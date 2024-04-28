//page js
var loc = false, locip, locproto = "http:";
var noNewSegs = false;
var isOn = false, isInfo = false, isNodes = false, isRgbw = false, cct = false;
var whites = [0,0,0];
var selColors;
var powered = [true];
var selectedFx = 0;
var selectedPal = 0;
var csel = 0;
var currentPreset = -1;
var lastUpdate = 0;
var segCount = 0, ledCount = 0, lowestUnused = 0, maxSeg = 0, lSeg = 0;
var tr = 7;
var d = document;
var palettesData;
var fxdata = [];
var pJson = {}, eJson = {}, lJson = {};
var pN = "", pI = 0, pNum = 0;
var pmt = 1, pmtLS = 0, pmtLast = 0;
var lastinfo = {};
var ws, cpick, ranges;
var cfg = {
	theme:{base:"dark", bg:{url:""}, alpha:{bg:0.6,tab:0.8}, color:{bg:""}},
	comp :{colors:{picker: true, rgb: false, quick: true, hex: false}, labels:true, pcmbot:false, pid:true, seglen:false}
};
var hol = [
	[0,11,24,4,"https://aircoookie.github.io/xmas.png"], // christmas
	[0,2,17,1,"https://images.alphacoders.com/491/491123.jpg"], // st. Patrick's day
	[2022,3,17,2,"https://aircoookie.github.io/easter.png"],
	[2023,3,9,2,"https://aircoookie.github.io/easter.png"],
	[2024,2,31,2,"https://aircoookie.github.io/easter.png"]
];

function handleVisibilityChange() {if (!d.hidden && new Date () - lastUpdate > 3000) requestJson();}
function sCol(na, col) {d.documentElement.style.setProperty(na, col);}
function gId(c) {return d.getElementById(c);}
function gEBCN(c) {return d.getElementsByClassName(c);}
function isEmpty(o) {return Object.keys(o).length === 0;}
function isObj(i) { return (i && typeof i === 'object' && !Array.isArray(i)); }

function applyCfg()
{
	cTheme(cfg.theme.base === "light");
	var bg = cfg.theme.color.bg;
	if (bg) sCol('--c-1', bg);
	var ccfg = cfg.comp.colors;
	//gId('picker').style.display = "none"; // ccfg.picker ? "block":"none";
	//gId('vwrap').style.display = "none"; // ccfg.picker ? "block":"none";
	//gId('rgbwrap').style.display = ccfg.rgb ? "block":"none";
	gId('qcs-w').style.display = ccfg.quick ? "block":"none";
	var l = cfg.comp.labels; //l = false;
	var e = d.querySelectorAll('.tab-label');
	for (var i=0; i<e.length; i++) e[i].style.display = l ? "block":"none";
	e = d.querySelectorAll('.label');
	for (var i=0; i<e.length; i++) e[i].style.display = l ? "block":"none";
	e = d.querySelector('.hd');
	e.style.display = l ? "block":"none";
	//sCol('--tbp',l ? "14px 14px 10px 14px":"10px 22px 4px 22px");
	sCol('--bbp',l ? "9px 0 7px 0":"10px 0 4px 0");
	sCol('--bhd',l ? "block":"none");
	sCol('--bmt',l ? "0px":"5px");
	sCol('--t-b', cfg.theme.alpha.tab);
	size();
	localStorage.setItem('wledUiCfg', JSON.stringify(cfg));
}

function tglTheme()
{
	cfg.theme.base = (cfg.theme.base === "light") ? "dark":"light";
	applyCfg();
}

function tglLabels()
{
	cfg.comp.labels = !cfg.comp.labels;
	applyCfg();
}

function cTheme(light) {
	if (light) {
	sCol('--c-1','#eee');
	sCol('--c-f','#000');
	sCol('--c-2','#ddd');
	sCol('--c-3','#bbb');
	sCol('--c-4','#aaa');
	sCol('--c-5','#999');
	sCol('--c-6','#999');
	sCol('--c-8','#888');
	sCol('--c-b','#444');
	sCol('--c-c','#333');
	sCol('--c-e','#111');
	sCol('--c-d','#222');
	sCol('--c-r','#c21');
	sCol('--c-g','#2c1');
	sCol('--c-l','#26c');
	sCol('--c-o','rgba(204, 204, 204, 0.9)');
	sCol('--c-sb','#0003'); sCol('--c-sbh','#0006');
	sCol('--c-tb','rgba(204, 204, 204, var(--t-b))');
	sCol('--c-tba','rgba(170, 170, 170, var(--t-b))');
	sCol('--c-tbh','rgba(204, 204, 204, var(--t-b))');
	gId('imgw').style.filter = "invert(0.8)";
	} else {
	sCol('--c-1','#111');
	sCol('--c-f','#fff');
	sCol('--c-2','#222');
	sCol('--c-3','#333');
	sCol('--c-4','#444');
	sCol('--c-5','#555');
	sCol('--c-6','#666');
	sCol('--c-8','#888');
	sCol('--c-b','#bbb');
	sCol('--c-c','#ccc');
	sCol('--c-e','#eee');
	sCol('--c-d','#ddd');
	sCol('--c-r','#e42');
	sCol('--c-g','#4e2');
	sCol('--c-l','#48a');
	sCol('--c-o','rgba(34, 34, 34, 0.9)');
	sCol('--c-sb','#fff3'); sCol('--c-sbh','#fff5');
	sCol('--c-tb','rgba(34, 34, 34, var(--t-b))');
	sCol('--c-tba','rgba(102, 102, 102, var(--t-b))');
	sCol('--c-tbh','rgba(51, 51, 51, var(--t-b))');
	gId('imgw').style.filter = "unset";
	}
}

function loadBg(iUrl)
{
	let bg = document.getElementById('bg');
	let img = document.createElement("img");
	img.src = iUrl;
	img.addEventListener('load', (event) => {
		var a = parseFloat(cfg.theme.alpha.bg);
		if (isNaN(a)) a = 0.6;
		bg.style.opacity = a;
		bg.style.backgroundImage = `url(${img.src})`;
		img = null;
	});
}

function loadSkinCSS(cId)
{
	if (!gId(cId))	// check if element exists
	{
		var h  = document.getElementsByTagName('head')[0];
		var l  = document.createElement('link');
		l.id   = cId;
		l.rel  = 'stylesheet';
		l.type = 'text/css';
		l.href = getURL('/skin.css');
		l.media = 'all';
		h.appendChild(l);
	}
}

function getURL(path) {
	return (loc ? locproto + "//" + locip : "") + path;
}
async function onLoad()
{
	let l = window.location;
	if (l.protocol == "file:") {
		loc = true;
		locip = localStorage.getItem('locIp');
		if (!locip) {
			locip = prompt("File Mode. Please enter WLED IP!");
			localStorage.setItem('locIp', locip);
		}
	} else {
		// detect reverse proxy and/or HTTPS
		let pathn = l.pathname;
		let paths = pathn.slice(1,pathn.endsWith('/')?-1:undefined).split("/");
		if (paths[0]==="sliders") paths.shift();
		//while (paths[0]==="") paths.shift();
		locproto = l.protocol;
		locip = l.hostname + (l.port ? ":" + l.port : "");
		if (paths.length > 0 && paths[0]!=="") {
			loc = true;
			locip +=  "/" + paths[0];
		} else if (locproto==="https:") {
			loc = true;
		}
	}
	var sett = localStorage.getItem('wledUiCfg');
	if (sett) cfg = mergeDeep(cfg, JSON.parse(sett));

	makeWS();

	applyCfg();
	if (cfg.theme.bg.url=="" || cfg.theme.bg.url === "https://picsum.photos/1920/1080") {
		var iUrl = cfg.theme.bg.url;
		fetch(getURL("/holidays.json"), {
			method: 'get'
		})
		.then((res)=>{
			return res.json();
		})
		.then((json)=>{
			if (Array.isArray(json)) hol = json;
			//TODO: do some parsing first
		})
		.catch((e)=>{
			console.log("holidays.json does not contain array of holidays. Defaults loaded.");
		})
		.finally(()=>{
			var today = new Date();
			for (var i=0; i<hol.length; i++) {
				var yr = hol[i][0]==0 ? today.getFullYear() : hol[i][0];
				var hs = new Date(yr,hol[i][1],hol[i][2]);
				var he = new Date(hs);
				he.setDate(he.getDate() + hol[i][3]);
				if (today>=hs && today<he) iUrl = hol[i][4];
			}
			if (iUrl !== "") loadBg(iUrl);
		});
	} else
		loadBg(cfg.theme.bg.url);
	loadSkinCSS('skinCss');

	var cd = gId('csl').children;
	for (var i = 0; i < cd.length; i++) cd[i].style.backgroundColor = "rgb(0, 0, 0)";
	selectSlot(0);
	cpick.on("input:end", ()=>{
		setColor(1);
	});
	pmtLS = localStorage.getItem('wledPmt');

	// Load initial data
	loadPalettes(()=>{
		loadPalettesData(redrawPalPrev);
		loadFX(()=>{
			loadFXData();
			loadPresets(()=>{
				requestJson();
			});
		});
	});

	d.addEventListener("visibilitychange", handleVisibilityChange, false);
	size();
	gId("cv").style.opacity=0;
	var sls = d.querySelectorAll('input[type="range"]');
	for (var sl of sls) {
		sl.addEventListener('touchstart', toggleBubble);
		sl.addEventListener('touchend', toggleBubble);
	}
}

var timeout;
function showToast(text, error = false)
{
	if (error) gId('connind').style.backgroundColor = "var(--c-r)";
	var x = gId("toast");
	x.innerHTML = text;
	x.className = error ? "error":"show";
	clearTimeout(timeout);
	x.style.animation = 'none';
	timeout = setTimeout(()=>{ x.classList.remove("show"); }, 2900);
	if (error) console.log(text);
}

function showErrorToast()
{
	if (ws && ws.readyState === WebSocket.OPEN) {
		// if we received a timeout force WS reconnect
		ws.close();
		ws = null;
		if (lastinfo.ws > -1) setTimeout(makeWS,500);
	}
	showToast('Connection to light failed!', true);
}

function clearErrorToast() {gId("toast").className = gId("toast").className.replace("error", "");}

function getRuntimeStr(rt)
{
	var t = parseInt(rt);
	var days = Math.floor(t/86400);
	var hrs = Math.floor((t - days*86400)/3600);
	var mins = Math.floor((t - days*86400 - hrs*3600)/60);
	var str = days ? (days + " " + (days == 1 ? "day" : "days") + ", ") : "";
	str += (hrs || days) ? (hrs + " " + (hrs == 1 ? "hour" : "hours")) : "";
	if (!days && hrs) str += ", ";
	if (t > 59 && !days) str += mins + " min";
	if (t < 3600 && t > 59) str += ", ";
	if (t < 3600) str += (t - mins*60) + " sec";
	return str;
}

function inforow(key, val, unit = "")
{
	return `<tr><td class="keytd">${key}</td><td class="valtd">${val}${unit}</td></tr>`;
}

function pName(i)
{
	var n = "Preset " + i;
	if (pJson && pJson[i] && pJson[i].n) n = pJson[i].n;
	return n;
}

function isPlaylist(i)
{
	return pJson[i].playlist && pJson[i].playlist.ps;
}

function papiVal(i)
{
	if (!pJson || !pJson[i]) return "";
	var o = Object.assign({},pJson[i]);
	if (o.win) return o.win;
	delete o.n; delete o.p; delete o.ql;
	return JSON.stringify(o);
}

function qlName(i)
{
	if (!pJson || !pJson[i] || !pJson[i].ql) return "";
	return pJson[i].ql;
}

function cpBck()
{
	var copyText = gId("bck");

	copyText.select();
	copyText.setSelectionRange(0, 999999);
	d.execCommand("copy");
	showToast("Copied to clipboard!");
}

function loadPresets(callback = null)
{
	//1st boot (because there is a callback)
	if (callback && pmt == pmtLS && pmt > 0) {
		//we have a copy of the presets in local storage and don't need to fetch another one
        pJson = JSON.parse(localStorage.getItem("wledP"));
		populatePresets();
		pmtLast = pmt;
		callback();
		return;
	}

	//afterwards
	if (!callback && pmt == pmtLast) return;

	pmtLast = pmt;

	fetch(getURL('/presets.json'), {
		method: 'get'
	})
	.then(res => {
		if (!res.ok) showErrorToast();
		return res.json();
	})
	.then(json => {
		clearErrorToast();
		pJson = json;
		populatePresets();
	})
	.catch(function (error) {
		showToast(error, true);
		console.log(error);
	})
	.finally(()=>{
		if (callback) setTimeout(callback,99);
	});
}

function loadPalettes(callback = null)
{
	fetch(getURL('/json/palettes'), {
		method: 'get'
	})
	.then(res => {
		if (!res.ok) showErrorToast();
		return res.json();
	})
	.then(json => {
		clearErrorToast();
		lJson = Object.entries(json);
		populatePalettes();
	})
	.catch(function (error) {
		showToast(error, true);
	})
	.finally(()=>{
		if (callback) callback();
	});
}

function loadFX(callback = null)
{
	fetch(getURL('/json/effects'), {
		method: 'get'
	})
	.then(res => {
		if (!res.ok) showErrorToast();
		return res.json();
	})
	.then(json => {
		clearErrorToast();
		eJson = Object.entries(json);
		populateEffects();
	})
	.catch(function (error) {
		showToast(error, true);
	})
	.finally(()=>{
		if (callback) callback();
	});
}

function loadFXData(callback = null)
{
	fetch(getURL('/json/fxdata'), {
		method: 'get'
	})
	.then(res => {
		if (!res.ok) showErrorToast();
		return res.json();
	})
	.then(json => {
		clearErrorToast();
		fxdata = json||[];
		// add default value for Solid
		fxdata.shift()
		fxdata.unshift("@;!;");
	})
	.catch(function (error) {
		fxdata = [];
		showToast(error, true);
	})
	.finally(()=>{
		if (callback) callback();
		updateUI();
	});
}

var pQL = [];
function populateQL()
{
	var cn = "";
	if (pQL.length > 0) {
		pQL.sort((a,b) => (a[0]>b[0]));
		for (var key of (pQL||[])) {
			cn += `<button class="btn btn-xs psts" id="p${key[0]}qlb" title="${key[2]?key[2]:''}" onclick="setPreset(${key[0]});">${key[1]}</button>`;
		}
	}
	gId('pql').innerHTML = cn;
}

function populatePresets()
{
	if (!pJson) {pJson={};return};
	delete pJson["0"];
	var cn = ""; //`<p class="label">All presets</p>`;
	var arr = Object.entries(pJson);
	arr.sort(cmpP);
	pQL = [];
	var is = [];
	pNum = 0;
	for (var key of (arr||[]))
	{
		if (!isObj(key[1])) continue;
		let i = parseInt(key[0]);
		var qll = key[1].ql;
		if (qll) pQL.push([i, qll, pName(i)]);
		is.push(i);

		cn += `<div class="lstI c pres" id="p${i}o" onclick="setPreset(${i})">`;
		//if (cfg.comp.pid) cn += `<div class="pid">${i}</div>`;
		cn += `${isPlaylist(i)?"<i class='icons btn-icon'>&#xe139;</i>":""}<span class="lstIname">${pName(i)}</span></div>`;
    	pNum++;
	}
	gId('pcont').innerHTML = cn;
	updatePA();
	populateQL();
}

function parseInfo() {
	var li   = lastinfo;
	var name = li.name;
	gId('namelabel').innerHTML = name;
//		if (name === "Dinnerbone") d.documentElement.style.transform = "rotate(180deg)";
	if (li.live) name = "(Live) " + name;
	if (loc)     name = "(L) " + name;
	d.title     = name;
	isRgbw      = li.leds.wv;
	ledCount    = li.leds.count;
	syncTglRecv = li.str;
	maxSeg      = li.leds.maxseg;
	pmt         = li.fs.pmt;
	cct         = li.leds.cct;
}

function populateInfo(i)
{
	var cn="";
	var heap = i.freeheap/1000;
	heap = heap.toFixed(1);
	var pwr = i.leds.pwr;
	var pwru = "Not calculated";
	if (pwr > 1000) {pwr /= 1000; pwr = pwr.toFixed((pwr > 10) ? 0 : 1); pwru = pwr + " A";}
	else if (pwr > 0) {pwr = 50 * Math.round(pwr/50); pwru = pwr + " mA";}
  	var urows="";
	if (i.u) {
		for (const [k, val] of Object.entries(i.u)) {
			if (val[1])
				urows += inforow(k,val[0],val[1]);
			else
				urows += inforow(k,val);
		}
	}
	var vcn = "Kuuhaku";
	if (i.ver.startsWith("0.14.")) vcn = "Hoshi";
	if (i.ver.includes("-bl")) vcn = "Supāku";
	if (i.cn) vcn = i.cn;

	cn += `v${i.ver} "${vcn}"<br><br><table>
${urows}
${inforow("Build",i.vid)}
${inforow("Signal strength",i.wifi.signal +"% ("+ i.wifi.rssi, " dBm)")}
${inforow("Uptime",getRuntimeStr(i.uptime))}
${inforow("Time",i.time)}
${inforow("Free heap",heap," kB")}
${i.psram?inforow("Free PSRAM",(i.psram/1024).toFixed(1)," kB"):""}
${inforow("Estimated current",pwru)}
${inforow("Average FPS",i.leds.fps)}
${inforow("MAC address",i.mac)}
${inforow("Filesystem",i.fs.u + "/" + i.fs.t + " kB (" +Math.round(i.fs.u*100/i.fs.t) + "%)")}
${inforow("Environment",i.arch + " " + i.core + " (" + i.lwip + ")")}
</table>`;
	gId('kv').innerHTML = cn;
}

function populateSegments(s)
{
	var cn = "";
	segCount = (s.seg||[]).length;
	lowestUnused = 0; lSeg = 0;

	if (segCount > 1) {
		for (var y = 0; y < segCount && y<4; y++)
		{
			var inst=s.seg[y];
			let i = parseInt(inst.id);
			powered[i] = inst.on;
			if (i == lowestUnused) lowestUnused = i+1;
			if (i > lSeg) lSeg = i;

			cn +=
`<div class="label h">${(inst.n&&inst.n!=='')?inst.n:('Segment '+y)}</div>
<div>
	<label class="check schkl">
		&nbsp;
		<input type="checkbox" id="seg${i}sel" onchange="selSeg(${i})" ${inst.sel ? "checked":""}>
		<span class="checkmark schk"></span>
	</label>
	<div class="il">
		<i class="icons slider-icon pwr ${powered[i] ? "act":""}" id="seg${i}pwr" onclick="setSegPwr(${i})" title="${inst.n}">&#xe08f;</i>
		<div id="sliderSeg${i}Bri" class="sliderwrap il">
			<input id="seg${i}bri" onchange="setSegBri(${i})" oninput="updateTrail(this)" max="255" min="1" type="range" value="${inst.bri}" />
			<div class="sliderdisplay"></div>
		</div>
		<output class="sliderbubble"></output>
	</div>
</div>`;
		}
		//if (gId('buttonBri').className !== 'active') tglBri(true);
	} else {
		//tglBri(false);
	}
	//gId('buttonBri').style.display = (segCount > 1) ? "block" : "none";
	gId('segcont').innerHTML = cn;
	for (var i = 0; i < segCount && i<4; i++) updateTrail(gId(`seg${i}bri`));
}

function btype(b)
{
	switch (b) {
		case 2:
		case 32: return "ESP32";
		case 1:
		case 82: return "ESP8266";
	}
	return "?";
}

function bname(o)
{
	if (o.name=="WLED") return o.ip;
	return o.name;
}

function populateNodes(i,n)
{
	var cn="";
	var urows="";
	var nnodes = 0;
	if (n.nodes) {
		n.nodes.sort((a,b) => (a.name).localeCompare(b.name));
		for (var x=0;x<n.nodes.length;x++) {
			var o = n.nodes[x];
			if (o.name) {
				var url = `<button class="btn tab" title="${o.ip}" onclick="location.assign('http://${o.ip}');">${bname(o)}</button>`;
				urows += inforow(url,`${btype(o.type)}<br><i>${o.vid==0?"N/A":o.vid}</i>`);
				nnodes++;
			}
		}
	}
	if (i.ndc < 0) cn += `Instance List is disabled.`;
	else if (nnodes == 0) cn += `No other instances found.`;
	cn += `<table class="infot">
	${urows}
	${inforow("Current instance:",i.name)}
	</table>`;
	gId('kn').innerHTML = cn;
}

function loadNodes()
{
	fetch(getURL('/json/nodes'), {
		method: 'get'
	})
	.then(res => {
		if (!res.ok) showToast('Could not load Node list!', true);
		return res.json();
	})
	.then(json => {
		clearErrorToast();
		populateNodes(lastinfo, json);
	})
	.catch(function (error) {
		showToast(error, true);
		console.log(error);
	});
}

function populateEffects()
{
	var effects = eJson;
	var html = "";

	effects.shift(); //remove solid
	for (let i = 0; i < effects.length; i++) effects[i] = {id: effects[i][0], name:effects[i][1]};
	effects.sort((a,b) => (a.name).localeCompare(b.name));
	effects.unshift({
		"id": 0,
		"name": "Solid@;!;0"
	});

	for (let i = 0; i < effects.length; i++) {
		// WLEDSR: add slider and color control to setEffect (used by requestjson)
		if (effects[i].name.indexOf("RSVD") < 0) {
			var posAt = effects[i].name.indexOf("@");
			var extra = '';
			if (posAt > 0)
				extra = effects[i].name.substr(posAt);
			else
				posAt = 999;
			html += generateListItemHtml(
				'fx',
				effects[i].id,
				effects[i].name.substr(0,posAt),
				'setEffect',
				'','',
				extra
			);
		}
	}
	gId('fxlist').innerHTML=html;
}

function populatePalettes()
{
	var palettes = lJson;
	palettes.shift(); //remove default
	for (let i = 0; i < palettes.length; i++) {
		palettes[i] = {
			"id": palettes[i][0],
			"name": palettes[i][1]
		};
	}
	palettes.sort((a,b) => (a.name).localeCompare(b.name));
	palettes.unshift({
		"id": 0,
		"name": "Default",
	});
	var html = "";
	for (let i = 0; i < palettes.length; i++) {
		html += generateListItemHtml(
			'palette',
		    palettes[i].id,
            palettes[i].name,
            'setPalette',
			`<div class="lstIprev"></div>`
        );
	}
	gId('pallist').innerHTML=html;
}

function redrawPalPrev()
{
	let palettes = d.querySelectorAll('#pallist .lstI');
	for (let i = 0; i < palettes.length; i++) {
		let id = palettes[i].dataset.id;
		let lstPrev = palettes[i].querySelector('.lstIprev');
		if (lstPrev) {
			lstPrev.style = genPalPrevCss(id);
		}
	}
}

function genPalPrevCss(id)
{
	if (!palettesData) return;

	var paletteData = palettesData[id];
	var previewCss = "";

	if (!paletteData) return 'display: none';

	// We need at least two colors for a gradient
	if (paletteData.length == 1) {
		paletteData[1] = paletteData[0];
		if (Array.isArray(paletteData[1])) {
			paletteData[1][0] = 255;
		}
	}

	var gradient = [];
	for (let j = 0; j < paletteData.length; j++) {
		const element = paletteData[j];
		let r;
		let g;
		let b;
		let index = false;
		if (Array.isArray(element)) {
			index = element[0]/255*100;
			r = element[1];
			g = element[2];
			b = element[3];
		} else if (element == 'r') {
			r = Math.random() * 255;
			g = Math.random() * 255;
			b = Math.random() * 255;
		} else {
			if (selColors) {
				let e = element[1] - 1;
				r = selColors[e][0];
				g = selColors[e][1];
				b = selColors[e][2];
			}
		}
		if (index === false) {
			index = j / paletteData.length * 100;
		}

		gradient.push(`rgb(${r},${g},${b}) ${index}%`);
	}

	return `background: linear-gradient(to right,${gradient.join()});`;
}

function generateOptionItemHtml(id, name)
{
    return `<option value="${id}">${name}</option>`;
}

function generateListItemHtml(listName, id, name, clickAction, extraHtml = '', extraClass = '', extraPar = '')
{
    return `<div class="lstI ${extraClass}" data-id="${id}" data-opt="${extraPar}" onClick="${clickAction}(${id})">
	<div class="lstIcontent">
		<span class="lstIname">
			${name}
		</span>
	</div>
	${extraHtml}
</div>`;
}

//update the 'sliderdisplay' background div of a slider for a visual indication of slider position
function updateTrail(e)
{
	if (e==null) return;
	var max = e.hasAttribute('max') ? e.attributes.max.value : 255;
	var perc = e.value * 100 / max;
	perc = parseInt(perc);
	if (perc < 50) perc += 2;
	var val = `linear-gradient(90deg, var(--c-f) ${perc}%, var(--c-4) ${perc}%)`;
	e.parentNode.getElementsByClassName('sliderdisplay')[0].style.background = val;
	var b = e.parentNode.parentNode.getElementsByTagName('output')[0];
	if (b) b.innerHTML = e.value;
}

//rangetouch slider function
function toggleBubble(e)
{
	var b = e.target.parentNode.parentNode.getElementsByTagName('output')[0];
	b.classList.toggle('sliderbubbleshow');
}

function updatePA()
{
	var ps = gEBCN("pres");
	for (let i = 0; i < ps.length; i++) {
		ps[i].classList.remove('selected');;
	}
	ps = gEBCN("psts");
	for (let i = 0; i < ps.length; i++) {
		ps[i].classList.remove('selected');;
	}
	if (currentPreset > 0) {
        var acv = gId(`p${currentPreset}o`);
		if (acv) acv.classList.add('selected');
		acv = gId(`p${currentPreset}qlb`);
		if (acv) acv.classList.add('selected');
    }
}

function updateUI()
{
	gId('buttonPower').className = (isOn) ? "active":"";

	var sel = 0;
	if (lJson && lJson.length) {
		for (var i=0; i<lJson.length; i++) if (lJson[i].id == selectedPal) {sel = i; break;}
		gId('palBtn').innerHTML = '<i class="icons">&#xe2b3;</i> ' + lJson[sel].name;
	}
	sel = 0;
	if (eJson && eJson.length) {
		for (var i=0; i<eJson.length; i++) if (eJson[i].id == selectedFx) {sel = i; break;}
		var posAt = eJson[sel].name.indexOf("@");
		if (posAt<=0) posAt=999;
		gId('fxBtn').innerHTML = '<i class="icons">&#xe0e8;</i> ' + eJson[sel].name.substr(0,posAt);
	}

	updateTrail(gId('sliderBri'));
	updateTrail(gId('sliderSpeed'));
	updateTrail(gId('sliderIntensity'));

	gId('wwrap').style.display = (isRgbw) ? "block":"none";
	gId("wbal").style.display = (cct) ? "block":"none";
	gId('kwrap').style.display = (cct) ? "none":"block";

	updatePA();
	redrawPalPrev();
	updatePSliders();

	var l = cfg.comp.labels; //l = false;
	var e = d.querySelectorAll('.label');
	for (var i=0; i<e.length; i++) e[i].style.display = l ? "block":"none";
}

function cmpP(a, b)
{
	if (!a[1].n) return (a[0] > b[0]);
	// playlists follow presets
	var name = (a[1].playlist ? '~' : ' ') + a[1].n;
	return name.localeCompare((b[1].playlist ? '~' : ' ') + b[1].n, undefined, {numeric: true});
}

function makeWS() {
	if (ws) return;
	let url = loc ? getURL('/ws').replace("http","ws") : "ws://"+window.location.hostname+"/ws";
	ws = new WebSocket(url);
	ws.onmessage = (e)=>{
		var json = JSON.parse(e.data);
		if (json.leds) return; //liveview packet
		clearTimeout(jsonTimeout);
		jsonTimeout = null;
		lastUpdate = new Date();
		clearErrorToast();
	  	gId('connind').style.backgroundColor = "var(--c-l)";
		// json object should contain json.info AND json.state (but may not)
		var i = json.info;
		if (i) {
			lastinfo = i;
			parseInfo();
			if (isInfo) populateInfo(i);
		} else
			i = lastinfo;
		var s = json.state ? json.state : json;
		readState(s);
	};
	ws.onclose = (e)=>{
		gId('connind').style.backgroundColor = "var(--c-r)";
		ws = null;
		if (lastinfo.ws > -1) setTimeout(makeWS,500);
	}
	ws.onopen = (e)=>{
		ws.send("{'v':true}");
		reqsLegal = true;
		clearErrorToast();
	}
}

function readState(s,command=false)
{
	if (!s) return false;

	isOn = s.on;
	gId('sliderBri').value= s.bri;
	nlA = s.nl.on;
	nlDur = s.nl.dur;
	nlTar = s.nl.tbri;
	nlFade = s.nl.fade;
	syncSend = s.udpn.send;
	if (s.pl<0)	currentPreset = s.ps;
	else currentPreset = s.pl;
	tr = s.transition/10;

	var selc=0; var ind=0;
	populateSegments(s);
	for (let i = 0; i < (s.seg||[]).length; i++)
	{
		if(s.seg[i].sel) {selc = ind; break;} ind++;
	}
	var i=s.seg[selc];
	if (!i) {
		showToast('No Segments!', true);
		updateUI();
		return;
	}
  
	selColors = i.col;
	var cd = gId('csl').children;
	for (let e = cd.length-1; e >= 0; e--)
	{
		var r,g,b,w;
		r = i.col[e][0];
		g = i.col[e][1];
		b = i.col[e][2];
		if (isRgbw) w = i.col[e][3];
		cd[e].style.backgroundColor = "rgb(" + r + "," + g + "," + b + ")";
		if (isRgbw) whites[e] = parseInt(w);
		selectSlot(csel);
	}
	gId('sliderW').value = whites[csel];
	if (i.cct && i.cct>=0) gId("sliderA").value = i.cct;

	gId('sliderSpeed').value = i.sx;
	gId('sliderIntensity').value = i.ix;
/*
	gId('sliderC1').value  = i.f1x ? i.f1x : 0;
	gId('sliderC2').value  = i.f2x ? i.f2x : 0;
	gId('sliderC3').value  = i.f3x ? i.f3x : 0;
*/
	if (s.error && s.error != 0) {
	  var errstr = "";
	  switch (s.error) {
		case 10:
		  errstr = "Could not mount filesystem!";
		  break;
		case 11:
		  errstr = "Not enough space to save preset!";
		  break;
		case 12:
		  errstr = "Preset not found.";
		  break;
		case 13:
		  errstr = "Missing IR.json.";
		  break;
		case 19:
		  errstr = "A filesystem error has occurred.";
		  break;
		}
	  showToast('Error ' + s.error + ": " + errstr, true);
	}

	selectedPal = i.pal;
	selectedFx = i.fx;
	updateUI();
}

var jsonTimeout;
var reqsLegal = false;

function requestJson(command=null)
{
	gId('connind').style.backgroundColor = "var(--c-r)";
	if (command && !reqsLegal) return; //stop post requests from chrome onchange event on page restore
	if (!jsonTimeout) jsonTimeout = setTimeout(showErrorToast, 3000);
	var req = null;
	var useWs = (ws && ws.readyState === WebSocket.OPEN);
	var type = command ? 'post':'get';
	if (command) {
		if (useWs || !command.ps) command.v = true; // force complete /json/si API response
		command.time = Math.floor(Date.now() / 1000);
		req = JSON.stringify(command);
		if (req.length > 1000) useWs = false; //do not send very long requests over websocket
	};

	if (useWs) {
		ws.send(req?req:'{"v":true}');
		return;
	} else if (command && command.ps) { //refresh UI if we don't use WS (async loading of presets)
		setTimeout(requestJson,200);
	}

	fetch(getURL('/json/si'), {
		method: type,
		headers: {
			"Content-type": "application/json; charset=UTF-8"
		},
		body: req
	})
	.then(res => {
		if (!res.ok) showErrorToast();
		return res.json();
	})
	.then(json => {
		clearTimeout(jsonTimeout);
		jsonTimeout = null;
		lastUpdate = new Date();
		clearErrorToast();
		gId('connind').style.backgroundColor = "var(--c-g)";
		if (!json) { showToast('Empty response', true); return; }
		if (json.success) return;
		if (json.info) {
			lastinfo = json.info;
			parseInfo();
			if (isInfo) populateInfo(lastinfo);
		}
		var s = json.state ? json.state : json;
		readState(s);
		reqsLegal = true;
	})
	.catch(function (error) {
		showToast(error, true);
		console.log(error);
	});
}

function togglePower()
{
	isOn = !isOn;
	var obj = {"on": isOn};
	requestJson(obj);
}

function toggleInfo()
{
	if (isNodes) toggleNodes();
	isInfo = !isInfo;
	if (isInfo) requestJson();
	gId('info').style.transform = (isInfo) ? "translateY(0px)":"translateY(100%)";
	gId('buttonI').className = (isInfo) ? "active":"";
}

function toggleNodes()
{
	if (isInfo) toggleInfo();
	isNodes = !isNodes;
	if (isNodes) loadNodes();
	gId('nodes').style.transform = (isNodes) ? "translateY(0px)":"translateY(100%)";
	gId('buttonNodes').className = (isNodes) ? "active":"";
}
/*
function tglBri(b=null)
{
	if (b===null) b = gId(`briwrap`).style.display === "block";
	gId('briwrap').style.display = !b ? "block":"none";
	gId('buttonBri').className = !b ? "active":"";
	size();
}
*/
function tglCP()
{
	var p = gId('buttonCP').className === "active";
	gId('buttonCP').className = !p ? "active":"";
	gId('picker').style.display = !p ? "block":"none";
	gId('vwrap').style.display = !p ? "block":"none";
	gId('rgbwrap').style.display = !p ? "block":"none";
	var csl = gId('Slots').style.display === "block";
	gId('Slots').style.display = !csl ? "block":"none";
	//var ps = gId(`Presets`).style.display === "block";
	//gId('Presets').style.display = !ps ? "block":"none";
}

function tglCs(i)
{
	var pss = gId(`p${i}cstgl`).checked;
	gId(`p${i}o1`).style.display = pss? "block" : "none";
	gId(`p${i}o2`).style.display = !pss? "block" : "none";
}

function selSeg(s)
{
	var sel = gId(`seg${s}sel`).checked;
	var obj = {"seg": {"id": s, "sel": sel}};
	requestJson(obj);
}

function tglPalDropdown()
{
	var p = gId('palDropdown').style;
	p.display = (p.display==='block'?'none':'block');
	gId('fxDropdown').style.display = 'none';
	if (p.display==='block')
		gId('palDropdown').scrollIntoView({
			behavior: 'smooth',
			block: 'center',
		});
}

function tglFxDropdown()
{
	var p = gId('fxDropdown').style;
	p.display = (p.display==='block'?'none':'block');
	gId('palDropdown').style.display = 'none';
	if (p.display==='block')
		gId('fxDropdown').scrollIntoView({
			behavior: 'smooth',
			block: 'center',
		});
}

function setSegPwr(s)
{
	var obj = {"seg": {"id": s, "on": !powered[s]}};
	requestJson(obj);
}

function setSegBri(s)
{
	var obj = {"seg": {"id": s, "bri": parseInt(gId(`seg${s}bri`).value)}};
	requestJson(obj);
}

function setEffect(ind = 0)
{
	tglFxDropdown();
	var obj = {"seg": {"fx": parseInt(ind), "fxdef":true}}; // fxdef sets effect parameters to default values, TODO add client setting
	requestJson(obj);
}

function setPalette(paletteId = null)
{
	tglPalDropdown();
	var obj = {"seg": {"pal": paletteId}};
	requestJson(obj);
}

function setBri()
{
	var obj = {"bri": parseInt(gId('sliderBri').value)};
	requestJson(obj);
}

function setSpeed()
{
	var obj = {"seg": {"sx": parseInt(gId('sliderSpeed').value)}};
	requestJson(obj);
}

function setIntensity()
{
	var obj = {"seg": {"ix": parseInt(gId('sliderIntensity').value)}};
	requestJson(obj);
}

function setLor(i)
{
	var obj = {"lor": i};
	requestJson(obj);
}

function setPreset(i)
{
	var obj = {"ps": i};
	if (isPlaylist(i)) obj.on = true;
	showToast("Loading preset " + pName(i) +" (" + i + ")");
	requestJson(obj);
}

function selectSlot(b)
{
	csel = b;
	var cd = gId('csl').children;
	for (let i = 0; i < cd.length; i++) cd[i].classList.remove('xxs-w');
	cd[b].classList.add('xxs-w');
	setPicker(cd[b].style.backgroundColor);
	gId('sliderW').value = whites[b];
	redrawPalPrev();
	updatePSliders();
}

var lasth = 0;
function pC(col)
{
	if (col == "rnd") {
		col = {h: 0, s: 0, v: 100};
		col.s = Math.floor((Math.random() * 50) + 50);
		do {
			col.h = Math.floor(Math.random() * 360);
		} while (Math.abs(col.h - lasth) < 50);
		lasth = col.h;
	}
	setPicker(col);
	setColor(0);
}

function updatePSliders() {
	//update RGB sliders
	var col = cpick.color.rgb;
	gId('sliderR').value = col.r;
	gId('sliderG').value = col.g;
	gId('sliderB').value = col.b;

	//update hex field
	var str = cpick.color.hexString.substring(1);
	var w = whites[csel];
	if (w > 0) str += w.toString(16);

	//update value slider
	var v = gId('sliderV');
	v.value = cpick.color.value;
	//background color as if color had full value
	var hsv = {"h":cpick.color.hue,"s":cpick.color.saturation,"v":100}; 
	var c = iro.Color.hsvToRgb(hsv);
	var cs = 'rgb('+c.r+','+c.g+','+c.b+')';
	v.nextElementSibling.style.backgroundImage = `linear-gradient(90deg, #000 0%, ${cs})`;

	//update Kelvin slider
	gId('sliderK').value = cpick.color.kelvin;
}

function setPicker(rgb) {
	var c = new iro.Color(rgb);
	if (c.value > 0) cpick.color.set(c);
	else cpick.color.setChannel('hsv', 'v', 0);
}

function fromV()
{
	cpick.color.setChannel('hsv', 'v', d.getElementById('sliderV').value);
}

function fromK()
{
	cpick.color.set({ kelvin: d.getElementById('sliderK').value });
}

function fromRgb()
{
	var r = gId('sliderR').value;
	var g = gId('sliderG').value;
	var b = gId('sliderB').value;
	setPicker(`rgb(${r},${g},${b})`);
	setColor(0);
}

// sets color from picker: 0=all, 1=leaving picker/HSV, 2=ignore white channel
function setColor(sr)
{
	var cd = gId('csl').children; // color slots
	if (sr == 1 && cd[csel].style.backgroundColor == 'rgb(0, 0, 0)') cpick.color.setChannel('hsv', 'v', 100);
	cd[csel].style.backgroundColor = cpick.color.rgbString;
	if (sr != 2) whites[csel] = parseInt(gId('sliderW').value);
	var col = cpick.color.rgb;
	var obj = {"seg": {"col": [[col.r, col.g, col.b, whites[csel]],[],[]]}};
	if (sr==1 || gId(`picker`).style.display !== "block") obj.seg.fx = 0;
	if (csel == 1) {
		obj = {"seg": {"col": [[],[col.r, col.g, col.b, whites[csel]],[]]}};
	} else if (csel == 2) {
		obj = {"seg": {"col": [[],[],[col.r, col.g, col.b, whites[csel]]]}};
	}
	requestJson(obj);
}

function setBalance(b)
{
	var obj = {"seg": {"cct": parseInt(b)}};
	requestJson(obj);
}

var hc = 0;
setInterval(()=>{if (!isInfo) return; hc+=18; if (hc>300) hc=0; if (hc>200)hc=306; if (hc==144) hc+=36; if (hc==108) hc+=18;
gId('heart').style.color = `hsl(${hc}, 100%, 50%)`;}, 910);

function openGH() { window.open("https://github.com/Aircoookie/WLED/wiki"); }

var cnfr = false;
function cnfReset()
{
	if (!cnfr) {
		var bt = gId('resetbtn');
		bt.style.color = "#f00";
		bt.innerHTML = "Confirm Reboot";
		cnfr = true; return;
	}
	window.location.href = "/reset";
}

function loadPalettesData(callback = null)
{
	if (palettesData) return;
	const lsKey = "wledPalx";
	var palettesDataJson = localStorage.getItem(lsKey);
	if (palettesDataJson) {
		try {
			palettesDataJson = JSON.parse(palettesDataJson);
			if (palettesDataJson && palettesDataJson.vid == lastinfo.vid) {
				palettesData = palettesDataJson.p;
				if (callback) callback(); //redrawPalPrev()
				return;
			}
		} catch (e) {}
	}

	palettesData = {};
	getPalettesData(0, ()=>{
		localStorage.setItem(lsKey, JSON.stringify({
			p: palettesData,
			vid: lastinfo.vid
		}));
		if (callback) setTimeout(callback, 99); //redrawPalPrev()
	});
}

function getPalettesData(page, callback)
{
	fetch(getURL(`/json/palx?page=${page}`), {
		method: 'get',
		headers: {
			"Content-type": "application/json; charset=UTF-8"
		}
	})
	.then((res)=>{
		if (!res.ok) showErrorToast();
		return res.json();
	})
	.then((json)=>{
		palettesData = Object.assign({}, palettesData, json.p);
		if (page < json.m) setTimeout(()=>{ getPalettesData(page + 1, callback); }, 50);
		else callback();
	})
	.catch((e)=>{
		showToast(e, true);
	});
}

function search(f,l=null)
{
	f.nextElementSibling.style.display=(f.value!=='')?'block':'none';
	if (!l) return;
	var el = gId(l).querySelectorAll('.lstI');
	for (i = 0; i < el.length; i++) {
		var it = el[i];
		var itT = it.querySelector('.lstIname').innerText.toUpperCase();
		it.style.display = itT.indexOf(f.value.toUpperCase())>-1?'':'none';
	}
}

function clean(c)
{
	c.style.display='none';
	var i=c.previousElementSibling;
	i.value='';
	i.focus();
	i.dispatchEvent(new Event('input'));
}

function unfocusSliders()
{
	gId("sliderBri").blur();
	gId("sliderSpeed").blur();
	gId("sliderIntensity").blur();
}

//sliding UI
const _C = d.querySelector('.container'), N = 1;

let iSlide = 0, x0 = null, scrollS = 0, locked = false, w;

function unify(e) {	return e.changedTouches ? e.changedTouches[0] : e; }

function hasIroClass(classList)
{
	for (var i = 0; i < classList.length; i++) {
		var element = classList[i];
		if (element.startsWith('Iro')) return true;
	}
	return false;
}
//required by rangetouch.js
function lock(e)
{
	var l = e.target.classList;
	var pl = e.target.parentElement.classList;

	if (l.contains('noslide') || hasIroClass(l) || hasIroClass(pl)) return;

	x0 = unify(e).clientX;
	scrollS = gEBCN("tabcontent")[iSlide].scrollTop;

	_C.classList.toggle('smooth', !(locked = true));
}
//required by rangetouch.js
function move(e)
{
	if(!locked) return;
	var clientX = unify(e).clientX;
	var dx = clientX - x0;
	var s = Math.sign(dx);
	var f = +(s*dx/w).toFixed(2);

	if((clientX != 0) &&
		(iSlide > 0 || s < 0) && (iSlide < N - 1 || s > 0) &&
		f > 0.12 &&
		gEBCN("tabcontent")[iSlide].scrollTop == scrollS)
	{
		_C.style.setProperty('--i', iSlide -= s);
		f = 1 - f;
		updateTablinks(iSlide);
	}
	_C.style.setProperty('--f', f);
	_C.classList.toggle('smooth', !(locked = false));
	x0 = null;
}

function size()
{
	var h = gId('top').clientHeight;
	sCol('--th', h + "px");
    sCol("--tp", h - (gId(`briwrap`).style.display === "block" ? 0 : gId(`briwrap`).clientTop) + "px");
    sCol("--bh", "0px");
}

function mergeDeep(target, ...sources)
{
	if (!sources.length) return target;
	const source = sources.shift();

	if (isObj(target) && isObj(source)) {
		for (const key in source) {
			if (isObj(source[key])) {
				if (!target[key]) Object.assign(target, { [key]: {} });
				mergeDeep(target[key], source[key]);
			} else {
				Object.assign(target, { [key]: source[key] });
			}
		}
	}
	return mergeDeep(target, ...sources);
}

size();
window.addEventListener('resize', size, false);

_C.addEventListener('mousedown', lock, false);
_C.addEventListener('touchstart', lock, false);

_C.addEventListener('mouseout', move, false);
_C.addEventListener('mouseup', move, false);
_C.addEventListener('touchend', move, false);
