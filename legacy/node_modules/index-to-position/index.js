// Prevent `String#lastIndexOf` treat negative index as `0`
const safeLastIndexOf = (string, searchString, index) =>
	index < 0 ? -1 : string.lastIndexOf(searchString, index);

function getPosition(text, textIndex) {
	const lineBreakBefore = safeLastIndexOf(text, '\n', textIndex - 1);
	const column = textIndex - lineBreakBefore - 1;

	let line = 0;
	for (
		let index = lineBreakBefore;
		index >= 0;
		index = safeLastIndexOf(text, '\n', index - 1)
	) {
		line++;
	}

	return {line, column};
}

export default function indexToLineColumn(text, textIndex, {oneBased = false} = {}) {
	if (typeof text !== 'string') {
		throw new TypeError('Text parameter should be a string');
	}

	if (!Number.isInteger(textIndex)) {
		throw new TypeError('Index parameter should be an integer');
	}

	if (textIndex < 0 || (textIndex >= text.length && text.length > 0)) {
		throw new RangeError('Index out of bounds');
	}

	const position = getPosition(text, textIndex);

	return oneBased ? {line: position.line + 1, column: position.column + 1} : position;
}
