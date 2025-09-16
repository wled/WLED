#  [![NPM version][npm-image]][npm-url] [![Build Status][travis-image]][travis-url] [![Dependency Status][daviddm-image]][daviddm-url] [![Coverage Status][coveralls-image]][coveralls-url]

> Turn anything into an array


## Install

```sh
$ npm install --save array-ify
```


## Usage

```js
var arrayify = require('array-ify');

arrayify('unicorn');
//=> ['unicorn']

arrayify(['unicorn']);
//=> ['unicorn']

arrayify(null);
//=> [null]

arrayify(undefined);
//=> [undefined]
```


## Related

- [arrify](https://github.com/sindresorhus/arrify) - Convert a value to an array

The difference that is this module does **NOT** turn `null` or `undefined` to an empty array.


## License

MIT © [Steve Mao](https://github.com/stevemao)


[npm-image]: https://badge.fury.io/js/array-ify.svg
[npm-url]: https://npmjs.org/package/array-ify
[travis-image]: https://travis-ci.org/stevemao/array-ify.svg?branch=master
[travis-url]: https://travis-ci.org/stevemao/array-ify
[daviddm-image]: https://david-dm.org/stevemao/array-ify.svg?theme=shields.io
[daviddm-url]: https://david-dm.org/stevemao/array-ify
[coveralls-image]: https://coveralls.io/repos/stevemao/array-ify/badge.svg
[coveralls-url]: https://coveralls.io/r/stevemao/array-ify
