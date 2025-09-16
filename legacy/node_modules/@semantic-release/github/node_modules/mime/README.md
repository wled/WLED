<!--
  -- This file is auto-generated from src/README_js.md. Changes should be made there.
  -->
# Mime

[![NPM downloads](https://img.shields.io/npm/dm/mime)](https://www.npmjs.com/package/mime)
[![Mime CI](https://github.com/broofa/mime/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/broofa/mime/actions/workflows/ci.yml?query=branch%3Amain)

An API for MIME type information.

- All `mime-db` types
- Compact and dependency-free [![mime's badge](https://deno.bundlejs.com/?q=mime&badge)](https://bundlejs.com/?q=mime)
- Full TS support


> [!Note]
> `mime@4` is now `latest`.  If you're upgrading from `mime@3`, note the following:
> * `mime@4` is API-compatible with `mime@3`, with ~~one~~ two exceptions:
>   * Direct imports of `mime` properties [no longer supported](https://github.com/broofa/mime/issues/295)
>   * `mime.define()` cannot be called on the default `mime` object
> * ESM module support is required.   [ESM Module FAQ](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c).
> * Requires an [ES2020](https://caniuse.com/?search=es2020) or newer runtime
> * Built-in Typescript types (`@types/mime` no longer needed)

## Installation

```bash
npm install mime
```

## Quick Start

For the full version (800+ MIME types, 1,000+ extensions):

```javascript
import mime from 'mime';

mime.getType('txt');                    // ⇨ 'text/plain'
mime.getExtension('text/plain');        // ⇨ 'txt'
```

### Lite Version [![mime/lite's badge](https://deno.bundlejs.com/?q=mime/lite&badge)](https://bundlejs.com/?q=mime/lite)

`mime/lite` is a drop-in `mime` replacement, stripped of unofficial ("`prs.*`", "`x-*`", "`vnd.*`") types:

```javascript
import mime from 'mime/lite';
```

## API

### `mime.getType(pathOrExtension)`

Get mime type for the given file path or extension. E.g.

```javascript
mime.getType('js');             // ⇨ 'text/javascript'
mime.getType('json');           // ⇨ 'application/json'

mime.getType('txt');            // ⇨ 'text/plain'
mime.getType('dir/text.txt');   // ⇨ 'text/plain'
mime.getType('dir\\text.txt');  // ⇨ 'text/plain'
mime.getType('.text.txt');      // ⇨ 'text/plain'
mime.getType('.txt');           // ⇨ 'text/plain'
```

`null` is returned in cases where an extension is not detected or recognized

```javascript
mime.getType('foo/txt');        // ⇨ null
mime.getType('bogus_type');     // ⇨ null
```

### `mime.getExtension(type)`

Get file extension for the given mime type. Charset options (often included in Content-Type headers) are ignored.

```javascript
mime.getExtension('text/plain');               // ⇨ 'txt'
mime.getExtension('application/json');         // ⇨ 'json'
mime.getExtension('text/html; charset=utf8');  // ⇨ 'html'
```

### `mime.getAllExtensions(type)`

> [!Note]
> New in `mime@4`

Get all file extensions for the given mime type.

```javascript --run default
mime.getAllExtensions('image/jpeg'); // ⇨ Set(3) { 'jpeg', 'jpg', 'jpe' }
```

## Custom `Mime` instances

The default `mime` objects are immutable.  Custom, mutable versions can be created as follows...
### new Mime(type map [, type map, ...])

Create a new, custom mime instance.  For example, to create a mutable version of the default `mime` instance:

```javascript
import { Mime } from 'mime/lite';

import standardTypes from 'mime/types/standard.js';
import otherTypes from 'mime/types/other.js';

const mime = new Mime(standardTypes, otherTypes);
```

Each argument is passed to the `define()` method, below. For example `new Mime(standardTypes, otherTypes)` is synonomous with `new Mime().define(standardTypes).define(otherTypes)`

### `mime.define(type map [, force = false])`

> [!Note]
> Only available on custom `Mime` instances

Define MIME type -> extensions.

Attempting to map a type to an already-defined extension will `throw` unless the `force` argument is set to `true`.

```javascript
mime.define({'text/x-abc': ['abc', 'abcd']});

mime.getType('abcd');            // ⇨ 'text/x-abc'
mime.getExtension('text/x-abc')  // ⇨ 'abc'
```

## Command Line

### Extension -> type

```bash
$ mime scripts/jquery.js
text/javascript
```

### Type -> extension

```bash
$ mime -r image/jpeg
jpeg
```
