// @ts-check
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
      use: { ...devices['Pixel 5'] },
    },
  ],

  // Starts the static file server before tests, shuts it down after
  webServer: {
    command: 'node server.js',
    port: 5001,
    reuseExistingServer: false,
    timeout: 5_000,
  },
});
