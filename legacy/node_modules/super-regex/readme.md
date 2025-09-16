# super-regex

> Make a regular expression time out if it takes too long to execute

This can be used to prevent [ReDoS vulnerabilities](https://en.wikipedia.org/wiki/ReDoS) when running a regular expression against untrusted user input.

This package also has a better API than the built-in regular expression methods. For example, none of the methods mutate the regex.

The timeout only works in Node.js. In the browser, it will simply not time out.

## Install

```sh
npm install super-regex
```

## Usage

```js
import {isMatch} from 'super-regex';

console.log(isMatch(/\d+/, getUserInput(), {timeout: 1000}));
```

## API

### isMatch(regex, string, options?)

Returns a boolean for whether the given `regex` matches the given `string`.

If the regex takes longer to match than the given timeout, it returns `false`.

*This method is similar to [`RegExp#test`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp/test), but differs in that the given `regex` is [never mutated, even when it has the `/g` flag](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp/test#using_test_on_a_regex_with_the_global_flag).*

### firstMatch(regex, string, options?)

Returns the first `Match` or `undefined` if there was no match.

If the regex takes longer to match than the given timeout, it returns `undefined`.

### matches(regex, string, options?)

Returns an iterable of `Match`es.

If the regex takes longer to match than the given timeout, it returns an empty array.

**The `regex` must have the `/g` flag.**

#### options

Type: `object`

##### timeout?

Type: `number` *(integer)*

The time in milliseconds to wait before timing out.

##### matchTimeout?

Type: `number` *(integer)*

Only works in `matches()`.

The time in milliseconds to wait before timing out when searching for each match.

### Match

```ts
{
	match: string;
	index: number;
	groups: string[];
	namedGroups: {string: string}; // object with string values
	input: string;
}
```

## Related

- [function-timeout](https://github.com/sindresorhus/function-timeout) - Make a synchronous function have a timeout
