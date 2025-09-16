# **commit-analyzer**

[**semantic-release**](https://github.com/semantic-release/semantic-release) plugin to analyze commits with [conventional-changelog](https://github.com/conventional-changelog/conventional-changelog)

[![Build Status](https://github.com/semantic-release/commit-analyzer/workflows/Test/badge.svg)](https://github.com/semantic-release/commit-analyzer/actions?query=workflow%3ATest+branch%3Amaster) [![npm latest version](https://img.shields.io/npm/v/@semantic-release/commit-analyzer/latest.svg)](https://www.npmjs.com/package/@semantic-release/commit-analyzer)
[![npm next version](https://img.shields.io/npm/v/@semantic-release/commit-analyzer/next.svg)](https://www.npmjs.com/package/@semantic-release/commit-analyzer)
[![npm beta version](https://img.shields.io/npm/v/@semantic-release/commit-analyzer/beta.svg)](https://www.npmjs.com/package/@semantic-release/commit-analyzer)

| Step             | Description                                                                                                                                         |
| ---------------- | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| `analyzeCommits` | Determine the type of release by analyzing commits with [conventional-changelog](https://github.com/conventional-changelog/conventional-changelog). |

## Install

```bash
$ npm install @semantic-release/commit-analyzer -D
```

## Usage

The plugin can be configured in the [**semantic-release** configuration file](https://github.com/semantic-release/semantic-release/blob/master/docs/usage/configuration.md#configuration):

```json
{
  "plugins": [
    [
      "@semantic-release/commit-analyzer",
      {
        "preset": "angular",
        "releaseRules": [
          { "type": "docs", "scope": "README", "release": "patch" },
          { "type": "refactor", "release": "patch" },
          { "type": "style", "release": "patch" }
        ],
        "parserOpts": {
          "noteKeywords": ["BREAKING CHANGE", "BREAKING CHANGES"]
        }
      }
    ],
    "@semantic-release/release-notes-generator"
  ]
}
```

With this example:

- the commits that contains `BREAKING CHANGE` or `BREAKING CHANGES` in their body will be considered breaking changes.
- the commits with a 'docs' `type`, a 'README' `scope` will be associated with a `patch` release
- the commits with a 'refactor' `type` will be associated with a `patch` release
- the commits with a 'style' `type` will be associated with a `patch` release

**Note**: Your commits must be formatted **exactly** as specified by the chosen convention. For example the [Angular Commit Message Conventions](https://github.com/angular/angular.js/blob/master/DEVELOPERS.md#-git-commit-guidelines) require the `BREAKING CHANGE` keyword to be followed by a colon (`:`) and to be in the **footer** of the commit message.

## Configuration

### Options

| Option         | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   | Default                                                                                                                           |
| -------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------- |
| `preset`       | [conventional-changelog](https://github.com/conventional-changelog/conventional-changelog) preset (possible values: [`angular`](https://github.com/conventional-changelog/conventional-changelog/tree/master/packages/conventional-changelog-angular), [`atom`](https://github.com/conventional-changelog/conventional-changelog/tree/master/packages/conventional-changelog-atom), [`codemirror`](https://github.com/conventional-changelog/conventional-changelog/tree/master/packages/conventional-changelog-codemirror), [`ember`](https://github.com/conventional-changelog/conventional-changelog/tree/master/packages/conventional-changelog-ember), [`eslint`](https://github.com/conventional-changelog/conventional-changelog/tree/master/packages/conventional-changelog-eslint), [`express`](https://github.com/conventional-changelog/conventional-changelog/tree/master/packages/conventional-changelog-express), [`jquery`](https://github.com/conventional-changelog/conventional-changelog/tree/master/packages/conventional-changelog-jquery), [`jshint`](https://github.com/conventional-changelog/conventional-changelog/tree/master/packages/conventional-changelog-jshint), [`conventionalcommits`](https://github.com/conventional-changelog/conventional-changelog/tree/master/packages/conventional-changelog-conventionalcommits)). | [`angular`](https://github.com/conventional-changelog/conventional-changelog/tree/master/packages/conventional-changelog-angular) |
| `config`       | npm package name of a custom [conventional-changelog](https://github.com/conventional-changelog/conventional-changelog) preset.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               | -                                                                                                                                 |
| `parserOpts`   | Additional [conventional-commits-parser](https://github.com/conventional-changelog/conventional-changelog/tree/master/packages/conventional-commits-parser#conventionalcommitsparseroptions) options that will extends the ones loaded by `preset` or `config`. This is convenient to use a [conventional-changelog](https://github.com/conventional-changelog/conventional-changelog) preset with some customizations without having to create a new module.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 | -                                                                                                                                 |
| `releaseRules` | An external module, a path to a module or an `Array` of rules. See [`releaseRules`](#releaserules).                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | See [`releaseRules`](#releaserules)                                                                                               |
| `presetConfig` | Additional configuration passed to the [conventional-changelog](https://github.com/conventional-changelog/conventional-changelog) preset. Used for example with [conventional-changelog-conventionalcommits](https://github.com/conventional-changelog/conventional-changelog-config-spec/blob/master/versions/2.0.0/README.md).                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              | -                                                                                                                                 |

**Notes**: in order to use a `preset` it must be installed (for example to use the [eslint preset](https://github.com/conventional-changelog/conventional-changelog/tree/master/packages/conventional-changelog-eslint) you must install it with `npm install conventional-changelog-eslint -D`)

**Note**: `config` will be overwritten by the values of `preset`. You should use either `preset` or `config`, but not both.

**Note**: Individual properties of `parserOpts` will override ones loaded with an explicitly set `preset` or `config`. If `preset` or `config` are not set, only the properties set in `parserOpts` will be used.

**Note**: For presets that expects a configuration object, such as [`conventionalcommits`](https://github.com/conventional-changelog/conventional-changelog/tree/master/packages/conventional-changelog-conventionalcommits), the `presetConfig` option **must** be set.

#### releaseRules

Release rules are used when deciding if the commits since the last release warrant a new release. If you define custom release rules the [default rules](lib/default-release-rules.js) will be used if nothing matched. Those rules will be matched against the commit objects resulting of [conventional-commits-parser](https://github.com/conventional-changelog/conventional-changelog/tree/master/packages/conventional-commits-parser) parsing. Each rule property can be defined as a [glob](https://github.com/micromatch/micromatch#matching-features).

##### Rules definition

This is an `Array` of rule objects. A rule object has a `release` property and 1 or more criteria.

```json
{
  "plugins": [
    [
      "@semantic-release/commit-analyzer",
      {
        "preset": "angular",
        "releaseRules": [
          { "type": "docs", "scope": "README", "release": "patch" },
          { "type": "refactor", "scope": "core-*", "release": "minor" },
          { "type": "refactor", "release": "patch" },
          { "scope": "no-release", "release": false }
        ]
      }
    ],
    "@semantic-release/release-notes-generator"
  ]
}
```

##### Rules matching

Each commit will be compared with each rule and when it matches, the commit will be associated with the release type in the rule's `release` property. If a commit match multiple rules, the highest release type (`major` > `minor` > `patch`) is associated with the commit.

See [release types](lib/default-release-types.js) for the release types hierarchy.

With the previous example:

- Commits with `type` 'docs' and `scope` 'README' will be associated with a `patch` release.
- Commits with `type` 'refactor' and `scope` starting with 'core-' (i.e. 'core-ui', 'core-rules', ...) will be associated with a `minor` release.
- Other commits with `type` 'refactor' (without `scope` or with a `scope` not matching the glob `core-*`) will be associated with a `patch` release.
- Commits with scope `no-release` will not be associated with a release type.

##### Default rules matching

If a commit doesn't match any rule in `releaseRules` it will be evaluated against the [default release rules](lib/default-release-rules.js).

With the previous example:

- Commits with a breaking change will be associated with a `major` release.
- Commits with `type` 'feat' will be associated with a `minor` release.
- Commits with `type` 'fix' will be associated with a `patch` release.
- Commits with `type` 'perf' will be associated with a `patch` release.
- Commits with scope `no-release` will not be associated with a release type even if they have a breaking change or the `type` 'feat', 'fix' or 'perf'.

##### No rules matching

If a commit doesn't match any rules in `releaseRules` or in [default release rules](lib/default-release-rules.js) then no release type will be associated with the commit.

With the previous example:

- Commits with `type` 'style' will not be associated with a release type.
- Commits with `type` 'test' will not be associated with a release type.
- Commits with `type` 'chore' will not be associated with a release type.

##### Multiple commits

If there is multiple commits that match one or more rules, the one with the highest release type will determine the global release type.

Considering the following commits:

- `docs(README): Add more details to the API docs`
- `feat(API): Add a new method to the public API`

With the previous example the release type determined by the plugin will be `minor`.

##### Specific commit properties

The properties to set in the rules will depends on the commit style chosen. For example [conventional-changelog-angular](https://github.com/conventional-changelog/conventional-changelog/blob/master/packages/conventional-changelog-angular) use the commit properties `type`, `scope` and `subject` but [conventional-changelog-eslint](https://github.com/conventional-changelog/conventional-changelog/blob/master/packages/conventional-changelog-eslint) uses `tag` and `message`.

For example with `eslint` preset:

```json
{
  "plugins": [
    [
      "@semantic-release/commit-analyzer",
      {
        "preset": "eslint",
        "releaseRules": [
          { "tag": "Docs", "message": "*README*", "release": "patch" },
          { "tag": "New", "release": "patch" }
        ]
      }
    ],
    "@semantic-release/release-notes-generator"
  ]
}
```

With this configuration:

- Commits with `tag` 'Docs', that contains 'README' in their header message will be associated with a `patch` release.
- Commits with `tag` 'New' will be associated with a `patch` release.
- Commits with `tag` 'Breaking' will be associated with a `major` release (per [default release rules](lib/default-release-rules.js)).
- Commits with `tag` 'Fix' will be associated with a `patch` release (per [default release rules](lib/default-release-rules.js)).
- Commits with `tag` 'Update' will be associated with a `minor` release (per [default release rules](lib/default-release-rules.js)).
- All other commits will not be associated with a release type.

##### External package / file

`releaseRules` can also reference a module, either by it's `npm` name or path:

```json
{
  "plugins": [
    [
      "@semantic-release/commit-analyzer",
      {
        "preset": "angular",
        "releaseRules": "./config/release-rules.cjs"
      }
    ],
    "@semantic-release/release-notes-generator"
  ]
}
```

```js
// File: config/release-rules.cjs
module.exports = [
  { type: "docs", scope: "README", release: "patch" },
  { type: "refactor", scope: "core-*", release: "minor" },
  { type: "refactor", release: "patch" },
];
```
