// Inject visual alignment guide into the page
// Run this: fetch('visual-alignment-debug.js').then(r => r.text()).then(eval)

const style = document.createElement('style');
style.innerHTML = `
	.alignment-debug {
		position: absolute;
		opacity: 0.5;
		pointer-events: none;
		z-index: 9999;
		font-size: 10px;
		color: white;
		padding: 2px 4px;
	}
	.align-pill { background: rgba(0, 255, 0, 0.3); border: 1px dashed #0f0; }
	.align-info { background: rgba(255, 0, 0, 0.3); border: 1px dashed #f00; }
	.align-container { background: rgba(0, 0, 255, 0.1); border: 1px dotted #00f; }
`;
document.head.appendChild(style);

function addDebugBox(element, type, label) {
	if (!element) return;
	const rect = element.getBoundingClientRect();
	const box = document.createElement('div');
	box.className = `alignment-debug align-${type}`;
	box.textContent = label + ` L:${rect.left.toFixed(0)}`;
	box.style.left = rect.left + 'px';
	box.style.top = rect.top + 'px';
	box.style.width = rect.width + 'px';
	box.style.height = rect.height + 'px';
	document.body.appendChild(box);
}

// Colors
addDebugBox(document.getElementById('qcs-w'), 'container', '#qcs-w');
addDebugBox(document.querySelector('#Colors .qcs'), 'pill', '.qcs');
addDebugBox(document.getElementById('colorsInfoPanel'), 'info', '#colorsInfo');

// Effects
addDebugBox(document.getElementById('fxlist'), 'container', '#fxlist');
addDebugBox(document.querySelector('#Effects .fx-pill-row'), 'pill', '.fx-pill-row');
addDebugBox(document.getElementById('fxInfoPanel'), 'info', '#fxInfo');

// Favourites
addDebugBox(document.getElementById('pcont'), 'container', '#pcont');
addDebugBox(document.querySelector('#Favourites .fx-pill-row'), 'pill', '.fx-pill-row');
addDebugBox(document.getElementById('presetsInfoPanel'), 'info', '#presetsInfo');

console.log('Alignment debug boxes added. Green=pills, Red=info, Blue=containers');
