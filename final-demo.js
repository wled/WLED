const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();
  
  await page.setViewportSize({ width: 480, height: 900 });
  
  // Show the demo page
  await page.goto('http://localhost:8080/favorites-demo.htm');
  await page.waitForTimeout(500);
  
  await page.screenshot({ path: 'final-demo.png', fullPage: true });
  console.log('✓ Final demo screenshot: arrows on right side next to star');
  
  await browser.close();
})().catch(console.error);
