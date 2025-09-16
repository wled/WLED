# @semantic-release/exec

[**semantic-release**](https://github.com/semantic-release/semantic-release) plugin to execute custom shell commands.

[![Build Status](https://github.com/semantic-release/exec/workflows/Test/badge.svg)](https://github.com/semantic-release/exec/actions?query=workflow%3ATest+branch%3Amaster) [![npm latest version](https://img.shields.io/npm/v/@semantic-release/exec/latest.svg)](https://www.npmjs.com/package/@semantic-release/exec)
[![npm next version](https://img.shields.io/npm/v/@semantic-release/exec/next.svg)](https://www.npmjs.com/package/@semantic-release/exec)
[![npm beta version](https://img.shields.io/npm/v/@semantic-release/exec/beta.svg)](https://www.npmjs.com/package/@semantic-release/exec)

| Step               | Description                                                                                             |
| ------------------ | ------------------------------------------------------------------------------------------------------- |
| `verifyConditions` | Execute a shell command to verify if the release should happen.                                         |
| `analyzeCommits`   | Execute a shell command to determine the type of release.                                               |
| `verifyRelease`    | Execute a shell command to verifying a release that was determined before and is about to be published. |
| `generateNotes`    | Execute a shell command to generate the release note.                                                   |
| `prepare`          | Execute a shell command to prepare the release.                                                         |
| `publish`          | Execute a shell command to publish the release.                                                         |
| `success`          | Execute a shell command to notify of a new release.                                                     |
| `fail`             | Execute a shell command to notify of a failed release.                                                  |

## Install

```bash
$ npm install @semantic-release/exec -D
```

## Usage

The plugin can be configured in the [**semantic-release** configuration file](https://github.com/semantic-release/semantic-release/blob/master/docs/usage/configuration.md#configuration):

```json
{
  "plugins": [
    "@semantic-release/commit-analyzer",
    "@semantic-release/release-notes-generator",
    [
      "@semantic-release/exec",
      {
        "verifyConditionsCmd": "./verify.sh",
        "publishCmd": "./publish.sh ${nextRelease.version} ${branch.name} ${commits.length} ${Date.now()}"
      }
    ]
  ]
}
```

With this example:

- the shell command `./verify.sh` will be executed on the [verify conditions step](https://github.com/semantic-release/semantic-release#release-steps)
- the shell command `./publish.sh 1.0.0 master 3 870668040000` (for the release of version `1.0.0` from branch `master` with `3` commits on `August 4th, 1997 at 2:14 AM`) will be executed on the [publish step](https://github.com/semantic-release/semantic-release#release-steps)

**Note**: it's required to define a plugin for the [analyze commits step](https://github.com/semantic-release/semantic-release#release-steps). If no [analyzeCommitsCmd](#analyzecommitscmd) is defined the plugin [@semantic-release/commit-analyzer](https://github.com/semantic-release/commit-analyzer) must be defined in the `plugins` list.

## Configuration

### Options

| Options               | Description                                                                                                                                                                                                                                                                                                                              |
| --------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `verifyConditionsCmd` | The shell command to execute during the verify condition step. See [verifyConditionsCmd](#verifyconditionscmd).                                                                                                                                                                                                                          |
| `analyzeCommitsCmd`   | The shell command to execute during the analyze commits step. See [analyzeCommitsCmd](#analyzecommitscmd).                                                                                                                                                                                                                               |
| `verifyReleaseCmd`    | The shell command to execute during the verify release step. See [verifyReleaseCmd](#verifyreleasecmd).                                                                                                                                                                                                                                  |
| `generateNotesCmd`    | The shell command to execute during the generate notes step. See [generateNotesCmd](#generatenotescmd).                                                                                                                                                                                                                                  |
| `prepareCmd`          | The shell command to execute during the prepare step. See [prepareCmd](#preparecmd).                                                                                                                                                                                                                                                     |
| `addChannelCmd`       | The shell command to execute during the add channel step. See [addChannelCmd](#addchannelcmd).                                                                                                                                                                                                                                           |
| `publishCmd`          | The shell command to execute during the publish step. See [publishCmd](#publishcmd).                                                                                                                                                                                                                                                     |
| `successCmd`          | The shell command to execute during the success step. See [successCmd](#successcmd).                                                                                                                                                                                                                                                     |
| `failCmd`             | The shell command to execute during the fail step. See [failCmd](#failcmd).                                                                                                                                                                                                                                                              |
| `shell`               | The shell to use to run the command. See [execa#shell](https://github.com/sindresorhus/execa#shell).                                                                                                                                                                                                                                     |
| `execCwd`             | The path to use as current working directory when executing the shell commands. This path is relative to the path from which **semantic-release** is running. For example if **semantic-release** runs from `/my-project` and `execCwd` is set to `buildScripts` then the shell command will be executed from `/my-project/buildScripts` |

Each shell command is generated with [Lodash template](https://lodash.com/docs#template). All the [`context` object keys](https://github.com/semantic-release/semantic-release/blob/master/docs/developer-guide/plugin.md#context) passed to semantic-release plugins are available as template options.

## verifyConditionsCmd

Execute a shell command to verify if the release should happen.

| Command property | Description                                                              |
| ---------------- | ------------------------------------------------------------------------ |
| `exit code`      | `0` if the verification is successful, or any other exit code otherwise. |
| `stdout`         | Write only the reason for the verification to fail.                      |
| `stderr`         | Can be used for logging.                                                 |

## analyzeCommitsCmd

| Command property | Description                                                                                                                                                |
| ---------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `exit code`      | Any non `0` code is considered as an unexpected error and will stop the `semantic-release` execution with an error.                                        |
| `stdout`         | Only the release type (`major`, `minor` or `patch` etc..) can be written to `stdout`. If no release has to be done the command must not write to `stdout`. |
| `stderr`         | Can be used for logging.                                                                                                                                   |

## verifyReleaseCmd

| Command property | Description                                                              |
| ---------------- | ------------------------------------------------------------------------ |
| `exit code`      | `0` if the verification is successful, or any other exit code otherwise. |
| `stdout`         | Only the reason for the verification to fail can be written to `stdout`. |
| `stderr`         | Can be used for logging.                                                 |

## generateNotesCmd

| Command property | Description                                                                                                         |
| ---------------- | ------------------------------------------------------------------------------------------------------------------- |
| `exit code`      | Any non `0` code is considered as an unexpected error and will stop the `semantic-release` execution with an error. |
| `stdout`         | Only the release note must be written to `stdout`.                                                                  |
| `stderr`         | Can be used for logging.                                                                                            |

## prepareCmd

| Command property | Description                                                                                                         |
| ---------------- | ------------------------------------------------------------------------------------------------------------------- |
| `exit code`      | Any non `0` code is considered as an unexpected error and will stop the `semantic-release` execution with an error. |
| `stdout`         | Can be used for logging.                                                                                            |
| `stderr`         | Can be used for logging.                                                                                            |

## addChannelCmd

| Command property | Description                                                                                                                                                                                                                                        |
| ---------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `exit code`      | Any non `0` code is considered as an unexpected error and will stop the `semantic-release` execution with an error.                                                                                                                                |
| `stdout`         | The `release` information can be written to `stdout` as parseable JSON (for example `{"name": "Release name", "url": "http://url/release/1.0.0"}`). If the command write non parseable JSON to `stdout` no `release` information will be returned. |
| `stderr`         | Can be used for logging.                                                                                                                                                                                                                           |

## publishCmd

| Command property | Description                                                                                                                                                                                                                                        |
| ---------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `exit code`      | Any non `0` code is considered as an unexpected error and will stop the `semantic-release` execution with an error.                                                                                                                                |
| `stdout`         | The `release` information can be written to `stdout` as parseable JSON (for example `{"name": "Release name", "url": "http://url/release/1.0.0"}`). If the command write non parseable JSON to `stdout` no `release` information will be returned. |
| `stderr`         | Can be used for logging.                                                                                                                                                                                                                           |

## successCmd

| Command property | Description                                                                                                         |
| ---------------- | ------------------------------------------------------------------------------------------------------------------- |
| `exit code`      | Any non `0` code is considered as an unexpected error and will stop the `semantic-release` execution with an error. |
| `stdout`         | Can be used for logging.                                                                                            |
| `stderr`         | Can be used for logging.                                                                                            |

## failCmd

| Command property | Description                                                                                                         |
| ---------------- | ------------------------------------------------------------------------------------------------------------------- |
| `exit code`      | Any non `0` code is considered as an unexpected error and will stop the `semantic-release` execution with an error. |
| `stdout`         | Can be used for logging.                                                                                            |
| `stderr`         | Can be used for logging.                                                                                            |
