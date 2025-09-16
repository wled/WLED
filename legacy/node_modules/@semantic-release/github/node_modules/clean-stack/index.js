import escapeStringRegexp from 'escape-string-regexp';
import getHomeDirectory from '#home-directory';

const extractPathRegex = /\s+at.*[(\s](.*)\)?/;
const pathRegex = /^(?:(?:(?:node|node:[\w/]+|(?:(?:node:)?internal\/[\w/]*|.*node_modules\/(?:babel-polyfill|pirates)\/.*)?\w+)(?:\.js)?:\d+:\d+)|native)/;

export default function cleanStack(stack, {pretty = false, basePath, pathFilter} = {}) {
	const basePathRegex = basePath && new RegExp(`(file://)?${escapeStringRegexp(basePath.replace(/\\/g, '/'))}/?`, 'g');
	const homeDirectory = pretty ? getHomeDirectory() : '';

	if (typeof stack !== 'string') {
		return undefined;
	}

	return stack.replace(/\\/g, '/')
		.split('\n')
		.filter(line => {
			const pathMatches = line.match(extractPathRegex);
			if (pathMatches === null || !pathMatches[1]) {
				return true;
			}

			const match = pathMatches[1];

			// Electron
			if (
				match.includes('.app/Contents/Resources/electron.asar')
				|| match.includes('.app/Contents/Resources/default_app.asar')
				|| match.includes('node_modules/electron/dist/resources/electron.asar')
				|| match.includes('node_modules/electron/dist/resources/default_app.asar')
			) {
				return false;
			}

			return pathFilter
				? !pathRegex.test(match) && pathFilter(match)
				: !pathRegex.test(match);
		})
		.filter(line => line.trim() !== '')
		.map(line => {
			if (basePathRegex) {
				line = line.replace(basePathRegex, '');
			}

			if (pretty) {
				line = line.replace(extractPathRegex, (m, p1) => m.replace(p1, p1.replace(homeDirectory, '~')));
			}

			return line;
		})
		.join('\n');
}
