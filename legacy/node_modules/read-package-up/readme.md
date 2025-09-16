# read-package-up

> Read the closest package.json file

## Why

- [Finds the closest package.json](https://github.com/sindresorhus/find-up)
- [Throws more helpful JSON errors](https://github.com/sindresorhus/parse-json)
- [Normalizes the data](https://github.com/npm/normalize-package-data#what-normalization-currently-entails)

## Install

```sh
npm install read-package-up
```

## Usage

```js
import {readPackageUp} from 'read-package-up';

console.log(await readPackageUp());
/*
{
	packageJson: {
		name: 'awesome-package',
		version: '1.0.0',
		…
	},
	path: '/Users/sindresorhus/dev/awesome-package/package.json'
}
*/
```

## API

### readPackageUp(options?)

Returns a `Promise<object>`, or `Promise<undefined>` if no `package.json` was found.

### readPackageUpSync(options?)

Returns the result object, or `undefined` if no `package.json` was found.

#### options

Type: `object`

##### cwd

Type: `URL | string`\
Default: `process.cwd()`

The directory to start looking for a package.json file.

##### normalize

Type: `boolean`\
Default: `true`

[Normalize](https://github.com/npm/normalize-package-data#what-normalization-currently-entails) the package data.

## Related

- [read-pkg](https://github.com/sindresorhus/read-pkg) - Read a package.json file
- [package-up](https://github.com/sindresorhus/package-up) - Find the closest package.json file
- [find-up](https://github.com/sindresorhus/find-up) - Find a file by walking up parent directories
- [pkg-conf](https://github.com/sindresorhus/pkg-conf) - Get namespaced config from the closest package.json
