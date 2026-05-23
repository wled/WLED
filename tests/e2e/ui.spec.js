// @ts-check
const { test, expect } = require('@playwright/test');

test.describe('WLED UI smoke tests', () => {
	test('page loads and shows bottom nav', async ({ page }) => {
		await page.goto('/index.htm');
		await expect(page.locator('#bot')).toBeVisible();
	});

	test('bottom nav shows Colours, Effects, Favourites', async ({ page }) => {
		await page.goto('/index.htm');
		const bot = page.locator('#bot');
		await expect(bot.getByRole('button', { name: 'Colours' })).toBeVisible();
		await expect(bot.getByRole('button', { name: 'Effects' })).toBeVisible();
		await expect(bot.getByRole('button', { name: 'Favourites' })).toBeVisible();
	});

	test('Settings cog is in the top bar', async ({ page }) => {
		await page.goto('/index.htm');
		await expect(page.locator('#buttonSettings')).toBeVisible();
	});

	test('Favourites button opens Presets tab', async ({ page }) => {
		await page.goto('/index.htm');
		await page.locator('#bot').getByRole('button', { name: 'Favourites' }).click();
		await expect(page.locator('#Presets')).toBeVisible();
	});

	test('Presets page has no Preset or Playlist create buttons', async ({ page }) => {
		await page.goto('/index.htm');
		await page.locator('#bot').getByRole('button', { name: 'Favourites' }).click();
		await expect(page.locator('#putil')).toBeEmpty();
	});
});
