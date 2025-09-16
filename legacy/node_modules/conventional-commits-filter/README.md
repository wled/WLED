# conventional-commits-filter 

[![ESM-only package][package]][package-url]
[![NPM version][npm]][npm-url]
[![Node version][node]][node-url]
[![Dependencies status][deps]][deps-url]
[![Install size][size]][size-url]
[![Build status][build]][build-url]
[![Coverage status][coverage]][coverage-url]

[package]: https://img.shields.io/badge/package-ESM--only-ffe536.svg
[package-url]: https://nodejs.org/api/esm.html

[npm]: https://img.shields.io/npm/v/conventional-commits-filter.svg
[npm-url]: https://npmjs.com/package/conventional-commits-filter

[node]: https://img.shields.io/node/v/conventional-commits-filter.svg
[node-url]: https://nodejs.org

[deps]: https://img.shields.io/librariesio/release/npm/conventional-commits-filter
[deps-url]: https://libraries.io/npm/conventional-commits-filter/tree

[size]: https://packagephobia.com/badge?p=conventional-commits-filter
[size-url]: https://packagephobia.com/result?p=conventional-commits-filter

[build]: https://img.shields.io/github/actions/workflow/status/conventional-changelog/conventional-changelog/tests.yaml?branch=master
[build-url]: https://github.com/conventional-changelog/conventional-changelog/actions

[coverage]: https://coveralls.io/repos/github/conventional-changelog/conventional-changelog/badge.svg?branch=master
[coverage-url]: https://coveralls.io/github/conventional-changelog/conventional-changelog?branch=master

Filter out reverted commits parsed by conventional-commits-parser.

## Install

```bash
# pnpm
pnpm add conventional-commits-filter
# yarn
yarn add conventional-commits-filter
# npm
npm i conventional-commits-filter
```

## Usage

```js
import {
  filterRevertedCommitsSync,
  filterRevertedCommits,
  filterRevertedCommitsStream
} from 'conventional-commits-filter'
import { pipeline } from 'stream/promises'
import { Readable } from 'stream'

const commits = [{
  type: 'revert',
  scope: null,
  subject: 'feat(): amazing new module',
  header: 'revert: feat(): amazing new module\n',
  body: 'This reverts commit 56185b7356766d2b30cfa2406b257080272e0b7a.\n',
  footer: null,
  notes: [],
  references: [],
  revert: {
    header: 'feat(): amazing new module',
    hash: '56185b7356766d2b30cfa2406b257080272e0b7a',
    body: null
  },
  hash: '789d898b5f8422d7f65cc25135af2c1a95a125ac\n',
  raw: {
    type: 'revert',
    scope: null,
    subject: 'feat(): amazing new module',
    header: 'revert: feat(): amazing new module\n',
    body: 'This reverts commit 56185b7356766d2b30cfa2406b257080272e0b7a.\n',
    footer: null,
    notes: [],
    references: [],
    revert: {
      header: 'feat(): amazing new module',
      hash: '56185b7356766d2b30cfa2406b257080272e0b7a',
      body: null
    },
    hash: '789d898b5f8422d7f65cc25135af2c1a95a125ac\n'
  }
}, {
  type: 'Features',
  scope: null,
  subject: 'wow',
  header: 'amazing new module\n',
  body: null,
  footer: 'BREAKING CHANGE: Not backward compatible.\n',
  notes: [],
  references: [],
  revert: null,
  hash: '56185b',
  raw: {
    type: 'feat',
    scope: null,
    subject: 'amazing new module',
    header: 'feat(): amazing new module\n',
    body: null,
    footer: 'BREAKING CHANGE: Not backward compatible.\n',
    notes: [],
    references: [],
    revert: null,
    hash: '56185b7356766d2b30cfa2406b257080272e0b7a\n'
  }
}, {
  type: 'What',
  scope: null,
  subject: 'new feature',
  header: 'feat(): new feature\n',
  body: null,
  footer: null,
  notes: [],
  references: [],
  revert: null,
  hash: '815a3f0',
  raw: {
    type: 'feat',
    scope: null,
    subject: 'new feature',
    header: 'feat(): new feature\n',
    body: null,
    footer: null,
    notes: [],
    references: [],
    revert: null,
    hash: '815a3f0717bf1dfce007bd076420c609504edcf3\n'
  }
}, {
  type: 'Chores',
  scope: null,
  subject: 'first commit',
  header: 'chore: first commit\n',
  body: null,
  footer: null,
  notes: [],
  references: [],
  revert: null,
  hash: '74a3e4d6d25',
  raw: {
    type: 'chore',
    scope: null,
    subject: 'first commit',
    header: 'chore: first commit\n',
    body: null,
    footer: null,
    notes: [],
    references: [],
    revert: null,
    hash: '74a3e4d6d25dee2c0d6483a0a3887417728cbe0a\n'
  }
}];

// to filter reverted commits syncronously:
for (const commit of filterRevertedCommitsSync(commits)) {
  console.log(commit)
}

// to filter reverted commits in async iterables:
await pipeline(
  commits,
  filterRevertedCommits,
  async function* (filteredCommits) {
    for await (const commit of filteredCommits) {
      console.log(commit)
    }
  }
)

// to filter reverted commits in streams:
Readable.from(commits)
  .pipe(filterRevertedCommitsStream())
  .on('data', commit => console.log(commit))
```

Output:

```js
{
  type: 'What',
  scope: null,
  subject: 'new feature',
  header: 'feat(): new feature\n',
  body: null,
  footer: null,
  notes: [],
  references: [],
  revert: null,
  hash: '815a3f0',
  raw: {
    type: 'feat',
    scope: null,
    subject: 'new feature',
    header: 'feat(): new feature\n',
    body: null,
    footer: null,
    notes: [],
    references: [],
    revert: null,
    hash: '815a3f0717bf1dfce007bd076420c609504edcf3\n'
  }
}
{
  type: 'Chores',
  scope: null,
  subject: 'first commit',
  header: 'chore: first commit\n',
  body: null,
  footer: null,
  notes: [],
  references: [],
  revert: null,
  hash: '74a3e4d6d25',
  raw: {
    type: 'chore',
    scope: null,
    subject: 'first commit',
    header: 'chore: first commit\n',
    body: null,
    footer: null,
    notes: [],
    references: [],
    revert: null,
    hash: '74a3e4d6d25dee2c0d6483a0a3887417728cbe0a\n'
  }
}
```

## License

MIT © [Steve Mao](https://github.com/stevemao)
