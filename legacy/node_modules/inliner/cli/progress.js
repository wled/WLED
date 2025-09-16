var styles = require('ansi-styles');
var ansi = require('ansi-escapes');

console.warn(ansi.cursorHide + '\n\n' + ansi.cursorUp() +
        ansi.cursorSavePosition);

module.exports = function (progress, jobs, keep) {
  if (keep) {
    process.stderr.write(
      ansi.cursorRestorePosition +
      ansi.cursorLeft +
      ansi.eraseLines(2) +
      styles.red.open + '‣ ' + styles.red.close +
      styles.gray.open + progress + styles.gray.close +
      '\n\n' +
      ansi.cursorSavePosition +
      '\n\n'
    );
  }

  var remaining = jobs.breakdown.join(', ');
  if (remaining) {
    remaining = ' remaining: ' + remaining;
  }

  var t = jobs.todo / jobs.total * 100;
  if (t < 1) {
    t = 0;
  } else {
    t = t | 0;
  }

  var str = styles.green.open +
    (100 - t) + '%' +
    styles.green.close +
    remaining +
    styles.gray.open +
    '\nLast job: ' + progress +
    styles.gray.close;

  process.stderr.write(
    ansi.cursorRestorePosition +
    ansi.cursorLeft +
    ansi.eraseLines(2) +
    str.trim() + '\n');
};

module.exports.end = function (time) {
  var diff = process.hrtime(time);
  process.stderr.write(styles.green.open + 'Time: ' + diff[0] + 's ' +
    (diff[1] / 1e6).toFixed(3) + 'ms\n' + styles.green.close);
  process.stderr.write(ansi.cursorShow);
};

// tidy up cursor
'exit SIGINT SIGTERM'.split(' ').map(function (event) {
  process.once(event, function () {
    process.stderr.write(ansi.cursorShow); // put the cursor back
    try { process.kill(process.pid, event); } catch (e) {}
  });
});
