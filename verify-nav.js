const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();
  const viewport = { width: 1280, height: 800 };
  await page.setViewportSize(viewport);

  try {
    await page.goto('http://localhost:8080/index.htm', { waitUntil: 'domcontentloaded' });
    await page.waitForTimeout(2000);

    // Take screenshot of bottom nav area
    await page.screenshot({ path: 'bottom-nav.png', fullPage: true });
    console.log('Full page screenshot saved');

    // Check bottom nav buttons
    const botNav = await page.evaluate(() => {
      const buttons = Array.from(document.querySelectorAll('.tab.bot button'));
      return buttons.map((btn, idx) => ({
        index: idx,
        text: btn.textContent.trim(),
        visible: btn.offsetParent !== null
      }));
    });

    console.log('Bottom nav buttons:', JSON.stringify(botNav, null, 2));

    // Check if palette preview bars exist
    const previewBars = await page.evaluate(() => {
      const lstIprevs = document.querySelectorAll('.lstIprev');
      return {
        count: lstIprevs.length,
        hidden: Array.from(lstIprevs).map(el => window.getComputedStyle(el).display)
      };
    });

    console.log('Palette preview bars:', JSON.stringify(previewBars, null, 2));

    // Check bottom nav styling
    const botStyle = await page.evaluate(() => {
      const bot = document.querySelector('.tab.bot');
      if (bot) {
        const styles = window.getComputedStyle(bot);
        return {
          display: styles.display,
          justifyContent: styles.justifyContent,
          gap: styles.gap,
          alignItems: styles.alignItems
        };
      }
      return null;
    });

    console.log('Bottom nav styling:', JSON.stringify(botStyle, null, 2));

  } catch (err) {
    console.error('Error:', err.message);
  }

  await browser.close();
})();
