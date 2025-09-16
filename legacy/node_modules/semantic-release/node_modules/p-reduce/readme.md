# p-reduce

> Reduce a list of values using promises into a promise for a value

Useful when you need to calculate some accumulated value based on async resources.

## Install

```
$ npm install p-reduce
```

## Usage

```js
import pReduce from 'p-reduce';
import humanInfo from 'human-info'; // Not a real module

const names = [
	getUser('sindresorhus').then(info => info.name),
	'Addy Osmani',
	'Pascal Hartig',
	'Stephen Sawchuk'
];

const totalAge = await pReduce(names, async (total, name) => {
	const info = await humanInfo(name);
	return total + info.age;
}, 0);

console.log(totalAge);
//=> 125
```

## API

### pReduce(input, reducer, initialValue?)

Returns a `Promise` that is fulfilled when all promises in `input` and ones returned from `reducer` are fulfilled, or rejects if any of the promises reject. The fulfilled value is the result of the reduction.

#### input

Type: `Iterable<Promise|any>`

Iterated over serially in the `reducer` function.

#### reducer(previousValue, currentValue, index)

Type: `Function`

Expected to return a value. If a `Promise` is returned, it's awaited before continuing with the next iteration.

#### initialValue

Type: `unknown`

Value to use as `previousValue` in the first `reducer` invocation.

## Related

- [p-each-series](https://github.com/sindresorhus/p-each-series) - Iterate over promises serially
- [p-map-series](https://github.com/sindresorhus/p-map-series) - Map over promises serially
- [p-map](https://github.com/sindresorhus/p-map) - Map over promises concurrently
- [More…](https://github.com/sindresorhus/promise-fun)

---

<div align="center">
	<b>
		<a href="https://tidelift.com/subscription/pkg/npm-p-reduce?utm_source=npm-p-reduce&utm_medium=referral&utm_campaign=readme">Get professional support for this package with a Tidelift subscription</a>
	</b>
	<br>
	<sub>
		Tidelift helps make open source sustainable for maintainers while giving companies<br>assurances about security, maintenance, and licensing for their dependencies.
	</sub>
</div>
