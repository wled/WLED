# clean-stack

> Clean up error stack traces

Removes the mostly unhelpful internal Node.js entries.

Also works in Electron.

## Install

```sh
npm install clean-stack
```

## Usage

```js
import cleanStack from 'clean-stack';

const error = new Error('Missing unicorn');

console.log(error.stack);
/*
Error: Missing unicorn
    at Object.<anonymous> (/Users/sindresorhus/dev/clean-stack/unicorn.js:2:15)
    at Module._compile (module.js:409:26)
    at Object.Module._extensions..js (module.js:416:10)
    at Module.load (module.js:343:32)
    at Function.Module._load (module.js:300:12)
    at Function.Module.runMain (module.js:441:10)
    at startup (node.js:139:18)
*/

console.log(cleanStack(error.stack));
/*
Error: Missing unicorn
    at Object.<anonymous> (/Users/sindresorhus/dev/clean-stack/unicorn.js:2:15)
*/
```

## API

### cleanStack(stack, options?)

Returns the cleaned stack or `undefined` if the given `stack` is `undefined`.

#### stack

Type: `string | undefined`

The `stack` property of an [`Error`](https://github.com/microsoft/TypeScript/blob/eac073894b172ec719ca7f28b0b94fc6e6e7d4cf/lib/lib.es5.d.ts#L972-L976).

#### options

Type: `object`

##### pretty

Type: `boolean`\
Default: `false`

Prettify the file paths in the stack:

`/Users/sindresorhus/dev/clean-stack/unicorn.js:2:15` → `~/dev/clean-stack/unicorn.js:2:15`

##### basePath

Type: `string?`

Remove the given base path from stack trace file paths, effectively turning absolute paths into relative ones. It will also transform absolute file URLs into relative paths.

Example with `'/Users/sindresorhus/dev/clean-stack'` as `basePath`:

`/Users/sindresorhus/dev/clean-stack/unicorn.js:2:15` → `unicorn.js:2:15`

##### pathFilter

Type: `(path: string) => boolean`

Remove the stack lines where the given function returns `false`. The function receives the path part of the stack line.

```js
import cleanStack from 'clean-stack';

const error = new Error('Missing unicorn');

console.log(cleanStack(error.stack));
// Error: Missing unicorn
//     at Object.<anonymous> (/Users/sindresorhus/dev/clean-stack/unicorn.js:2:15)
//     at Object.<anonymous> (/Users/sindresorhus/dev/clean-stack/omit-me.js:1:16)

const pathFilter = path => !/omit-me/.test(path);

console.log(cleanStack(error.stack, {pathFilter}));
// Error: Missing unicorn
//     at Object.<anonymous> (/Users/sindresorhus/dev/clean-stack/unicorn.js:2:15)
```

## Related

- [extract-stack](https://github.com/sindresorhus/extract-stack) - Extract the actual stack of an error
- [stack-utils](https://github.com/tapjs/stack-utils) - Captures and cleans stack traces
