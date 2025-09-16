# function-timeout

> Make a synchronous function have a timeout

This can be useful if you accept external data and want to ensure processing it does not take too long.

The timeout only works in Node.js. When used in a browser, the function will be wrapped, but never time out.

*I have a [different package](https://github.com/sindresorhus/super-regex) to prevent [ReDoS](https://en.wikipedia.org/wiki/ReDoS) for regexes.*

## Install

```sh
npm install function-timeout
```

## Usage

```js
import functionTimeout, {isTimeoutError} from 'function-timeout';

const generateNumbers = count => {
	// Imagine this takes a long time.
};

const generateNumbersWithTimeout = functionTimeout(generateNumbers, {timeout: 100});

try {
	console.log(generateNumbersWithTimeout(500));
} catch (error) {
	if (isTimeoutError(error)) {
		console.error('Timed out');
	} else {
		throw error;
	}
}
```

## API

### functionTimeout(function, options?)

Returns a wrapped version of the given function that throws a timeout error if the execution takes longer than the given timeout.

#### options

Type: `object`

##### timeout?

Type: `number` *(integer)*

The time in milliseconds to wait before timing out.

Keep in mind that execution time can vary between different hardware and Node.js versions. Set a generous timeout to avoid flakiness.

### isTimeoutError(error)

Returns a boolean for whether the given error is a timeout error.

## Related

- [super-regex](https://github.com/sindresorhus/super-regex) - Make a regular expression time out if it takes too long to execute
- [p-timeout](https://github.com/sindresorhus/p-timeout) - Timeout a promise after a certain amount of time
