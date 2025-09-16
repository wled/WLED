# index-to-position

> Convert a string index to its line and column position

## Install

```sh
npm install index-to-position
```

## Usage

```js
import indexToPosition from 'index-to-position';

indexToPosition('hello\nworld\n!', 7);
//=> {line: 1, column: 1}
```

## API

### `indexToPosition(text, index, options?)`

#### text

Type: `string`

The text in which to find the line and column position.

#### index

Type: `number`

The index in the string for which to find the line and column position.

#### options

Type: `object`

##### oneBased

Type: `boolean`\
Default: `false`

Whether to use 1-based or 0-based indexing for the result.
