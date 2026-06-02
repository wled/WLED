// @ts-check
const { test, expect } = require('@playwright/test');

const effects   = require('../fixtures/effects.json');
const fxdata    = require('../fixtures/fxdata.json');
const palettes  = require('../fixtures/palettes.json');
const stateInfo = require('../fixtures/state-info.json');
const presets   = require('../fixtures/presets.json');

const TEST_EFFECT_NAME = 'Blink';
const TEST_EFFECT_ID   = effects.indexOf(TEST_EFFECT_NAME); // 1

async function mockDeviceApi(page, capturedJsonPosts = []) {
  await page.route('**/json/**', async r => {
    if (r.request().method() === 'POST') {
      try { capturedJsonPosts.push(JSON.parse(r.request().postData() || '{}')); } catch (_) {}
    }
    r.fulfill({ json: { success: true } });
  });
  await page.route('**/json/palx**',   r => r.fulfill({ json: { m: 0, p: {} } }));
  await page.route('**/json/si',       r => r.fulfill({ json: stateInfo }));
  await page.route('**/json/effects',  r => r.fulfill({ json: effects }));
  await page.route('**/json/fxdata',   r => r.fulfill({ json: fxdata }));
  await page.route('**/json/palettes', r => r.fulfill({ json: palettes }));
  await page.route('**/presets.json',  r => r.fulfill({ json: presets }));
  await page.route('**/edit**',        r => r.fulfill({ json: { version: stateInfo.info.ver, neverAsk: true } }));
  await page.route('**/skin.css',      r => r.fulfill({ status: 200, contentType: 'text/css', body: '' }));
  await page.route('**/upload',        r => r.fulfill({ status: 200, contentType: 'text/plain', body: 'OK' }));
  await page.route('**/ws',            r => r.abort());
}

async function loadPage(page, capturedJsonPosts = []) {
  await mockDeviceApi(page, capturedJsonPosts);
  await page.addInitScript(() => { localStorage.setItem('pcm', 'false'); });
  await page.goto('/index.htm');
  await page.evaluate(() => localStorage.removeItem('wled_user_fx_presets'));
  await page.reload();
}

async function goToEffects(page) {
  await page.locator('.tablinks', { hasText: 'Effects' }).click();
  // Hearts are visibility:hidden until the effect is selected — wait for DOM attachment only.
  await page.waitForSelector('#fxlist .fx-preset-add', { timeout: 10_000, state: 'attached' });
}

async function goToFavourites(page) {
  await page.locator('.tablinks', { hasText: 'Favourites' }).click();
}

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

async function addEffect(page) {
  await goToEffects(page);
  await selectEffect(page, TEST_EFFECT_ID);
  await page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"]) .fx-preset-add`).click();
  await page.waitForTimeout(300);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

test.describe('Favourites tab UI (REQ 3.24)', () => {

  test('search field is not visible', async ({ page }) => {
    await loadPage(page);
    await goToFavourites(page);
    await expect(page.locator('#psFind')).not.toBeVisible();
  });

  test('z_cycle_preset pill (101) is not shown', async ({ page }) => {
    await loadPage(page);
    await addEffect(page);
    await goToFavourites(page);
    await expect(page.locator('#p101o')).toHaveCount(0);
  });

  test('no dropdown arrow on pills', async ({ page }) => {
    await loadPage(page);
    await addEffect(page);
    await goToFavourites(page);
    await expect(page.locator('#pcont .e-icon')).toHaveCount(0);
  });

  test('no preset number on pills', async ({ page }) => {
    await loadPage(page);
    await addEffect(page);
    await goToFavourites(page);
    await expect(page.locator('#pcont .pid')).toHaveCount(0);
  });

  test('added effect pill shows a red − remove button', async ({ page }) => {
    await loadPage(page);
    await addEffect(page);
    await goToFavourites(page);
    // First user preset lands at ID 6 (after fixed presets 1–5)
    await expect(page.locator('.fx-pill-row:has(#p6o) .fx-preset-rm')).toBeVisible();
    await expect(page.locator('.fx-pill-row:has(#p6o) .fx-preset-add')).toHaveCount(0);
  });

  test('clicking − on Favourites pill removes effect and restores + on Effects tab', async ({ page }) => {
    await loadPage(page);
    await addEffect(page);
    await goToFavourites(page);

    await page.locator('.fx-pill-row:has(#p6o) .fx-preset-rm').click();
    await expect(page.locator('#toast')).toContainText('Effect removed from favorites!');

    // Pill must disappear after upload completes
    await page.waitForTimeout(300);
    await expect(page.locator('#p6o')).toHaveCount(0);

    // Effects tab must show + again for this effect
    await goToEffects(page);
    const effectRow = page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"])`);
    await expect(effectRow.locator('.fx-preset-add')).toBeVisible();
    await expect(effectRow.locator('.fx-preset-rm')).toHaveCount(0);
  });

  test('Favourites tab shows no pills when no effects are added', async ({ page }) => {
    await loadPage(page);
    await goToFavourites(page);
    // Fixture has only preset 101 (z_cycle_preset), which is filtered out — 0 preset pills expected
    // (.pres.lstI targets actual pills; .pres.c is the "no presets" error message, not a pill)
    await expect(page.locator('#pcont .pres.lstI')).toHaveCount(0);
  });

});

test.describe('REQ 3.25 — no error after removing last user preset', () => {

  test('removing last preset does not send ps:101 to device', async ({ page }) => {
    const jsonPosts = [];
    await loadPage(page, jsonPosts);
    await addEffect(page);

    jsonPosts.length = 0; // clear posts from the add operation

    await goToEffects(page);
    await page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"]) .fx-preset-rm`).click();
    await page.waitForTimeout(300);

    const playlistSwitch = jsonPosts.filter(b => b.ps === 101);
    expect(playlistSwitch).toHaveLength(0);
  });

  test('no error toast after removing last preset', async ({ page }) => {
    await loadPage(page);
    await addEffect(page);

    await goToEffects(page);
    await page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"]) .fx-preset-rm`).click();
    await page.waitForTimeout(300);

    // Toast must show the success message only — no error variant
    const toast = page.locator('#toast');
    await expect(toast).toContainText('Effect removed from favorites!');
    await expect(toast).not.toContainText('Error');
  });

});
