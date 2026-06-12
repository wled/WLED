const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();
  await page.setViewportSize({ width: 375, height: 812 });

  // Check the arrow demo page
  await page.goto('http://localhost:8080/arrow-demo.htm');
  await page.waitForTimeout(1000);

  // Take screenshot of demo
  await page.screenshot({ path: 'demo-arrows.png', fullPage: true });
  console.log('Demo screenshot saved: demo-arrows.png');

  // Check the main page with mock favorites
  await page.goto('http://localhost:8080/index.htm');
  await page.waitForTimeout(2000);

  // Click on Favorites tab (button at index 3)
  const favTab = page.locator('button.tablinks').nth(3);
  await favTab.click();
  await page.waitForTimeout(500);

  // Inject some mock preset data to simulate favorites
  await page.evaluate(() => {
    const mockPresets = {
      "1": { "name": "Preset 1", "ql": 1 },
      "2": { "name": "Preset 2", "ql": 1 },
      "3": { "name": "Preset 3", "ql": 1 }
    };
    localStorage.setItem('wledP', JSON.stringify(mockPresets));
    localStorage.setItem('wledPm', JSON.stringify([1, 2, 3])); // fav order
  });

  // Reload to see the presets
  await page.reload();
  await page.waitForTimeout(2000);

  // Take screenshot with favorites
  await page.screenshot({ path: 'favorites-with-arrows.png', fullPage: true });
  console.log('Favorites screenshot saved: favorites-with-arrows.png');

  // Check arrow elements
  const arrows = await page.locator('.fav-arrow-up, .fav-arrow-down').count();
  console.log(`Arrow buttons found: ${arrows}`);

  await browser.close();
})().catch(err => {
  console.error('Error:', err);
  process.exit(1);
});
