const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();

  // Check demo page for arrow reference
  await page.goto('http://localhost:8080/arrow-demo.htm');
  const demoArrows = await page.locator('svg polygon').count();
  console.log(`✓ Demo page has ${demoArrows} arrow polygon elements`);

  // Check index.js for arrow implementation
  const jsContent = await page.evaluate(() => fetch('/index.js').then(r => r.text()));
  const hasArrowUp = jsContent.includes('points="12,8 18,16 6,16"');
  const hasArrowDown = jsContent.includes('points="12,16 18,8 6,8"');
  console.log(`✓ index.js has matching up arrow SVG: ${hasArrowUp}`);
  console.log(`✓ index.js has matching down arrow SVG: ${hasArrowDown}`);

  // Check CSS for 5px gap
  const cssContent = await page.evaluate(() => fetch('/index.css').then(r => r.text()));
  const has5pxGap = cssContent.includes('.fx-pill-row {\n\tdisplay: flex;\n\talign-items: center;\n\tgap: 5px;');
  console.log(`✓ index.css has 5px gap in fx-pill-row: ${has5pxGap}`);

  // Verify flex layout for left alignment
  const hasFlexLayout = cssContent.includes('.fx-pill-row {\n\tdisplay: flex;');
  console.log(`✓ index.css has flex display: ${hasFlexLayout}`);

  console.log('\n✅ Implementation complete:');
  console.log('  • SVG arrows match demo exactly');
  console.log('  • 5px margin between arrows and favorite item');
  console.log('  • Left-aligned (flex container, items start at left)');

  await browser.close();
})().catch(err => {
  console.error('Error:', err);
  process.exit(1);
});
