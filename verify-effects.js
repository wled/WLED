const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();
  const viewport = { width: 1280, height: 800 };
  await page.setViewportSize(viewport);

  try {
    await page.goto('http://localhost:8080/index.htm', { waitUntil: 'domcontentloaded' });
    await page.waitForTimeout(2000);

    // Take a screenshot of the entire page
    await page.screenshot({ path: 'effects-page.png', fullPage: true });
    console.log('Full page screenshot saved to effects-page.png');

    // Check for info panel
    const infoPanelExists = await page.evaluate(() => {
      const panel = document.querySelector('#fxInfoPanel');
      if (panel) {
        const text = panel.innerText;
        const styles = window.getComputedStyle(panel);
        return {
          exists: true,
          text: text,
          marginBottom: styles.marginBottom,
          padding: styles.padding,
          backgroundColor: styles.backgroundColor
        };
      }
      return { exists: false };
    });

    console.log('Info panel:', JSON.stringify(infoPanelExists, null, 2));

    // Check arrow styles if they exist
    const arrowStyles = await page.evaluate(() => {
      const arrow = document.querySelector('.fav-arrow-up');
      if (arrow) {
        const styles = window.getComputedStyle(arrow);
        return {
          transform: styles.transform,
          padding: styles.padding,
          width: styles.width
        };
      }
      return { exists: false };
    });

    console.log('Arrow styles:', JSON.stringify(arrowStyles, null, 2));

    // Check star button styles if it exists
    const starStyles = await page.evaluate(() => {
      const star = document.querySelector('.fx-preset-btn');
      if (star) {
        const styles = window.getComputedStyle(star);
        return {
          display: styles.display,
          alignItems: styles.alignItems,
          justifyContent: styles.justifyContent,
          fontSize: styles.fontSize
        };
      }
      return { exists: false };
    });

    console.log('Star button styles:', JSON.stringify(starStyles, null, 2));

  } catch (err) {
    console.error('Error:', err.message);
  }

  await browser.close();
})();
