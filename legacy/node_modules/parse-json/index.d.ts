import type {JsonObject} from 'type-fest';

/**
Exposed for `instanceof` checking.
*/
export class JSONError extends Error { // eslint-disable-line @typescript-eslint/naming-convention
	/**
	The filename displayed in the error message, if any.
	*/
	fileName: string;

	/**
	The printable section of the JSON which produces the error.
	*/
	readonly codeFrame: string;

	/**
	The raw version of `codeFrame` without colors.
	*/
	readonly rawCodeFrame: string;
}

// Get `reviver`` parameter from `JSON.parse()`.
export type Reviver = Parameters<typeof JSON['parse']>['1'];

/**
Parse JSON with more helpful errors.

@param string - A valid JSON string.
@param reviver - Prescribes how the value originally produced by parsing is transformed, before being returned. See [`JSON.parse` docs](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/JSON/parse#Using_the_reviver_parameter
) for more.
@param filename - The filename displayed in the error message.
@returns A parsed JSON object.
@throws A {@link JSONError} when there is a parsing error.

@example
```
import parseJson, {JSONError} from 'parse-json';

const json = '{\n\t"foo": true,\n}';

parseJson(json);
// JSONError: Expected double-quoted property name in JSON at position 16 (line 3 column 1)

//   1 | {
//   2 |   "foo": true,
// > 3 | }
//     | ^

parseJson(json, 'foo.json');
// JSONError: Expected double-quoted property name in JSON at position 16 (line 3 column 1) in foo.json

//   1 | {
//   2 |   "foo": true,
// > 3 | }
//     | ^
//   fileName: 'foo.json',
//   [cause]: SyntaxError: Expected double-quoted property name in JSON at position 16 (line 3 column 1)
//       at JSON.parse (<anonymous>)
//       at ...

// You can also add the filename at a later point
try {
	parseJson(json);
} catch (error) {
	if (error instanceof JSONError) {
		error.fileName = 'foo.json';
	}

	throw error;
}
// JSONError: Expected double-quoted property name in JSON at position 16 (line 3 column 1) in foo.json

//   1 | {
//   2 |   "foo": true,
// > 3 | }
//     | ^

//   fileName: 'foo.json',
//   [cause]: SyntaxError: Expected double-quoted property name in JSON at position 16 (line 3 column 1)
//       at JSON.parse (<anonymous>)
//       at ...
```
*/
export default function parseJson(string: string, reviver?: Reviver, filename?: string): JsonObject;

/**
Parse JSON with more helpful errors.

@param string - A valid JSON string.
@param filename - The filename displayed in the error message.
@returns A parsed JSON object.
@throws A {@link JSONError} when there is a parsing error.

@example
```
import parseJson, {JSONError} from 'parse-json';

const json = '{\n\t"foo": true,\n}';

parseJson(json);
// JSONError: Expected double-quoted property name in JSON at position 16 (line 3 column 1)

//   1 | {
//   2 |   "foo": true,
// > 3 | }
//     | ^

parseJson(json, 'foo.json');
// JSONError: Expected double-quoted property name in JSON at position 16 (line 3 column 1) in foo.json

//   1 | {
//   2 |   "foo": true,
// > 3 | }
//     | ^
//   fileName: 'foo.json',
//   [cause]: SyntaxError: Expected double-quoted property name in JSON at position 16 (line 3 column 1)
//       at JSON.parse (<anonymous>)
//       at ...

// You can also add the filename at a later point
try {
	parseJson(json);
} catch (error) {
	if (error instanceof JSONError) {
		error.fileName = 'foo.json';
	}

	throw error;
}
// JSONError: Expected double-quoted property name in JSON at position 16 (line 3 column 1) in foo.json

//   1 | {
//   2 |   "foo": true,
// > 3 | }
//     | ^

//   fileName: 'foo.json',
//   [cause]: SyntaxError: Expected double-quoted property name in JSON at position 16 (line 3 column 1)
//       at JSON.parse (<anonymous>)
//       at ...
```
*/
export default function parseJson(string: string, filename?: string): JsonObject;
