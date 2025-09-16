import functionTimeout, {isTimeoutError} from 'function-timeout';
import timeSpan from 'time-span';

const resultToMatch = result => ({
	match: result[0],
	index: result.index,
	groups: result.slice(1),
	namedGroups: result.groups ?? {},
	input: result.input,
});

const context = {};

export function isMatch(regex, string, {timeout} = {}) {
	try {
		return functionTimeout(() => structuredClone(regex).test(string), {timeout, context})();
	} catch (error) {
		if (isTimeoutError(error)) {
			return false;
		}

		throw error;
	}
}

export function firstMatch(regex, string, {timeout} = {}) {
	try {
		const result = functionTimeout(() => structuredClone(regex).exec(string), {timeout, context})();

		if (result === null) {
			return;
		}

		return resultToMatch(result);
	} catch (error) {
		if (isTimeoutError(error)) {
			return;
		}

		throw error;
	}
}

export function matches(regex, string, {timeout = Number.POSITIVE_INFINITY, matchTimeout = Number.POSITIVE_INFINITY} = {}) {
	if (!regex.global) {
		throw new Error('The regex must have the global flag, otherwise, use `firstMatch()` instead');
	}

	return {
		* [Symbol.iterator]() {
			try {
				const matches = string.matchAll(regex); // The regex is only executed when iterated over.

				while (true) {
					// `matches.next` must be called within an arrow function so that it doesn't loose its context.
					const nextMatch = functionTimeout(() => matches.next(), {
						context,
						timeout: (timeout !== Number.POSITIVE_INFINITY || matchTimeout !== Number.POSITIVE_INFINITY) ? Math.min(timeout, matchTimeout) : undefined,
					});

					const end = timeSpan();
					const {value, done} = nextMatch();
					timeout -= Math.ceil(end());

					if (done) {
						break;
					}

					yield resultToMatch(value);
				}
			} catch (error) {
				if (!isTimeoutError(error)) {
					throw error;
				}
			}
		},
	};
}
