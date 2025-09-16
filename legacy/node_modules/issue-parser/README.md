# issue-parser

Parser for [Github](https://github.com), [GitLab](https://gitlab.com) and [Bitbucket](https://bitbucket.org) issues actions, references and mentions

<!--status-badges start -->

[![Node CI Workflow Status][github-actions-ci-badge]][github-actions-ci-link]

<!--status-badges end -->

The parser can identify:
- GitHub [closing keywords](https://help.github.com/articles/closing-issues-using-keywords), [duplicate keyword](https://help.github.com/articles/about-duplicate-issues-and-pull-requests), [issue references](https://guides.github.com/features/issues/#notifications) and [user mentions](https://guides.github.com/features/issues/#notifications)
- GitLab [closing keywords](https://docs.gitlab.com/ee/user/project/issues/automatic_issue_closing.html), [duplicate keyword](https://gitlab.com/gitlab-org/gitlab-ce/merge_requests/12845), [issue references](https://about.gitlab.com/2016/03/08/gitlab-tutorial-its-all-connected) and [user mentions](https://about.gitlab.com/2016/03/08/gitlab-tutorial-its-all-connected)
- Bitbucket [closing keywords](https://confluence.atlassian.com/bitbucket/resolve-issues-automatically-when-users-push-code-221451126.html), [issue references](https://confluence.atlassian.com/bitbucket/mark-up-comments-issues-and-commit-messages-321859781.html) and [user mentions](https://confluence.atlassian.com/bitbucket/mark-up-comments-issues-and-commit-messages-321859781.html)
- [Custom](#custom-format) or [additional](#extend-existing-format) keywords

## Install

```bash
$ npm install --save issue-parser
```

## Usage

### GitHub format

```js
const issueParser = require('issue-parser');
const parse = issueParser('github');

parse('Issue description, ref user/package#1, Fix #2, Duplicate of #3 /cc @user');
/*
{
  refs: [{raw: 'user/package#1', slug: 'user/package', prefix: '#', issue: '1'}],
  actions: {
    close: [{raw: 'Fix #2', action: 'Fix', prefix: '#', issue: '2'}],
    duplicate: [{raw: 'Duplicate of #3', action: 'Duplicate of', prefix: '#', issue: '3'}],
  },
  mentions: [{raw: '@user', prefix: '@', user: 'user'}],
}
*/
```

### GitLab format

```js
const issueParser = require('issue-parser');
const parse = issueParser('gitlab');

parse('Issue description, ref group/user/package#1, !2, implement #3, /duplicate #4 /cc @user');
/*
{
  refs: [
    {raw: 'group/user/package#1', slug: 'group/user/package', prefix: '#', issue: '1'},
    {raw: '!2', slug: 'group/user/package', prefix: '!', issue: '2'},
  ],
  actions: {
    close: [{raw: 'implement #3', action: 'Implement', prefix: '#', issue: '4'}],
    duplicate: [{raw: 'Duplicate of #4', action: 'Duplicate of', prefix: '#', issue: '4'}],
  },
  mentions: [{raw: '@user', prefix: '@', user: 'user'}],
}
*/
```

### Bitbucket format

```js
const issueParser = require('issue-parser');
const parse = issueParser('bitbucket');

parse('Issue description, ref user/package#1, fixing #2. /cc @user');
/*
{
  refs: [{raw: 'user/package#1', slug: 'user/package', prefix: '#', issue: '1'}],
  actions: {
    close: [{raw: 'fixing #2', action: 'Fixing', prefix: '#', issue: '2'}],
  },
  mentions: [{raw: '@user', prefix: '@', user: 'user'}],
}
*/
```

### Custom format

```js
const issueParser = require('issue-parser');
const parse = issueParser({actions: {fix: ['complete'], hold: ['holds up']}, issuePrefixes: ['🐛']});

parse('Issue description, related to user/package🐛1, Complete 🐛2, holds up 🐛3');
/*
{
  refs: [{raw: 'user/package🐛1', slug: 'user/package', prefix: '🐛', issue: '1'}],
  actions: {
    fix: [{raw: 'Complete 🐛2', action: 'Complete', prefix: '🐛', issue: '2'}],
    hold: [{raw: 'holds up 🐛3', action: 'Holds up', prefix: '🐛', issue: '3'}],
  },
}
*/
```

### Extend existing format

```js
const issueParser = require('issue-parser');
const parse = issueParser('github', {actions: {parent: ['parent of'], related: ['related to']}});

parse('Issue description, ref user/package#1, Fix #2, Parent of #3, related to #4 /cc @user');
/*
{
  refs: [{raw: 'user/package#1', slug: 'user/package', prefix: '#', issue: '1'}],
  actions: {
    close: [{raw: 'Fix #2', action: 'Fix', prefix: '#', issue: '2'}],
    parent: [{raw: 'Parent of #3', action: 'Parent of', prefix: '#', issue: '3'}],
    related: [{raw: 'related to #4', action: 'Related to', prefix: '#', issue: '4'}],
  },
  mentions: [{raw: '@user', prefix: '@', user: 'user'}],
}
*/
```

## Features

### Parse references

```text
#1
```
```js
{refs: [{raw: '#1', slug: undefined, prefix: '#', issue: '1'}]}
```

### Parse repository slug

```text
owner/repo#1
```
```js
{refs: [{raw: 'owner/repo#1', slug: 'owner/repo', prefix: '#', issue: '1'}]}
```

### Parse closing keywords

```text
Fix #1
```
```js
{actions: {close: [{raw: 'Fix #1', action: 'Fix', slug: undefined, prefix: '#', issue: '1'}]}}
```

### Parse duplicate keywords

```text
Duplicate of #1
```
```js
{actions: {duplicate: [{raw: 'Duplicate of #1', action: 'Duplicate of', slug: undefined, prefix: '#', issue: '1'}]}}
```

### Parse user mentions

```text
@user
```
```js
{mentions: [{raw: '@user', prefix: '@', user: 'user'}]}
```

### Parse references with full issue URL

```text
https://github.com/owner/repo/pull/1

Fix https://github.com/owner/repo/issues/2
```
```js
{
  refs: [{raw: 'https://github.com/owner/repo/pull/1', slug: 'owner/repo', prefix: undefined, issue: '1'},]
  actions: {
    close: [
      {raw: 'Fix https://github.com/owner/repo/issues/2', action: 'Fix', slug: 'owner/repo', prefix: undefined, issue: '2'}
    ]
  }
}
```

### Ignore keywords case

```text
FIX #1
```
```js
{actions: {close: [{raw: 'FIX #1', action: 'Fix', slug: undefined, prefix: '#', issue: '1'}]}}
```

### Support delimiters between action keyword and issue

```text
Fix: #1
```
```js
{actions: {close: [{raw: 'Fix: #1', action: 'Fix', slug: undefined, prefix: '#', issue: '1'}]}}
```

### Ignore references in back-tick quotes

```text
Fix #1 `Fix #2` @user1 `@user2`
```
```js
{
  actions: {close: [{raw: 'Fix #1', action: 'Fix', slug: undefined, prefix: '#', issue: '1'}]},
  mentions: [{raw: '@user1', prefix: '@', user: 'user1'}]
}
```

### Include references in escaped back-tick quotes

```text
\`Fix #1\` \`@user\`
```
```js
{
  actions: {close: [{raw: 'Fix #1', action: 'Fix', slug: undefined, prefix: '#', issue: '1'}]},
  mentions: [{raw: '@user1', prefix: '@', user: 'user1'}]
}
```

### Ignore references in fenced blocks

````text
Fix #1

```js
console.log('Fix #2');
```

@user1

```js
console.log('@user2');
```
````
```js
{
  actions: {close: [{raw: 'Fix #1', action: 'Fix', slug: undefined, prefix: '#', issue: '1'}]},
  mentions: [{raw: '@user1', prefix: '@', user: 'user1'}]
}
```

### Include references in escaped fenced blocks

```text
\`\`\`
Fix #1
\`\`\`

\`\`\`
@user
\`\`\`
```
```js
{
  actions: {close: [{raw: 'Fix #1', action: 'Fix', slug: undefined, prefix: '#', issue: '1'}]},
  mentions: [{raw: '@user', prefix: '@', user: 'user'}]
}
```

### Ignore references in &lt;code&gt; tags

```text
Fix #1
<code>Fix #2</code>
<code><code>Fix #3</code></code>
@user1
<code>@user2</code>
```
```js
{
  actions: {close: [{raw: 'Fix #1', action: 'Fix', slug: undefined, prefix: '#', issue: '1'}]},
  mentions: [{raw: '@user1', prefix: '@', user: 'user1'}]
}
```

### Include references in escaped &lt;code&gt; tags

```text
`<code>`Fix #1`</code>`
`<code>`@user`</code>`
```
```js
{
  actions: {close: [{raw: 'Fix #1', action: 'Fix', slug: undefined, prefix: '#', issue: '1'}]},
  mentions: [{raw: '@user', prefix: '@', user: 'user'}]
}
```

### Ignore malformed references

```text
Fix #1 Fix #2a Fix a#3
```
```js
{actions: {close: [{raw: 'Fix #1', action: 'Fix', slug: undefined, prefix: '#', issue: '1'}]}}
```

## API

### issueParser([options], [overrides]) => parse

Create a [parser](#parsetext--result).

#### options

Type: `Object` `String`<br>
Parser options. Can be `github`, `gitlab` or `bitbucket` for predefined options, or an object for custom options.

##### actions

Type: `Object`<br>
Default:
`{close: ['close', 'closes', 'closed', 'closing', 'fix', 'fixes', 'fixed', 'fixing', 'resolve', 'resolves', 'resolved', 'resolving', 'implement', 'implements', 'implemented', 'implementing'],
  duplicate: ['Duplicate of', '/duplicate']}`

Object with type of action as key and array of keywords as value.

Each keyword match will be placed in the corresponding property of the [`result`](#result) `action` object. For example the with the configuration `{actions: fix: ['fixed', 'fixing']}` each action matching `fixed` or  `fixing` will be under `result.actions.fix`.

##### delimiters

Type: `Array<String>` `String`<br>
Default: `[':']`

List of delimiter characters allowed between an action keywords and the issue reference. The characters space (` `) and tab (`  `) are always allowed.

##### mentionsPrefixes

Type: `Array<String>` `String`<br>
Default: `['@']`

List of keywords used to identify user mentions.

##### issuePrefixes

Type: `Array<String>` `String`<br>
Default: `['#', 'gh-']`

List of keywords used to identify issues and pull requests.

##### hosts

Type: `Array<String>` `String`<br>
Default: `['https://github.com', 'https://gitlab.com']`

List of base URL used to identify issues and pull requests with [full URL](#parse-references-with-full-issue-url).

##### issueURLSegments

Type: `Array<String>` `String`<br>
Default: `['issues', 'pull', 'merge_requests']`

List of URL segment used to identify issues and pull requests with [full URL](#parse-references-with-full-issue-url).

#### overrides

Type: `Object`<br>
Option overrides. Useful when using predefined [`options`](#options) (such as `github`, `gitlab` or `bitbucket`). The `overrides` object can define the same properties as [`options`](#options).

For example, the following will use all the `github` predefined options but with a different `hosts` option:
```js
const issueParser = require('issue-parser');
const parse = issueParser('github', {hosts: ['https://custom-url.com']});
```

### parse(text) => Result

Parse an issue description and returns a [Result](#result) object.

#### text

Type: `String`

Issue text to parse.

### Result

#### actions

Type: `Object`

List of matching actions by type.<br>
Each type of action is an array of objects with the following properties:

| Name   | Type     | Description                                                                           |
|--------|----------|---------------------------------------------------------------------------------------|
| raw    | `String` | The raw value parsed, for example `Fix #1`.                                           |
| action | `String` | The keyword used to identify the action, capitalized.                                 |
| slug   | `String` | The repository owner and name, for issue referred as `<owner>/<repo>#<issue number>`. |
| prefix | `String` | The prefix used to identify the issue.                                                |
| issue  | `String` | The issue number.                                                                     |

#### refs

Type: `Array<Object>`

List of issues and pull requests referenced, but not matched with an action.<br>
Each reference has the following properties:

| Name   | Type     | Description                                                                           |
|--------|----------|---------------------------------------------------------------------------------------|
| raw    | `String` | The raw value parsed, for example `#1`.                                               |
| slug   | `String` | The repository owner and name, for issue referred as `<owner>/<repo>#<issue number>`. |
| prefix | `String` | The prefix used to identify the issue.                                                |
| issue  | `String` | The issue number.                                                                     |

#### mentions

Type: `Array<Object>`

List of users mentioned.<br>
Each mention has the following properties:

| Name   | Type     | Description                                 |
|--------|----------|---------------------------------------------|
| raw    | `String` | The raw value parsed, for example `@user`.  |
| prefix | `String` | The prefix used to identify the mention.    |
| user   | `String` | The user name                               |

#### allRefs

Type: `Array<Object>`

List of all issues and pull requests [referenced](#refs) or matching an [action](#actions-1).<br>
Each reference has the following properties:

| Name   | Type     | Description                                                                                          |
|--------|----------|------------------------------------------------------------------------------------------------------|
| raw    | `String` | The raw value parsed, for example `Fix #1`.                                                          |
| action | `String` | The keyword used to identify the action or the duplicate, capitalized. Only if matched by an action. |
| slug   | `String` | The repository owner and name, for issue referred as `<owner>/<repo>#<issue number>`.                |
| prefix | `String` | The prefix used to identify the issue.                                                               |
| issue  | `String` | The issue number.                                                                                    |


[github-actions-ci-link]: https://github.com/semantic-release/issue-parser/actions/workflows/test.yml

[github-actions-ci-badge]: https://github.com/semantic-release/issue-parser/actions/workflows/test.yml/badge.svg
