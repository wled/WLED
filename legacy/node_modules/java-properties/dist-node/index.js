'use strict';

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.of = exports.PropertiesFile = void 0;

var _fs = _interopRequireDefault(require("fs"));

function _interopRequireDefault(obj) {
  return obj && obj.__esModule ? obj : {
    default: obj
  };
}
/*
 * properties
 *
 * Copyright (c) 2013 Matt Steele
 * Licensed under the MIT license.
 */


class PropertiesFile {
  constructor(...args) {
    this.objs = {};

    if (args.length) {
      this.of.apply(this, args);
    }
  }

  makeKeys(line) {
    if (line && line.indexOf('#') !== 0) {
      //let splitIndex = line.indexOf('=');
      let separatorPositions = ['=', ':'].map(sep => {
        return line.indexOf(sep);
      }).filter(index => {
        return index > -1;
      });
      let splitIndex = Math.min(...separatorPositions);
      let key = line.substring(0, splitIndex).trim();
      let value = line.substring(splitIndex + 1).trim(); // if keys already exists ...

      if (this.objs.hasOwnProperty(key)) {
        // if it is already an Array
        if (Array.isArray(this.objs[key])) {
          // just push the new value
          this.objs[key].push(value);
        } else {
          // transform the value into Array
          let oldValue = this.objs[key];
          this.objs[key] = [oldValue, value];
        }
      } else {
        // the key does not exists
        const escapedValue = value.replace(/"/g, '\\"') // escape "
        .replace(/\\:/g, ':') // remove \ before :
        .replace(/\\=/g, '='); // remove \ before =

        this.objs[key] = unescape(JSON.parse('"' + escapedValue + '"'));
      }
    }
  }

  addFile(file) {
    let data = _fs.default.readFileSync(file, 'utf-8');

    let items = data.split(/\r?\n/);
    let me = this;

    for (let i = 0; i < items.length; i++) {
      let line = items[i];

      while (line.substring(line.length - 1) === '\\') {
        line = line.slice(0, -1);
        let nextLine = items[i + 1];
        line = line + nextLine.trim();
        i++;
      }

      me.makeKeys(line);
    }
  }

  of(...args) {
    for (let i = 0; i < args.length; i++) {
      this.addFile(args[i]);
    }
  }

  get(key, defaultValue) {
    if (this.objs.hasOwnProperty(key)) {
      if (Array.isArray(this.objs[key])) {
        let ret = [];

        for (let i = 0; i < this.objs[key].length; i++) {
          ret[i] = this.interpolate(this.objs[key][i]);
        }

        return ret;
      } else {
        return typeof this.objs[key] === 'undefined' ? '' : this.interpolate(this.objs[key]);
      }
    }

    return defaultValue;
  }

  getLast(key, defaultValue) {
    if (this.objs.hasOwnProperty(key)) {
      if (Array.isArray(this.objs[key])) {
        var lg = this.objs[key].length;
        return this.interpolate(this.objs[key][lg - 1]);
      } else {
        return typeof this.objs[key] === 'undefined' ? '' : this.interpolate(this.objs[key]);
      }
    }

    return defaultValue;
  }

  getFirst(key, defaultValue) {
    if (this.objs.hasOwnProperty(key)) {
      if (Array.isArray(this.objs[key])) {
        return this.interpolate(this.objs[key][0]);
      } else {
        return typeof this.objs[key] === 'undefined' ? '' : this.interpolate(this.objs[key]);
      }
    }

    return defaultValue;
  }

  getInt(key, defaultIntValue) {
    let val = this.getLast(key);

    if (!val) {
      return defaultIntValue;
    } else {
      return parseInt(val, 10);
    }
  }

  getFloat(key, defaultFloatValue) {
    let val = this.getLast(key);

    if (!val) {
      return defaultFloatValue;
    } else {
      return parseFloat(val);
    }
  }

  getBoolean(key, defaultBooleanValue) {
    function parseBool(b) {
      return !/^(false|0)$/i.test(b) && !!b;
    }

    let val = this.getLast(key);

    if (!val) {
      return defaultBooleanValue || false;
    } else {
      return parseBool(val);
    }
  }

  set(key, value) {
    this.objs[key] = value;
  }

  interpolate(s) {
    let me = this;
    return s.replace(/\\\\/g, '\\').replace(/\$\{([A-Za-z0-9\.\-\_]*)\}/g, function (match) {
      return me.getLast(match.substring(2, match.length - 1));
    });
  }

  getKeys() {
    let keys = [];

    for (let key in this.objs) {
      keys.push(key);
    }

    return keys;
  }

  getMatchingKeys(matchstr) {
    let keys = [];

    for (let key in this.objs) {
      if (key.search(matchstr) !== -1) {
        keys.push(key);
      }
    }

    return keys;
  }

  reset() {
    this.objs = {};
  }

} // Retain 'of' from v1 for backward compatibility


exports.PropertiesFile = PropertiesFile;

let of = function of(...args) {
  let globalFile = new PropertiesFile();
  globalFile.of.apply(globalFile, args);
  return globalFile;
};

exports.of = of;
