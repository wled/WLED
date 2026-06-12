const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();
  
  await page.setViewportSize({ width: 480, height: 900 });
  await page.goto('http://localhost:8080/index.htm');
  await page.waitForTimeout(2000);
  
  // Get button gap measurements from settings page
  const buttonGap = await page.evaluate(() => {
    // Calculate what 2vh is in pixels
    const vhInPx = window.innerHeight / 100;
    const gap2vh = 2 * vhInPx;
    
    return {
      viewportHeight: window.innerHeight,
      oneVhInPx: vhInPx,
      gap2vhInPx: gap2vh,
      buttonMargin: '2vh (top margin only, bottom is 0)'
    };
  });
  
  console.log('Settings page button gap:', JSON.stringify(buttonGap, null, 2));
  
  await browser.close();
})().catch(console.error);
