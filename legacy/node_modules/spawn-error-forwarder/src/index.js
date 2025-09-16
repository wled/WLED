'use strict';

function createErr (code, stderr) {
  return new Error(stderr);
}

module.exports = function (child, errFactory) {
  errFactory = errFactory || createErr;
  var stderr = [];
  child.stderr.on('data', function (chunk) {
    stderr.push(chunk);
  });
  child.on('close', function (code) {
    if (code !== 0) {
      child.stdout.emit('error', errFactory(
        code,
        Buffer.concat(stderr).toString()
      ));
    }
  });
  return child;
};
