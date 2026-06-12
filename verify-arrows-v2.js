const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();

  // Listen for console messages to see errors
  page.on('console', msg => console.log('PAGE LOG:', msg.type(), msg.text()));

  try {
    await page.goto('http://localhost:8080/index.htm', { waitUntil: 'domcontentloaded' });

    // Wait a bit longer for any scripts to load
    await page.waitForTimeout(3000);

    // Take a screenshot focusing on the area where arrow buttons should be
    await page.screenshot({ path: 'favorites-full.png', fullPage: true });
    console.log('Full page screenshot saved');

    // Check if arrow buttons exist and get their computed styles
    const arrowStyles = await page.evaluate(() => {
      const upArrow = document.querySelector('.fav-arrow-up');
      if (upArrow) {
        const styles = window.getComputedStyle(upArrow);
        return {
          padding: styles.padding,
          paddingLeft: styles.paddingLeft,
          paddingRight: styles.paddingRight,
          transform: styles.transform,
          width: styles.width,
          height: styles.height,
          exists: true
        };
      }
      return { exists: false, message: 'Arrow button not found' };
    });

    console.log('Arrow styles:', JSON.stringify(arrowStyles, null, 2));

  } catch (err) {
    console.error('Error:', err.message);
  }

  await browser.close();
})();
