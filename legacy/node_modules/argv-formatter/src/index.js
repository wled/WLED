'use strict';

function argify (key, value) {
  var single = key.length === 1;
  return {
    single: single,
    flag: single ? '-' + key : '--' + key,
    value: value
  };
}

function options (object) {
  return Object.keys(object)
    .filter(function (key) {
      return key !== '_';
    })
    .map(function (key) {
      return argify(key, object[key]);
    })
    .filter(function (arg) {
      return arg.value;
    })
    .reduce(function (args, arg) {
      if (arg.single) {
        args.push(arg.flag);
        if (arg.value !== true) {
          args.push(arg.value.toString());
        }
      }
      else {
        if (arg.value !== true) {
          args.push(arg.flag + '=' + arg.value);
        }
        else {
          args.push(arg.flag);
        }
      }
      return args;
    }, []);
}

function args (object) {
  if (object._) {
    return (Array.isArray(object._) ? object._ : [object._])
      .map(function (value) {
        return value.toString();
      });
  }
  return [];
}

exports.format = function formatArgv (object) {
  return options(object).concat(args(object));
};
