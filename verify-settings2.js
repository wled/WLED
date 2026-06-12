const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();
  
  await page.setViewportSize({ width: 480, height: 900 });
  // Try the main page with settings link
  await page.goto('http://localhost:8080/index.htm');
  await page.waitForTimeout(2000);
  
  // Click settings button (cog icon in bottom nav)
  await page.click('button.bot-settings');
  await page.waitForTimeout(1000);
  
  await page.screenshot({ path: 'settings-panel.png', fullPage: true });
  console.log('✓ Settings page screenshot saved');
  
  await browser.close();
})().catch(console.error);
