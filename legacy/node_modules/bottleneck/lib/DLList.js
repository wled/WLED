"use strict";

var DLList;
DLList = class DLList {
  constructor(incr, decr) {
    this.incr = incr;
    this.decr = decr;
    this._first = null;
    this._last = null;
    this.length = 0;
  }

  push(value) {
    var node;
    this.length++;

    if (typeof this.incr === "function") {
      this.incr();
    }

    node = {
      value,
      prev: this._last,
      next: null
    };

    if (this._last != null) {
      this._last.next = node;
      this._last = node;
    } else {
      this._first = this._last = node;
    }

    return void 0;
  }

  shift() {
    var value;

    if (this._first == null) {
      return;
    } else {
      this.length--;

      if (typeof this.decr === "function") {
        this.decr();
      }
    }

    value = this._first.value;

    if ((this._first = this._first.next) != null) {
      this._first.prev = null;
    } else {
      this._last = null;
    }

    return value;
  }

  first() {
    if (this._first != null) {
      return this._first.value;
    }
  }

  getArray() {
    var node, ref, results;
    node = this._first;
    results = [];

    while (node != null) {
      results.push((ref = node, node = node.next, ref.value));
    }

    return results;
  }

  forEachShift(cb) {
    var node;
    node = this.shift();

    while (node != null) {
      cb(node), node = this.shift();
    }

    return void 0;
  }

  debug() {
    var node, ref, ref1, ref2, results;
    node = this._first;
    results = [];

    while (node != null) {
      results.push((ref = node, node = node.next, {
        value: ref.value,
        prev: (ref1 = ref.prev) != null ? ref1.value : void 0,
        next: (ref2 = ref.next) != null ? ref2.value : void 0
      }));
    }

    return results;
  }

};
module.exports = DLList;