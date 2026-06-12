const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();
  
  await page.setViewportSize({ width: 480, height: 900 });
  await page.goto('http://localhost:8080/index.htm');
  await page.waitForTimeout(2000);
  
  // Click on Favourites tab (4th button, index 3)
  const tabs = page.locator('button.tablinks');
  await tabs.nth(3).click();
  await page.waitForTimeout(500);
  
  await page.screenshot({ path: 'favourites-info-panel.png', fullPage: true });
  console.log('✓ Favourites tab screenshot saved');
  
  await browser.close();
})().catch(console.error);
