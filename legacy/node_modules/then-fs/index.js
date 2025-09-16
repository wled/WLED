'use strict';
var fs = Object.create(require('fs'))
var Promise = require('promise')

module.exports = exports = fs
for (var key in fs)
  if (!(typeof fs[key] != 'function'
      || key.match(/Sync$/)
      || key.match(/^[A-Z]/)
      || key.match(/^create/) 
      || key.match(/^(un)?watch/)
      ))
  add(key)

function add(key) {
  var original = fs[key]
  if (key !== 'exists')
    fs[key] = Promise.denodeify(original)
  else
    fs[key] = function() {
      var args = [].slice.call(arguments)
      return new Promise(function(resolve, reject) {
        args.push(resolve)
        original.apply(null, args)
      })
    }
}
