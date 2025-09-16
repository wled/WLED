'use strict';

var spawn    = require('child_process').spawn;
var through  = require('through2');
var split    = require('split2');
var traverse = require('traverse');
var fields   = require('./fields');
var toArgv   = require('argv-formatter').format;
var combine  = require('stream-combiner2');
var fwd      = require('spawn-error-forwarder');

var END = '==END==';
var FIELD = '==FIELD==';

function format (fieldMap) {
  return fieldMap.map(function (field) {
      return '%' + field.key;
    })
    .join(FIELD) + END;
}

function trim () {
  return through(function (chunk, enc, callback) {
    if (!chunk) {
      callback();
    }
    else {
      callback(null, chunk);
    }
  });
}

function log (args, options) {
  return fwd(spawn('git', ['log'].concat(args), options), function (code, stderr) {
    return new Error('git log failed:\n\n' + stderr);
  })
  .stdout;
}

function args (config, fieldMap) {
  config.format = format(fieldMap);
  return toArgv(config);
}

exports.parse = function parseLogStream (config, options) {
  config  = config || {};
  var map = fields.map();
  return combine.obj([
    log(args(config, map), options),
    split(END + '\n'),
    trim(),
    through.obj(function (chunk, enc, callback) {
      var fields = chunk.toString('utf8').split(FIELD);
      callback(null, map.reduce(function (parsed, field, index) {
        var value = fields[index];
        traverse(parsed).set(field.path, field.type ? new field.type(value) : value);
        return parsed;
      }, {}));
    })
  ]);
};

exports.fields = fields.config;
