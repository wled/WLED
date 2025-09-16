module.exports = image;

var debug = require('debug')('inliner');
var mime = require('mime');
var SVGO = require('svgo');
var svgo = new SVGO();
var basename = require('path').basename;
var Promise = require('es6-promise').Promise; // jshint ignore:line

function image(url) {
  url = url.replace(/\??#.*$/, '');
  var inliner = this;
  this.emit('progress', 'get image ' + basename(url));
  return this.get(url, { encoding: 'binary' }).then(function then(res) {
    if (url.indexOf('data:') === 0) {
      return url;
    }

    debug('image loaded: %s', url);

    // if the url is SVG, let's compress and use the XML directly
    if (res.body && mime.lookup(url) === 'image/svg+xml' &&
      !inliner.options.nosvg) {
      return new Promise(function (resolve, reject) {
        svgo.optimize(res.body.toString(), function (result) {
          debug('optimising svg');
          if (result.error) {
            debug('errored', result.error);
            return reject(new Error(result.error));
          }
          // supposing/given:
          // - we are in a CSS single quoted string
          // - this css *may* be in a HTML file, in a <style>
          // - this css *may* be in a HTML file, in a double-quoted attribute
          // - the optimizer possibly replaced:
          //   - & by &amp;  <<< unhandled: add a &amp; in the svg text to test
          //   - ' by &apos;
          //   - " by &quot; (not around attributes)
          // Thus, we have to do a tricky escape pattern
          // ... we use % encoding for html level escape
          // ... we need to use proper \ escape for css level escape
          // ... (or we could have a ' in ' problem (nested sinle quotes))
          // ... so, before anything, we need to escape both \ and %
          var replacements = {
            '<': '%3C',  // html escape
            '"': '%22',  // html escape
            '&quot;': '%22',  // html escape
            '#': '%23',  // css url path escape
            '\n': '\\n', // css string escape
            '\'': '\\\'',// css string escape
            '&apos;': '\\\'',// css string escape
          };
          var body = result.data.replace(/\\/g, '\\').replace(/%/g, '%25')
                     .replace(/[<"#\n']|&quot;|&apos;/g, function (m) {
            return replacements[m];
          });
          resolve('data:image/svg+xml;utf8,' + body);
        });
      });
    }

    // otherwise we base64 encode the image
    return 'data:' + res.headers['content-type'] + ';base64,' +
            new Buffer(res.body, 'binary').toString('base64');

  }).catch(function errorHandle(error) {
    debug('image %s failed to load', url, error);
    throw error;
  });
}
