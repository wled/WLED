// @ts-check
const { test, expect } = require('@playwright/test');
const path = require('path');

const effects   = require('../fixtures/effects.json');
const fxdata    = require('../fixtures/fxdata.json');
const palettes  = require('../fixtures/palettes.json');
const stateInfo = require('../fixtures/state-info.json');
const presets   = require('../fixtures/presets.json');

// "Blink" is at index 1 in the effects array — we use it as the test subject.
// data-id on the list item equals the effect's index in the effects array.
const TEST_EFFECT_NAME = 'Blink';
const TEST_EFFECT_ID   = effects.indexOf(TEST_EFFECT_NAME); // 1

/**
 * Register all WLED device API mocks.
 * Captured upload body is written to capturedUpload[] so tests can inspect it.
 */
async function mockDeviceApi(page, capturedUpload = []) {
  // Playwright routes are LIFO — last registered is matched first.
  // Register the catch-all first so specific routes (registered after) take priority.
  await page.route('**/json/**',       r => r.fulfill({ json: { success: true } }));
  // Palette extended data — return empty set (no custom palettes)
  await page.route('**/json/palx**',   r => r.fulfill({ json: { m: 0, p: {} } }));
  await page.route('**/json/si',       r => r.fulfill({ json: stateInfo }));
  await page.route('**/json/effects',  r => r.fulfill({ json: effects }));
  await page.route('**/json/fxdata',   r => r.fulfill({ json: fxdata }));
  await page.route('**/json/palettes', r => r.fulfill({ json: palettes }));

  // presets.json — served from LittleFS; return the fixture
  await page.route('**/presets.json',  r => r.fulfill({ json: presets }));

  // Version-info check via /edit endpoint — return "already known" so the dialog is suppressed
  await page.route('**/edit**',        r => r.fulfill({
    json: { version: stateInfo.info.ver, neverAsk: true }
  }));

  // skin.css — cfg.comp.css may be enabled; return empty CSS to prevent Init failed
  await page.route('**/skin.css',      r => r.fulfill({
    status: 200, contentType: 'text/css', body: ''
  }));

  // /upload — capture the posted body and confirm success
  await page.route('**/upload', async r => {
    const body = r.request().postData();
    capturedUpload.push(body);
    r.fulfill({ status: 200, contentType: 'text/plain', body: 'OK' });
  });

  // Block WebSocket — not needed (ws:-1 in state-info means makeWS() returns immediately)
  await page.route('**/ws', r => r.abort());
}

/** Navigate to the main page, switch to Effects tab, and wait until the list is ready. */
async function loadPage(page, capturedUpload = []) {
  await mockDeviceApi(page, capturedUpload);
  await page.addInitScript(() => {
    // Prevent PC mode — this project targets mobile only
    localStorage.setItem('pcm', 'false');
  });
  await page.goto('/index.htm');
  // Clear any fx preset state left from prior real-browser usage at this origin.
  // Done via evaluate (not addInitScript) so mid-test reloads don't wipe in-progress state.
  await page.evaluate(() => localStorage.removeItem('wled_user_fx_presets'));
  await page.reload();
  // Click the Effects tab to ensure the fxlist is visible.
  // The tab is a bottom-nav link whose text contains "Effects".
  await page.locator('.tablinks', { hasText: 'Effects' }).click();
  // Hearts exist in the DOM but are visibility:hidden until an effect is selected.
  await page.waitForSelector('#fxlist .fx-preset-add', { timeout: 10_000, state: 'attached' });
}

/**
 * Simulate selecting an effect (setting selectedFx + adding .selected class) without
 * triggering a real device request.  This makes the outline heart visible so it can be clicked.
 */
