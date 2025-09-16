export type Options = {
	/**
	Whether to use 1-based or 0-based indexing for the result.

	@default false
	*/
	readonly oneBased?: boolean;
};

/**
Convert a string index to its line and column position.

@param text - The text in which to find the line and column position.
@param index - The index in the string for which to find the line and column position.
@returns The line and column position.
*/
export default function indexToPosition(
	text: string,
	index: number,
	options?: Options
): {line: number; column: number};
