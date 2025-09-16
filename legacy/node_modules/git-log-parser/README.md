git-log-parser [![Build Status](https://travis-ci.org/bendrucker/git-log-parser.svg?branch=master)](https://travis-ci.org/bendrucker/git-log-parser)
==============

Run `git log` and return a stream of commit objects.

## Setup

```bash
$ npm install git-log-parser
```

## API

#### `log.parse(config, options)` -> `Stream(commits)`

Accepts a `config` object mapping to the [options accepted by `git log`](http://git-scm.com/docs/git-log). `config` will be automatically converted to command line options and flags by [argv-formatter](https://github.com/bendrucker/argv-formatter). Returns a stream of commit objects. 

`options` is passed directly to [`child_process.spawn`](https://nodejs.org/api/child_process.html#child_process_child_process_spawn_command_args_options).

A commit is structured as follows:

```js
{
  commit: {
    'long': '4bba6092ecb2571301ca0daa2c55336ea2c74ea2',
    'short': '4bba609'
  },
  tree: {
    'long': 'b4ef3379e639f8c0034831deae8f6ce63dd41566',
    'short': 'b4ef337'
  },
  author: {
    'name': 'Ben Drucker',
    'email': 'bvdrucker@gmail.com',
    'date': new Date('2014-11-20T14:39:01.000Z')
  },
  committer: {
    'name': 'Ben Drucker',
    'email': 'bvdrucker@gmail.com',
    'date': new Date('2014-11-20T14:39:01.000Z')
  },
  subject: 'Initial commit',
  body: 'The commit body'
}
```

`author.date` and `commiter.date` are `Date` objects while all other values are strings.

If you just want an array of commits, use [stream-to-array](https://www.npmjs.com/package/stream-to-array) to wrap the returned stream.

#### `log.fields` -> `Object`

Commit objects contain the most frequently used commit information. However, the [field mappings](https://github.com/bendrucker/git-log-parser/blob/master/src/fields.js) used to format and then parse log output can be amended before calling the parser. Consult the [full range of formatting placeholders](http://opensource.apple.com/source/Git/Git-19/src/git-htmldocs/pretty-formats.txt) and add the placeholder to the object tree if you wish to add extra fields.

## Example

Get all commits from earlier than an hour ago and stream them to `stdout` as pretty-printed JSON

```js
var log      = require('git-log-parser');
var through2 = require('through2');

log.parse({
  before: new Date(Date.now() - 60 * 60 * 1000)
})
.pipe(through2.obj(function (chunk, enc, callback) {
  callback(null, JSON.stringify(chunk, undefined, 2));
}))
.pipe(process.stdout);
```

Note that `before` is stringified and passed directly as an argument to `git log`. No special handling is required for any standard `git log` option. You can filter by committer, time, or any other field supported by [`git log`](http://git-scm.com/docs/git-log).
