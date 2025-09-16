# conventional-commits-parser

[![ESM-only package][package]][package-url]
[![NPM version][npm]][npm-url]
[![Node version][node]][node-url]
[![Dependencies status][deps]][deps-url]
[![Install size][size]][size-url]
[![Build status][build]][build-url]
[![Coverage status][coverage]][coverage-url]

[package]: https://img.shields.io/badge/package-ESM--only-ffe536.svg
[package-url]: https://nodejs.org/api/esm.html

[npm]: https://img.shields.io/npm/v/conventional-commits-parser.svg
[npm-url]: https://npmjs.com/package/conventional-commits-parser

[node]: https://img.shields.io/node/v/conventional-commits-parser.svg
[node-url]: https://nodejs.org

[deps]: https://img.shields.io/librariesio/release/npm/conventional-commits-parser
[deps-url]: https://libraries.io/npm/conventional-commits-parser/tree

[size]: https://packagephobia.com/badge?p=conventional-commits-parser
[size-url]: https://packagephobia.com/result?p=conventional-commits-parser

[build]: https://img.shields.io/github/actions/workflow/status/conventional-changelog/conventional-changelog/tests.yaml?branch=master
[build-url]: https://github.com/conventional-changelog/conventional-changelog/actions

[coverage]: https://coveralls.io/repos/github/conventional-changelog/conventional-changelog/badge.svg?branch=master
[coverage-url]: https://coveralls.io/github/conventional-changelog/conventional-changelog?branch=master

Parse raw conventional commits.

<hr />
<a href="#install">Install</a>
<span>&nbsp;&nbsp;•&nbsp;&nbsp;</span>
<a href="#usage">Usage</a>
<span>&nbsp;&nbsp;•&nbsp;&nbsp;</span>
<a href="#conventional-commit-message-format">Message format</a>
<span>&nbsp;&nbsp;•&nbsp;&nbsp;</span>
<a href="#api">API</a>
<span>&nbsp;&nbsp;•&nbsp;&nbsp;</span>
<a href="#cli">CLI</a>
<br />
<hr />

## Install

```bash
# pnpm
pnpm add conventional-commits-parser
# yarn
yarn add conventional-commits-parser
# npm
npm i conventional-commits-parser
```

## Usage

```js
import {
  CommitParser,
  parseCommits,
  parseCommitsStream
} from 'conventional-commits-parser'
import { pipeline } from 'stream/promises'
import { Readable } from 'stream'

const rawCommitMessage = 'feat(scope): broadcast $destroy event on scope destruction\nCloses #1'

// to parse raw commit message manually:
const parser = new CommitParser(options)

console.log(parser.parse(rawCommitMessage))

// to parse raw commit messages async iterables:
await pipeline(
  [rawCommitMessage],
  parseCommits(options),
  async function* (parsedCommits) {
    for await (const commit of parsedCommits) {
      console.log(commit)
    }
  }
)

// to parse raw commit messages streams:
Readable.from([rawCommitMessage])
  .pipe(parseCommitsStream(options))
  .on('data', commit => console.log(commit))
```

Parser expects raw commit message. Examples:

```js
'feat(scope): broadcast $destroy event on scope destruction\nCloses #1'
'feat(ng-list): Allow custom separator\nbla bla bla\n\nBREAKING CHANGE: some breaking change.\nThanks @stevemao\n'
```

It will return parsed commit object. Examples:

```js
{
  type: 'feat',
  scope: 'scope',
  subject: 'broadcast $destroy event on scope destruction',
  merge: null,
  header: 'feat(scope): broadcast $destroy event on scope destruction',
  body: null,
  footer: 'Closes #1',
  notes: [],
  references:
   [{
     action: 'Closes',
     owner: null,
     repository: null,
     issue: '1',
     raw: '#1',
     prefix: '#'
   }],
  mentions: [],
  revert: null
}
{
  type: 'feat',
  scope: 'ng-list',
  subject: 'Allow custom separator',
  merge: null,
  header: 'feat(ng-list): Allow custom separator',
  body: 'bla bla bla',
  footer: 'BREAKING CHANGE: some breaking change.\nThanks @stevemao',
  notes:
   [{
     title: 'BREAKING CHANGE',
     text: 'some breaking change.\nThanks @stevemao'
   }],
  references: [],
  mentions: ['stevemao'],
  revert: null
}
```

