module.exports = findAssets;

var cheerio = require('cheerio');
var debug = require('debug')('inliner');

function findAssets(html, cheerioLoadOptions) {
  var $ = cheerio.load(html, cheerioLoadOptions);
  debug('loaded DOM');

  var tasks = this.jobs.tasks;
  var res = Object.keys(tasks).reduce(function (acc, task) {
    if (task === 'html') { // skip html task
      return acc;
    }

    acc[task] = $(tasks[task]);

    return acc;
  }, {});

  res.$ = $;

  return res;
}