async function selectEffect(page, effectId) {
  await page.evaluate((id) => {
    window.selectedFx = id;
    const parent = document.getElementById('fxlist');
    const prev = parent && parent.querySelector('.lstI.selected');
    if (prev) prev.classList.remove('selected');
    const el = parent && parent.querySelector(`.lstI[data-id="${id}"]`);
    if (el) el.classList.add('selected');
  }, effectId);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

test.describe('fx-preset feature', () => {

  test('effects list renders a + button for non-Solid effects', async ({ page }) => {
    await loadPage(page);

    // Solid (id 0) must NOT have a preset button
    const solidItem = page.locator(`#fxlist .lstI[data-id="0"]`);
    await expect(solidItem.locator('.fx-preset-btn')).toHaveCount(0);

    // Every other visible effect must have an add button
    const addButtons = page.locator('#fxlist .fx-preset-add');
    await expect(addButtons).not.toHaveCount(0);
  });

  test('clicking + shows toast "Effect added to favorites!" and button switches to red −', async ({ page }) => {
    const uploaded = [];
    await loadPage(page, uploaded);

    const effectRow = page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"])`);
    await selectEffect(page, TEST_EFFECT_ID);
    const addBtn = effectRow.locator('.fx-preset-add');
    await expect(addBtn).toBeVisible();
    await addBtn.click();

    // Toast fires synchronously before the first await in addEffectAsPreset
    await expect(page.locator('#toast')).toContainText('Effect added to favorites!');

    // Button must switch to the remove variant immediately (DOM update is synchronous)
    await expect(effectRow.locator('.fx-preset-rm')).toBeVisible();
    await expect(effectRow.locator('.fx-preset-add')).toHaveCount(0);
  });

  test('adding a preset saves the correct entry to localStorage', async ({ page }) => {
    await loadPage(page);

    await selectEffect(page, TEST_EFFECT_ID);
    await page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"]) .fx-preset-add`).click();

    const stored = await page.evaluate(() =>
      JSON.parse(localStorage.getItem('wled_user_fx_presets') || '[]')
    );
    expect(stored).toHaveLength(1);
    expect(stored[0].effectId).toBe(TEST_EFFECT_ID);
    expect(stored[0].effectName).toBe(TEST_EFFECT_NAME);
    expect(stored[0].presetId).toBe(6); // first user preset, after fixed presets 1–5
  });

  test('preset upload contains the new preset with numeric fx ID and correct P2', async ({ page }) => {
    const uploaded = [];
    await loadPage(page, uploaded);

    await selectEffect(page, TEST_EFFECT_ID);
    await page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"]) .fx-preset-add`).click();

    // Wait for the async upload to complete (page.route fulfills instantly so this is fast)
    await page.waitForTimeout(300);

    expect(uploaded).toHaveLength(1);
    // The upload body is multipart — verify the JSON payload is in there
    const body = uploaded[0] || '';
    // Parse the embedded JSON from the multipart body
    const jsonMatch = body.match(/(\{[\s\S]*\})/);
    expect(jsonMatch).not.toBeNull();
    const presetsUploaded = JSON.parse(jsonMatch[1]);

    // New preset must exist with a numeric fx field
    expect(presetsUploaded['6']).toBeDefined();
    expect(presetsUploaded['6'].seg[0].fx).toBe(TEST_EFFECT_ID); // number, not string

    // _FX_PRESETS_BASE=5, so user presets start at ID 6; 1 preset → P1=6, P2=6
    expect(presetsUploaded['101'].win).toBe('P1=6&P2=6&PL=~');
  });

  test('clicking − shows toast "Effect removed from favorites!" and button switches back to +', async ({ page }) => {
    await loadPage(page);

    const effectRow = page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"])`);

    // Add first (heart only visible while effect is selected)
    await selectEffect(page, TEST_EFFECT_ID);
    await effectRow.locator('.fx-preset-add').click();
    await expect(effectRow.locator('.fx-preset-rm')).toBeVisible();

    // Now remove
    await effectRow.locator('.fx-preset-rm').click();
    await expect(page.locator('#toast')).toContainText('Effect removed from favorites!');
    await expect(effectRow.locator('.fx-preset-add')).toBeVisible();
    await expect(effectRow.locator('.fx-preset-rm')).toHaveCount(0);
  });

  test('removing a preset clears it from localStorage', async ({ page }) => {
    await loadPage(page);

    const effectRow = page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"])`);
    await selectEffect(page, TEST_EFFECT_ID);
    await effectRow.locator('.fx-preset-add').click();
    await effectRow.locator('.fx-preset-rm').click();

    const stored = await page.evaluate(() =>
      JSON.parse(localStorage.getItem('wled_user_fx_presets') || '[]')
    );
    expect(stored).toHaveLength(0);
  });

  test('page reload restores − button for a previously added preset', async ({ page }) => {
    await loadPage(page);

    // Add the preset (heart only visible while effect is selected)
    await selectEffect(page, TEST_EFFECT_ID);
    await page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"]) .fx-preset-add`).click();

    // Reload — mocks are re-registered before reload so they apply to the new navigation.
    // After reload the app defaults to the first tab; click Effects to make fxlist visible.
    await mockDeviceApi(page);
    await page.reload();
    await page.locator('.tablinks', { hasText: 'Effects' }).click();
    await page.waitForSelector('#fxlist .fx-preset-rm', { timeout: 10_000 });

    const effectRow = page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"])`);
    await expect(effectRow.locator('.fx-preset-rm')).toBeVisible();
    await expect(effectRow.locator('.fx-preset-add')).toHaveCount(0);
  });

});
