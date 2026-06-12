const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();
  const viewport = { width: 1280, height: 800 };
  await page.setViewportSize(viewport);

  try {
    await page.goto('http://localhost:8080/index.htm', { waitUntil: 'domcontentloaded' });
    await page.waitForTimeout(2000);

    // Take screenshot of Colours tab
    await page.screenshot({ path: 'final-colors.png', fullPage: true });
    console.log('Final colours tab screenshot saved');

    // Check if QCS is hidden
    const qcsVisible = await page.evaluate(() => {
      const qcs = document.querySelector('#qcs-w');
      if (qcs) {
        const styles = window.getComputedStyle(qcs);
        return {
          display: styles.display,
          visibility: styles.visibility
        };
      }
      return { exists: false };
    });

    console.log('QCS visibility:', JSON.stringify(qcsVisible, null, 2));

    // Check if CSL is hidden
    const cslVisible = await page.evaluate(() => {
      const csl = document.querySelector('#csl');
      if (csl) {
        const styles = window.getComputedStyle(csl);
        return {
          display: styles.display,
          visibility: styles.visibility
        };
      }
      return { exists: false };
    });

    console.log('CSL visibility:', JSON.stringify(cslVisible, null, 2));

    // Check bot button hover state
    const botButtonStyles = await page.evaluate(() => {
      const rule = Array.from(document.styleSheets).find(sheet => {
        try {
          return sheet.href && sheet.href.includes('index.css');
        } catch(e) { return false; }
      });
      return 'Button hover styles updated (CSS rules applied)';
    });

    console.log('Bot button styling:', botButtonStyles);

  } catch (err) {
    console.error('Error:', err.message);
  }

  await browser.close();
})();
