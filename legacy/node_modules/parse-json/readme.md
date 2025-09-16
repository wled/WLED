# parse-json

> Parse JSON with more helpful errors

## Install

```sh
npm install parse-json
```

## Usage

```js
import parseJson, {JSONError} from 'parse-json';

const json = '{\n\t"foo": true,\n}';


JSON.parse(json);
/*
SyntaxError: Expected double-quoted property name in JSON at position 16 (line 3 column 1)
*/


parseJson(json);
/*
JSONError: Expected double-quoted property name in JSON at position 16 (line 3 column 1)

  1 | {
  2 |   "foo": true,
> 3 | }
    | ^
*/


parseJson(json, 'foo.json');
/*
JSONError: Expected double-quoted property name in JSON at position 16 (line 3 column 1) in foo.json

  1 | {
  2 |   "foo": true,
> 3 | }
    | ^
  fileName: 'foo.json',
  [cause]: SyntaxError: Expected double-quoted property name in JSON at position 16 (line 3 column 1)
      at JSON.parse (<anonymous>)
      at ...
*/


// You can also add the filename at a later point
try {
	parseJson(json);
} catch (error) {
	if (error instanceof JSONError) {
		error.fileName = 'foo.json';
	}

	throw error;
}
/*
JSONError: Expected double-quoted property name in JSON at position 16 (line 3 column 1) in foo.json

  1 | {
  2 |   "foo": true,
> 3 | }
    | ^

  fileName: 'foo.json',
  [cause]: SyntaxError: Expected double-quoted property name in JSON at position 16 (line 3 column 1)
      at JSON.parse (<anonymous>)
      at ...
*/
```

## API

### parseJson(string, reviver?, filename?)

Throws a `JSONError` when there is a parsing error.

#### string

Type: `string`

#### reviver

Type: `Function`

Prescribes how the value originally produced by parsing is transformed, before being returned. See [`JSON.parse` docs](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/JSON/parse#Using_the_reviver_parameter
) for more.

#### filename

Type: `string`

The filename displayed in the error message.

### JSONError

Exposed for `instanceof` checking.

#### fileName

Type: `string`

The filename displayed in the error message.

#### codeFrame

Type: `string`

The printable section of the JSON which produces the error.

#### rawCodeFrame

Type: `string`

The raw version of `codeFrame` without colors.