## Conventional Commit Message Format

A minimum input should contain a raw message.

Each commit message consists of a **merge header**, a **header** (mandatory), a **body** and a **footer**. **Mention** (optional) someone using the `@` notation.

```
<merge>
<header>
<body>
<footer>
```

### merge

The merge header may optionally have a special format that includes other parts, such as **branch**, **issueId** or **source**.

```
Merge branch <branch>
Merge pull request <issue-id> from <source>
```

### header

The header may optionally have a special format that includes other parts, such as **type**, **scope** and **subject**. You could **reference** (optional) issues here.

```
<type>(<scope>): <subject>
```

### footer

The footer should contain any information about **Important Notes** (optional) and is also the place to **reference** (optional) issues.

```
<important note>
<references>
```

### other parts

This module will only parse the message body. However, it is possible to include other fields such as hash, committer or date.

```
My commit message
-sideNotes-
It should warn the correct unfound file names.
Also it should continue if one file cannot be found.
Tests are added for these
```

Then `sideNotes` will be `It should warn the correct unfound file names.\nAlso it should continue if one file cannot be found.\nTests are added for these`. You can customize the `fieldPattern`.

## API

### `new CommitParser(options?: ParserOptions)`

Creates parser instance with `parse` method.

### `parseCommits(options?: ParserOptions)`

Create async generator function to parse async iterable of raw commits. If there is any malformed commits it will be gracefully ignored (an empty data will be emitted so down async iterable can notice).

### `parseCommitsStream(options?: ParserOptions)`

Creates an transform stream. If there is any malformed commits it will be gracefully ignored (an empty data will be emitted so down stream can notice).

### ParserOptions

#### mergePattern

Type: `RegExp`
Default: null

Pattern to match merge headers. EG: branch merge, GitHub or GitLab like pull requests headers. When a merge header is parsed, the next line is used for conventional header parsing.

For example, if we have a commit

```
Merge pull request #1 from user/feature/feature-name

feat(scope): broadcast $destroy event on scope destruction
```

We can parse it with these options and the default headerPattern:

```js
{
  mergePattern: /^Merge pull request #(\d+) from (.*)$/,
  mergeCorrespondence: ['id', 'source']
}
```

#### mergeCorrespondence

Type: `string[]`,
Default: null

Used to define what capturing group of `mergePattern`.

#### headerPattern

Type: `RegExp`,
Default: `/^(\w*)(?:\(([\w\$\.\-\* ]*)\))?\: (.*)$/`

Used to match header pattern.

#### headerCorrespondence

Type: `string[]`,
Default `['type', 'scope', 'subject']`

Used to define what capturing group of `headerPattern` captures what header part. The order of the array should correspond to the order of `headerPattern`'s capturing group. If the part is not captured it is `null`.

#### referenceActions

Type: `string[]`,
Default: `['close', 'closes', 'closed', 'fix', 'fixes', 'fixed', 'resolve', 'resolves', 'resolved']`

Keywords to reference an issue. This value is case **insensitive**.

Set it to `null` to reference an issue without any action.

#### issuePrefixes

Type: `string[]`,
Default: `['#']`

The prefixes of an issue. EG: In `gh-123` `gh-` is the prefix.

#### issuePrefixesCaseSensitive

Type: `boolean`,
Default: false

Used to define if `issuePrefixes` should be considered case sensitive.

#### noteKeywords

Type: `string[]`,
Default: `['BREAKING CHANGE', 'BREAKING-CHANGE']`

Keywords for important notes. This value is case **insensitive**.

#### notesPattern

Type: `function`,
Default: `noteKeywordsSelection => ^[\\s|*]*(' + noteKeywordsSelection + ')[:\\s]+(.*)` where `noteKeywordsSelection` is `join(noteKeywords, '|')`

A function that takes `noteKeywordsSelection` and returns a `RegExp` to be matched against the notes.

#### fieldPattern

Type: `RegExp`,
Default: `/^-(.*?)-$/`

Pattern to match other fields.

#### revertPattern

Type: `RegExp`,
Default: `/^Revert\s"([\s\S]*)"\s*This reverts commit (\w*)\./`

Pattern to match what this commit reverts.

