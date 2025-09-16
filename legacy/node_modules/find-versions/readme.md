# find-versions

> Find semver versions in a string: `unicorn v1.2.3` → `1.2.3`

## Install

```sh
npm install find-versions
```

## Usage

```js
import findVersions from 'find-versions';

findVersions('unicorn v1.2.3 rainbow 2.3.4+build.1');
//=> ['1.2.3', '2.3.4+build.1']

findVersions('cp (GNU coreutils) 8.22', {loose: true});
//=> ['8.22.0']
```

## API

### findVersions(stringWithVersions, options?)

#### stringWithVersions

Type: `string`

#### options

Type: `object`

##### loose

Type: `boolean`\
Default: `false`

Also match non-semver versions like `1.88`. They're coerced into semver compliant versions.

## Related

- [find-versions-cli](https://github.com/sindresorhus/find-versions-cli) - CLI for this module
