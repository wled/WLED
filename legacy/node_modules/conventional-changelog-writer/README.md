# conventional-changelog-writer

[![ESM-only package][package]][package-url]
[![NPM version][npm]][npm-url]
[![Node version][node]][node-url]
[![Dependencies status][deps]][deps-url]
[![Install size][size]][size-url]
[![Build status][build]][build-url]
[![Coverage status][coverage]][coverage-url]

[package]: https://img.shields.io/badge/package-ESM--only-ffe536.svg
[package-url]: https://nodejs.org/api/esm.html

[npm]: https://img.shields.io/npm/v/conventional-changelog-writer.svg
[npm-url]: https://npmjs.com/package/conventional-changelog-writer

[node]: https://img.shields.io/node/v/conventional-changelog-writer.svg
[node-url]: https://nodejs.org

[deps]: https://img.shields.io/librariesio/release/npm/conventional-changelog-writer
[deps-url]: https://libraries.io/npm/conventional-changelog-writer/tree

[size]: https://packagephobia.com/badge?p=conventional-changelog-writer
[size-url]: https://packagephobia.com/result?p=conventional-changelog-writer

[build]: https://img.shields.io/github/actions/workflow/status/conventional-changelog/conventional-changelog/tests.yaml?branch=master
[build-url]: https://github.com/conventional-changelog/conventional-changelog/actions

[coverage]: https://coveralls.io/repos/github/conventional-changelog/conventional-changelog/badge.svg?branch=master
[coverage-url]: https://coveralls.io/github/conventional-changelog/conventional-changelog?branch=master

Write logs based on conventional commits and templates.

<hr />
<a href="#install">Install</a>
<span>&nbsp;&nbsp;•&nbsp;&nbsp;</span>
<a href="#usage">Usage</a>
<span>&nbsp;&nbsp;•&nbsp;&nbsp;</span>
<a href="#api">API</a>
<span>&nbsp;&nbsp;•&nbsp;&nbsp;</span>
<a href="#customization-guide">Customization</a>
<span>&nbsp;&nbsp;•&nbsp;&nbsp;</span>
<a href="#cli">CLI</a>
<br />
<hr />

## Install

```bash
# pnpm
pnpm add conventional-changelog-writer
# yarn
yarn add conventional-changelog-writer
# npm
npm i conventional-changelog-writer
```

## Usage

```js
import {
  writeChangelogString,
  writeChangelog,
  writeChangelogStream
} from 'conventional-changelog-writer'
import { pipeline } from 'stream/promises'

// to write changelog from commits to string:
console.log(await writeChangelogString(commits, context, options))

// to write changelog from commits to async iterable:
await pipeline(
  commits,
  writeChangelog(context, options),
  async function* (changelog) {
    for await (const chunk of changelog) {
      console.log(chunk)
    }
  }
)

// to write changelog from commits to stream:
commitsStream
  .pipe(writeChangelogStream(context, options))
  .pipe(process.stdout)
```

Commits it an async iterable of commit objects that looks like this:

```js
{ hash: '9b1aff905b638aa274a5fc8f88662df446d374bd',
  header: 'feat(scope): broadcast $destroy event on scope destruction',
  type: 'feat',
  scope: 'scope',
  subject: 'broadcast $destroy event on scope destruction',
  body: null,
  footer: 'Closes #1',
  notes: [],
  references: [ { action: 'Closes', owner: null, repository: null, issue: '1', raw: '#1' } ] }
{ hash: '13f31602f396bc269076ab4d389cfd8ca94b20ba',
  header: 'feat(ng-list): Allow custom separator',
  type: 'feat',
  scope: 'ng-list',
  subject: 'Allow custom separator',
  body: 'bla bla bla',
  footer: 'BREAKING CHANGE: some breaking change',
  notes: [ { title: 'BREAKING CHANGE', text: 'some breaking change' } ],
  references: [] }
```

Parts of the commits will be formatted and combined into a log based on the handlebars context, templates and options.

The output log might look something like this:

