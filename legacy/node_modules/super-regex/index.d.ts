import {type Options} from 'function-timeout';

export type Match = {
	match: string;
	index: number;
	groups: string[];
	namedGroups: Record<string, string>;
	input: string;
};

export type MatchesOptions = {
	/**
	The time in milliseconds to wait before timing out when searching for each match.
	*/
	readonly matchTimeout?: number;
} & Options;

/**
Returns a boolean for whether the given `regex` matches the given `string`.

If the regex takes longer to match than the given timeout, it returns `false`.

_This method is similar to [`RegExp#test`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp/test), but differs in that the given `regex` is [never mutated, even when it has the `/g` flag](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp/test#using_test_on_a_regex_with_the_global_flag)._

@example
```
import {isMatch} from 'super-regex';

console.log(isMatch(/\d+/, getUserInput(), {timeout: 1000}));
```
*/
export function isMatch(regex: RegExp, string: string, options?: Options): boolean;

/**
Returns the first match or `undefined` if there was no match.

If the regex takes longer to match than the given timeout, it returns `undefined`.

@example
```
import {firstMatch} from 'super-regex';

console.log(firstMatch(/\d+/, getUserInput(), {timeout: 1000}));
```
*/
export function firstMatch(regex: RegExp, string: string, options?: Options): Match | undefined;

/**
Returns an iterable of matches.

If the regex takes longer to match than the given timeout, it returns an empty array.

__The `regex` must have the `/g` flag.__

@example
```
import {matches} from 'super-regex';

console.log([...matches(/\d+/, getUserInput(), {timeout: 1000})]);
```
*/
export function matches(regex: RegExp, string: string, options?: MatchesOptions): Iterable<Match>;

export {Options} from 'function-timeout';
