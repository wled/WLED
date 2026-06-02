// @ts-check
const { test, expect } = require('@playwright/test');

const effects   = require('../fixtures/effects.json');
const fxdata    = require('../fixtures/fxdata.json');
const palettes  = require('../fixtures/palettes.json');
const stateInfo = require('../fixtures/state-info.json');
const presets   = require('../fixtures/presets.json');

// Preset 2 is used as the active preset for all auto-save tests.
// It must exist in pJson so _schedulePresetAutoSave() does not bail out early.
const ACTIVE_PRESET_ID = 2;
const ACTIVE_PRESET_NAME = 'Test Preset';

// Presets fixture extended with preset 2 so pJson[2] is populated.
const presetsWithActive = {
  ...presets,
  [ACTIVE_PRESET_ID]: { n: ACTIVE_PRESET_NAME },
};

/**
 * Register all WLED device API mocks.
 *
 * @param {import('@playwright/test').Page} page
 * @param {object[]} capturedJsonPosts  Receives every JSON body POSTed to /json/si.
 * @param {object}   stateOverride      Optional replacement for the state-info fixture.
 */
async function mockDeviceApi(page, capturedJsonPosts = [], stateOverride = null) {
  const siResponse = stateOverride ?? {
    ...stateInfo,
    state: { ...stateInfo.state, ps: ACTIVE_PRESET_ID, pl: -1 },
  };

  // Playwright routes are LIFO — register the catch-all first so more-specific
  // routes registered afterwards take priority.
  await page.route('**/json/**', async r => {
    if (r.request().method() === 'POST') {
      try { capturedJsonPosts.push(JSON.parse(r.request().postData() || '{}')); } catch (_) {}
    }
    r.fulfill({ json: { success: true } });
  });
  await page.route('**/json/palx**',   r => r.fulfill({ json: { m: 0, p: {} } }));
  // /json/si handles both GET (initial state) and POST (commands).  POST bodies
  // must be captured here because the more-specific route takes LIFO priority
  // over the catch-all above and POSTs would otherwise never reach it.
  await page.route('**/json/si', async r => {
    if (r.request().method() === 'POST') {
      try { capturedJsonPosts.push(JSON.parse(r.request().postData() || '{}')); } catch (_) {}
    }
    r.fulfill({ json: siResponse });
  });
  await page.route('**/json/effects',  r => r.fulfill({ json: effects }));
  await page.route('**/json/fxdata',   r => r.fulfill({ json: fxdata }));
  await page.route('**/json/palettes', r => r.fulfill({ json: palettes }));
  await page.route('**/presets.json',  r => r.fulfill({ json: presetsWithActive }));
  await page.route('**/edit**',        r => r.fulfill({ json: { version: stateInfo.info.ver, neverAsk: true } }));
  await page.route('**/skin.css',      r => r.fulfill({ status: 200, contentType: 'text/css', body: '' }));
  await page.route('**/upload',        r => r.fulfill({ status: 200, contentType: 'text/plain', body: 'OK' }));
  await page.route('**/ws',            r => r.abort());
}

/**
 * Load the page and wait for initialisation to finish.
 */
async function loadPage(page, capturedJsonPosts = [], stateOverride = null) {
  await mockDeviceApi(page, capturedJsonPosts, stateOverride);
  await page.addInitScript(() => { localStorage.setItem('pcm', 'false'); });
  await page.goto('/index.htm');
  await page.evaluate(() => localStorage.removeItem('wled_user_fx_presets'));
  await page.reload();
  // Wait for initI() to complete: reqsLegal is set to true only after the first
  // successful /json/si response, which allows requestJson() to send commands.
  await page.waitForFunction(() => window.reqsLegal === true, { timeout: 10_000 });
  await page.bringToFront();
  // Fire a short in-page timer to warm up Chromium's timer subsystem.
  // Without this, the first test in a fresh browser context can have its
  // 500ms debounce timer throttled past the test timeout.
  await page.evaluate(() => new Promise(r => setTimeout(r, 50)));
  await page.locator('.tablinks', { hasText: 'Effects' }).click();
  await page.waitForSelector('#sliderSpeed', { timeout: 10_000, state: 'attached' });
}

/**
 * Fire a slider change event, wait for the immediate /json/si response
 * (keeps the Playwright event loop active so Chromium doesn't throttle the
 * debounce timer), then wait for the psave POST that the 500ms debounce fires.
 *
 * @param {import('@playwright/test').Page} page
 * @param {string} sliderId  e.g. 'sliderSpeed'
 * @param {number} value
 */
