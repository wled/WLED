module.exports = resolve;
var debug = require('debug')('inliner');

function resolve(inliner, todo, $) {
  debug('start %s styles', todo.length);
  return todo.map(function links(style) {
    var css = $(style).text();
    inliner.emit('progress', 'processing inline css');
    return inliner.cssImports(inliner.url, css)
      .then(inliner.cssImages.bind(inliner, inliner.url))
      .then(function then(css) {
        $(style).text(css);
        inliner.jobs.done.styles();
      });
  });
}
