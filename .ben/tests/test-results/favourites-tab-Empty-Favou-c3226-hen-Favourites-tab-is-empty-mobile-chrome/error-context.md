# Instructions

- Following Playwright test failed.
- Explain why, be concise, respect Playwright best practices.
- Provide a snippet of code with the fix, if possible.

# Test info

- Name: favourites-tab.spec.js >> Empty Favourites state (no fx presets) >> shows Restore default favourites pill when Favourites tab is empty
- Location: specs\favourites-tab.spec.js:148:3

# Error details

```
Error: expect(locator).toBeVisible() failed

Locator: locator('#restoreDefaultBtn .lstIname')
Expected: visible
Timeout: 5000ms
Error: element(s) not found

Call log:
  - Expect "toBeVisible" with timeout 5000ms
  - waiting for locator('#restoreDefaultBtn .lstIname')

```

```yaml
- text: Loading WLED UI...
- button ""
- button "⌂"
- button ""
- text: 🔅
- slider: "128"
- paragraph: Apply colour changes, go back to effect and star it to save colour palette
- paragraph: Click buttons below to change the effect.
- radio "Solid" [checked]
- text: Solid ☆
- radio "Aurora"
- text: Aurora
- radio "Blink"
- text: Blink
- radio "Bouncing Balls"
- text: Bouncing Balls
- radio "Breathe"
- text: Breathe
- radio "Chase"
- text: Chase
- radio "Dots"
- text: Dots
- radio "Dynamic"
- text: Dynamic
- radio "Fire 2012"
- text: Fire 2012
- radio "Fireworks"
- text: Fireworks
- radio "Juggle"
- text: Juggle
- radio "Rain"
- text: Rain
- radio "Rainbow"
- text: Rainbow
- radio "Rainbow Runner"
- text: Rainbow Runner
- radio "Sinelon"
- text: Sinelon
- radio "Sparkle"
- text: Sparkle
- radio "Strobe"
- text: Strobe
- radio "Tetrix"
- text: Tetrix
- radio "Wave"
- text: Wave
- radio "Wipe"
- text: Wipe
- checkbox [checked]
- text:  Segment 0 
- checkbox [checked]
- text:  Add segment
- paragraph:
  - text: "Transition:"
  - spinbutton: "0.7"
  - text: s
- combobox:
  - option "Fade" [selected]
  - option "Fairy Dust"
  - option "Swipe right"
  - option "Swipe left"
  - option "Push right"
  - option "Push left"
  - option "Outside-in"
  - option "Inside-out"
- paragraph: Re-order your effects by using the arrows, or remove by clicking on the star.
- paragraph: You have no favorites saved
- button "Colours"
- button "Effects"
- button "Favourites"
- text: WLED
- button ""
- text: Loading...
- button "Refresh"
- button "Instance List"
- button "Update WLED"
- button "Reboot WLED"
- text: Made with ❤︎ by
- link "Aircoookie":
  - /url: https://github.com/Aircoookie/
- text: and the
- link "WLED community":
  - /url: https://wled.discourse.group/
- button ""
- text: WLED instances Loading...
- button "Refresh"
- text: Loading...  ? To use built-in effects, use an override button below. You can return to realtime mode by pressing the star in the top left corner.
- button "Override once"
- button "Override until reboot"
- text: For best performance, it is recommended to turn off the streaming source when not in use.
```

# Test source

