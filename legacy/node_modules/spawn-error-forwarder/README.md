spawn-error-forwarder [![Build Status](https://travis-ci.org/bendrucker/spawn-error-forwarder.svg?branch=master)](https://travis-ci.org/bendrucker/spawn-error-forwarder)
=====================

Emit errors on stdout stream for a spawned child process. Useful for capturing errors from a spawned process when you want the output from stdout. 

## Setup
```bash
$ npm install spawn-error-forwarder
```

## API

#### `fwd(child [, errFactory]` -> `child`

Buffers `child.stderr` output. If the spawned process exits with a code `> 0`, the buffered output of `child.stderr` is used to generate an error which is emitted on `child.stdout`. By default, the error message is the output of `child.stderr`. If you provide an `errFactory` function, it will be called with `code, stderr` where `code` is the child's exit code and `stderr` is string that contains the output of `child.stderr`. `errFactory` should return an `Error` to be emitted on `child.stdout`. 

## Example

```js
var fwd   = require('spawn-error-forwarder');
var spawn = require('child_process').spawn;
var child = spawn('git', ['log', 'non-existent-path']);

fwd(child, function (code, stderr) {
  return new Error('git log exited with ' + code + ':\n\n' + stderr);
});

child.stdout
  .on('error', console.error.bind(console))
  .pipe(process.stdout);
```

We want to pipe the output of `git log` to `process.stdout` but since we're providing a path that doesn't exist git will exit with a non-zero code and we'll log its output with `console.error`. 
