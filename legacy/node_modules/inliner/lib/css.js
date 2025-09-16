var match = /url\((?:['"]*)(?!['"]*data:)(.*?)(?:['"]*)\)/g;

module.exports = {
  getImages: getImages,
  getImports: getImports,
};

var Promise = require('es6-promise').Promise; // jshint ignore:line
var debug = require('debug')('inliner');
var basename = require('path').basename;
var CHARTSET_RE = /@charset\s+["']([\w\-_]+)["'];/i;
var iconv = require('iconv-lite');


function getImages(root, css) {
  var inliner = this;

  if (inliner.options.images === false) {
    return css;
  }

  var singleURLMatch = /url\(\s*(['"]*)(?!['"]*data:)(.*?)(['"]*)\s*\)/;
  var matches = css.match(match) || [];
  var images = matches.map(function eachURL(url) {
    var match = url.match(singleURLMatch);
    var source = match[2];
    return {
      source: match.input,
      resolved: inliner.resolve(root, source),
    };
  });

  debug('adding %s CSS assets', images.length);
  inliner.jobs.add('images', images.length);

  return Promise.all(images.map(function map(url) {
    return inliner.image(url.resolved).then(function then(dataURL) {
      inliner.jobs.done.images();
      css = replace(css, url.source, 'url(\'' + dataURL + '\')');
      return css;
    });
  })).then(function then() {
    return css;
  });
}

function replace(body, source, target) {
  return body.split(source).join(target);
}

function getImports(root, css) {
  css = convertCharset(css);
  // change to a string in case the CSS is a buffer, which is the case
  // when we're reading off the local file system
  if (typeof css !== 'string') {
    css = css.toString();
  }
  var position = css.indexOf('@import');
  var inliner = this;

  if (position !== -1) {
    inliner.jobs.add('link', 1);
    var match = (css.match(/@import(\s*.*?);/) || [null, ''])[1];
    var url = match.replace(/url/, '')
      .replace(/['}"()]/g, '')
      .replace(/;/, '')
      .trim()
      .split(' '); // clean up

    // if url has a length > 1, then we have media types to target
    var resolvedURL = inliner.resolve(root, url[0]);
    return inliner.get(resolvedURL).then(function then(res) {
      var importedCSS = convertCharset(res.body);
      inliner.jobs.done.links();
      inliner.emit('progress', 'import ' + basename(resolvedURL));
      if (url.length > 1) {
        url.shift();
        importedCSS = '@media ' + url.join(' ') + '{' + importedCSS + '}';
      }

      css = css.replace('@import' + match + ';', importedCSS);
      return getImports.call(inliner, root, css);
    });
  }

  if (inliner.options.compressCSS) {
    inliner.emit('progress', 'compress css');
    css = compress(css);
  }

  return Promise.resolve(css);
}

function compress(css) {
  return css
    .replace(/\s+/g, ' ')
    .replace(/:\s+/g, ':')
    .replace(/\/\*.*?\*\//g, '')
    .replace(/\} /g, '}')
    .replace(/ \{/g, '{')
    // .replace(/\{ /g, '{')
    .replace(/; /g, ';')
    .replace(/\n+/g, '');
}

function convertCharset(cssBody) {
  var end = 0;
  var maxLength = 64;
  if (cssBody) {
    end = cssBody.length > maxLength ? maxLength : cssBody.length;
    var charsetData = cssBody.slice(0, end).toString();
    var matchs = CHARTSET_RE.exec(charsetData);
    if (matchs) {
      var charset = matchs[1].toLowerCase();
      if (charset && charset !== 'utf-8' && charset !== 'utf8') {
        debug('decoding from: %s', charset);
        if (iconv.encodingExists(charset)) {
          return iconv.encode(iconv.decode(cssBody, charset), 'utf-8')
            .toString()
            .replace(CHARTSET_RE, '');
        }
        console.warn('no such charset: ' + charset);
      }
    }
  }
  return cssBody;
}
