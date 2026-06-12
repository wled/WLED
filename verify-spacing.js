const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();
  
  await page.setViewportSize({ width: 480, height: 900 });
  await page.goto('http://localhost:8080/index.htm');
  await page.waitForTimeout(2000);
  
  await page.screenshot({ path: 'updated-spacing.png', fullPage: true });
  console.log('✓ Screenshot saved');
  
  // Verify the bottom margin
  const spacing = await page.evaluate(() => {
    const panel = document.getElementById('colorsInfoPanel');
    const style = window.getComputedStyle(panel);
    const vhInPx = window.innerHeight / 100;
    
    return {
      marginBottom: style.marginBottom,
      calculated2vhInPx: 2 * vhInPx,
      viewportHeight: window.innerHeight
    };
  });
  
  console.log('Info panel bottom margin:', JSON.stringify(spacing, null, 2));
  
  await browser.close();
})().catch(console.error);
