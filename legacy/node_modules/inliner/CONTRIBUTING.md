# Contributing

When filing bugs, if it's an inlining issue, ensure you include full test source to work against.

This can be via a [PR](https://github.com/remy/inliner/pulls) or by linking to a [gist](https://gist.github.com) that include at least **two** (first) files:

- `<issue>.src.html`
- `<issue>.result.html`

When these are put in the [fixtures](https://github.com/remy/inliner/tree/master/test/fixtures) directory, they are automatically tested against.

If there are any external assets the example needs, please also include these and name them with the same root as your example, i.e. `<issue>.css` or `<issue>.min.js` etc.

In addition `<issue>.opts.json` can be loaded to help specify runtime options during the test.

**To test a single fixture you can use: `FILTER=<issue> npm test`**

## Commit messages

Commit messages must follow the [Angular-style](https://github.com/angular/angular.js/blob/master/CONTRIBUTING.md#commit-message-format) commit format (but excluding the scope).

i.e:

```text
fix: minified scripts being removed

Also includes tests
```

This will allow for the automatic changelog to generate correctly.

## Code standards

Ensure that your code adheres to the included `.jshintrc` and `.jscsrc` configs.
