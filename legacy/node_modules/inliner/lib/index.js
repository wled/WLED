module.exports = Inliner;

var debug = require('debug')('inliner');
var events = require('events');
var path = require('path');
var util = require('util');
var fs = require('then-fs');
var assign = require('lodash.assign');
var forEach = require('lodash.foreach');
var Promise = require('es6-promise').Promise; // jshint ignore:line
var findAssets = require('./find-assets');
var iconv = require('iconv-lite');
var charset = require('charset');
var jschardet = require('jschardet');
var querystring = require('querystring');

// note: these tasks (excluding HTML), match up to files in lib/tasks/*.js
var tasks = {
  html: 'html', // this is abritrary since it's manually processed
  js: 'script',
  svg: 'svg',
  links: 'link[rel=stylesheet]',
  favicon: 'link[rel=icon],link[rel="shortcut icon"],' +
    'link[rel="apple-touch-icon"],link[rel="apple-touch-startup-image"]',
  styles: 'style',
  'style-attrs': '[style]:not(svg *)',  // only style attrs in HTML, not SVG
  images: 'img',
  videos: 'video',
};
var taskRunner = Object.keys(tasks).reduce(function (acc, curr) {
  if (curr !== 'html') {
    acc[curr] = require('./tasks/' + curr);
  }
  return acc;
}, {});

// source is typicaly a URL, but can be a file location OR an HTML string
function Inliner(source, options, callback) {
  var inliner = this;
  events.EventEmitter.call(this);

  // allow for source to be optional
  if (typeof source !== 'string') {
    callback = options;
    options = source;
  }

  if (typeof options === 'function') {
    callback = options;
    options = {};
  }

  if (!options) {
    options = {};
  }

  if (options.url) {
    this.url = options.url;
  }

  if (options.filename) {
    this.filename = options.filename;
  }

  if (options.source) {
    this.source = options.source;
  } else {
    this.source = source;
  }

  // this is an intentioal change. `.headers` is compatible with the request
  // module, but -H and --header is compatible (i.e. the same as) cURL
  if (options.header) {
    options.headers = options.header;
    delete options.header;
  }

  if (options.headers && !Array.isArray(options.headers)) {
    options.headers = [options.headers];
  }

  if (options.headers && Array.isArray(options.headers)) {
    options.headers = options.headers.reduce(function (acc, curr) {
      if (typeof curr === 'string') {
        var parts = curr.split(':').map(function (s) {
          return s.trim();
        });
        acc[parts[0]] = parts[1];
      } else {
        var key = Object.keys(curr);
        acc[key] = curr[key];
      }
      return acc;
    }, {});
  }

  if (options.headers && typeof options.headers[0] === 'string') {
    // convert to an object of key/value pairs
    options.headers = options.headers.reduce(function (acc, curr) {
      var pair = querystring.parse(curr);
      var key = Object.keys(pair).shift();
      acc[key] = pair[key];
      return acc;
    }, {});
  }

  this.headers = options.headers || {};

  this.callback = function wrapper(error, res) {
    // noop the callback once it's fired
    inliner.callback = function noop() {
      inliner.emit('error', 'callback fired again');
    };

    callback(error, res);
  };
  this.options = assign({}, Inliner.defaults(), options);
  this.jobs = {
    total: 0,
    todo: 0,
    tasks: tasks,
    add: this.addJob.bind(this),
    breakdown: {},
    done: {},
  };

  Object.keys(this.jobs.tasks).forEach(function (key) {
    this.jobs.breakdown[key] = 0;
    this.jobs.done[key] = this.completeJob.bind(this, key);
  }.bind(this));

  this.isFile = options.useStdin || false;

  this.on('error', function localErrorHandler(event) {
    inliner.callback(event);
  });

  // this allows the user code to get the object back before
  // it starts firing events
  if (this.source) {
    if (typeof setImmediate === 'undefined') {
      global.setImmediate = function setImmediatePolyfill(fn) {
        // :-/
        setTimeout(fn, 0);
      };
    }
    this.promise = new Promise(function (resolve) {
      global.setImmediate(function () {
        resolve(inliner.main());
      });
    });
  } else {
    this.promise = Promise.reject(new Error('No source to inline'));
  }

  return this;
}

