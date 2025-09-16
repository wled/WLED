# time-span

> Simplified high resolution timing

Uses [`process.hrtime`](https://nodejs.org/api/process.html#processhrtimebigint) in Node.js and [`performance.now`](https://developer.mozilla.org/en-US/docs/Web/API/Performance/now) in browsers ([accurate to 5 microseconds](https://developer.mozilla.org/en-US/docs/Web/API/DOMHighResTimeStamp)).

## Install

```
$ npm install time-span
```

## Usage

```js
import timeSpan from 'time-span';

const end = timeSpan();

timeConsumingFn();

console.log(end());
//=> 1745.3186

console.log(end.rounded());
//=> 1745

console.log(end.seconds());
//=> 1.7453186
```

## API

### `const end = timeSpan()`

Returns a function, that when called, returns the time difference.

#### end()

Elapsed milliseconds.

#### end.rounded()

Elapsed milliseconds rounded.

#### end.seconds()

Elapsed seconds.

#### end.nanoseconds()

Elapsed nanoseconds.
