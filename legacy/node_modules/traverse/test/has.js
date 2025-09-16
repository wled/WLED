'use strict';

var test = require('tape');
var v = require('es-value-fixtures');
var traverse = require('../');

test('has', function (t) {
	var obj = { a: 2, b: [4, 5, { c: 6 }] };

	t.equal(traverse(obj).has(['b', 2, 'c']), true);
	t.equal(traverse(obj).has(['b', 2, 'c', 0]), false);
	t.equal(traverse(obj).has(['b', 2, 'd']), false);
	t.equal(traverse(obj).has([]), true);
	t.equal(traverse(obj).has(['a']), true);
	t.equal(traverse(obj).has(['a', 2]), false);

	t.test('symbols', { skip: !v.hasSymbols }, function (st) {
		/* eslint no-restricted-properties: 1 */
		var globalSymbol = Symbol.for('d');
		var localSymbol = Symbol('e');

		obj[globalSymbol] = {};
		obj[globalSymbol][localSymbol] = 7;
		obj[localSymbol] = 8;

		st.equal(traverse(obj).has([globalSymbol]), true);
		st.equal(traverse(obj).has([globalSymbol, globalSymbol]), false);
		st.equal(traverse(obj).has([globalSymbol, localSymbol]), true);
		st.equal(traverse(obj).has([localSymbol]), true);
		st.equal(traverse(obj).has([localSymbol]), true);
		st.equal(traverse(obj).has([Symbol('e')]), false);
		st.equal(traverse(obj).has([Symbol.for('d')]), true);
		st.equal(traverse(obj).has([Symbol.for('e')]), false);

		st.end();
	});

	t.end();
});
