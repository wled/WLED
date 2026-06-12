const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();
  
  await page.setViewportSize({ width: 375, height: 667 });
  await page.goto('http://localhost:8080/index.htm');
  await page.waitForTimeout(2000);
  
  // Inject mock data
  await page.evaluate(() => {
    window.pJson = {
      "1": { "name": "Warm White" },
      "2": { "name": "Cool Blue" },
      "3": { "name": "Rainbow" }
    };
  });
  
  // Call populatePresets
  await page.evaluate(() => {
    if (typeof populatePresets === 'function') {
      populatePresets(false);
    }
  });
  
  await page.waitForTimeout(500);
  
  // Click Favorites tab (4th button, index 3)
  const tabs = page.locator('button.tablinks');
  const count = await tabs.count();
  console.log(`Found ${count} tabs`);
  
  if (count >= 4) {
    await tabs.nth(3).click();
    await page.waitForTimeout(500);
  }
  
  // Screenshot
  await page.screenshot({ path: 'favorites-demo.png', fullPage: true });
  console.log('Screenshot saved: favorites-demo.png');
  
  const arrows = await page.locator('.fav-arrow-up, .fav-arrow-down').count();
  console.log(`Arrow buttons on page: ${arrows}`);
  
  await browser.close();
})().catch(console.error);
