// @ts-check
// Tests for the fx pill row layout: heart icon outside the pill, text centring,
// Solid has no heart, and Favourites page non-cycle presets show a non-interactive heart.
const { test, expect } = require('@playwright/test');

const effects   = require('../fixtures/effects.json');
const fxdata    = require('../fixtures/fxdata.json');
const palettes  = require('../fixtures/palettes.json');
const stateInfo = require('../fixtures/state-info.json');
const presets   = require('../fixtures/presets.json');

const TEST_EFFECT_NAME = 'Blink';
const TEST_EFFECT_ID   = effects.indexOf(TEST_EFFECT_NAME); // 1

async function mockDeviceApi(page) {
  await page.route('**/json/**',    r => r.fulfill({ json: { success: true } }));
  await page.route('**/json/palx**', r => r.fulfill({ json: { m: 0, p: {} } }));
  await page.route('**/json/si',    r => r.fulfill({ json: stateInfo }));
  await page.route('**/json/effects', r => r.fulfill({ json: effects }));
  await page.route('**/json/fxdata',  r => r.fulfill({ json: fxdata }));
  await page.route('**/json/palettes', r => r.fulfill({ json: palettes }));
  await page.route('**/presets.json', r => r.fulfill({ json: presets }));
  await page.route('**/edit**',     r => r.fulfill({ json: { version: stateInfo.info.ver, neverAsk: true } }));
  await page.route('**/skin.css',   r => r.fulfill({ status: 200, contentType: 'text/css', body: '' }));
  await page.route('**/upload',     r => r.fulfill({ status: 200, contentType: 'text/plain', body: 'OK' }));
  await page.route('**/ws',         r => r.abort());
}

