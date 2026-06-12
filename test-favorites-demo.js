const { chromium } = require('playwright');
const fs = require('fs');

(async () => {
  const browser = await chromium.launch();
  const context = await browser.createBrowserContext();
  const page = await context.newPage();
  
  // Set mobile viewport
  await page.setViewportSize({ width: 375, height: 667 });
  
  // Go to index.htm
  await page.goto('http://localhost:8080/index.htm');
  await page.waitForTimeout(2000);
  
  // Use devtools to inject test data and trigger rendering
  await page.evaluate(() => {
    // Mock some presets
    const mockData = {
      "1": { "name": "Warm White" },
      "2": { "name": "Cool Blue" },
      "3": { "name": "Rainbow" },
      "4": { "name": "Fire" },
      "5": { "name": "Ocean" }
    };
    
    // Store in localStorage
    localStorage.setItem('wledP', JSON.stringify(mockData));
    
    // Create a mock for pJson
    window.pJson = mockData;
    
    // Trigger the populatePresets function if it exists
    if (typeof populatePresets === 'function') {
      populatePresets(false);
    }
  });
  
  // Wait for render
  await page.waitForTimeout(1000);
  
  // Click Favorites tab
  const tabs = await page.locator('button.tablinks');
  const tabCount = await tabs.count();
  if (tabCount > 3) {
    await tabs.nth(3).click();
    await page.waitForTimeout(500);
  }
  
  // Take screenshot
  await page.screenshot({ path: 'favorites-demo.png', fullPage: true });
  console.log('✓ Demo screenshot saved: favorites-demo.png');
  
  // Check for arrow elements
  const arrows = await page.locator('svg polygon').count();
  console.log(`✓ Found ${arrows} arrow SVG elements on page`);
  
  // Get some details about the layout
  const pillRows = await page.locator('.fx-pill-row').count();
  console.log(`✓ Found ${pillRows} favorite rows`);
  
  await browser.close();
})().catch(err => {
  console.error('Error:', err);
  process.exit(1);
});
