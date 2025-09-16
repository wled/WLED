# hook-std

> Hook and modify stdout and stderr

## Install

```sh
npm install hook-std
```

## Usage

```js
import assert from 'node:assert';
import {hookStdout} from 'hook-std';

const promise = hookStdout(output => {
	promise.unhook();
	assert.strictEqual(output.trim(), 'unicorn');
});

console.log('unicorn');
await promise;
```

You can also unhook using the second `transform` method parameter:

```js
import assert from 'node:assert';
import {hookStdout} from 'hook-std';

const promise = hookStdout((output, unhook) => {
	unhook();
	assert.strictEqual(output.trim(), 'unicorn');
});

console.log('unicorn');
await promise;
```

## API

### hookStd(options?, transform)

Hook streams in [`streams` option](#streams), or stdout and stderr if none are specified.

Returns a `Promise` with a `unhook()` method which, when called, unhooks both stdout and stderr and resolves the `Promise` with an empty result.

### hookStdout(options?, transform)

Hook stdout.

Returns a `Promise` with a `unhook()` method which, when called, unhooks stdout and resolves the `Promise` with an empty result.

### hookStderr(options?, transform)

Hook stderr.

Returns a `Promise` with a `unhook()` method which, when called, unhooks stderr and resolves the `Promise` with an empty result.

#### options

Type: `object`

##### silent

Type: `boolean`\
Default: `true`

Suppress stdout/stderr output.

##### once

Type: `boolean`\
Default: `false`

Automatically unhook after the first call.

##### streams

Type: `stream.Writable[]`\
Default: `[process.stdout, process.stderr]`

The [writable streams](https://nodejs.org/api/stream.html#stream_writable_streams) to hook. This can be useful for libraries allowing users to configure a writable stream to write to.

#### transform

Type: `Function`

Receives stdout/stderr as the first argument and the unhook method as the second argument. Return a string to modify it. Optionally, when in silent mode, you may return a `boolean` to influence the return value of `.write(…)`.
