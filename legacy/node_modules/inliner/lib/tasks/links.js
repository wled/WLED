module.exports = resolve;
var debug = require('debug')('inliner');
var basename = require('path').basename;

function resolve(inliner, todo, $) {
  debug('start %s links', todo.length);
  return todo.map(function links(link) {
    var url = $(link).attr('href');
    if (inliner.options.skipAbsoluteUrls &&
        (url.indexOf('//') === 0 || url.indexOf('http') === 0)) {
      debug('skipping remote links');
      inliner.emit('progress', 'skipping remote links');
      return false;
    }
    if (url.indexOf('http') !== 0) {
      url = inliner.resolve(inliner.url, url);
    }
    inliner.emit('progress', 'processing external css ' + basename(url));
    return inliner.get(url).then(function then(res) {
      var css = res.body;
      inliner.jobs.done.links();
      return inliner.cssImports(url, css)
        .then(inliner.cssImages.bind(inliner, url));
    }).then(function then(css) {
      $(link).replaceWith('<style>' + css + '</style>');
    });
  });
}
