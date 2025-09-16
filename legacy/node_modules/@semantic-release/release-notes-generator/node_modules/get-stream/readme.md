# get-stream

> Get a stream as a string or buffer

## Install

```sh
npm install get-stream
```

## Usage

```js
import fs from 'node:fs';
import getStream from 'get-stream';

const stream = fs.createReadStream('unicorn.txt');

console.log(await getStream(stream));
/*
              ,,))))))));,
           __)))))))))))))),
\|/       -\(((((''''((((((((.
-*-==//////((''  .     `)))))),
/|\      ))| o    ;-.    '(((((                                  ,(,
         ( `|    /  )    ;))))'                               ,_))^;(~
            |   |   |   ,))((((_     _____------~~~-.        %,;(;(>';'~
            o_);   ;    )))(((` ~---~  `::           \      %%~~)(v;(`('~
                  ;    ''''````         `:       `:::|\,__,%%    );`'; ~
                 |   _                )     /      `:|`----'     `-'
           ______/\/~    |                 /        /
         /~;;.____/;;'  /          ___--,-(   `;;;/
        / //  _;______;'------~~~~~    /;;/\    /
       //  | |                        / ;   \;;,\
      (<_  | ;                      /',/-----'  _>
       \_| ||_                     //~;~~~~~~~~~
           `\_|                   (,~~
                                   \~\
                                    ~~
*/
```

## API

The methods returns a promise that resolves when the `end` event fires on the stream, indicating that there is no more data to be read. The stream is switched to flowing mode.

### getStream(stream, options?)

Get the given `stream` as a string.

#### options

Type: `object`

##### encoding

Type: `string`\
Default: `'utf8'`

The [encoding](https://nodejs.org/api/buffer.html#buffers-and-character-encodings) of the incoming stream.

##### maxBuffer

Type: `number`\
Default: `Infinity`

Maximum length of the returned string. If it exceeds this value before the stream ends, the promise will be rejected with a `MaxBufferError` error.

### getStreamAsBuffer(stream, options?)

Get the given `stream` as a buffer.

It honors the `maxBuffer` option as above, but it refers to byte length rather than string length.

```js
import {getStreamAsBuffer} from 'get-stream';

const stream = fs.createReadStream('unicorn.png');

console.log(await getStreamAsBuffer(stream));
```

## Errors

If the input stream emits an `error` event, the promise will be rejected with the error. The buffered data will be attached to the `bufferedData` property of the error.

```js
import getStream from 'get-stream';

try {
	await getStream(streamThatErrorsAtTheEnd('unicorn'));
} catch (error) {
	console.log(error.bufferedData);
	//=> 'unicorn'
}
```

## Tip

You may not need this package if all you need is a string:

```js
import fs from 'node:fs';

const stream = fs.createReadStream('unicorn.txt', {encoding: 'utf8'});
const array = await stream.toArray();

console.log(array.join(''));
```

## FAQ

### How is this different from [`concat-stream`](https://github.com/maxogden/concat-stream)?

This module accepts a stream instead of being one and returns a promise instead of using a callback. The API is simpler and it only supports returning a string or buffer. It doesn't have a fragile type inference. You explicitly choose what you want. And it doesn't depend on the huge `readable-stream` package.

## Related

- [get-stdin](https://github.com/sindresorhus/get-stdin) - Get stdin as a string or buffer
- [into-stream](https://github.com/sindresorhus/into-stream) - The opposite of this package
