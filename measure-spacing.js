const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();
  
  await page.setViewportSize({ width: 480, height: 900 });
  await page.goto('http://localhost:8080/index.htm');
  await page.waitForTimeout(2000);
  
  // Colors tab should be open by default
  await page.screenshot({ path: 'colors-spacing.png', fullPage: true });
  console.log('✓ Colors tab screenshot saved');
  
  // Get info panel details
  const infoPanelTop = await page.evaluate(() => {
    const panel = document.getElementById('colorsInfoPanel');
    const rect = panel.getBoundingClientRect();
    const navBar = document.querySelector('.tab.top');
    const navRect = navBar.getBoundingClientRect();
    const spacing = rect.top - navRect.bottom;
    return {
      navBarBottom: navRect.bottom,
      infoPanelTop: rect.top,
      spacing: spacing,
      computedMargin: window.getComputedStyle(panel).margin
    };
  });
  
  console.log('Spacing info:', infoPanelTop);
  
  await browser.close();
})().catch(console.error);