```js
## 0.0.1 "this is a title" (2015-05-29)


### Features

* **ng-list:** Allow custom separator ([13f3160](https://github.com/a/b/commits/13f3160))
* **scope:** broadcast $destroy event on scope destruction ([9b1aff9](https://github.com/a/b/commits/9b1aff9)), closes [#1](https://github.com/a/b/issues/1)


### BREAKING CHANGES

* some breaking change
```

## API

### writeChangelog(context?: Context, options?: Options, includeDetails?: boolean)

Creates an async generator function to generate changelog entries from commits.

If `includeDetails` is `true`, instead of emitting strings of changelog, it emits objects containing the details the block.

### writeChangelogStream(context?: Context, options?: Options, includeDetails?: boolean): Transform

Creates a transform stream which takes commits and outputs changelog entries.

If `includeDetails` is `true`, instead of emitting strings of changelog, it emits objects containing the details the block.

### writeChangelogString(commits: Iterable<Commit> | AsyncIterable<Commit>, context?: Context, options?: Options): Promise<string>

Create a changelog from commits.

#### context

Variables that will be interpolated to the template. This object contains, but not limits to the following fields.

##### version

Type: `string`

Version number of the up-coming release. If `version` is found in the last commit before generating logs, it will be overwritten.

##### title

Type: `string`

##### isPatch

Type: `boolean` Default: `semver.patch(context.version) !== 0`

By default, this value is true if `version`'s patch is `0`.

##### host

Type: `string`

The hosting website. Eg: `'https://github.com'` or `'https://bitbucket.org'`

##### owner

Type: `string`

The owner of the repository. Eg: `'stevemao'`.

##### repository

Type: `string`

The repository name on `host`. Eg: `'conventional-changelog-writer'`.

##### repoUrl

Type: `string`

The whole repository url. Eg: `'https://github.com/conventional-changelog/conventional-changelog-writer'`.
Should be used as a fallback when `context.repository` doesn't exist.

##### linkReferences

Type: `boolean` Default: `true` if (`context.repository` or `context.repoUrl`), `context.commit` and `context.issue` are truthy

Should all references be linked?

##### commit

Type: `string` Default: `'commits'`

Commit keyword in the url if `context.linkReferences === true`.

##### issue

Type: `string` Default: `'issues'`

Issue or pull request keyword in the url if `context.linkReferences === true`.

##### date

Type: `string` Default: formatted (`'yyyy-mm-dd'`) today's date in timezone set by [`timeZone`](#timeZone) option.

If `version` is found in the last commit, `committerDate` will overwrite this.

#### options

Writer options.

##### transform

Type: `function` Default: `defaultCommitTransform` exported function.

A function to transform commits. Should return diff object which will be merged with the original commit.

##### groupBy

Type: `string` Default: `'type'`

How to group the commits. EG: based on the same type. If this value is falsy, commits are not grouped.

##### commitGroupsSort

Type: `function`, `string` or `array`

A compare function used to sort commit groups. If it's a string or array, it sorts on the property(ies) by `localeCompare`. Will not sort if this is a falsy value.

##### commitsSort

Type: `function`, `string` or `array` Default: `'header'`

A compare function used to sort commits. If it's a string or array, it sorts on the property(ies) by `localeCompare`. Will not sort if this is a falsy value.

##### noteGroupsSort

Type: `function`, `string` or `array` Default: `'title'`

A compare function used to sort note groups. If it's a string or array, it sorts on the property(ies) by `localeCompare`. Will not sort if this is a falsy value.

##### notesSort

Type: `function`, `string` or `array` Default: `'text'`

A compare function used to sort note groups. If it's a string or array, it sorts on the property(ies) by `localeCompare`. Will not sort if this is a falsy value.

##### generateOn

Type: `function`, `string` or `null` Default: if `commit.version` is a valid semver.

When the upstream finishes pouring the commits it will generate a block of logs if `doFlush` is `true`. However, you can generate more than one block based on this criteria (usually a version) even if there are still commits from the upstream.

If this value is a `string`, it checks the existence of the field. Set to `null` to disable it.

###### generateOn(keyCommit: Commit, commitsGroup: Commit[], context: FinalContext, options: FinalOptions): boolean

**NOTE**: It checks on the transformed commit chunk instead of the original one (you can check on the original by access the `raw` object on the `commit`). However, if the transformed commit is ignored it falls back to the original commit.

