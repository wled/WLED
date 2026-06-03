// @ts-check
// Tests for simple_timer.htm — REQ 3.29 / REQ 2.7 / REQ 2.8
const { test, expect } = require('@playwright/test');

const PRESETS_NO_100 = {
	'6':   { n: 'Pride 2015', on: true, bri: 128, seg: [{ id: 0, grp: 3, fx: 63 }] },
	'101': { n: 'z_cycle_preset', win: 'P1=6&P2=6&PL=~' }
};
const PRESETS_WITH_100 = {
	...PRESETS_NO_100,
	'100': { n: 'z_lights_off', on: false }
};
const FX_PRESETS = [{ effectId: 63, presetId: 6, effectName: 'Pride 2015' }];

/** Intercept all device API calls the page might make */
async function mockApi(page, presetsFixture = PRESETS_NO_100) {
	await page.route('**/presets.json', r => r.fulfill({ json: presetsFixture }));
	await page.route('**/upload', r => r.fulfill({ status: 200, body: 'OK' }));
	await page.route('**/settings/time', r => r.fulfill({ status: 200, body: 'OK' }));
	await page.route('**/json/**', r => r.fulfill({ json: {} }));
	await page.route('**/skin.css', r => r.fulfill({ status: 200, contentType: 'text/css', body: '' }));
}

async function withFxPresets(page) {
	await page.addInitScript((data) => {
		localStorage.setItem('wled_user_fx_presets', JSON.stringify(data));
	}, FX_PRESETS);
}

async function withNoFxPresets(page) {
	await page.addInitScript(() => {
		localStorage.removeItem('wled_user_fx_presets');
	});
}

async function loadTimer(page) {
	await page.goto('/simple_timer.htm');
	// Wait for common.js init to complete (style.css loaded, stInit called)
	await page.waitForFunction(() => document.getElementById('stSaveBtn') !== null);
}

// ── Structure ──────────────────────────────────────────────────────────────

test('page loads with "Simple Timer" heading', async ({ page }) => {
	await mockApi(page);
	await withFxPresets(page);
	await loadTimer(page);
	await expect(page.locator('h1')).toHaveText('Simple Timer');
});

test('page has Back button, enabled checkbox, time inputs, and SAVE button', async ({ page }) => {
	await mockApi(page);
	await withFxPresets(page);
	await loadTimer(page);
	await expect(page.locator('.st-back')).toBeVisible();
	await expect(page.locator('#stEnabled')).toBeVisible();
	await expect(page.locator('#stCurrentTime')).toBeVisible();
	await expect(page.locator('#stOnTime')).toBeVisible();
	await expect(page.locator('#stOffTime')).toBeVisible();
	await expect(page.locator('#stSaveBtn')).toBeVisible();
});

test('Timer enabled checkbox is above the form sections', async ({ page }) => {
	await mockApi(page);
	await withFxPresets(page);
	await loadTimer(page);
	const enabledBox = page.locator('#stEnabled');
	const currentTime = page.locator('#stCurrentTime');
	const enabledY = (await enabledBox.boundingBox()).y;
	const currentTimeY = (await currentTime.boundingBox()).y;
	expect(enabledY).toBeLessThan(currentTimeY);
});

// ── No presets state ───────────────────────────────────────────────────────

test('SAVE is disabled and warning shown when no fx presets in localStorage', async ({ page }) => {
	await mockApi(page);
	await withNoFxPresets(page);
	await loadTimer(page);
	await expect(page.locator('#stSaveBtn')).toBeDisabled();
	await expect(page.locator('#stNoPresetsWarn')).toBeVisible();
	await expect(page.locator('#stEffectSelect')).not.toBeVisible();
});

test('effect dropdown is populated when fx presets exist', async ({ page }) => {
	await mockApi(page);
	await withFxPresets(page);
	await loadTimer(page);
	await expect(page.locator('#stEffectSelect')).toBeVisible();
	await expect(page.locator('#stEffectSelect option')).toHaveCount(1);
	await expect(page.locator('#stEffectSelect option').first()).toHaveText('Pride 2015');
	await expect(page.locator('#stNoPresetsWarn')).not.toBeVisible();
});

// ── Enable / disable greying ───────────────────────────────────────────────

test('form body has st-disabled class on load (timer off by default)', async ({ page }) => {
	await mockApi(page);
	await withFxPresets(page);
	await loadTimer(page);
	await expect(page.locator('#stFormBody')).toHaveClass(/st-disabled/);
});

test('form body loses st-disabled when checkbox is checked', async ({ page }) => {
	await mockApi(page);
	await withFxPresets(page);
	await loadTimer(page);
	await page.locator('#stEnabled').check();
	await expect(page.locator('#stFormBody')).not.toHaveClass(/st-disabled/);
});

test('form body re-gains st-disabled when checkbox is unchecked', async ({ page }) => {
	await mockApi(page);
	await withFxPresets(page);
	await loadTimer(page);
	await page.locator('#stEnabled').check();
	await page.locator('#stEnabled').uncheck();
	await expect(page.locator('#stFormBody')).toHaveClass(/st-disabled/);
});

test('enabled state restores from localStorage on load', async ({ page }) => {
	await mockApi(page);
	await withFxPresets(page);
	await page.addInitScript(() => {
		localStorage.setItem('wled_simple_timer', JSON.stringify({
			onTime: '07:00', offTime: '22:00', presetId: 6, enabled: true
		}));
	});
	await loadTimer(page);
	await expect(page.locator('#stEnabled')).toBeChecked();
	await expect(page.locator('#stFormBody')).not.toHaveClass(/st-disabled/);
});

// ── Settings persistence ───────────────────────────────────────────────────