async function fireSliderAndWaitForPsave(page, sliderId, value) {
  // Listen for the slider-update response and the subsequent psave response.
  const sliderResponseDone = page.waitForResponse(
    r => r.url().includes('/json/si') && r.request().method() === 'POST',
    { timeout: 3_000 }
  );
  const psaveResponse = page.waitForResponse(
    r => r.url().includes('/json/si') &&
         r.request().method() === 'POST' &&
         (r.request().postData() || '').includes('"psave"'),
    { timeout: 5_000 }
  );

  await page.locator('#' + sliderId).evaluate((el, v) => {
    el.value = String(v);
    el.dispatchEvent(new Event('change'));
  }, value);

  // Await the slider-update response first.  This "warms" Playwright's network
  // event loop so the debounce timer fires promptly instead of being throttled.
  await sliderResponseDone;

  // Now wait for the psave that fires after the 500ms debounce.
  await psaveResponse;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

test.describe('preset auto-save on slider change', () => {

  test('moving the Speed slider sends psave with the active preset ID after 500ms', async ({ page }) => {
    const posts = [];
    await loadPage(page, posts);
    await fireSliderAndWaitForPsave(page, 'sliderSpeed', 200);

    const saveCalls = posts.filter(b => b.psave === ACTIVE_PRESET_ID);
    expect(saveCalls).toHaveLength(1);
    expect(saveCalls[0].n).toBe(ACTIVE_PRESET_NAME);
  });

  test('moving the Intensity slider sends psave with the active preset ID after 500ms', async ({ page }) => {
    const posts = [];
    await loadPage(page, posts);
    await fireSliderAndWaitForPsave(page, 'sliderIntensity', 50);

    const saveCalls = posts.filter(b => b.psave === ACTIVE_PRESET_ID);
    expect(saveCalls).toHaveLength(1);
    expect(saveCalls[0].n).toBe(ACTIVE_PRESET_NAME);
  });

  test('auto-save is debounced — rapid slider moves result in only one psave call', async ({ page }) => {
    const posts = [];
    await loadPage(page, posts);

    const psaveResponse = page.waitForResponse(
      r => r.url().includes('/json/si') &&
           r.request().method() === 'POST' &&
           (r.request().postData() || '').includes('"psave"'),
      { timeout: 5_000 }
    );
    // Warm the network loop with a single pre-fire so subsequent timers
    // aren't throttled, then fire two more rapid changes.
    const sliderResponseDone = page.waitForResponse(
      r => r.url().includes('/json/si') && r.request().method() === 'POST',
      { timeout: 3_000 }
    );
    await page.locator('#sliderSpeed').evaluate(el => {
      el.value = '100';
      el.dispatchEvent(new Event('change'));
    });
    await sliderResponseDone;

    // Remaining rapid changes — debounce resets each time.
    for (const val of [150, 200]) {
      await page.locator('#sliderSpeed').evaluate((el, v) => {
        el.value = String(v);
        el.dispatchEvent(new Event('change'));
      }, val);
    }

    // Exactly one psave should fire (the debounce collapsed all three).
    await psaveResponse;

    const saveCalls = posts.filter(b => b.psave === ACTIVE_PRESET_ID);
    expect(saveCalls).toHaveLength(1);
  });

  test('auto-save does not fire before the 500ms debounce window elapses', async ({ page }) => {
    const posts = [];
    await loadPage(page, posts);

    await page.locator('#sliderSpeed').evaluate(el => {
      el.value = '200';
      el.dispatchEvent(new Event('change'));
    });

    // Check before the debounce window has elapsed.
    await page.waitForTimeout(100);

    const saveCalls = posts.filter(b => b.psave === ACTIVE_PRESET_ID);
    expect(saveCalls).toHaveLength(0);
  });

  test('auto-save does not fire when no preset is active (currentPreset = 0)', async ({ page }) => {
    const noPresetState = {
      ...stateInfo,
      state: { ...stateInfo.state, ps: 0, pl: -1 },
    };

    const posts = [];
    await loadPage(page, posts, noPresetState);

    await page.locator('#sliderSpeed').evaluate(el => {
      el.value = '200';
      el.dispatchEvent(new Event('change'));
    });

    // The psave guard (currentPreset <= 0) should prevent any save.
    // Use a real-time wait — waitForResponse would time out, which is a test error.
    await page.waitForTimeout(1200);

    const saveCalls = posts.filter(b => b.psave !== undefined);
    expect(saveCalls).toHaveLength(0);
  });

});
