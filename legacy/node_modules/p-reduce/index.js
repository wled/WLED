'use strict';

const pReduce = (iterable, reducer, initialValue) => new Promise((resolve, reject) => {
	const iterator = iterable[Symbol.iterator]();
	let index = 0;

	const next = async total => {
		const element = iterator.next();

		if (element.done) {
			resolve(total);
			return;
		}

		try {
			const value = await Promise.all([total, element.value]);
			next(reducer(value[0], value[1], index++));
		} catch (error) {
			reject(error);
		}
	};

	next(initialValue);
});

module.exports = pReduce;
// TODO: Remove this for the next major release
module.exports.default = pReduce;
