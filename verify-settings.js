const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();
  
  await page.setViewportSize({ width: 480, height: 900 });
  await page.goto('http://localhost:8080/settings');
  await page.waitForTimeout(1000);
  
  await page.screenshot({ path: 'settings-info-panel.png', fullPage: true });
  console.log('✓ Settings page screenshot saved');
  
  await browser.close();
})().catch(console.error);
