#!/usr/bin/env node

var readFileSync = require('fs').readFileSync;
var Promise = require('es6-promise').Promise; // jshint ignore:line
var ansi = require('ansi-escapes');

main();

function main() {
  var argv = require('./options')(process.argv.slice(2));

  if (argv.debug) {
    require('debug').enable('inliner');
  }

  var url = argv._.shift();

  if (argv.version) {
    console.log(require('../package.json').version || 'development');
    process.exit(0);
  }

  if ((!url && !argv.useStdin) || argv.help) {
    var usage = readFileSync(
      __dirname + '/../docs/usage.txt', 'utf8'
    );
    console.log(usage);
    process.exit(0);
  }

  var Inliner = require('../');

  var p = Promise.resolve(url);

  if (argv.useStdin) {
    p = new Promise(function (resolve) {
      process.stdin.resume();
      process.stdin.setEncoding('utf8');

      var data = '';

      process.stdin.on('data', function (chunk) {
        data += chunk;
      });

      process.stdin.on('end', function () {
        resolve(data);
      });
    });
  }

  var time = process.hrtime();
  p.then(function (source) {
    var inliner = new Inliner(source, argv, function result(error, html) {
      if (error) {
        var message = Inliner.errors[error.code] || error.message;
        console.error(message);

        if (argv.debug) {
          console.error(error.stack);
        }
        process.exit(1);
      }

      console.log(html);
    });

    return inliner;
  }).then(function (inliner) {
    // checks for available update and returns an instance
    // note: we're doing this after we kick off inliner, since there's a
    // noticeable lag in boot because of it
    var defaults = require('lodash.defaults');
    var pkg = JSON.parse(readFileSync(__dirname + '/../package.json'));

    require('update-notifier')({
      pkg: defaults(pkg, { version: '0.0.0' }),
    }).notify();

    if (argv.verbose) {
      var jobs = {};
      var update = require('./progress');
      var progress = '';

      inliner.on('progress', function progressEvent(event) {
        progress = event;
        // console.log(JSON.stringify({ type: 'progress', progress, jobs }));
        update(event, jobs, argv.debug);
      });

      inliner.on('jobs', function jobsEvent(event) {
        jobs = event;
        // console.log(JSON.stringify({ type: 'jobs', progress, jobs }));
        update(progress, jobs, argv.debug);
      });

      inliner.on('warning', function warningEvent(event) {
        progress = event;
        // console.log(JSON.stringify({ type: 'warning', progress, jobs }));
        update(event, jobs, true);
      });

      inliner.on('end', function () {
        update.end(time);
      });

      'exit SIGINT SIGTERM'.split(' ').map(function (event) {
        process.once(event, function () {
          process.stderr.write(ansi.cursorShow); // put the cursor back
          try { process.kill(process.pid, event); } catch (e) {}
        });
      });
    } else {
      inliner.on('warning', function progress(event) {
        console.warn('warning: ' + event);
      });
    }
  }).catch(function (error) {
    console.error(error.stack);
  });
}
