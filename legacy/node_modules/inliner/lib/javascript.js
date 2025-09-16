module.exports = uglify;

var debug = require('debug')('inliner');
var UglifyJS = require('uglify-js');

function uglify(source) {
  var notIESafe = !this.options.iesafe;

  this.emit('progress', 'compressing javascript');

  source = source.trim();

  if (source === '') {
    this.jobs.done.js();
    return '';
  }

  debug('uglifying %sbytes', source.length);

  var result = '';

  try {
    result = UglifyJS.minify(source, {
      fromString: true,
      // must set screw_ie8 for each option group
      // https://github.com/mishoo/UglifyJS2/issues/1204#issuecomment-234714094
      // jscs:disable requireCamelCaseOrUpperCaseIdentifiers
      compress: { screw_ie8: notIESafe },
      mangle: { screw_ie8: notIESafe },
      output: { screw_ie8: notIESafe },
      // jscs:enable requireCamelCaseOrUpperCaseIdentifiers
    }).code;
  } catch (e) {
    // failed to uglify, just return it plain
    result = source;
  }

  this.jobs.done.js();

  return result;
}