```ts
  52  |     window.selectedFx = id;
  53  |     const parent = document.getElementById('fxlist');
  54  |     const prev = parent && parent.querySelector('.lstI.selected');
  55  |     if (prev) prev.classList.remove('selected');
  56  |     const el = parent && parent.querySelector(`.lstI[data-id="${id}"]`);
  57  |     if (el) el.classList.add('selected');
  58  |   }, effectId);
  59  | }
  60  | 
  61  | async function addEffect(page) {
  62  |   await goToEffects(page);
  63  |   await selectEffect(page, TEST_EFFECT_ID);
  64  |   await page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"]) .fx-preset-add`).click();
  65  |   await page.waitForTimeout(300);
  66  | }
  67  | 
  68  | // ---------------------------------------------------------------------------
  69  | // Tests
  70  | // ---------------------------------------------------------------------------
  71  | 
  72  | test.describe('Favourites tab UI (REQ 3.24)', () => {
  73  | 
  74  |   test('search field is not visible', async ({ page }) => {
  75  |     await loadPage(page);
  76  |     await goToFavourites(page);
  77  |     await expect(page.locator('#psFind')).not.toBeVisible();
  78  |   });
  79  | 
  80  |   test('z_cycle_preset pill (101) is not shown', async ({ page }) => {
  81  |     await loadPage(page);
  82  |     await addEffect(page);
  83  |     await goToFavourites(page);
  84  |     await expect(page.locator('#p101o')).toHaveCount(0);
  85  |   });
  86  | 
  87  |   test('no dropdown arrow on pills', async ({ page }) => {
  88  |     await loadPage(page);
  89  |     await addEffect(page);
  90  |     await goToFavourites(page);
  91  |     await expect(page.locator('#pcont .e-icon')).toHaveCount(0);
  92  |   });
  93  | 
  94  |   test('no preset number on pills', async ({ page }) => {
  95  |     await loadPage(page);
  96  |     await addEffect(page);
  97  |     await goToFavourites(page);
  98  |     await expect(page.locator('#pcont .pid')).toHaveCount(0);
  99  |   });
  100 | 
  101 |   test('added effect pill shows a red − remove button', async ({ page }) => {
  102 |     await loadPage(page);
  103 |     await addEffect(page);
  104 |     await goToFavourites(page);
  105 |     // First user preset lands at ID 6 (after fixed presets 1–5)
  106 |     await expect(page.locator('.fx-pill-row:has(#p6o) .fx-preset-rm')).toBeVisible();
  107 |     await expect(page.locator('.fx-pill-row:has(#p6o) .fx-preset-add')).toHaveCount(0);
  108 |   });
  109 | 
  110 |   test('clicking − on Favourites pill removes effect and restores + on Effects tab', async ({ page }) => {
  111 |     await loadPage(page);
  112 |     await addEffect(page);
  113 |     await goToFavourites(page);
  114 | 
  115 |     await page.locator('.fx-pill-row:has(#p6o) .fx-preset-rm').click();
  116 |     await expect(page.locator('#toast')).toContainText('Effect removed from favorites!');
  117 | 
  118 |     // Pill must disappear after upload completes
  119 |     await page.waitForTimeout(300);
  120 |     await expect(page.locator('#p6o')).toHaveCount(0);
  121 | 
  122 |     // Effects tab must show + again for this effect
  123 |     await goToEffects(page);
  124 |     const effectRow = page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"])`);
  125 |     await expect(effectRow.locator('.fx-preset-add')).toBeVisible();
  126 |     await expect(effectRow.locator('.fx-preset-rm')).toHaveCount(0);
  127 |   });
  128 | 
  129 |   test('Favourites tab shows no pills when no effects are added', async ({ page }) => {
  130 |     await loadPage(page);
  131 |     await goToFavourites(page);
  132 |     // Fixture has only preset 101 (z_cycle_preset), which is filtered out — 0 preset pills expected
  133 |     // (.pres.lstI targets actual pills; .pres.c is the "no presets" error message, not a pill)
  134 |     await expect(page.locator('#pcont .pres.lstI')).toHaveCount(0);
  135 |   });
  136 | 
  137 | });
  138 | 
  139 | test.describe('Empty Favourites state (no fx presets)', () => {
  140 | 
  141 |   test('shows "You have no favorites saved" when Favourites tab is empty', async ({ page }) => {
  142 |     await loadPage(page);
  143 |     await goToFavourites(page);
  144 |     await expect(page.locator('#noFavsMsg')).toBeVisible();
  145 |     await expect(page.locator('#noFavsMsg')).toHaveText('You have no favorites saved');
  146 |   });
  147 | 
  148 |   test('shows Restore default favourites pill when Favourites tab is empty', async ({ page }) => {
  149 |     await loadPage(page);
  150 |     await goToFavourites(page);
  151 |     const pill = page.locator('#restoreDefaultBtn .lstIname');
> 152 |     await expect(pill).toBeVisible();
      |                        ^ Error: expect(locator).toBeVisible() failed
  153 |     await expect(pill).toHaveText('Restore default favourites');
  154 |   });
  155 | 
  156 |   test('Presets heading is hidden when Favourites tab is empty', async ({ page }) => {
  157 |     await loadPage(page);
  158 |     await goToFavourites(page);
  159 |     await expect(page.locator('#presetsHeading')).not.toBeVisible();
  160 |   });
  161 | 
  162 |   test('pql quick-links are hidden when Favourites tab is empty', async ({ page }) => {
  163 |     await loadPage(page);
  164 |     await goToFavourites(page);
  165 |     await expect(page.locator('#pql')).not.toBeVisible();
  166 |   });
  167 | 
  168 |   test('Restore default favourites button uploads cleaned presets and clears fx presets', async ({ page }) => {
  169 |     const capturedUploads = [];
  170 |     await mockDeviceApi(page, []);
  171 |     // Override upload to capture body
  172 |     await page.route('**/upload', async r => {
  173 |       capturedUploads.push(r.request().postData() || '');
  174 |       r.fulfill({ status: 200, body: 'OK' });
  175 |     });
  176 |     await page.addInitScript(() => { localStorage.setItem('pcm', 'false'); });
  177 |     // Pre-seed an fx preset so there is something to restore from
  178 |     await page.addInitScript(() => {
  179 |       localStorage.setItem('wled_user_fx_presets', JSON.stringify([
  180 |         { effectId: 1, presetId: 6, effectName: 'Blink' }
  181 |       ]));
  182 |     });
  183 |     await page.goto('/index.htm');
  184 |     await goToFavourites(page);
  185 |     await page.locator('#restoreDefaultBtn .lstIname').click();
  186 |     await page.waitForTimeout(600);
  187 | 
  188 |     // After restore, localStorage fx presets must be empty
  189 |     const stored = await page.evaluate(() => localStorage.getItem('wled_user_fx_presets'));
  190 |     expect(JSON.parse(stored || '[]')).toHaveLength(0);
  191 | 
  192 |     // An upload must have been made
  193 |     expect(capturedUploads.length).toBeGreaterThan(0);
  194 |     // The upload body must contain z_cycle_preset reset to P1=1&P2=5
  195 |     const body = capturedUploads.join('');
  196 |     expect(body).toContain('z_cycle_preset');
  197 |     expect(body).toContain('P1=1');
  198 |   });
  199 | 
  200 |   test('after restore, "You have no favorites saved" message is still shown', async ({ page }) => {
  201 |     await mockDeviceApi(page, []);
  202 |     await page.route('**/upload', r => r.fulfill({ status: 200, body: 'OK' }));
  203 |     await page.addInitScript(() => {
  204 |       localStorage.setItem('pcm', 'false');
  205 |       localStorage.setItem('wled_user_fx_presets', JSON.stringify([
  206 |         { effectId: 1, presetId: 6, effectName: 'Blink' }
  207 |       ]));
  208 |     });
  209 |     await page.goto('/index.htm');
  210 |     await goToFavourites(page);
  211 |     await page.locator('#restoreDefaultBtn .lstIname').click();
  212 |     await page.waitForTimeout(600);
  213 |     await expect(page.locator('#noFavsMsg')).toBeVisible();
  214 |   });
  215 | 
  216 | });
  217 | 
  218 | test.describe('REQ 3.25 — no error after removing last user preset', () => {
  219 | 
  220 |   test('removing last preset does not send ps:101 to device', async ({ page }) => {
  221 |     const jsonPosts = [];
  222 |     await loadPage(page, jsonPosts);
  223 |     await addEffect(page);
  224 | 
  225 |     jsonPosts.length = 0; // clear posts from the add operation
  226 | 
  227 |     await goToEffects(page);
  228 |     await page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"]) .fx-preset-rm`).click();
  229 |     await page.waitForTimeout(300);
  230 | 
  231 |     const playlistSwitch = jsonPosts.filter(b => b.ps === 101);
  232 |     expect(playlistSwitch).toHaveLength(0);
  233 |   });
  234 | 
  235 |   test('no error toast after removing last preset', async ({ page }) => {
  236 |     await loadPage(page);
  237 |     await addEffect(page);
  238 | 
  239 |     await goToEffects(page);
  240 |     await page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"]) .fx-preset-rm`).click();
  241 |     await page.waitForTimeout(300);
  242 | 
  243 |     // Toast must show the success message only — no error variant
  244 |     const toast = page.locator('#toast');
  245 |     await expect(toast).toContainText('Effect removed from favorites!');
  246 |     await expect(toast).not.toContainText('Error');
  247 |   });
  248 | 
  249 | });
  250 | 
```