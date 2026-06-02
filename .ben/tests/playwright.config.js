// @ts-check
const path = require('path');
const { defineConfig, devices } = require('@playwright/test');

module.exports = defineConfig({
  testDir: './specs',
  timeout: 15_000,
  retries: 0,
  reporter: 'list',

  use: {
    baseURL: 'http://127.0.0.1:5001',
    // Capture screenshots on failure for debugging
    screenshot: 'only-on-failure',
    // Don't wait for network idle — we control all network via route mocks
    waitForLoadState: 'domcontentloaded',
  },

  projects: [
    {
      // Use a mobile device so the mobile UI renders (not PC mode)
      name: 'mobile-chrome',
      use: {
        ...devices['Pixel 5'],
        // Prevent headless Chromium from throttling setTimeout to ≥1s on
        // "background" pages — without this, debounce timers fire too late.
        launchOptions: {
          args: ['--disable-background-timer-throttling', '--disable-renderer-backgrounding'],
        },
      },
    },
  ],

  // Starts the static file server before tests, shuts it down after
  webServer: {
    command: 'node "' + path.join(__dirname, 'server.js') + '"',
    port: 5001,
    reuseExistingServer: false,
    timeout: 5_000,
  },
});
