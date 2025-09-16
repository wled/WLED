module.exports = resolve;
var debug = require('debug')('inliner');
var Promise = require('es6-promise').Promise; // jshint ignore:line

function resolve(inliner, todo, $) {
  debug('start %s videos', todo.length);
  return todo.map(function videos(video) {
    var url = $(video).attr('src');
    var posterUrl = $(video).attr('poster');
    var promises = [];

    if (inliner.options.skipAbsoluteUrls &&
        (url.indexOf('//') === 0 || url.indexOf('http') === 0)) {
      debug('skipping remote video');
      inliner.emit('progress', 'skipping remote video');
    } else {
      debug('resolving local video');
      url = inliner.resolve(inliner.url, url);
      promises.push(inliner.image(url).then(function then(dataURL) {
        $(video).attr('src', dataURL);
      }));
    }

    if (inliner.options.skipAbsoluteUrls &&
        (posterUrl.indexOf('//') === 0 || posterUrl.indexOf('http') === 0)) {
      debug('skipping remote video');
      inliner.emit('progress', 'skipping remote video');
    } else {
      debug('resolving local poster');
      posterUrl = inliner.resolve(inliner.url, posterUrl);
      promises.push(inliner.image(posterUrl).then(function then(dataURL) {
        $(video).attr('poster', dataURL);
      }));
    }

    if (!promises.length) {
      return false;
    }
    return Promise.all(promises).then(inliner.jobs.done.videos);
  });
}

