module.exports = resolve;
var debug = require('debug')('inliner');

function resolve(inliner, todo, $) {
  debug('start %s links', todo.length);
  return todo.map(function images(image) {
    var url = $(image).attr('src');

    if (inliner.options.skipAbsoluteUrls &&
        (url.indexOf('//') === 0 || url.indexOf('http') === 0)) {
      debug('skipping remote image');
      inliner.emit('progress', 'skipping remote image');
      return false;
    }

    url = inliner.resolve(inliner.url, url);
    return inliner.image(url).then(function then(dataURL) {
      $(image).attr('src', dataURL);
    }).then(inliner.jobs.done.images);
  });
}

