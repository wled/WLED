const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();

  // Set viewport to mobile-like to see responsive layout
  await page.setViewportSize({ width: 375, height: 812 });

  await page.goto('http://localhost:8080/index.htm');

  // Wait for page to load
  await page.waitForTimeout(2000);

  // Take screenshot of full page
  await page.screenshot({ path: 'arrows-full.png', fullPage: true });
  console.log('Screenshot saved: arrows-full.png');

  // Check CSS properties
  const pillRow = page.locator('.fx-pill-row').first();
  const gap = await pillRow.evaluate(el => {
    return window.getComputedStyle(el).gap;
  }).catch(() => 'N/A');
  console.log(`CSS gap: ${gap}`);

  // Count elements
  const arrowCount = await page.locator('.fav-arrow-up, .fav-arrow-down').count();
  console.log(`Arrow buttons found: ${arrowCount}`);

  await browser.close();
})().catch(err => {
  console.error('Error:', err.message);
  process.exit(1);
});
