module.exports = resolve;
var debug = require('debug')('inliner');

function resolve(inliner, todo, $) {
  debug('start %s style attributes', todo.length);
  return todo.map(function links(style) {
    var css = $(style).attr('style');
    inliner.emit('progress', 'processing inline css');
    return inliner.cssImports(inliner.url, css)
      .then(inliner.cssImages.bind(inliner, inliner.url))
      .then(function then(css) {
        inliner.jobs.done['style-attrs']();
        $(style).attr('style', css);
      });
  });
}