util.inherits(Inliner, events.EventEmitter);
Inliner.prototype.updateTodo = updateTodo;
Inliner.prototype.addJob = addJob;
Inliner.prototype.completeJob = completeJob;
Inliner.prototype.cssImages = require('./css').getImages;
Inliner.prototype.cssImports = require('./css').getImports;
Inliner.prototype.image = require('./image');
Inliner.prototype.uglify = require('./javascript');
Inliner.prototype.resolve = resolve;
Inliner.prototype.removeComments = removeComments;
Inliner.prototype.get = require('./get');
Inliner.prototype.main = main;
Inliner.prototype.findAssets = findAssets;

// static properties and methods
Inliner.errors = require('./errors');
Inliner.defaults = require('./defaults');

// main thread of functionality that does all the inlining
function main() {
  var inliner = this;
  var url = this.source;

  return fs.exists(this.filename || url)
  .then(function exists(isFile) {
    if (!isFile) {
      throw new Error('Not a file - use URL parser');
    }

    debug('inlining file');

    inliner.isFile = true;
    inliner.url = url; // this is a hack for the `resolve` function later on
    return inliner.get(this.filename || url, { encoding: 'binary' });
  })
  .catch(function isUrl() {
    // make the best guess as to whether we're working with a url
    if (inliner.url || url.indexOf('<') === -1) {
      url = inliner.url || inliner.source;
      // check for protocol on URL
      if (url.indexOf('http') !== 0) {
        url = 'http://' + url;
      }

      inliner.url = url;

      debug('inlining url');
      return inliner.get(url, { encoding: 'binary' });
    }

    // otherwise we're dealing with an inline string
    debug('inlining by string: ', inliner.source);
    inliner.isFile = true;
    inliner.url = '.';
    var res = {
      body: new Buffer(inliner.source),
      headers: {
        'content-type': 'text/html',
      },
    };
    return res;
  })
  .then(inliner.jobs.add('html'))
  .then(function processHTML(res) {
    inliner.jobs.done.html();
    debug('processing HTML');

    debug(inliner.options);

    var body;
    var cheerioLoadOptions = {};
    var enc = inliner.options.encoding;

    // try to determine the encoding from the headers and the body
    if (!enc) {
      enc = charset(res.headers, res.body);
      enc = enc || jschardet.detect(res.body).encoding.toLowerCase();
    }

    // normalise to avoid any mistakes
    if (enc === 'utf-8' || enc === 'utf8') {
      enc = 'utf-8';
    }

    cheerioLoadOptions.decodeEntities = false;
    if (enc !== 'utf-8') {
      debug('decoding from: %s', enc);
      body = iconv.encode(iconv.decode(res.body, enc), 'utf-8');
    } else {
      body = res.body;
    }

    body = body.toString();

    // if we spot some SVG elements in the source,
    // then we'll parse as XML to correctly get the SVG
    if (body.indexOf('<?xml') !== -1 ||
        body.indexOf('<?XML') !== -1) {
      cheerioLoadOptions.xmlMode = true;
    }

    if (body.indexOf('<svg') !== -1) {
      cheerioLoadOptions.lowerCaseAttributeNames = false;
    }

    var todo = inliner.findAssets(body, cheerioLoadOptions);
    var $ = todo.$;
    delete todo.$;

    if (enc !== 'utf-8') {
      // when transcoding remove any meta tags setting the charset
      $('meta').each(function charMeta() {
        var attrs = $(this).attr();
        var content = attrs.content || '';
        if (attrs.charset || content.toLowerCase().indexOf('charset=') !== -1) {
          $(this).remove();
        }
      });
    }

    var promises = [];

    forEach(todo, function forEach(todo, key) {
      if (key === 'images' && !inliner.options.images) {
        // skip images if the user doesn't want them
        delete todo.images;
        debug('skipping images');
        return;
      }

      if (key === 'videos' && !inliner.options.videos) {
        // skip videos if the user doesn't want them
        delete todo.videos;
        debug('skipping videos');
        return;
      }

      if (!todo.length) {
        return;
      }

      inliner.jobs.add(key, todo.length);

      var tasks = taskRunner[key](inliner, todo.get(), $);
      promises = promises.concat(tasks);
    });

    return Promise.all(promises).then(function then() {
      var html = '';

      // remove comments
      if (!inliner.options.preserveComments) {
        inliner.removeComments($(':root')[0], $);
      }

      // collapse the white space
      if (inliner.options.collapseWhitespace) {
        debug('collapsing whitespace');
        $('pre, textarea').each(function () {
          $(this).html($(this).html()
              .replace(/\n/g, '~~nl~~')
              .replace(/\s/g, '~~s~~'));
        });

        $('script').each(function () {
          $(this).text($(this).text()
              .replace(/\n/g, '~~nl~~')
              .replace(/\s/g, '~~s~~'));
        });

        html = $.html()
          .replace(/\s+/g, ' ')
          .replace(/~~nl~~/g, '\n')
          .replace(/~~s~~/g, ' ');
      } else {
        html = $.html();
      }

      debug('completed: %s bytes', html.length);

      return html;
    });

  })
  .then(function then(html) {
    inliner.callback(null, html);
    inliner.emit('end');
  })
  .catch(function errHandler(error) {
    debug('fail', error.stack);
    inliner.callback(error);
    inliner.emit('error', error);
    throw error;
  });
}

