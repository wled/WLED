#  [![NPM version][npm-image]][npm-url] [![Build Status][travis-image]][travis-url] [![Dependency Status][daviddm-image]][daviddm-url] [![Coverage Status][coveralls-image]][coveralls-url]

> Get a compare function for array to sort


## Install

```sh
$ npm install --save compare-func
```


## Usage

```js
var compareFunc = require('compare-func');

// sort by an object property
[{x: 'b'}, {x: 'a'}, {x: 'c'}].sort(compareFunc('x'));
//=> [{x: 'a'}, {x: 'b'}, {x: 'c'}]

// sort by a nested object property
[{x: {y: 'b'}}, {x: {y: 'a'}}].sort(compareFunc('x.y'));
//=> [{x: {y: 'a'}}, {x: {y: 'b'}}]

// sort by the `x` propery, then `y`
[{x: 'c', y: 'c'}, {x: 'b', y: 'a'}, {x: 'b', y: 'b'}].sort(compareFunc(['x', 'y']));
//=> [{x: 'b', y: 'a'}, {x: 'b', y: 'b'}, {x: 'c', y: 'c'}]

// sort by the returned value
[{x: 'b'}, {x: 'a'}, {x: 'c'}].sort(compareFunc(function(el) {
  return el.x;
}));
//=> [{x: 'a'}, {x: 'b'}, {x: 'c'}]
```


## API

### compareFunc([property])

Returns a compare function for array to sort

#### property

Type: `string`, `function` or `array` of either

If missing it sorts on itself.

The string can be a [dot path](https://github.com/sindresorhus/dot-prop) to a nested object property.


## Related

- [sort-on](https://github.com/sindresorhus/sort-on) - Sort an array on an object property


## License

MIT © [Steve Mao](https://github.com/stevemao)


[npm-image]: https://badge.fury.io/js/compare-func.svg
[npm-url]: https://npmjs.org/package/compare-func
[travis-image]: https://travis-ci.org/stevemao/compare-func.svg?branch=master
[travis-url]: https://travis-ci.org/stevemao/compare-func
[daviddm-image]: https://david-dm.org/stevemao/compare-func.svg?theme=shields.io
[daviddm-url]: https://david-dm.org/stevemao/compare-func
[coveralls-image]: https://coveralls.io/repos/stevemao/compare-func/badge.svg
[coveralls-url]: https://coveralls.io/r/stevemao/compare-func
