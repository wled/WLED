const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();
  
  await page.setViewportSize({ width: 480, height: 900 });
  await page.goto('http://localhost:8080/favorites-demo.htm');
  await page.waitForTimeout(1000);
  
  await page.screenshot({ path: 'favorites-arrows-right.png', fullPage: true });
  console.log('✓ Updated screenshot saved');
  
  await browser.close();
})().catch(console.error);
