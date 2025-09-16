var assert = require('assert')
var color = require('bash-color')
var Promise = require('promise')
var fs = require('../')

var failed = false
var ready = Promise.resolve(null)
function action(fn, message) {
  ready = ready.then(function () {
    return fn()
      .then(function () {
        console.info(color.green('V ') + message)
      }, function (ex) {
        console.error(color.red('X ') + message)
        console.error(ex.stack || ex.message || ex)
        failed = true
      })
  })
}

action(fs.mkdir.bind(null, __dirname + '/fixture'), 'mkdir')
action(function () {
  return fs.stat(__dirname + '/fixture')
          .then(function (stat) {
              assert(stat.isDirectory())
          })
}, 'stat directory')

action(fs.writeFile.bind(null, __dirname + '/fixture/file.txt', 'hello world'), 'writeFile')
action(function () {
  return fs.readFile(__dirname + '/fixture/file.txt', 'utf8')
          .then(function (txt) {
              assert(txt === 'hello world')
          })
}, 'readFile')
action(function () {
  return fs.readdir(__dirname + '/fixture')
          .then(function (files) {
            assert(Array.isArray(files))
            assert(files.length === 1)
            assert(files[0] === 'file.txt')
          })
}, 'readdir')
action(fs.unlink.bind(null, __dirname + '/fixture/file.txt'), 'unlink')

action(fs.rmdir.bind(null, __dirname + '/fixture'), 'rmdir')
action(function () {
  return fs.stat(__dirname + '/fixture')
          .then(function (stat) {
            throw new Error('directory still exists after rmdir')
          }, function () {})
}, 'stat directory')

ready.done(function () {
  if (failed) process.exit(1)
})