#### revertCorrespondence

Type: `string[]`,
Default: `['header', 'hash']`

Used to define what capturing group of `revertPattern` captures what reverted commit fields. The order of the array should correspond to the order of `revertPattern`'s capturing group.

For example, if we had commit

```
Revert "throw an error if a callback is passed"

This reverts commit 9bb4d6c.
```

If configured correctly, the parsed result would be

```js
{
  revert: {
    header: 'throw an error if a callback is passed',
    hash: '9bb4d6c'
  }
}
```

It implies that this commit reverts a commit with header `'throw an error if a callback is passed'` and hash `'9bb4d6c'`.

#### commentChar

Type: `string` or `null`,
Default: null

What commentChar to use. By default it is `null`, so no comments are stripped.
Set to `#` if you pass the contents of `.git/COMMIT_EDITMSG` directly.

If you have configured the git commentchar via `git config core.commentchar` you'll want to pass what you have set there.

#### warn

Type: `function` or `boolean`,
Default: `() => {}`

What warn function to use. For example, `console.warn.bind(console)`. By default, it's a noop. If it is `true`, it will error if commit cannot be parsed (strict).

## CLI

You can use cli to practice writing commit messages or parse messages from files. Note: the sample output might be different. It's just for demonstration purposes.

If you run `conventional-commits-parser` without any arguments

```sh
$ conventional-commits-parser --help # for more details
```

You will enter an interactive shell. To show your parsed output enter "return" three times (or enter your specified separator).

```sh
> fix(title): a title is fixed


{"type":"fix","scope":"title","subject":"a title is fixed","header":"fix(title): a title is fixed","body":null,"footer":null,"notes":[],"references":[],"revert":null}
```

You can also use cli to parse messages from files.

If you have log.txt

```text
feat(ngMessages): provide support for dynamic message resolution

Prior to this fix it was impossible to apply a binding to a the ngMessage directive to represent the name of the error.

BREAKING CHANGE: The `ngMessagesInclude` attribute is now its own directive and that must be placed as a **child** element within the element with the ngMessages directive.

Closes #10036
Closes #9338
```

And you run

```sh
$ conventional-commits-parser log.txt
# or
$ cat log.txt | conventional-commits-parser
```

An array of json will be printed to stdout.

```sh
[
{"type":"feat","scope":"ngMessages","subject":"provide support for dynamic message resolution","header":"feat(ngMessages): provide support for dynamic message resolution","body":"Prior to this fix it was impossible to apply a binding to a the ngMessage directive to represent the name of the error.","footer":"BREAKING CHANGE: The `ngMessagesInclude` attribute is now its own directive and that must be placed as a **child** element within the element with the ngMessages directive.\nCloses #10036\nCloses #9338","notes":[{"title":"BREAKING CHANGE","text":"The `ngMessagesInclude` attribute is now its own directive and that must be placed as a **child** element within the element with the ngMessages directive."}],"references":[{"action":"Closes","owner":null,"repository":null,"issue":"10036","raw":"#10036"},{"action":"Closes","owner":null,"repository":null,"issue":"9338","raw":"#9338"}],"revert":null}
]
```

Commits should be split by at least three newlines (`\n\n\n`) or you can specify a separator as the second argument.

Eg: in log2.txt

```text

docs(ngMessageExp): split ngMessage docs up to show its alias more clearly
===

fix($animate): applyStyles from options on leave

Closes #10068
```

And you run

```sh
$ conventional-commits-parser log2.txt '==='
```

```sh
[
{"type":"docs","scope":"ngMessageExp","subject":"split ngMessage docs up to show its alias more clearly","header":"docs(ngMessageExp): split ngMessage docs up to show its alias more clearly","body":null,"footer":null,"notes":[],"references":[],"revert":null}
,
{"type":"fix","scope":"$animate","subject":"applyStyles from options on leave","header":"fix($animate): applyStyles from options on leave","body":null,"footer":"Closes #10068","notes":[],"references":[{"action":"Closes","owner":null,"repository":null,"issue":"10068","raw":"#10068"}],"revert":null}
]
```

Will be printed out.

You can specify one or more files. The output array will be in order of the input file paths. If you specify more than one separator, the last one will be used.

## License

MIT © [Steve Mao](https://github.com/stevemao)
