'use strict'

const { test } = require('tap')
const { parse, safeParse } = require('..')

const invalidTypes = [
  ' ',
  'null',
  'undefined',
  '/',
  'text / plain',
  'text/;plain',
  'text/"plain"',
  'text/p£ain',
  'text/(plain)',
  'text/@plain',
  'text/plain,wrong'
]

test('parse', function (t) {
  t.plan(13 + invalidTypes.length)
  t.test('should parse basic type', function (t) {
    t.plan(1)
    const type = parse('text/html')
    t.strictSame(type.type, 'text/html')
  })

  t.test('should parse with suffix', function (t) {
    t.plan(1)
    const type = parse('image/svg+xml')
    t.strictSame(type.type, 'image/svg+xml')
  })

  t.test('should parse basic type with surrounding OWS', function (t) {
    t.plan(1)
    const type = parse(' text/html ')
    t.strictSame(type.type, 'text/html')
  })

  t.test('should parse parameters', function (t) {
    t.plan(2)
    const type = parse('text/html; charset=utf-8; foo=bar')
    t.strictSame(type.type, 'text/html')
    t.same(type.parameters, {
      charset: 'utf-8',
      foo: 'bar'
    })
  })

  t.test('should parse parameters with extra LWS', function (t) {
    t.plan(2)
    const type = parse('text/html ; charset=utf-8 ; foo=bar')
    t.strictSame(type.type, 'text/html')
    t.same(type.parameters, {
      charset: 'utf-8',
      foo: 'bar'
    })
  })

  t.test('should lower-case type', function (t) {
    t.plan(1)
    const type = parse('IMAGE/SVG+XML')
    t.strictSame(type.type, 'image/svg+xml')
  })

  t.test('should lower-case parameter names', function (t) {
    t.plan(2)
    const type = parse('text/html; Charset=UTF-8')
    t.strictSame(type.type, 'text/html')
    t.same(type.parameters, {
      charset: 'UTF-8'
    })
  })

  t.test('should unquote parameter values', function (t) {
    t.plan(2)
    const type = parse('text/html; charset="UTF-8"')
    t.strictSame(type.type, 'text/html')
    t.same(type.parameters, {
      charset: 'UTF-8'
    })
  })

  t.test('should unquote parameter values with escapes', function (t) {
    t.plan(2)
    const type = parse('text/html; charset="UT\\F-\\\\\\"8\\""')
    t.strictSame(type.type, 'text/html')
    t.same(type.parameters, {
      charset: 'UTF-\\"8"'
    })
  })

  t.test('should handle balanced quotes', function (t) {
    t.plan(2)
    const type = parse('text/html; param="charset=\\"utf-8\\"; foo=bar"; bar=foo')
    t.strictSame(type.type, 'text/html')
    t.same(type.parameters, {
      param: 'charset="utf-8"; foo=bar',
      bar: 'foo'
    })
  })

  invalidTypes.forEach(function (type) {
    t.test('should throw on invalid media type ' + type, function (t) {
      t.plan(1)
      t.throws(parse.bind(null, type), 'invalid media type')
    })
  })

  t.test('should throw on invalid parameter format', function (t) {
    t.plan(3)
    t.throws(parse.bind(null, 'text/plain; foo="bar'), 'invalid parameter format')
    t.throws(parse.bind(null, 'text/plain; profile=http://localhost; foo=bar'), 'invalid parameter format')
    t.throws(parse.bind(null, 'text/plain; profile=http://localhost'), 'invalid parameter format')
  })

  t.test('should require argument', function (t) {
    t.plan(1)
    // @ts-expect-error should reject non-strings
    t.throws(parse.bind(null), 'argument header is required and must be a string')
  })

  t.test('should reject non-strings', function (t) {
    t.plan(1)
    // @ts-expect-error should reject non-strings
    t.throws(parse.bind(null, 7), 'argument header is required and must be a string')
  })
})

