import fs from 'node:fs';
import fsPromises from 'node:fs/promises';
import path from 'node:path';
import parseJson from 'parse-json';
import normalizePackageData from 'normalize-package-data';
import {toPath} from 'unicorn-magic';

const getPackagePath = cwd => path.resolve(toPath(cwd) ?? '.', 'package.json');

const _readPackage = (file, normalize) => {
	const json = typeof file === 'string'
		? parseJson(file)
		: file;

	if (normalize) {
		normalizePackageData(json);
	}

	return json;
};

export async function readPackage({cwd, normalize = true} = {}) {
	const packageFile = await fsPromises.readFile(getPackagePath(cwd), 'utf8');
	return _readPackage(packageFile, normalize);
}

export function readPackageSync({cwd, normalize = true} = {}) {
	const packageFile = fs.readFileSync(getPackagePath(cwd), 'utf8');
	return _readPackage(packageFile, normalize);
}

export function parsePackage(packageFile, {normalize = true} = {}) {
	const isObject = packageFile !== null && typeof packageFile === 'object' && !Array.isArray(packageFile);
	const isString = typeof packageFile === 'string';

	if (!isObject && !isString) {
		throw new TypeError('`packageFile` should be either an `object` or a `string`.');
	}

	const clonedPackageFile = isObject ? structuredClone(packageFile) : packageFile;

	return _readPackage(clonedPackageFile, normalize);
}
