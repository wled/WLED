const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();
  
  await page.setViewportSize({ width: 480, height: 900 });
  await page.goto('http://localhost:8080/index.htm');
  await page.waitForTimeout(2000);
  
  // Take screenshot of Colors tab (should be open by default)
  await page.screenshot({ path: 'colors-info-panel.png', fullPage: true });
  console.log('✓ Colors tab screenshot saved');
  
  // Click on Effects tab
  const tabs = page.locator('button.tablinks');
  await tabs.nth(1).click();
  await page.waitForTimeout(500);
  
  // Take screenshot of Effects tab
  await page.screenshot({ path: 'effects-info-panel.png', fullPage: true });
  console.log('✓ Effects tab screenshot saved');
  
  await browser.close();
})().catch(console.error);
