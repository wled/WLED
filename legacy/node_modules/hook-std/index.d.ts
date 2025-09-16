import {Buffer} from 'node:buffer';

/**
`unhook()` method which, when called, unhooks from a stream and resolves the Promise.
*/
export type Unhook = () => void;

/**
@param output - String from stream output.
@param unhook - Method which, when called, unhooks from stream.
@returns A Buffer or string to modify the value in the stream.
*/
export type Transform = (
	output: string,
	unhook: Unhook
) => Buffer | string | void;

/**
@param output - String from stream output.
@param unhook - Method which, when called, unhooks from stream.
@returns A boolean to influence the return value of `.write(…)`.
*/
export type SilentTransform = (
	output: string,
	unhook: Unhook
) => boolean | void;

export interface Options {
	/**
	Automatically unhook after the first call.

	@default false
	*/
	readonly once?: boolean;

	/**
	Suppress stdout/stderr output.

	@default true
	*/
	readonly silent?: boolean;
}

export interface StreamsOptions extends Options {
	/**
	The writable streams to hook. This can be useful for libraries allowing users to configure a writable stream to write to.

	@default [process.stdout, process.stderr]
	*/
	readonly streams?: readonly NodeJS.WritableStream[];
}

/**
Promise with a `unhook()` method which, when called, resolves the Promise with an empty result.
*/
export interface HookPromise extends Promise<void> {
	unhook: Unhook;
}

/**
Hook streams in the `streams` options, or stdout and stderr if none are specified.

@returns A `Promise` with a `unhook()` method which, when called, unhooks both stdout and stderr and resolves the `Promise` with an empty result.
*/
export function hookStd(transform: SilentTransform): HookPromise;
export function hookStd(
	options: StreamsOptions & {silent?: false},
	transform: Transform
): HookPromise;
export function hookStd(
	options: StreamsOptions & {silent?: true},
	transform: SilentTransform
): HookPromise;

/**
Hook stdout.

@returns A `Promise` with a `unhook()` method which, when called, unhooks stdout and resolves the `Promise` with an empty result.

@example
```
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

@example
```
import assert from 'node:assert';
import {hookStdout} from 'hook-std';

const promise = hookStdout((output, unhook) => {
	unhook();
	assert.strictEqual(output.trim(), 'unicorn');
});

console.log('unicorn');
await promise;
```
*/
export function hookStdout(transform: SilentTransform): HookPromise;
export function hookStdout(
	options: Options & {silent?: false},
	transform: Transform
): HookPromise;
export function hookStdout(
	options: Options & {silent?: true},
	transform: SilentTransform
): HookPromise;

/**
Hook stderr.

@returns A `Promise` with a `unhook()` method which, when called, unhooks stderr and resolves the `Promise` with an empty result.

@example
```
import assert from 'node:assert';
import {hookStderr} from 'hook-std';

const promise = hookStdout(output => {
	promise.unhook();
	assert.strictEqual(output.trim(), 'unicorn');
});

console.error('unicorn');
await promise;
```

You can also unhook using the second `transform` method parameter:

@example
```
import assert from 'node:assert';
import {hookStderr} from 'hook-std';

const promise = hookStderr((output, unhook) => {
	unhook();
	assert.strictEqual(output.trim(), 'unicorn');
});

console.error('unicorn');
await promise;
```
*/
export function hookStderr(transform: SilentTransform): HookPromise;
export function hookStderr(
	options: Options & {silent?: false},
	transform: Transform
): HookPromise;
export function hookStderr(
	options: Options & {silent?: true},
	transform: SilentTransform
): HookPromise;