async function loadPage(page) {
  await mockDeviceApi(page);
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
  // The heart is outside the .lstI pill — locate it via the .fx-pill-row wrapper
  await page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"]) > .fx-preset-add`).click();
  await page.waitForTimeout(300);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

test.describe('fx pill row layout — heart icon outside the pill', () => {

  test('non-Solid effect pill is wrapped in a .fx-pill-row div', async ({ page }) => {
    await loadPage(page);
    await goToEffects(page);

    // The .lstI for a non-Solid effect must be a direct child of .fx-pill-row
    const blinkRow = page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"])`);
    await expect(blinkRow).toBeVisible();
  });

  test('heart icon is a sibling of the pill, not nested inside it', async ({ page }) => {
    await loadPage(page);
    await goToEffects(page);

    // The .fx-preset-btn must be a direct child of .fx-pill-row (outside the .lstI pill)
    const row = page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"])`);
    // Heart icon inside .fx-pill-row but NOT inside .lstI
    const heartOutside = row.locator('> .fx-preset-btn');
    await expect(heartOutside).toHaveCount(1);
  });

  test('heart icon is NOT nested inside the .lstI pill element', async ({ page }) => {
    await loadPage(page);
    await goToEffects(page);

    const pill = page.locator(`#fxlist .lstI[data-id="${TEST_EFFECT_ID}"]`);
    await expect(pill.locator('.fx-preset-btn')).toHaveCount(0);
  });

  test('effect name text has centred alignment inside the pill', async ({ page }) => {
    await loadPage(page);
    await goToEffects(page);

    // #fxlist .lstIcontent must have text-align:center applied via CSS
    const content = page.locator(`#fxlist .lstI[data-id="${TEST_EFFECT_ID}"] .lstIcontent`).first();
    const textAlign = await content.evaluate(el => getComputedStyle(el).textAlign);
    expect(textAlign).toBe('center');
  });

});

test.describe('fx pill row layout — Solid effect (id=0) has a star', () => {

  test('Solid pill is wrapped in .fx-pill-row with a star icon', async ({ page }) => {
    await loadPage(page);
    await goToEffects(page);

    const solidRow = page.locator('#fxlist .fx-pill-row:has(.lstI[data-id="0"])');
    await expect(solidRow).toBeVisible();
    await expect(solidRow.locator('.fx-preset-btn')).toHaveCount(1);
  });

  test('Solid pill shows outline star (not added) by default', async ({ page }) => {
    await loadPage(page);
    await goToEffects(page);

    const solidRow = page.locator('#fxlist .fx-pill-row:has(.lstI[data-id="0"])');
    await expect(solidRow.locator('.fx-preset-add')).toBeVisible();
    await expect(solidRow.locator('.fx-preset-rm')).toHaveCount(0);
  });

});

test.describe('fx pill row layout — outline heart visibility (REQ 3.26.2a)', () => {

  test('non-Solid effect outline heart is hidden by default and visible only when effect is selected', async ({ page }) => {
    await loadPage(page);
    await goToEffects(page);

    const row = page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"])`);
    // Hidden (visibility:hidden) before the effect is selected
    await expect(row.locator('> .fx-preset-add')).not.toBeVisible();
    await expect(row.locator('> .fx-preset-rm')).toHaveCount(0);

    // Becomes visible once the effect is selected
    await selectEffect(page, TEST_EFFECT_ID);
    await expect(row.locator('> .fx-preset-add')).toBeVisible();
  });

  test('after adding, heart switches to solid red (fx-preset-rm) in the row', async ({ page }) => {
    await loadPage(page);
    await goToEffects(page);
    await selectEffect(page, TEST_EFFECT_ID);

    const row = page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"])`);
    await row.locator('> .fx-preset-add').click();

    await expect(row.locator('> .fx-preset-rm')).toBeVisible();
    await expect(row.locator('> .fx-preset-add')).toHaveCount(0);
  });

  test('after removing, heart switches back to outline (fx-preset-add) in the row', async ({ page }) => {
    await loadPage(page);
    await goToEffects(page);
    await selectEffect(page, TEST_EFFECT_ID);

    const row = page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"])`);
    await row.locator('> .fx-preset-add').click();
    await row.locator('> .fx-preset-rm').click();

    // Effect still selected so outline heart is visible again
    await expect(row.locator('> .fx-preset-add')).toBeVisible();
    await expect(row.locator('> .fx-preset-rm')).toHaveCount(0);
  });

});

test.describe('Favourites page — non-cycle presets show non-interactive outline heart', () => {

  test('a non-effect preset on Favourites shows .fx-preset-no-action heart', async ({ page }) => {
    // Seed a regular (non-effect) preset via localStorage so it appears on the Favourites tab.
    // Preset ID 2 is a plain preset — not in the wled_user_fx_presets list.
    await mockDeviceApi(page);
    await page.addInitScript(() => {
      localStorage.setItem('pcm', 'false');
    });
    // Route presets.json with a plain non-effect preset
    await page.route('**/presets.json', r => r.fulfill({
      json: {
        '2':   { n: 'My Preset', seg: [{ fx: 0 }] },
        '101': { n: 'z_cycle_preset', win: 'P1=1&P2=0&PL=~' }
      }
    }));
    await page.goto('/index.htm');
    await page.evaluate(() => localStorage.removeItem('wled_user_fx_presets'));
    await page.reload();

    await goToFavourites(page);
    // Preset 2 is not an effect preset — its pill row heart must be the non-action variant
    const row2 = page.locator('.fx-pill-row:has(#p2o)');
    await expect(row2.locator('.fx-preset-no-action')).toBeVisible();
  });

  test('.fx-preset-no-action heart on Favourites is not .fx-preset-add or .fx-preset-rm', async ({ page }) => {
    await mockDeviceApi(page);
    await page.addInitScript(() => { localStorage.setItem('pcm', 'false'); });
    await page.route('**/presets.json', r => r.fulfill({
      json: {
        '2':   { n: 'My Preset', seg: [{ fx: 0 }] },
        '101': { n: 'z_cycle_preset', win: 'P1=1&P2=0&PL=~' }
      }
    }));
    await page.goto('/index.htm');
    await page.evaluate(() => localStorage.removeItem('wled_user_fx_presets'));
    await page.reload();

    await goToFavourites(page);
    const noActionHeart = page.locator('.fx-pill-row:has(#p2o) .fx-preset-no-action');
    await expect(noActionHeart).toBeVisible();
    await expect(noActionHeart).not.toHaveClass(/fx-preset-add/);
    await expect(noActionHeart).not.toHaveClass(/fx-preset-rm/);
  });

  test('added effect preset on Favourites shows red solid heart (.fx-preset-rm), not no-action', async ({ page }) => {
    await loadPage(page);
    await addEffect(page);
    await goToFavourites(page);

    // Preset ID 6 is the first user-effect preset
    const row6 = page.locator('.fx-pill-row:has(#p6o)');
    await expect(row6.locator('.fx-preset-rm')).toBeVisible();
    await expect(row6.locator('.fx-preset-no-action')).toHaveCount(0);
  });

});