##### finalizeContext(context: FinalContext, options: FinalOptions, filteredCommits: Commit[], keyCommit: Commit | null, commits: Commit[]): FinalContext | Promise<FinalContext>

Type: `function` Default: pass through

Last chance to modify your context before generating a changelog.

##### debug

Type: `function` Default: `() => {}`

A function to get debug information.

##### reverse

Type: `boolean` Default: `false`

The normal order means reverse chronological order. `reverse` order means chronological order. Are the commits from upstream in the reverse order? You should only worry about this when generating more than one blocks of logs based on `generateOn`. If you find the last commit is in the wrong block inverse this value.

##### ignoreReverted

Type: `boolean` Default: `true`

If `true`, reverted commits will be ignored.

##### doFlush

Type: `boolean` Default: `true`

If `true`, the stream will flush out the last bit of commits (could be empty) to changelog.

##### mainTemplate

Type: `string` Default: [template.hbs](templates/template.hbs)

The main handlebars template.

##### headerPartial

Type: `string` Default: [header.hbs](templates/header.hbs)

##### commitPartial

Type: `string` Default: [commit.hbs](templates/commit.hbs)

##### footerPartial

Type: `string` Default: [footer.hbs](templates/footer.hbs)

##### partials

Type: `object`

Partials that used in the main template, if any. The key should be the partial name and the value should be handlebars template strings. If you are using handlebars template files, read files by yourself.

##### timeZone

Type: `string` Default: `'UTC'`

The timezone to use. The date in the changelog is generated based on timezone.

## Customization Guide

It is possible to customize this the changelog to suit your needs. Templates are written in [handlebars](http://handlebarsjs.com). You can customize all partials or the whole template. Template variables are from either `upstream` or `context`. The following are a suggested way of defining variables.

### upstream

Variables in upstream are commit specific and should be used per commit. Eg: *commit date* and *commit username*. You can think of them as "local" or "isolate" variables. A "raw" commit message (original commit poured from upstream) is attached to `commit`. `transform` can be used to modify a commit.

### context

context should be module specific and can be used across the whole log. Thus these variables should not be related to any single commit and should be generic information of the module or all commits. Eg: *repository url* and *author names*, etc. You can think of them as "global" or "root" variables.

Basically you can make your own templates and define all your template context. Extra context are based on commits from upstream and `options`. For more details, please checkout [handlebars](http://handlebarsjs.com) and the source code of this module. `finalizeContext` can be used at last to modify context before generating a changelog.

## CLI

```sh
$ conventional-changelog-writer --help # for more details
```

It works with [Line Delimited JSON](http://en.wikipedia.org/wiki/Line_Delimited_JSON).

If you have commits.ldjson

```js
{"hash":"9b1aff905b638aa274a5fc8f88662df446d374bd","header":"feat(ngMessages): provide support for dynamic message resolution","type":"feat","scope":"ngMessages","subject":"provide support for dynamic message resolution","body":"Prior to this fix it was impossible to apply a binding to a the ngMessage directive to represent the name of the error.","footer":"BREAKING CHANGE: The `ngMessagesInclude` attribute is now its own directive and that must be placed as a **child** element within the element with the ngMessages directive.\nCloses #10036\nCloses #9338","notes":[{"title":"BREAKING CHANGE","text":"The `ngMessagesInclude` attribute is now its own directive and that must be placed as a **child** element within the element with the ngMessages directive."}],"references":[{"action":"Closes","owner":null,"repository":null,"issue":"10036","raw":"#10036"},{"action":"Closes","owner":null,"repository":null,"issue":"9338","raw":"#9338"}]}
```

And you run

```sh
$ conventional-changelog-writer commits.ldjson -o options.js
```

The output might look something like this

```md
# 1.0.0 (2015-04-09)


### Features

* **ngMessages:** provide support for dynamic message resolution 9b1aff9, closes #10036 #9338


### BREAKING CHANGES

* The `ngMessagesInclude` attribute is now its own directive and that must be placed as a **child** element within the element with the ngMessages directive.
```

It is printed to stdout.

## License

MIT © [Steve Mao](https://github.com/stevemao)
