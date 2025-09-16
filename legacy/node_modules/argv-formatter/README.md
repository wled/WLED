argv-formatter
==============

Bizarro [minimist](https://github.com/substack/minimist): transform an options object into into a `process.argv`-like array. Helpful for using `child_process` and passing complex arguments more easily. 

## Setup

```bash
$ npm install --save argv-formatter
```

## API

#### `formatter.format(object)` -> `Array`

Accepts an `object` of containing options and arguments and returns an array of arguments. 

* options with a value of `true` will be included with a flag only (`{R: true}` -> `['-R']`)
* options that are falsey will be removed
* all values will be stringified, (`{D: new Date(0)}` -> `['-D', 'Thurs Jan 1 1970...']`)
* arguments can be passed under the `_` key as a value or array of values

## Examples

To generate arguments to a `git log` command for printing the short hashes of commits that have changed our test files:
```js
var args = formatter.format({
  _: './test/*',
  format: '%h'
});
console.log(args.join(' ')); // --format=%h ./test/*

```

[git-log-parser](https://github.com/bendrucker/git-log-parser) uses this to spawn a `git` process:

```js
var spawn     = require('child_process').spawn;
var formatter = require('argv-formatter');
var args      = formatter.format(options);
var child     = spawn('git', ['log'].concat(args));
```
