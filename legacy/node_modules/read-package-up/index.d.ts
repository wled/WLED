import {type Except} from 'type-fest';
import {
	readPackage,
	readPackageSync,
	type Options as ReadPackageOptions,
	type NormalizeOptions as ReadPackageNormalizeOptions,
	type PackageJson,
	type NormalizedPackageJson,
} from 'read-pkg';

export type Options = {
	/**
	The directory to start looking for a package.json file.

	@default process.cwd()
	*/
	cwd?: URL | string;
} & Except<ReadPackageOptions, 'cwd'>;

export type NormalizeOptions = {
	/**
	The directory to start looking for a package.json file.

	@default process.cwd()
	*/
	cwd?: URL | string;
} & Except<ReadPackageNormalizeOptions, 'cwd'>;

export type ReadResult = {
	packageJson: PackageJson;
	path: string;
};

export type NormalizedReadResult = {
	packageJson: NormalizedPackageJson;
	path: string;
};

/**
Read the closest `package.json` file.

@example
```
import {readPackageUp} from 'read-package-up';

console.log(await readPackageUp());
// {
// 	packageJson: {
// 		name: 'awesome-package',
// 		version: '1.0.0',
// 		…
// 	},
// 	path: '/Users/sindresorhus/dev/awesome-package/package.json'
// }
```
*/
export function readPackageUp(options?: NormalizeOptions): Promise<NormalizedReadResult | undefined>;
export function readPackageUp(options: Options): Promise<ReadResult | undefined>;

/**
Synchronously read the closest `package.json` file.

@example
```
import {readPackageUpSync} from 'read-package-up';

console.log(readPackageUpSync());
// {
// 	packageJson: {
// 		name: 'awesome-package',
// 		version: '1.0.0',
// 		…
// 	},
// 	path: '/Users/sindresorhus/dev/awesome-package/package.json'
// }
```
*/
export function readPackageUpSync(options?: NormalizeOptions): NormalizedReadResult | undefined;
export function readPackageUpSync(options: Options): ReadResult | undefined;

export {PackageJson, NormalizedPackageJson} from 'read-pkg';
