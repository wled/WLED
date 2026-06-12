const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();
  
  await page.setViewportSize({ width: 480, height: 900 });
  await page.goto('http://localhost:8080/index.htm');
  await page.waitForTimeout(2000);
  
  // Take screenshot showing all three panels aligned
  await page.screenshot({ path: 'all-panels-aligned.png', fullPage: true });
  console.log('✓ All panels screenshot saved');
  
  // Measure spacing for all three panels
  const measurements = await page.evaluate(() => {
    const colorPanel = document.getElementById('colorsInfoPanel');
    const effectsPanel = document.getElementById('fxInfoPanel');
    const favsPanel = document.getElementById('presetsInfoPanel');
    
    const getTopSpacing = (panel) => {
      const style = window.getComputedStyle(panel);
      return {
        margin: style.margin,
        marginTop: style.marginTop,
        top: panel.getBoundingClientRect().top
      };
    };
    
    return {
      colors: getTopSpacing(colorPanel),
      effects: getTopSpacing(effectsPanel),
      favs: getTopSpacing(favsPanel)
    };
  });
  
  console.log('Panel measurements:', JSON.stringify(measurements, null, 2));
  
  await browser.close();
})().catch(console.error);
