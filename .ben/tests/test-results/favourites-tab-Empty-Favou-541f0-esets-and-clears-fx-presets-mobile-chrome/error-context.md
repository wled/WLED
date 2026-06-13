# Instructions

- Following Playwright test failed.
- Explain why, be concise, respect Playwright best practices.
- Provide a snippet of code with the fix, if possible.

# Test info

- Name: favourites-tab.spec.js >> Empty Favourites state (no fx presets) >> Restore default favourites button uploads cleaned presets and clears fx presets
- Location: specs\favourites-tab.spec.js:168:3

# Error details

```
Test timeout of 15000ms exceeded.
```

```
Error: locator.click: Test timeout of 15000ms exceeded.
Call log:
  - waiting for locator('#restoreDefaultBtn .lstIname')

```

# Page snapshot

```yaml
- generic [active] [ref=e1]:
  - generic: Loading WLED UI...
  - generic [ref=e3]:
    - generic:
      - generic [ref=e4]:
        - button "" [ref=e5] [cursor=pointer]:
          - generic [ref=e6]: 
        - button "⌂" [ref=e7] [cursor=pointer]:
          - generic [ref=e8]: ⌂
        - button "" [ref=e9] [cursor=pointer]:
          - generic [ref=e10]: 
      - generic [ref=e11]:
        - generic [ref=e12]: 🔅
        - slider [ref=e15] [cursor=pointer]: "128"
  - generic [ref=e16]:
    - paragraph [ref=e20]: Apply colour changes, go back to effect and star it to save colour palette
    - generic [ref=e22]:
      - paragraph [ref=e24]: Click buttons below to change the effect.
      - generic [ref=e25]:
        - generic [ref=e26]:
          - generic "(0)" [ref=e28] [cursor=pointer]:
            - radio "Solid" [checked]
            - generic [ref=e29]: Solid
          - generic "Add to cycle" [ref=e30] [cursor=pointer]: ☆
        - generic "(8)" [ref=e33] [cursor=pointer]:
          - radio "Aurora"
          - generic [ref=e34]: Aurora
        - generic [ref=e35]:
          - generic "(1)" [ref=e37] [cursor=pointer]:
            - radio "Blink"
            - generic [ref=e38]: Blink
          - generic "Remove from cycle" [ref=e39] [cursor=pointer]: ★
        - generic "(12)" [ref=e42] [cursor=pointer]:
          - radio "Bouncing Balls"
          - generic [ref=e43]: Bouncing Balls
        - generic "(2)" [ref=e46] [cursor=pointer]:
          - radio "Breathe"
          - generic [ref=e47]: Breathe
        - generic "(11)" [ref=e50] [cursor=pointer]:
          - radio "Chase"
          - generic [ref=e51]: Chase
        - generic "(15)" [ref=e54] [cursor=pointer]:
          - radio "Dots"
          - generic [ref=e55]: Dots
        - generic "(4)" [ref=e58] [cursor=pointer]:
          - radio "Dynamic"
          - generic [ref=e59]: Dynamic
        - generic "(19)" [ref=e62] [cursor=pointer]:
          - radio "Fire 2012"
          - generic [ref=e63]: Fire 2012
        - generic "(16)" [ref=e66] [cursor=pointer]:
          - radio "Fireworks"
          - generic [ref=e67]: Fireworks
        - generic "(14)" [ref=e70] [cursor=pointer]:
          - radio "Juggle"
          - generic [ref=e71]: Juggle
        - generic "(17)" [ref=e74] [cursor=pointer]:
          - radio "Rain"
          - generic [ref=e75]: Rain
        - generic "(6)" [ref=e78] [cursor=pointer]:
          - radio "Rainbow"
          - generic [ref=e79]: Rainbow
        - generic "(7)" [ref=e82] [cursor=pointer]:
          - radio "Rainbow Runner"
          - generic [ref=e83]: Rainbow Runner
        - generic "(13)" [ref=e86] [cursor=pointer]:
          - radio "Sinelon"
          - generic [ref=e87]: Sinelon
        - generic "(9)" [ref=e90] [cursor=pointer]:
          - radio "Sparkle"
          - generic [ref=e91]: Sparkle
        - generic "(10)" [ref=e94] [cursor=pointer]:
          - radio "Strobe"
          - generic [ref=e95]: Strobe
        - generic "(18)" [ref=e98] [cursor=pointer]:
          - radio "Tetrix"
          - generic [ref=e99]: Tetrix
        - generic "(5)" [ref=e102] [cursor=pointer]:
          - radio "Wave"
          - generic [ref=e103]: Wave
        - generic "(3)" [ref=e106] [cursor=pointer]:
          - radio "Wipe"
          - generic [ref=e107]: Wipe
    - generic [ref=e109]:
      - generic [ref=e111] [cursor=pointer]:
        - generic:
          - checkbox [checked]
          - generic "Select" [ref=e112]
        - generic [ref=e113]:
          - generic "(un)Freeze" [ref=e114]: 
          - text: Segment 0
        - generic [ref=e115]: 
      - generic [ref=e117] [cursor=pointer]:
        - generic:
          - checkbox [checked]
          - generic "Select all" [ref=e118]
        - generic [ref=e119]:
          - generic [ref=e120]: 
          - text: Add segment
      - paragraph [ref=e121]:
        - text: "Transition:"
        - spinbutton [ref=e122]: "0.7"
        - text: s
      - combobox [ref=e124] [cursor=pointer]:
        - option "Fade" [selected]
        - option "Fairy Dust"
        - option "Swipe right"
        - option "Swipe left"
        - option "Push right"
        - option "Push left"
        - option "Outside-in"
        - option "Inside-out"
    - generic [ref=e125]:
      - paragraph [ref=e127]: Re-order your effects by using the arrows, or remove by clicking on the star.
      - paragraph [ref=e129]: You have no favorites saved
  - generic [ref=e131]:
    - button "Colours" [ref=e132] [cursor=pointer]
    - button "Effects" [ref=e133] [cursor=pointer]
    - button "Favourites" [ref=e134] [cursor=pointer]
  - generic [ref=e136] [cursor=pointer]: WLED
  - generic [ref=e137]:
    - button "" [ref=e138] [cursor=pointer]:
      - generic [ref=e139]: 
    - generic [ref=e141]: Loading...
    - generic [ref=e142]:
      - button "Refresh" [ref=e143] [cursor=pointer]
      - button "Instance List" [ref=e144] [cursor=pointer]
      - button "Update WLED" [ref=e145] [cursor=pointer]
      - button "Reboot WLED" [ref=e146] [cursor=pointer]
    - generic [ref=e147]:
      - text: Made with ❤︎ by
      - link "Aircoookie" [ref=e148] [cursor=pointer]:
        - /url: https://github.com/Aircoookie/
      - text: and the
      - link "WLED community" [ref=e149] [cursor=pointer]:
        - /url: https://wled.discourse.group/
  - generic [ref=e150]:
    - button "" [ref=e151] [cursor=pointer]:
      - generic [ref=e152]: 
    - generic [ref=e153]: WLED instances
    - generic [ref=e154]: Loading...
    - button "Refresh" [ref=e156] [cursor=pointer]
  - generic [ref=e158]: Loading...
  - generic [ref=e159]:
    - generic [ref=e160]: 
    - generic [ref=e161]: "?"
    - text: To use built-in effects, use an override button below.
    - text: You can return to realtime mode by pressing the star in the top left corner.
    - button "Override once" [ref=e162] [cursor=pointer]
    - button "Override until reboot" [ref=e163] [cursor=pointer]
    - text: For best performance, it is recommended to turn off the streaming source when not in use.
```

# Test source

```ts
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
  152 |     await expect(pill).toBeVisible();
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
> 185 |     await page.locator('#restoreDefaultBtn .lstIname').click();
      |                                                        ^ Error: locator.click: Test timeout of 15000ms exceeded.
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