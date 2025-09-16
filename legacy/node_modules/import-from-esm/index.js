import { createRequire } from 'node:module';
import { extname, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

import createDebug from 'debug';
import { moduleResolve } from 'import-meta-resolve';

const debug = createDebug('import-from-esm');
const require = createRequire(import.meta.url);

const EXTENSIONS = ['.js', '.mjs', '.cjs', '.json'];

function resolveToFileURL(...paths) {
	return pathToFileURL(resolve(...paths));
}

function tryResolve(moduleId, baseURL) {
	debug(`Trying to resolve '${moduleId}' from '${baseURL.href}'`);
	try {
		return moduleResolve(moduleId, baseURL, new Set(['node', 'import']));
	} catch (error) {
		debug(`Failed to resolve '${moduleId}' from '${baseURL.href}': ${String(error)}`);
	}
}

async function tryImport(fileURL) {
	if (!fileURL) {
		return;
	}

	try {
		debug(`Trying to determine file extension for '${fileURL.href}'`);
		const filePath = fileURLToPath(fileURL);
		const asJSON = extname(filePath) === '.json';

		debug(`Trying to import '${fileURL.href}'${asJSON ? ' as JSON' : ''}`);
		return asJSON ? require(filePath) : await import(fileURL);
	} catch (error) {
		debug(`Failed to determine file extension or to import '${fileURL.href}': ${String(error)}`);
		if (error instanceof SyntaxError) {
			throw error;
		}
	}
}

async function importFrom(fromDirectory, moduleId) {
	debug(`Executing importFrom('${fromDirectory}', '${moduleId}')`);

	let loadedModule;

	if (/^(\/|\.\.\/|\.\/|[a-zA-Z]:)/.test(moduleId)) {
		// If moduleId begins with '/', '../', './' or Windows path (e.g. "C:"),
		// resolve manually (so we can support extensionless imports)
		// - https://nodejs.org/api/modules.html#file-modules
		debug(`'${moduleId}' is a file module`);

		const localModulePath = resolveToFileURL(fromDirectory, moduleId);

		// Try to resolve exact file path
		loadedModule = await tryImport(localModulePath);

		if (!loadedModule && !EXTENSIONS.includes(extname(moduleId))) {
			// Try to resolve file path with added extensions

			for (const ext of EXTENSIONS) {
				// eslint-disable-next-line no-await-in-loop
				loadedModule = await tryImport(`${localModulePath}${ext}`);
				if (loadedModule) {
					break;
				}
			}
		}
	} else {
		// Let `import-meta-resolve` deal with resolving packages & import maps
		// - https://nodejs.org/api/modules.html#loading-from-node_modules-folders
		// - https://nodejs.org/api/packages.html#subpath-imports
		debug(`'${moduleId}' is not a file module`);

		const parentModulePath = resolveToFileURL(fromDirectory, 'noop.js');
		loadedModule = await tryImport(tryResolve(moduleId, parentModulePath));

		// Support for extensionless subpaths (not subpath exports)
		if (!loadedModule && !moduleId.startsWith('#')) {
			// Try to resolve file path with added extensions
			for (const ext of EXTENSIONS) {
				// eslint-disable-next-line no-await-in-loop
				loadedModule = await tryImport(tryResolve(`${moduleId}${ext}`, parentModulePath));

				if (loadedModule) {
					break;
				}
			}

			// Support for extensionless subpaths index files
			if (!loadedModule) {
				// Treat `moduleId` as a directory and try to resolve its index with added extensions
				for (const ext of EXTENSIONS) {
					// eslint-disable-next-line no-await-in-loop
					loadedModule = await tryImport(
						tryResolve(`${moduleId}/index${ext}`, parentModulePath),
					);

					if (loadedModule) {
						break;
					}
				}
			}
		}
	}

	if (!loadedModule) {
		const errorString = `Cannot find module '${moduleId}'`;
		debug(errorString);
		const error = new Error(errorString);
		error.code = 'MODULE_NOT_FOUND';
		throw error;
	}

	debug(`Successfully loaded module '${moduleId}' from '${fromDirectory}'`);

	return loadedModule.default ?? loadedModule;
}

importFrom.silent = async function (fromDirectory, moduleId) {
	try {
		return await importFrom(fromDirectory, moduleId);
	} catch {}
};

export default importFrom;
