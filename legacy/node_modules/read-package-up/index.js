import path from 'node:path';
import {findUp, findUpSync} from 'find-up-simple';
import {readPackage, readPackageSync} from 'read-pkg';

export async function readPackageUp(options) {
	const filePath = await findUp('package.json', options);
	if (!filePath) {
		return;
	}

	return {
		packageJson: await readPackage({...options, cwd: path.dirname(filePath)}),
		path: filePath,
	};
}

export function readPackageUpSync(options) {
	const filePath = findUpSync('package.json', options);
	if (!filePath) {
		return;
	}

	return {
		packageJson: readPackageSync({...options, cwd: path.dirname(filePath)}),
		path: filePath,
	};
}
