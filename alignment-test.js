// Run this in browser console on index.htm to measure alignment

console.log('=== ALIGNMENT TEST ===\n');

// Colors tab
const qcs = document.querySelector('#Colors .qcs');
const colorsInfo = document.getElementById('colorsInfoPanel');
if (qcs && colorsInfo) {
	const qcsLeft = qcs.getBoundingClientRect().left;
	const infoLeft = colorsInfo.getBoundingClientRect().left;
	const diff = qcsLeft - infoLeft;
	console.log('COLORS TAB:');
	console.log('  .qcs left: ' + qcsLeft.toFixed(1) + 'px');
	console.log('  #colorsInfoPanel left: ' + infoLeft.toFixed(1) + 'px');
	console.log('  DIFF: ' + diff.toFixed(1) + 'px ' + (Math.abs(diff) < 2 ? '✓ ALIGNED' : '✗ MISALIGNED'));
	console.log('');
}

// Effects tab
const fxPill = document.querySelector('#Effects .fx-pill-row');
const fxInfo = document.getElementById('fxInfoPanel');
if (fxPill && fxInfo) {
	const pillLeft = fxPill.getBoundingClientRect().left;
	const infoLeft = fxInfo.getBoundingClientRect().left;
	const diff = pillLeft - infoLeft;
	console.log('EFFECTS TAB:');
	console.log('  .fx-pill-row left: ' + pillLeft.toFixed(1) + 'px');
	console.log('  #fxInfoPanel left: ' + infoLeft.toFixed(1) + 'px');
	console.log('  DIFF: ' + diff.toFixed(1) + 'px ' + (Math.abs(diff) < 2 ? '✓ ALIGNED' : '✗ MISALIGNED'));
	console.log('');
}

// Favourites tab
const favPill = document.querySelector('#Favourites .fx-pill-row');
const favsInfo = document.getElementById('presetsInfoPanel');
if (favPill && favsInfo) {
	const pillLeft = favPill.getBoundingClientRect().left;
	const infoLeft = favsInfo.getBoundingClientRect().left;
	const diff = pillLeft - infoLeft;
	console.log('FAVOURITES TAB:');
	console.log('  .fx-pill-row left: ' + pillLeft.toFixed(1) + 'px');
	console.log('  #presetsInfoPanel left: ' + infoLeft.toFixed(1) + 'px');
	console.log('  DIFF: ' + diff.toFixed(1) + 'px ' + (Math.abs(diff) < 2 ? '✓ ALIGNED' : '✗ MISALIGNED'));
	console.log('');
}

// Also check container margins
console.log('=== CONTAINER ANALYSIS ===\n');
const qcsw = document.getElementById('qcs-w');
if (qcsw) {
	const styles = window.getComputedStyle(qcsw);
	console.log('#qcs-w:');
	console.log('  margin: ' + styles.margin);
	console.log('  left pos: ' + qcsw.getBoundingClientRect().left.toFixed(1) + 'px');
	console.log('  width: ' + qcsw.getBoundingClientRect().width.toFixed(1) + 'px');
	console.log('');
}

const pcont = document.getElementById('pcont');
if (pcont) {
	const styles = window.getComputedStyle(pcont);
	console.log('#pcont:');
	console.log('  margin: ' + styles.margin);
	console.log('  left pos: ' + pcont.getBoundingClientRect().left.toFixed(1) + 'px');
	console.log('  width: ' + pcont.getBoundingClientRect().width.toFixed(1) + 'px');
	console.log('');
}

const fxlist = document.getElementById('fxlist');
if (fxlist) {
	const styles = window.getComputedStyle(fxlist);
	console.log('#fxlist:');
	console.log('  margin: ' + styles.margin);
	console.log('  left pos: ' + fxlist.getBoundingClientRect().left.toFixed(1) + 'px');
	console.log('  width: ' + fxlist.getBoundingClientRect().width.toFixed(1) + 'px');
	console.log('');
}

// Check info panels
console.log('=== INFO PANELS ===\n');
if (colorsInfo) {
	const styles = window.getComputedStyle(colorsInfo);
	console.log('#colorsInfoPanel:');
	console.log('  margin: ' + styles.margin);
	console.log('  left pos: ' + colorsInfo.getBoundingClientRect().left.toFixed(1) + 'px');
	console.log('');
}
