import type {PackageJson as typeFestPackageJson} from 'type-fest';
import type {Package as normalizePackage} from 'normalize-package-data';

export type Options = {
	/**
	Current working directory.

	@default process.cwd()
	*/
	readonly cwd?: URL | string;

	/**
	[Normalize](https://github.com/npm/normalize-package-data#what-normalization-currently-entails) the package data.

	@default true
	*/
	readonly normalize?: boolean;
};

// eslint-disable-next-line @typescript-eslint/naming-convention
type _NormalizeOptions = {
	readonly normalize?: true;
};

export type NormalizeOptions = _NormalizeOptions & Options;

export type ParseOptions = Omit<Options, 'cwd'>;
export type NormalizeParseOptions = _NormalizeOptions & ParseOptions;

export type NormalizedPackageJson = PackageJson & normalizePackage;
export type PackageJson = typeFestPackageJson;

/**
@returns The parsed JSON.

@example
```
import {readPackage} from 'read-pkg';

console.log(await readPackage());
//=> {name: 'read-pkg', …}

console.log(await readPackage({cwd: 'some-other-directory'});
//=> {name: 'unicorn', …}
```
*/
export function readPackage(options?: NormalizeOptions): Promise<NormalizedPackageJson>;
export function readPackage(options: Options): Promise<PackageJson>;

/**
@returns The parsed JSON.

@example
```
import {readPackageSync} from 'read-pkg';

console.log(readPackageSync());
//=> {name: 'read-pkg', …}

console.log(readPackageSync({cwd: 'some-other-directory'});
//=> {name: 'unicorn', …}
```
*/
export function readPackageSync(options?: NormalizeOptions): NormalizedPackageJson;
export function readPackageSync(options: Options): PackageJson;

export function parsePackage(packageFile: PackageJson | string, options?: NormalizeParseOptions): NormalizedPackageJson;
export function parsePackage(packageFile: PackageJson | string, options: ParseOptions): PackageJson;
