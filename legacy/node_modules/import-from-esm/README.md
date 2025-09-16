# import-from-esm

[![Version](https://img.shields.io/npm/v/import-from-esm?logo=npm)](https://www.npmjs.com/package/import-from-esm)
[![Monthly Downloads](https://img.shields.io/npm/dm/import-from-esm)](https://www.npmjs.com/package/import-from-esm)
[![Test](https://img.shields.io/github/actions/workflow/status/sheerlox/import-from-esm/release.yml?logo=github)](https://github.com/sheerlox/import-from-esm/actions/workflows/release.yml)
[![CodeQL](https://img.shields.io/github/actions/workflow/status/sheerlox/import-from-esm/codeql.yml?logo=github&label=CodeQL)](https://github.com/sheerlox/import-from-esm/actions/workflows/codeql.yml)
[![Coverage](https://img.shields.io/sonar/coverage/sheerlox_import-from-esm?logo=sonarcloud&server=https%3A%2F%2Fsonarcloud.io)](https://sonarcloud.io/summary/overall?id=sheerlox_import-from-esm)
[![OpenSSF Scorecard](https://img.shields.io/ossf-scorecard/github.com/sheerlox/import-from-esm?label=openssf%20scorecard)
](https://securityscorecards.dev/viewer/?uri=github.com/sheerlox/import-from-esm)

## Overview

> Import a module like with [`require()`](https://nodejs.org/api/modules.html#modules_require_id) but from a given path (for ESM)

This library intends to be an _almost_ drop-in replacement of [`import-from`](https://github.com/sindresorhus/import-from) (from which it is forked), exposing the same API and behavior but also supporting ES modules (ESM). Just add `await` before `importFrom`/`importFrom.silent`

## Motivation

The main benefit of using `import-from` is that it abstracts the need to resolve the path and create a `require` statement. [Its code](https://github.com/sindresorhus/import-from/blob/v4.0.0/index.js) is really straightforward:

<!-- prettier-ignore-start -->
```js
(fromDirectory, moduleId) => createRequire(path.resolve(fromDirectory, "noop.js"))(moduleId);
```
<!-- prettier-ignore-end -->

In the case of `import-from-esm`, there are a few additional benefits because of the way ESM works:

1. Importing a package installed along a library (in the parent application) from that library is no longer possible ([which was the issue that made me work on this library](https://github.com/semantic-release/release-notes-generator/pull/544#issuecomment-1745455518)). You need to use `import.meta.resolve`, which is behind an experimental flag (although there's a ponyfill available at [wooorm/import-meta-resolve](https://github.com/wooorm/import-meta-resolve), which `import-from-esm` uses under-the-hood).
2. If the file you're trying to import (whether relative, package, export map, etc ...) is a JSON file, you need [to detect that and use](https://github.com/sheerlox/import-from-esm/blob/v1.3.1/index.js#L33-L37) import assertions or `require` (while the former is still in experimental).
3. File extensions are now mandatory for relative paths. `import-from-esm` re-introduces [`require`'s file extension discovery](https://nodejs.org/docs/latest-v18.x/api/modules.html#file-modules).

As you can see, there is quite a bit of complexity that [is abstracted behind `import-from-esm`](https://github.com/sheerlox/import-from-esm/blob/v1.3.1/index.js). The first bullet point issue affected both [`@semantic-release/commit-analyzer`](https://github.com/semantic-release/commit-analyzer/pull/537/files#diff-a558e4411f9515691b462dfd89640ec649509db79a4a86c5c8860d7bff173f95R28) and [`@semantic-release/release-notes-generator`](https://github.com/semantic-release/release-notes-generator/pull/544/files#diff-bee027b39eb704f3c940d54960f4f26693260c52d72707ac17d72f38f66da7d5R30). After spending hours on research to solve the issue, I realized that the work I was doing would benefit others as well, so I decided to create a package out of it.

As a proponent of ESM, I have put a lot of thought into poly-filling `require` features for `import`, but finally came to the conclusion that developing a package to facilitate the ecosystem transition to ESM by reducing friction was a good thing.

## Install

```
$ npm install import-from-esm
```

## Usage

```js
import importFrom from "import-from-esm";

// there is a file at `./foo/bar.{js,mjs,cjs,json}`

await importFrom("foo", "./bar");
```

## API

### importFrom(fromDirectory, moduleId)

Like `require()`, throws when the module can't be found.

### importFrom.silent(fromDirectory, moduleId)

Returns `undefined` instead of throwing when the module can't be found.

#### fromDirectory

Type: `string`

Directory to import from.

#### moduleId

Type: `string`

What you would use in `require()`.

## Tip

Create a partial using a bound function if you want to import from the same `fromDir` multiple times:

```js
const importFromFoo = importFrom.bind(null, "foo");

importFromFoo("./bar");
importFromFoo("./baz");
```
