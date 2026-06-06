const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1200, height: 800 } });
  await page.goto('http://localhost:8080/simple_timer.htm');
  await page.screenshot({ path: './timer_screenshot.png', fullPage: true });
  console.log('Screenshot saved to ./timer_screenshot.png');
  await browser.close();
})();
