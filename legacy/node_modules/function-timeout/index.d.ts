export type Options = {
	/**
	The time in milliseconds to wait before timing out.

	Keep in mind that execution time can vary between different hardware and Node.js versions. Set a generous timeout to avoid flakiness.
	*/
	readonly timeout?: number;
};

/**
Returns a wrapped version of the given function that throws a timeout error if the execution takes longer than the given timeout.

@example
```
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
*/
export default function functionTimeout<T extends Function>(function_: T, options?: Options): T; // eslint-disable-line @typescript-eslint/ban-types

/**
Returns a boolean for whether the given error is a timeout error.
*/
export function isTimeoutError(error: unknown): boolean;
