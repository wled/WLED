import pMap, {pMapIterable} from 'p-map';

export default async function pFilter(iterable, filterer, options) {
	const values = await pMap(
		iterable,
		(element, index) => Promise.all([filterer(element, index), element]),
		options,
	);

	return values.filter(value => Boolean(value[0])).map(value => value[1]);
}

export function pFilterIterable(iterable, filterer, options) {
	const values = pMapIterable(
		iterable,
		(element, index) => Promise.all([filterer(element, index), element]),
		options,
	);

	return {
		async * [Symbol.asyncIterator]() {
			for await (const [value, element] of values) {
				if (value) {
					yield element;
				}
			}
		},
	};
}