function removeComments(element, $) {
  if (!element || !element.childNodes) {
    return;
  }

  var nodes = element.childNodes;
  var i = nodes.length;

  while (i--) {
    var first = (nodes[i].nodeValue || '').charAt(0);
    if (nodes[i].type === 'comment' && first !== '[' && first !== '#') {
      $(nodes[i]).remove();
    }
    removeComments(nodes[i], $);
  }
}

function resolve(from, to) {
  if (!to) {
    to = from;
    from = this.url;
  }

  // don't resolve data urls (already inlined)
  if (to.indexOf('data:') === 0) {
    return to;
  }

  // always strip querystrings from requests off a local file
  if (this.isFile) {
    to = to.replace(/\??#.*$/, '');
  }

  // don't resolve http(s) urls (no need to resolve)
  if (to.indexOf('http:') === 0 || to.indexOf('https:') === 0) {
    return to;
  }

  if (!this.isFile || from.indexOf('http') === 0) {
    return require('url').resolve(from, to);
  }

  var base = path.dirname(from);
  return path.resolve(base, to);
}

function updateTodo() {
  var jobs = this.jobs;
  jobs.todo = Object.keys(jobs.breakdown).reduce(function (acc, key) {
    return acc + jobs.breakdown[key];
  }, 0);

  var breakdown = Object.keys(jobs.breakdown).map(function (key) {
    if (jobs.breakdown[key]) {
      return key + '(' + jobs.breakdown[key] + ')';
    }
    return false;
  }).filter(Boolean);

  this.emit('jobs', {
    total: jobs.total,
    todo: jobs.todo,
    breakdown: breakdown,
  });
}

function addJob(type) {
  var n = typeof arguments[1] === 'number' ? arguments[1] : 1;
  this.jobs.breakdown[type] += n;
  this.jobs.total += n;
  this.updateTodo();

  debug('%s: %s', type, n);

  // this allows me to include addJob as part of a promise chain
  return arguments[1];
}

function completeJob(type) {
  this.jobs.breakdown[type]--;
  this.updateTodo();

  // this allows me to include addJob as part of a promise chain
  return arguments[1];
}
