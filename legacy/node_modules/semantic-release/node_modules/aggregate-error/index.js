import indentString from 'indent-string';
import cleanStack from 'clean-stack';

const cleanInternalStack = stack => stack.replaceAll(/\s+at .*aggregate-error\/index.js:\d+:\d+\)?/g, '');

export default class AggregateError extends Error {
	#errors;

	name = 'AggregateError';

	constructor(errors) {
		if (!Array.isArray(errors)) {
			throw new TypeError(`Expected input to be an Array, got ${typeof errors}`);
		}

		errors = errors.map(error => {
			if (error instanceof Error) {
				return error;
			}

			if (error !== null && typeof error === 'object') {
				// Handle plain error objects with message property and/or possibly other metadata
				return Object.assign(new Error(error.message), error);
			}

			return new Error(error);
		});

		let message = errors
			.map(error =>
				// The `stack` property is not standardized, so we can't assume it exists
				typeof error.stack === 'string' && error.stack.length > 0 ? cleanInternalStack(cleanStack(error.stack)) : String(error),
			)
			.join('\n');
		message = '\n' + indentString(message, 4);
		super(message);

		this.#errors = errors;
	}

	get errors() {
		return [...this.#errors];
	}
}
