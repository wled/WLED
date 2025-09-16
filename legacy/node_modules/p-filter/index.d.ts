import type {Options} from 'p-map';

/**
Filter promises concurrently.

@param input - Iterated over concurrently in the `filterer` function.
@param filterer - The filterer function that decides whether an element should be included into result.

@example
```
import pFilter from 'p-filter';
import getWeather from 'get-weather'; // Not a real module

const places = [
	getCapital('Norway').then(info => info.name),
	'Bangkok, Thailand',
	'Berlin, Germany',
	'Tokyo, Japan',
];

const filterer = async place => {
	const weather = await getWeather(place);
	return weather.temperature > 30;
};

const result = await pFilter(places, filterer);

console.log(result);
//=> ['Bangkok, Thailand']
```
*/
export default function pFilter<ValueType>(
	input: Iterable<ValueType | PromiseLike<ValueType>>,
	filterer: (
		element: ValueType,
		index: number
	) => boolean | PromiseLike<boolean>,
	options?: Options
): Promise<ValueType[]>;

/**
Filter promises concurrently.

@param input - Iterated over concurrently in the `filterer` function.
@param filterer - The filterer function that decides whether an element should be included into result.
@param options - See the [`p-map` options](https://github.com/sindresorhus/p-map#options).
@returns An async iterable that iterates over the promises in `iterable` and ones returned from `filterer` concurrently, calling `filterer` for each element.

@example
```
import {pFilterIterable} from 'p-filter';
import getWeather from 'get-weather'; // Not a real module

async function * getPlaces() {
	const name = await getCapital('Norway');

	yield name;
	yield 'Bangkok, Thailand';
	yield 'Berlin, Germany';
	yield 'Tokyo, Japan';
}

const places = getPlaces();

const filterer = async place => {
	const weather = await getWeather(place);
	return weather.temperature > 30;
};

for await (const element of pFilterIterable(places, filterer)) {
	console.log(element);
}
//=> ['Bangkok, Thailand']
```
*/
export function pFilterIterable<ValueType>(
	input:
	| AsyncIterable<ValueType | PromiseLike<ValueType>>
	| Iterable<ValueType | PromiseLike<ValueType>>,
	filterer: (
		element: ValueType,
		index: number
	) => boolean | PromiseLike<boolean>,
	options?: Options
): AsyncIterable<ValueType>;

export {Options} from 'p-map';
