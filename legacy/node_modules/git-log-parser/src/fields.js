'use strict';

var traverse = require('traverse');

exports.config = {
  commit: {
    long: 'H',
    short: 'h'
  },
  tree: {
    long: 'T',
    short: 't'
  },
  author: {
    name: 'an',
    email: 'ae',
    date: {
      key: 'ai',
      type: Date
    }
  },
  committer: {
    name: 'cn',
    email: 'ce',
    date: {
      key: 'ci',
      type: Date
    }
  },
  subject: 's',
  body: 'b'
};

exports.map = function () {
  return traverse.reduce(exports.config, function (fields, node) {
    if (this.isLeaf && typeof node === 'string') {
      var typed = this.key === 'key';
      fields.push({
        path: typed ? this.parent.path : this.path,
        key: node,
        type: this.parent.node.type
      });
    }
    return fields;
  }, []);
};
