# semver-regex

> Regular expression for matching [semver](https://github.com/npm/node-semver) versions

## Install

```sh
npm install semver-regex
```

## Usage

```js
import semverRegex from 'semver-regex';

semverRegex().test('v1.0.0');
//=> true

semverRegex().test('1.2.3-alpha.10.beta.0+build.unicorn.rainbow');
//=> true

semverRegex().exec('unicorn 1.0.0 rainbow')[0];
//=> '1.0.0'

'unicorn 1.0.0 and rainbow 2.1.3'.match(semverRegex());
//=> ['1.0.0', '2.1.3']
```

## Important

If you run the regex against untrusted user input, it's recommended to truncate the string to a sensible length (for example, 50). And if you use this in a server context, you should also [give it a timeout](https://github.com/sindresorhus/super-regex).

**I do not consider ReDoS a valid vulnerability for this package. It's simply not possible to make it fully ReDoS safe. It's up to the user to set a timeout for the regex if they accept untrusted user input.** However, I'm happy to accept pull requests to improve the regex.

## Related

- [find-versions](https://github.com/sindresorhus/find-versions) - Find semver versions in a string
- [latest-semver](https://github.com/sindresorhus/latest-semver) - Get the latest stable semver version from an array of versions
- [to-semver](https://github.com/sindresorhus/to-semver) - Get an array of valid, sorted, and cleaned semver versions from an array of strings
- [semver-diff](https://github.com/sindresorhus/semver-diff) - Get the diff type of two semver versions: `0.0.1` `0.0.2` → `patch`
- [semver-truncate](https://github.com/sindresorhus/semver-truncate) - Truncate a semver version: `1.2.3` → `1.2.0`
