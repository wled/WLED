const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();
  const viewport = { width: 1280, height: 800 };
  await page.setViewportSize(viewport);

  try {
    await page.goto('http://localhost:8080/index.htm', { waitUntil: 'domcontentloaded' });
    await page.waitForTimeout(2000);

    // Take screenshot showing Colours tab with info panel
    await page.screenshot({ path: 'colors-tab-info.png', fullPage: true });
    console.log('Colours tab screenshot saved');

    // Check for info panel
    const infoPanelExists = await page.evaluate(() => {
      const panel = document.querySelector('#colorsInfoPanel');
      if (panel) {
        const text = panel.innerText;
        const styles = window.getComputedStyle(panel);
        return {
          exists: true,
          text: text,
          display: styles.display,
          marginTop: styles.marginTop,
          marginBottom: styles.marginBottom,
          padding: styles.padding
        };
      }
      return { exists: false };
    });

    console.log('Colours info panel:', JSON.stringify(infoPanelExists, null, 2));

  } catch (err) {
    console.error('Error:', err.message);
  }

  await browser.close();
})();