test('time values restore from localStorage on load', async ({ page }) => {
	await mockApi(page);
	await withFxPresets(page);
	await page.addInitScript(() => {
		localStorage.setItem('wled_simple_timer', JSON.stringify({
			onTime: '08:30', offTime: '23:15', presetId: 6, enabled: false
		}));
	});
	await loadTimer(page);
	await expect(page.locator('#stOnTime')).toHaveValue('08:30');
	await expect(page.locator('#stOffTime')).toHaveValue('23:15');
});

test('SAVE persists settings to localStorage', async ({ page }) => {
	await mockApi(page);
	await withFxPresets(page);
	await loadTimer(page);
	await page.locator('#stEnabled').check();
	await page.locator('#stOnTime').fill('07:00');
	await page.locator('#stOffTime').fill('22:00');
	await page.locator('#stSaveBtn').click();
	await page.waitForResponse('**/settings/time');
	const saved = await page.evaluate(() => JSON.parse(localStorage.getItem('wled_simple_timer') || '{}'));
	expect(saved.onTime).toBe('07:00');
	expect(saved.offTime).toBe('22:00');
	expect(saved.enabled).toBe(true);
	expect(saved.presetId).toBe(6);
});

// ── POST body — timer parameters ──────────────────────────────────────────

test('SAVE posts to /settings/time with correct ON/OFF slot parameters when enabled', async ({ page }) => {
	await mockApi(page);
	await withFxPresets(page);

	let capturedBody = '';
	await page.route('**/settings/time', r => {
		if (r.request().method() === 'POST') capturedBody = r.request().postData() || '';
		r.fulfill({ status: 200, body: 'OK' });
	});

	await loadTimer(page);
	await page.locator('#stEnabled').check();
	await page.locator('#stOnTime').fill('07:30');
	await page.locator('#stOffTime').fill('22:00');
	await page.locator('#stSaveBtn').click();
	await page.waitForResponse('**/settings/time');

	const p = new URLSearchParams(capturedBody);
	// Slot 0: ON
	expect(p.get('H0')).toBe('7');
	expect(p.get('N0')).toBe('30');
	expect(p.get('T0')).toBe('6');  // user's preset
	expect(p.get('W0')).toBe('255'); // all days enabled
	// Slot 1: OFF
	expect(p.get('H1')).toBe('22');
	expect(p.get('N1')).toBe('0');
	expect(p.get('T1')).toBe('100'); // z_lights_off
	expect(p.get('W1')).toBe('255');
	// NTP settings
	expect(p.get('NT')).toBe('1');
	expect(p.get('TZ')).toBe('0');
});

test('SAVE sends W0=0 and W1=0 when timer is disabled', async ({ page }) => {
	await mockApi(page);
	await withFxPresets(page);

	let capturedBody = '';
	await page.route('**/settings/time', r => {
		if (r.request().method() === 'POST') capturedBody = r.request().postData() || '';
		r.fulfill({ status: 200, body: 'OK' });
	});

	await loadTimer(page);
	// Leave checkbox unchecked
	await page.locator('#stOnTime').fill('07:30');
	await page.locator('#stOffTime').fill('22:00');
	await page.locator('#stSaveBtn').click();
	await page.waitForResponse('**/settings/time');

	const p = new URLSearchParams(capturedBody);
	expect(p.get('W0')).toBe('0');
	expect(p.get('W1')).toBe('0');
});

test('SAVE clears slots 2-15 in the POST body', async ({ page }) => {
	await mockApi(page);
	await withFxPresets(page);

	let capturedBody = '';
	await page.route('**/settings/time', r => {
		if (r.request().method() === 'POST') capturedBody = r.request().postData() || '';
		r.fulfill({ status: 200, body: 'OK' });
	});

	await loadTimer(page);
	await page.locator('#stEnabled').check();
	await page.locator('#stOnTime').fill('07:00');
	await page.locator('#stOffTime').fill('22:00');
	await page.locator('#stSaveBtn').click();
	await page.waitForResponse('**/settings/time');

	const p = new URLSearchParams(capturedBody);
	for (var i = 2; i < 16; i++) {
		expect(p.get('T' + i)).toBe('0');
		expect(p.get('W' + i)).toBe('0');
	}
});

// ── Preset 100 (z_lights_off) management ─────────────────────────────────

test('SAVE uploads presets.json with preset 100 when it does not exist', async ({ page }) => {
	await mockApi(page, PRESETS_NO_100);
	await withFxPresets(page);

	let uploadBody = '';
	await page.route('**/upload', async r => {
		// FormData body — read raw and look for the JSON blob content
		uploadBody = r.request().postData() || '';
		r.fulfill({ status: 200, body: 'OK' });
	});

	await loadTimer(page);
	await page.locator('#stEnabled').check();
	await page.locator('#stOnTime').fill('07:00');
	await page.locator('#stOffTime').fill('22:00');
	await page.locator('#stSaveBtn').click();
	await page.waitForResponse('**/settings/time');
	// Give the upload a moment to complete
	await page.waitForTimeout(300);

	expect(uploadBody).toContain('z_lights_off');
	expect(uploadBody).toContain('"100"');
});

test('SAVE does not upload presets.json when preset 100 already exists', async ({ page }) => {
	await mockApi(page, PRESETS_WITH_100);
	await withFxPresets(page);

	let uploadCalled = false;
	await page.route('**/upload', r => {
		uploadCalled = true;
		r.fulfill({ status: 200, body: 'OK' });
	});

	await loadTimer(page);
	await page.locator('#stEnabled').check();
	await page.locator('#stOnTime').fill('07:00');
	await page.locator('#stOffTime').fill('22:00');
	await page.locator('#stSaveBtn').click();
	await page.waitForResponse('**/settings/time');
	await page.waitForTimeout(300);

	expect(uploadCalled).toBe(false);
});
