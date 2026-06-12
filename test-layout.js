const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();
  await page.goto('http://localhost:8080/index.htm', { waitUntil: 'domcontentloaded' });

  // Wait for the UI to load
  await page.waitForTimeout(1000);

  // Get the Colors tab
  const colorTab = await page.$('#Colors');
  if (colorTab) {
    // Click Colors tab
    await page.click('button:has-text("Colors")');
    await page.waitForTimeout(500);

    // Take screenshot
    await page.screenshot({ path: 'colors-tab-layout.png' });

    // Check width of content elements
    const colorContent = await page.locator('#Colors #qcs-w, #Colors #hexw, #Colors #sliders').first();
    const box = await colorContent.boundingBox();
    console.log('Colors tab element width:', box?.width);
  }

  // Get the Effects tab
  const effectsTab = await page.$('#Effects');
  if (effectsTab) {
    // Click Effects tab
    await page.click('button:has-text("Effects")');
    await page.waitForTimeout(500);

    // Take screenshot
    await page.screenshot({ path: 'effects-tab-layout.png' });

    // Check width
    const fxContent = await page.locator('#Effects #fx').first();
    const box = await fxContent.boundingBox();
    console.log('Effects tab element width:', box?.width);
  }

  // Get the Favorites tab
  const favTab = await page.$('#Favourites');
  if (favTab) {
    // Click Favorites tab
    await page.click('button:has-text("Presets")');
    await page.waitForTimeout(500);

    // Take screenshot
    await page.screenshot({ path: 'favorites-tab-layout.png' });

    // Check width
    const palContent = await page.locator('#Favourites #palw').first();
    const box = await palContent.boundingBox();
    console.log('Favorites tab element width:', box?.width);
  }

  await browser.close();
})();
