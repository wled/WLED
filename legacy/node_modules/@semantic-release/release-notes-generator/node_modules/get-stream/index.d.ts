import {type Stream} from 'node:stream';
import {type Buffer} from 'node:buffer';

export class MaxBufferError extends Error {
	readonly name: 'MaxBufferError';
	constructor();
}

export type Options = {
	/**
	Maximum length of the returned string. If it exceeds this value before the stream ends, the promise will be rejected with a `MaxBufferError` error.

	@default Infinity
	*/
	readonly maxBuffer?: number;
};

export type OptionsWithEncoding<EncodingType = BufferEncoding> = {
	/**
	The [encoding](https://nodejs.org/api/buffer.html#buffers-and-character-encodings) of the incoming stream.

	@default 'utf8'
	*/
	readonly encoding?: EncodingType;
} & Options;

/**
Get the given `stream` as a string.

@returns A promise that resolves when the end event fires on the stream, indicating that there is no more data to be read. The stream is switched to flowing mode.

@example
```
import fs from 'node:fs';
import getStream from 'get-stream';

const stream = fs.createReadStream('unicorn.txt');

console.log(await getStream(stream));
//               ,,))))))));,
//            __)))))))))))))),
// \|/       -\(((((''''((((((((.
// -*-==//////((''  .     `)))))),
// /|\      ))| o    ;-.    '(((((                                  ,(,
//          ( `|    /  )    ;))))'                               ,_))^;(~
//             |   |   |   ,))((((_     _____------~~~-.        %,;(;(>';'~
//             o_);   ;    )))(((` ~---~  `::           \      %%~~)(v;(`('~
//                   ;    ''''````         `:       `:::|\,__,%%    );`'; ~
//                  |   _                )     /      `:|`----'     `-'
//            ______/\/~    |                 /        /
//          /~;;.____/;;'  /          ___--,-(   `;;;/
//         / //  _;______;'------~~~~~    /;;/\    /
//        //  | |                        / ;   \;;,\
//       (<_  | ;                      /',/-----'  _>
//        \_| ||_                     //~;~~~~~~~~~
//            `\_|                   (,~~
//                                    \~\
//                                     ~~
```
*/
export default function getStream(stream: Stream, options?: OptionsWithEncoding): Promise<string>;

/**
Get the given `stream` as a buffer.

It honors the `maxBuffer` option as above, but it refers to byte length rather than string length.

@example
```
import {getStreamAsBuffer} from 'get-stream';

const stream = fs.createReadStream('unicorn.png');

console.log(await getStreamAsBuffer(stream));
```
*/
export function getStreamAsBuffer(stream: Stream, options?: Options): Promise<Buffer>;
