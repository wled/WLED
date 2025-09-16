import {codeFrameColumns} from '@babel/code-frame';
import indexToPosition from 'index-to-position';

const getCodePoint = character => `\\u{${character.codePointAt(0).toString(16)}}`;

export class JSONError extends Error {
	name = 'JSONError';
	fileName;
	#input;
	#jsonParseError;
	#message;
	#codeFrame;
	#rawCodeFrame;

	constructor(messageOrOptions) {
		// JSONError constructor used accept string
		// TODO[>=9]: Remove this `if` on next major version
		if (typeof messageOrOptions === 'string') {
			super();
			this.#message = messageOrOptions;
		} else {
			const {jsonParseError, fileName, input} = messageOrOptions;
			// We cannot pass message to `super()`, otherwise the message accessor will be overridden.
			// https://262.ecma-international.org/14.0/#sec-error-message
			super(undefined, {cause: jsonParseError});

			this.#input = input;
			this.#jsonParseError = jsonParseError;
			this.fileName = fileName;
		}

		Error.captureStackTrace?.(this, JSONError);
	}

	get message() {
		this.#message ??= `${addCodePointToUnexpectedToken(this.#jsonParseError.message)}${this.#input === '' ? ' while parsing empty string' : ''}`;

		const {codeFrame} = this;
		return `${this.#message}${this.fileName ? ` in ${this.fileName}` : ''}${codeFrame ? `\n\n${codeFrame}\n` : ''}`;
	}

	set message(message) {
		this.#message = message;
	}

	#getCodeFrame(highlightCode) {
		// TODO[>=9]: Remove this `if` on next major version
		if (!this.#jsonParseError) {
			return;
		}

		const input = this.#input;

		const location = getErrorLocation(input, this.#jsonParseError.message);
		if (!location) {
			return;
		}

		return codeFrameColumns(input, {start: location}, {highlightCode});
	}

	get codeFrame() {
		this.#codeFrame ??= this.#getCodeFrame(/* highlightCode */ true);
		return this.#codeFrame;
	}

	get rawCodeFrame() {
		this.#rawCodeFrame ??= this.#getCodeFrame(/* highlightCode */ false);
		return this.#rawCodeFrame;
	}
}

const getErrorLocation = (string, message) => {
	const match = message.match(/in JSON at position (?<index>\d+)(?: \(line (?<line>\d+) column (?<column>\d+)\))?$/);

	if (!match) {
		return;
	}

	let {index, line, column} = match.groups;

	if (line && column) {
		return {line: Number(line), column: Number(column)};
	}

	index = Number(index);

	// The error location can be out of bounds.
	if (index === string.length) {
		const {line, column} = indexToPosition(string, string.length - 1, {oneBased: true});
		return {line, column: column + 1};
	}

	return indexToPosition(string, index, {oneBased: true});
};

const addCodePointToUnexpectedToken = message => message.replace(
	// TODO[engine:node@>=20]: The token always quoted after Node.js 20
	/(?<=^Unexpected token )(?<quote>')?(.)\k<quote>/,
	(_, _quote, token) => `"${token}"(${getCodePoint(token)})`,
);

export default function parseJson(string, reviver, fileName) {
	if (typeof reviver === 'string') {
		fileName = reviver;
		reviver = undefined;
	}

	try {
		return JSON.parse(string, reviver);
	} catch (error) {
		throw new JSONError({
			jsonParseError: error,
			fileName,
			input: string,
		});
	}
}