test('safeParse', function (t) {
  t.plan(13 + invalidTypes.length)
  t.test('should safeParse basic type', function (t) {
    t.plan(1)
    const type = safeParse('text/html')
    t.strictSame(type.type, 'text/html')
  })

  t.test('should safeParse with suffix', function (t) {
    t.plan(1)
    const type = safeParse('image/svg+xml')
    t.strictSame(type.type, 'image/svg+xml')
  })

  t.test('should safeParse basic type with surrounding OWS', function (t) {
    t.plan(1)
    const type = safeParse(' text/html ')
    t.strictSame(type.type, 'text/html')
  })

  t.test('should safeParse parameters', function (t) {
    t.plan(2)
    const type = safeParse('text/html; charset=utf-8; foo=bar')
    t.strictSame(type.type, 'text/html')
    t.same(type.parameters, {
      charset: 'utf-8',
      foo: 'bar'
    })
  })

  t.test('should safeParse parameters with extra LWS', function (t) {
    t.plan(2)
    const type = safeParse('text/html ; charset=utf-8 ; foo=bar')
    t.strictSame(type.type, 'text/html')
    t.same(type.parameters, {
      charset: 'utf-8',
      foo: 'bar'
    })
  })

  t.test('should lower-case type', function (t) {
    t.plan(1)
    const type = safeParse('IMAGE/SVG+XML')
    t.strictSame(type.type, 'image/svg+xml')
  })

  t.test('should lower-case parameter names', function (t) {
    t.plan(2)
    const type = safeParse('text/html; Charset=UTF-8')
    t.strictSame(type.type, 'text/html')
    t.same(type.parameters, {
      charset: 'UTF-8'
    })
  })

  t.test('should unquote parameter values', function (t) {
    t.plan(2)
    const type = safeParse('text/html; charset="UTF-8"')
    t.strictSame(type.type, 'text/html')
    t.same(type.parameters, {
      charset: 'UTF-8'
    })
  })

  t.test('should unquote parameter values with escapes', function (t) {
    t.plan(2)
    const type = safeParse('text/html; charset="UT\\F-\\\\\\"8\\""')
    t.strictSame(type.type, 'text/html')
    t.same(type.parameters, {
      charset: 'UTF-\\"8"'
    })
  })

  t.test('should handle balanced quotes', function (t) {
    t.plan(2)
    const type = safeParse('text/html; param="charset=\\"utf-8\\"; foo=bar"; bar=foo')
    t.strictSame(type.type, 'text/html')
    t.same(type.parameters, {
      param: 'charset="utf-8"; foo=bar',
      bar: 'foo'
    })
  })

  invalidTypes.forEach(function (type) {
    t.test('should return dummyContentType on invalid media type ' + type, function (t) {
      t.plan(2)
      t.equal(safeParse(type).type, '')
      t.equal(Object.keys(safeParse(type).parameters).length, 0)
    })
  })

  t.test('should return dummyContentType on invalid parameter format', function (t) {
    t.plan(6)
    t.equal(safeParse('text/plain; foo="bar').type, '')
    t.equal(Object.keys(safeParse('text/plain; foo="bar').parameters).length, 0)

    t.equal(safeParse('text/plain; profile=http://localhost; foo=bar').type, '')
    t.equal(Object.keys(safeParse('text/plain; profile=http://localhost; foo=bar').parameters).length, 0)

    t.equal(safeParse('text/plain; profile=http://localhost').type, '')
    t.equal(Object.keys(safeParse('text/plain; profile=http://localhost').parameters).length, 0)
  })

  t.test('should return dummyContentType on missing argument', function (t) {
    t.plan(2)
    // @ts-expect-error should reject non-strings
    t.equal(safeParse().type, '')
    // @ts-expect-error should reject non-strings
    t.equal(Object.keys(safeParse().parameters).length, 0)
  })

  t.test('should return dummyContentType on non-strings', function (t) {
    t.plan(2)
    // @ts-expect-error should reject non-strings
    t.equal(safeParse(null).type, '')
    // @ts-expect-error should reject non-strings
    t.equal(Object.keys(safeParse(null).parameters).length, 0)
  })
})
