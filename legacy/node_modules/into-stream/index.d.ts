import {Readable as ReadableStream} from 'stream';

export type Input =
	| Buffer
	| NodeJS.TypedArray
	| ArrayBuffer
	| string
	| Iterable<Buffer | string>
	| AsyncIterable<Buffer | string>;

/* eslint-disable @typescript-eslint/ban-types */
export type ObjectInput =
	| object
	| Iterable<object>
	| AsyncIterable<object>;
/* eslint-enable @typescript-eslint/ban-types */

declare const intoStream: {
	/**
	Convert object `input` into a stream.

	@param input - The object input to convert to a stream.
	@returns A [readable object stream](https://nodejs.org/api/stream.html#stream_object_mode).
	*/
	object: (input: ObjectInput | Promise<ObjectInput>) => ReadableStream;

	/**
	Convert `input` into a stream. Adheres to the requested chunk size, except for `array` where each element will be a chunk.

	@param input - The input to convert to a stream.
	@returns A [readable stream](https://nodejs.org/api/stream.html#stream_class_stream_readable).

	@example
	```
	import intoStream from 'into-stream';

	intoStream('unicorn').pipe(process.stdout);
	//=> 'unicorn'
	```
	*/
	(input: Input | Promise<Input>): ReadableStream;
};

export default intoStream;
