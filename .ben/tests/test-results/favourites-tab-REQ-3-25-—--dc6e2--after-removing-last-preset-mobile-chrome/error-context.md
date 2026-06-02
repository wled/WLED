# Instructions

- Following Playwright test failed.
- Explain why, be concise, respect Playwright best practices.
- Provide a snippet of code with the fix, if possible.

# Test info

- Name: favourites-tab.spec.js >> REQ 3.25 — no error after removing last user preset >> no error toast after removing last preset
- Location: specs\favourites-tab.spec.js:156:3

# Error details

```
Error: expect(locator).toContainText(expected) failed

Locator: locator('#toast')
Expected substring: "Effect removed from favorites!"
Received string:    "Effect added to favorites!"
Timeout: 5000ms

Call log:
  - Expect "toContainText" with timeout 5000ms
  - waiting for locator('#toast')
    4 × locator resolved to <div id="toast" class="show-green" onclick="clearErrorToast(100);">Effect added to favorites!</div>
      - unexpected value "Effect added to favorites!"
    10 × locator resolved to <div class="" id="toast" onclick="clearErrorToast(100);">Effect added to favorites!</div>
       - unexpected value "Effect added to favorites!"

```

```yaml
- text: Effect added to favorites!
```

# Test source

```ts
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
  139 | test.describe('REQ 3.25 — no error after removing last user preset', () => {
  140 | 
  141 |   test('removing last preset does not send ps:101 to device', async ({ page }) => {
  142 |     const jsonPosts = [];
  143 |     await loadPage(page, jsonPosts);
  144 |     await addEffect(page);
  145 | 
  146 |     jsonPosts.length = 0; // clear posts from the add operation
  147 | 
  148 |     await goToEffects(page);
  149 |     await page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"]) .fx-preset-rm`).click();
  150 |     await page.waitForTimeout(300);
  151 | 
  152 |     const playlistSwitch = jsonPosts.filter(b => b.ps === 101);
  153 |     expect(playlistSwitch).toHaveLength(0);
  154 |   });
  155 | 
  156 |   test('no error toast after removing last preset', async ({ page }) => {
  157 |     await loadPage(page);
  158 |     await addEffect(page);
  159 | 
  160 |     await goToEffects(page);
  161 |     await page.locator(`#fxlist .fx-pill-row:has(.lstI[data-id="${TEST_EFFECT_ID}"]) .fx-preset-rm`).click();
  162 |     await page.waitForTimeout(300);
  163 | 
  164 |     // Toast must show the success message only — no error variant
  165 |     const toast = page.locator('#toast');
> 166 |     await expect(toast).toContainText('Effect removed from favorites!');
      |                         ^ Error: expect(locator).toContainText(expected) failed
  167 |     await expect(toast).not.toContainText('Error');
  168 |   });
  169 | 
  170 | });
  171 | 
```