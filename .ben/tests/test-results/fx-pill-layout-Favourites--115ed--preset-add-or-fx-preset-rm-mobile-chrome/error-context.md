# Instructions

- Following Playwright test failed.
- Explain why, be concise, respect Playwright best practices.
- Provide a snippet of code with the fix, if possible.

# Test info

- Name: fx-pill-layout.spec.js >> Favourites page — non-cycle presets show non-interactive outline heart >> .fx-preset-no-action heart on Favourites is not .fx-preset-add or .fx-preset-rm
- Location: specs\fx-pill-layout.spec.js:204:3

# Error details

```
Error: expect(locator).toBeVisible() failed

Locator: locator('.fx-pill-row:has(#p2o) .fx-preset-no-action')
Expected: visible
Timeout: 5000ms
Error: element(s) not found

Call log:
  - Expect "toBeVisible" with timeout 5000ms
  - waiting for locator('.fx-pill-row:has(#p2o) .fx-preset-no-action')

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
- text: My Preset ★
- button "Move up" [disabled]:
  - img
- button "Move down" [disabled]:
  - img
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
  119 |     await expect(solidRow).toBeVisible();
  120 |     await expect(solidRow.locator('.fx-preset-btn')).toHaveCount(1);
  121 |   });
  122 | 
  123 |   test('Solid pill shows outline star (not added) by default', async ({ page }) => {
  124 |     await loadPage(page);
  125 |     await goToEffects(page);
  126 | 
  127 |     const solidRow = page.locator('#fxlist .fx-pill-row:has(.lstI[data-id="0"])');
  128 |     await expect(solidRow.locator('.fx-preset-add')).toBeVisible();
  129 |     await expect(solidRow.locator('.fx-preset-rm')).toHaveCount(0);
  130 |   });
  131 | 
  132 | });
  133 | 
  134 | test.describe('fx pill row layout — outline heart visibility (REQ 3.26.2a)', () => {
  135 | 
  136 |   test('non-Solid effect outline heart is hidden by default and visible only when effect is selected', async ({ page }) => {
  137 |     await loadPage(page);
  138 |     await goToEffects(page);
  139 | 
  140 |     const row = page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"])`);
  141 |     // Hidden (visibility:hidden) before the effect is selected
  142 |     await expect(row.locator('> .fx-preset-add')).not.toBeVisible();
  143 |     await expect(row.locator('> .fx-preset-rm')).toHaveCount(0);
  144 | 
  145 |     // Becomes visible once the effect is selected
  146 |     await selectEffect(page, TEST_EFFECT_ID);
  147 |     await expect(row.locator('> .fx-preset-add')).toBeVisible();
  148 |   });
  149 | 
  150 |   test('after adding, heart switches to solid red (fx-preset-rm) in the row', async ({ page }) => {
  151 |     await loadPage(page);
  152 |     await goToEffects(page);
  153 |     await selectEffect(page, TEST_EFFECT_ID);
  154 | 
  155 |     const row = page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"])`);
  156 |     await row.locator('> .fx-preset-add').click();
  157 | 
  158 |     await expect(row.locator('> .fx-preset-rm')).toBeVisible();
  159 |     await expect(row.locator('> .fx-preset-add')).toHaveCount(0);
  160 |   });
  161 | 
  162 |   test('after removing, heart switches back to outline (fx-preset-add) in the row', async ({ page }) => {
  163 |     await loadPage(page);
  164 |     await goToEffects(page);
  165 |     await selectEffect(page, TEST_EFFECT_ID);
  166 | 
  167 |     const row = page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"])`);
  168 |     await row.locator('> .fx-preset-add').click();
  169 |     await row.locator('> .fx-preset-rm').click();
  170 | 
  171 |     // Effect still selected so outline heart is visible again
  172 |     await expect(row.locator('> .fx-preset-add')).toBeVisible();
  173 |     await expect(row.locator('> .fx-preset-rm')).toHaveCount(0);
  174 |   });
  175 | 
  176 | });
  177 | 
  178 | test.describe('Favourites page — non-cycle presets show non-interactive outline heart', () => {
  179 | 
  180 |   test('a non-effect preset on Favourites shows .fx-preset-no-action heart', async ({ page }) => {
  181 |     // Seed a regular (non-effect) preset via localStorage so it appears on the Favourites tab.
  182 |     // Preset ID 2 is a plain preset — not in the wled_user_fx_presets list.
  183 |     await mockDeviceApi(page);
  184 |     await page.addInitScript(() => {
  185 |       localStorage.setItem('pcm', 'false');
  186 |     });
  187 |     // Route presets.json with a plain non-effect preset
  188 |     await page.route('**/presets.json', r => r.fulfill({
  189 |       json: {
  190 |         '2':   { n: 'My Preset', seg: [{ fx: 0 }] },
  191 |         '101': { n: 'z_cycle_preset', win: 'P1=1&P2=0&PL=~' }
  192 |       }
  193 |     }));
  194 |     await page.goto('/index.htm');
  195 |     await page.evaluate(() => localStorage.removeItem('wled_user_fx_presets'));
  196 |     await page.reload();
  197 | 
  198 |     await goToFavourites(page);
  199 |     // Preset 2 is not an effect preset — its pill row heart must be the non-action variant
  200 |     const row2 = page.locator('.fx-pill-row:has(#p2o)');
  201 |     await expect(row2.locator('.fx-preset-no-action')).toBeVisible();
  202 |   });
  203 | 
  204 |   test('.fx-preset-no-action heart on Favourites is not .fx-preset-add or .fx-preset-rm', async ({ page }) => {
  205 |     await mockDeviceApi(page);
  206 |     await page.addInitScript(() => { localStorage.setItem('pcm', 'false'); });
  207 |     await page.route('**/presets.json', r => r.fulfill({
  208 |       json: {
  209 |         '2':   { n: 'My Preset', seg: [{ fx: 0 }] },
  210 |         '101': { n: 'z_cycle_preset', win: 'P1=1&P2=0&PL=~' }
  211 |       }
  212 |     }));
  213 |     await page.goto('/index.htm');
  214 |     await page.evaluate(() => localStorage.removeItem('wled_user_fx_presets'));
  215 |     await page.reload();
  216 | 
  217 |     await goToFavourites(page);
  218 |     const noActionHeart = page.locator('.fx-pill-row:has(#p2o) .fx-preset-no-action');
> 219 |     await expect(noActionHeart).toBeVisible();
      |                                 ^ Error: expect(locator).toBeVisible() failed
  220 |     await expect(noActionHeart).not.toHaveClass(/fx-preset-add/);
  221 |     await expect(noActionHeart).not.toHaveClass(/fx-preset-rm/);
  222 |   });
  223 | 
  224 |   test('added effect preset on Favourites shows red solid heart (.fx-preset-rm), not no-action', async ({ page }) => {
  225 |     await loadPage(page);
  226 |     await addEffect(page);
  227 |     await goToFavourites(page);
  228 | 
  229 |     // Preset ID 6 is the first user-effect preset
  230 |     const row6 = page.locator('.fx-pill-row:has(#p6o)');
  231 |     await expect(row6.locator('.fx-preset-rm')).toBeVisible();
  232 |     await expect(row6.locator('.fx-preset-no-action')).toHaveCount(0);
  233 |   });
  234 | 
  235 | });
  236 | 
```