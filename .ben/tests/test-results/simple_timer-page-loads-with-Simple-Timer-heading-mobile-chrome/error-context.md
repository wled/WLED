# Instructions

- Following Playwright test failed.
- Explain why, be concise, respect Playwright best practices.
- Provide a snippet of code with the fix, if possible.

# Test info

- Name: simple_timer.spec.js >> page loads with "Simple Timer" heading
- Location: specs\simple_timer.spec.js:44:1

# Error details

```
Error: expect(locator).toHaveText(expected) failed

Locator: locator('h1')
Expected: "Simple Timer"
Timeout: 5000ms
Error: element(s) not found

Call log:
  - Expect "toHaveText" with timeout 5000ms
  - waiting for locator('h1')

```

```yaml
- button "Back"
- checkbox "Timer enabled"
- text: Timer enabled
- paragraph: Current time
- textbox: 15:29
- paragraph: Lights ON
- paragraph: At
- textbox
- paragraph: Lights OFF
- paragraph: At
- textbox
- paragraph: Display effect
- combobox:
  - option "Pride 2015" [selected]
- button "SAVE"
```

# Test source

```ts
  1   | // @ts-check
  2   | // Tests for simple_timer.htm — REQ 3.29 / REQ 2.7 / REQ 2.8
  3   | const { test, expect } = require('@playwright/test');
  4   | 
  5   | const PRESETS_NO_100 = {
  6   | 	'6':   { n: 'Pride 2015', on: true, bri: 128, seg: [{ id: 0, grp: 3, fx: 63 }] },
  7   | 	'101': { n: 'z_cycle_preset', win: 'P1=6&P2=6&PL=~' }
  8   | };
  9   | const PRESETS_WITH_100 = {
  10  | 	...PRESETS_NO_100,
  11  | 	'100': { n: 'z_lights_off', on: false }
  12  | };
  13  | const FX_PRESETS = [{ effectId: 63, presetId: 6, effectName: 'Pride 2015' }];
  14  | 
  15  | /** Intercept all device API calls the page might make */
  16  | async function mockApi(page, presetsFixture = PRESETS_NO_100) {
  17  | 	await page.route('**/presets.json', r => r.fulfill({ json: presetsFixture }));
  18  | 	await page.route('**/upload', r => r.fulfill({ status: 200, body: 'OK' }));
  19  | 	await page.route('**/settings/time', r => r.fulfill({ status: 200, body: 'OK' }));
  20  | 	await page.route('**/json/**', r => r.fulfill({ json: {} }));
  21  | 	await page.route('**/skin.css', r => r.fulfill({ status: 200, contentType: 'text/css', body: '' }));
  22  | }
  23  | 
  24  | async function withFxPresets(page) {
  25  | 	await page.addInitScript((data) => {
  26  | 		localStorage.setItem('wled_user_fx_presets', JSON.stringify(data));
  27  | 	}, FX_PRESETS);
  28  | }
  29  | 
  30  | async function withNoFxPresets(page) {
  31  | 	await page.addInitScript(() => {
  32  | 		localStorage.removeItem('wled_user_fx_presets');
  33  | 	});
  34  | }
  35  | 
  36  | async function loadTimer(page) {
  37  | 	await page.goto('/simple_timer.htm');
  38  | 	// Wait for common.js init to complete (style.css loaded, stInit called)
  39  | 	await page.waitForFunction(() => document.getElementById('stSaveBtn') !== null);
  40  | }
  41  | 
  42  | // ── Structure ──────────────────────────────────────────────────────────────
  43  | 
  44  | test('page loads with "Simple Timer" heading', async ({ page }) => {
  45  | 	await mockApi(page);
  46  | 	await withFxPresets(page);
  47  | 	await loadTimer(page);
> 48  | 	await expect(page.locator('h1')).toHaveText('Simple Timer');
      |                                   ^ Error: expect(locator).toHaveText(expected) failed
  49  | });
  50  | 
  51  | test('page has Back button, enabled checkbox, time inputs, and SAVE button', async ({ page }) => {
  52  | 	await mockApi(page);
  53  | 	await withFxPresets(page);
  54  | 	await loadTimer(page);
  55  | 	await expect(page.locator('.st-back')).toBeVisible();
  56  | 	await expect(page.locator('#stEnabled')).toBeVisible();
  57  | 	await expect(page.locator('#stCurrentTime')).toBeVisible();
  58  | 	await expect(page.locator('#stOnTime')).toBeVisible();
  59  | 	await expect(page.locator('#stOffTime')).toBeVisible();
  60  | 	await expect(page.locator('#stSaveBtn')).toBeVisible();
  61  | });
  62  | 
  63  | test('Timer enabled checkbox is above the form sections', async ({ page }) => {
  64  | 	await mockApi(page);
  65  | 	await withFxPresets(page);
  66  | 	await loadTimer(page);
  67  | 	const enabledBox = page.locator('#stEnabled');
  68  | 	const currentTime = page.locator('#stCurrentTime');
  69  | 	const enabledY = (await enabledBox.boundingBox()).y;
  70  | 	const currentTimeY = (await currentTime.boundingBox()).y;
  71  | 	expect(enabledY).toBeLessThan(currentTimeY);
  72  | });
  73  | 
  74  | // ── No presets state ───────────────────────────────────────────────────────
  75  | 
  76  | test('SAVE is disabled and warning shown when no fx presets in localStorage', async ({ page }) => {
  77  | 	await mockApi(page);
  78  | 	await withNoFxPresets(page);
  79  | 	await loadTimer(page);
  80  | 	await expect(page.locator('#stSaveBtn')).toBeDisabled();
  81  | 	await expect(page.locator('#stNoPresetsWarn')).toBeVisible();
  82  | 	await expect(page.locator('#stEffectSelect')).not.toBeVisible();
  83  | });
  84  | 
  85  | test('effect dropdown is populated when fx presets exist', async ({ page }) => {
  86  | 	await mockApi(page);
  87  | 	await withFxPresets(page);
  88  | 	await loadTimer(page);
  89  | 	await expect(page.locator('#stEffectSelect')).toBeVisible();
  90  | 	await expect(page.locator('#stEffectSelect option')).toHaveCount(1);
  91  | 	await expect(page.locator('#stEffectSelect option').first()).toHaveText('Pride 2015');
  92  | 	await expect(page.locator('#stNoPresetsWarn')).not.toBeVisible();
  93  | });
  94  | 
  95  | // ── Enable / disable greying ───────────────────────────────────────────────
  96  | 
  97  | test('form body has st-disabled class on load (timer off by default)', async ({ page }) => {
  98  | 	await mockApi(page);
  99  | 	await withFxPresets(page);
  100 | 	await loadTimer(page);
  101 | 	await expect(page.locator('#stFormBody')).toHaveClass(/st-disabled/);
  102 | });
  103 | 
  104 | test('form body loses st-disabled when checkbox is checked', async ({ page }) => {
  105 | 	await mockApi(page);
  106 | 	await withFxPresets(page);
  107 | 	await loadTimer(page);
  108 | 	await page.locator('#stEnabled').check();
  109 | 	await expect(page.locator('#stFormBody')).not.toHaveClass(/st-disabled/);
  110 | });
  111 | 
  112 | test('form body re-gains st-disabled when checkbox is unchecked', async ({ page }) => {
  113 | 	await mockApi(page);
  114 | 	await withFxPresets(page);
  115 | 	await loadTimer(page);
  116 | 	await page.locator('#stEnabled').check();
  117 | 	await page.locator('#stEnabled').uncheck();
  118 | 	await expect(page.locator('#stFormBody')).toHaveClass(/st-disabled/);
  119 | });
  120 | 
  121 | test('enabled state restores from localStorage on load', async ({ page }) => {
  122 | 	await mockApi(page);
  123 | 	await withFxPresets(page);
  124 | 	await page.addInitScript(() => {
  125 | 		localStorage.setItem('wled_simple_timer', JSON.stringify({
  126 | 			onTime: '07:00', offTime: '22:00', presetId: 6, enabled: true
  127 | 		}));
  128 | 	});
  129 | 	await loadTimer(page);
  130 | 	await expect(page.locator('#stEnabled')).toBeChecked();
  131 | 	await expect(page.locator('#stFormBody')).not.toHaveClass(/st-disabled/);
  132 | });
  133 | 
  134 | // ── Settings persistence ───────────────────────────────────────────────────
  135 | 
  136 | test('time values restore from localStorage on load', async ({ page }) => {
  137 | 	await mockApi(page);
  138 | 	await withFxPresets(page);
  139 | 	await page.addInitScript(() => {
  140 | 		localStorage.setItem('wled_simple_timer', JSON.stringify({
  141 | 			onTime: '08:30', offTime: '23:15', presetId: 6, enabled: false
  142 | 		}));
  143 | 	});
  144 | 	await loadTimer(page);
  145 | 	await expect(page.locator('#stOnTime')).toHaveValue('08:30');
  146 | 	await expect(page.locator('#stOffTime')).toHaveValue('23:15');
  147 | });
  148 | 
```