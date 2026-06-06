const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();
  await page.goto('http://localhost:8080/simple_timer.htm', { waitUntil: 'networkidle' });
  await page.screenshot({ path: 'timer_screenshot.png', fullPage: true });
  console.log('Screenshot saved to timer_screenshot.png');
  await browser.close();
})();
