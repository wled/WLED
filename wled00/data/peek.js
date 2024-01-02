function peek(c) {
	// Check for canvas support
	var ctx = c.getContext('2d');
	if (ctx) { // Access the rendering context
		// use parent WS or open new
		var ws;
		try {
			ws = top.window.ws;
		} catch (e) {}
		if (ws && ws.readyState === WebSocket.OPEN) {
			ws.send("{'lv':true}");
		} else {
			ws = new WebSocket((window.location.protocol == "https:"?"wss":"ws")+"://"+document.location.host+"/ws");
			ws.onopen = ()=>{
				ws.send("{'lv':true}");
			}
		}
		ws.binaryType = "arraybuffer";
		ws.addEventListener('message',(e)=>{
			// function processWSData(e) {
			try {
				if (toString.call(e.data) === '[object ArrayBuffer]') {
					let leds = new Uint8Array(e.data);
					if (leds[0] != 76 || leds[1] != 2 || !ctx) return; //'L', set in ws.cpp
					let mW = leds[2]; // matrix width
					let mH = leds[3]; // matrix height
					let pPL = Math.min(c.width / mW, c.height / mH); // pixels per LED (width of circle)
					let lOf = Math.floor((c.width - pPL*mW)/2); //left offset (to center matrix)
					var i = 4; //same offset as in ws.cpp
					ctx.clearRect(0, 0, c.width, c.height); //WLEDMM
					function colorAmp(color) {
						if (color == 0) return 0;
						return 25+225*color/255;
					} //WLEDMM in range 55 - 205
					for (y=0.5;y<mH;y++) for (x=0.5; x<mW; x++) {
						if (leds[i]!= 0 || leds[i+1]!= 0 || leds[i+2]!= 0) { //WLEDMM: do not show blacks
							ctx.fillStyle = `rgb(${colorAmp(leds[i])},${colorAmp(leds[i+1])},${colorAmp(leds[i+2])})`;
							ctx.beginPath();
							ctx.arc(x*pPL+lOf, y*pPL, pPL*0.4, 0, 2 * Math.PI);
							ctx.fill();
						}
						i+=3;
					}
				}
			} catch (err) {
				console.error("Peek WS error:",err);
			} 
		});
	}
}