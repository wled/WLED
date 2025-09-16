# @semantic-release/git

[**semantic-release**](https://github.com/semantic-release/semantic-release) plugin to commit release assets to the project's [git](https://git-scm.com/) repository.

[![Build Status](https://github.com/semantic-release/git/workflows/Test/badge.svg)](https://github.com/semantic-release/git/actions?query=workflow%3ATest+branch%3Amaster) [![npm latest version](https://img.shields.io/npm/v/@semantic-release/git/latest.svg)](https://www.npmjs.com/package/@semantic-release/git)
[![npm next version](https://img.shields.io/npm/v/@semantic-release/git/next.svg)](https://www.npmjs.com/package/@semantic-release/git)
[![npm beta version](https://img.shields.io/npm/v/@semantic-release/git/beta.svg)](https://www.npmjs.com/package/@semantic-release/git)

| Step               | Description                                                                                                                        |
|--------------------|------------------------------------------------------------------------------------------------------------------------------------|
| `verifyConditions` | Verify the access to the remote Git repository, the commit [`message`](#message) and the [`assets`](#assets) option configuration. |
| `prepare`          | Create a release commit, including configurable file assets.                                                                       |

## Install

```bash
$ npm install @semantic-release/git -D
```

## Usage

The plugin can be configured in the [**semantic-release** configuration file](https://github.com/semantic-release/semantic-release/blob/master/docs/usage/configuration.md#configuration):

```json
{
  "plugins": [
    "@semantic-release/commit-analyzer",
    "@semantic-release/release-notes-generator",
    ["@semantic-release/git", {
      "assets": ["dist/**/*.{js,css}", "docs", "package.json"],
      "message": "chore(release): ${nextRelease.version} [skip ci]\n\n${nextRelease.notes}"
    }]
  ]
}
```

With this example, for each release a release commit will be pushed to the remote Git repository with:
- a message formatted like `chore(release): <version> [skip ci]\n\n<release notes>`
- the `.js` and `.css` files in the `dist` directory, the files in the `docs` directory and the `package.json`

### Merging between semantic-release branches

This plugin will, by default, create commit messages with the keyword `[skip ci]`, so they won't trigger a new unnecessary CI build. If you are using **semantic-release** with [multiple branches](https://github.com/semantic-release/semantic-release/blob/beta/docs/usage/workflow-configuration.md), when merging a branch with a head being a release commit, a CI job will be triggered on the target branch. Depending on the CI service that might create an unexpected behavior as the head of the target branch might be ignored by the build due to the `[skip ci]` keyword.

To avoid any unexpected behavior we recommend to use the [`--no-ff` option](https://git-scm.com/docs/git-merge#Documentation/git-merge.txt---no-ff) when merging branches used by **semantic-release**.

**Note**: This concerns only merges done between two branches configured in the [`branches` option](https://github.com/semantic-release/semantic-release/blob/beta/docs/usage/configuration.md#branches).

## Configuration

### Git authentication

The Git user associated with the [Git credentials](https://github.com/semantic-release/semantic-release/blob/master/docs/usage/ci-configuration.md#authentication) has to be able to push commit to the [release branch](https://github.com/semantic-release/semantic-release/blob/master/docs/usage/configuration.md#branch).

When configuring branches permission on a Git hosting service (e.g. [GitHub protected branches](https://help.github.com/articles/about-protected-branches), [GitLab protected branches](https://docs.gitlab.com/ee/user/project/protected_branches.html) or [Bitbucket branch permissions](https://confluence.atlassian.com/bitbucket/branch-permissions-385912271.html)) it might be necessary to create a specific configuration in order to allow the **semantic-release** user to bypass global restrictions. For example on GitHub you can uncheck "Include administrators" and configure **semantic-release** to use an administrator user, so the plugin can push the release commit without requiring [status checks](https://help.github.com/articles/about-required-status-checks) and [pull request reviews](https://help.github.com/articles/about-required-reviews-for-pull-requests).

### Environment variables

| Variable              | Description                                                                                                                                                              | Default                              |
|-----------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------|--------------------------------------|
| `GIT_AUTHOR_NAME`     | The author name associated with the release commit. See [Git environment variables](https://git-scm.com/book/en/v2/Git-Internals-Environment-Variables#_committing).     | @semantic-release-bot.               |
| `GIT_AUTHOR_EMAIL`    | The author email associated with the release commit. See [Git environment variables](https://git-scm.com/book/en/v2/Git-Internals-Environment-Variables#_committing).    | @semantic-release-bot email address. |
| `GIT_COMMITTER_NAME`  | The committer name associated with the release commit. See [Git environment variables](https://git-scm.com/book/en/v2/Git-Internals-Environment-Variables#_committing).  | @semantic-release-bot.               |
| `GIT_COMMITTER_EMAIL` | The committer email associated with the release commit. See [Git environment variables](https://git-scm.com/book/en/v2/Git-Internals-Environment-Variables#_committing). | @semantic-release-bot email address. |

### Options

| Options   | Description                                                                                                                  | Default                                                                        |
|-----------|------------------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------|
| `message` | The message for the release commit. See [message](#message).                                                                 | `chore(release): ${nextRelease.version} [skip ci]\n\n${nextRelease.notes}`     |
| `assets`  | Files to include in the release commit. Set to `false` to disable adding files to the release commit. See [assets](#assets). | `['CHANGELOG.md', 'package.json', 'package-lock.json', 'npm-shrinkwrap.json']` |

#### `message`

The message for the release commit is generated with [Lodash template](https://lodash.com/docs#template). The following variables are available:

| Parameter           | Description                                                                                                                             |
|---------------------|-----------------------------------------------------------------------------------------------------------------------------------------|
| `branch`            | The branch from which the release is done.                                                                                              |
| `branch.name`       | The branch name.                                                                                                                        |
| `branch.type`       | The [type of branch](https://github.com/semantic-release/semantic-release/blob/beta/docs/usage/workflow-configuration.md#branch-types). |
| `branch.channel`    | The distribution channel on which to publish releases from this branch.                                                                 |
| `branch.range`      | The range of [semantic versions](https://semver.org) to support on this branch.                                                         |
| `branch.prerelease` | The pre-release detonation to append to [semantic versions](https://semver.org) released from this branch.                              |
| `lastRelease`       | `Object` with `version`, `gitTag` and `gitHead` of the last release.                                                                    |
| `nextRelease`       | `Object` with `version`, `gitTag`, `gitHead` and `notes` of the release being done.                                                     |

**Note**: It is recommended to include `[skip ci]` in the commit message to not trigger a new build. Some CI service support the `[skip ci]` keyword only in the subject of the message.

##### `message` examples

The `message` `Release <%= nextRelease.version %> - <%= new Date().toLocaleDateString('en-US', {year: 'numeric', month: 'short', day: 'numeric', hour: 'numeric', minute: 'numeric' }) %> [skip ci]\n\n<%= nextRelease.notes %>` will generate the commit message:

> Release v1.0.0 - Oct. 21, 2015 1:24 AM \[skip ci\]<br><br>## 1.0.0<br><br>### Features<br>* Generate 1.21 gigawatts of electricity<br>...

#### `assets`

Can be an `Array` or a single entry. Each entry can be either:
- a [glob](https://github.com/micromatch/micromatch#matching-features)
- or an `Object` with a `path` property containing a [glob](https://github.com/micromatch/micromatch#matching-features).

Each entry in the `assets` `Array` is globbed individually. A [glob](https://github.com/micromatch/micromatch#matching-features) can be a `String` (`"dist/**/*.js"` or `"dist/mylib.js"`) or an `Array` of `String`s that will be globbed together (`["dist/**", "!**/*.css"]`).

If a directory is configured, all the files under this directory and its children will be included.

**Note**: If a file has a match in `assets` it will be included even if it also has a match in `.gitignore`.

##### `assets` examples

`'dist/*.js'`: include all `js` files in the `dist` directory, but not in its sub-directories.

`'dist/**/*.js'`: include all `js` files in the `dist` directory and its sub-directories.

`[['dist', '!**/*.css']]`: include all files in the `dist` directory and its sub-directories excluding the `css` files.

`[['dist', '!**/*.css'], 'package.json']`: include `package.json` and all files in the `dist` directory and its sub-directories excluding the `css` files.

`[['dist/**/*.{js,css}', '!**/*.min.*']]`: include all `js` and `css` files in the `dist` directory and its sub-directories excluding the minified version.

### Examples

When used with the [@semantic-release/changelog](https://github.com/semantic-release/changelog) or [@semantic-release/npm](https://github.com/semantic-release/npm) plugins:
- The [@semantic-release/changelog](https://github.com/semantic-release/changelog) plugin must be called first in order to update the changelog file so the `@semantic-release/git` and [@semantic-release/npm](https://github.com/semantic-release/npm) plugins can include it in the release.
- The [@semantic-release/npm](https://github.com/semantic-release/npm) plugin must be called second in order to update the `package.json` file so the `@semantic-release/git` plugin can include it in the release commit.

```json
{
  "plugins": [
    "@semantic-release/commit-analyzer",
    "@semantic-release/release-notes-generator",
    "@semantic-release/changelog",
    "@semantic-release/npm",
    "@semantic-release/git"
  ],
}
```

### GPG signature

Using GPG, you can [sign and verify tags and commits](https://git-scm.com/book/id/v2/Git-Tools-Signing-Your-Work). With GPG keys, the release tags and commits made by Semantic-release are verified and other people can trust that they were really were made by your account.

#### Generate the GPG keys

If you already have a GPG public and private key you can skip this step and go to the [Get the GPG keys ID and the public key content](#get-the-gpg-keys-id-and-the-public-key-content) step.

[Download and install the GPG command line tools](https://www.gnupg.org/download/#binary) for your operating system.

Create a GPG key

```bash
$ gpg --full-generate-key
```

At the prompt select the `RSA and RSA` king of key, enter `4096` for the keysize, specify how long the key should be valid, enter yout name, the email associated with your Git hosted account and finally set a long and hard to guess passphrase.

#### Get the GPG keys ID and the public key content

Use the `gpg --list-secret-keys --keyid-format LONG` command to list your GPG keys. From the list, copy the GPG key ID you just created.

```bash
$ gpg --list-secret-keys --keyid-format LONG
/Users/<user_home>/.gnupg/pubring.gpg
---------------------------------------
sec   rsa4096/XXXXXXXXXXXXXXXX 2017-12-01 [SC]
uid                 <your_name> <your_email>
ssb   rsa4096/YYYYYYYYYYYYYYYY 2017-12-01 [E]
```
the GPG key ID if 16 character string, on the on the `sec` line, after `rsa4096`. In this example, the GPG key ID is `XXXXXXXXXXXXXXXX`.

Export the public key (replace XXXXXXXXXXXXXXXX with your key ID):

```bash
$ gpg --armor --export XXXXXXXXXXXXXXXX
```

Copy your GPG key, beginning with -----BEGIN PGP PUBLIC KEY BLOCK----- and ending with -----END PGP PUBLIC KEY BLOCK-----

#### Add the GPG key to your Git hosted account

##### Add the GPG key to GitHub

In GitHub **Settings**, click on **SSH and GPG keys** in the sidebar, then on the **New GPG Key** button.

Paste the entire GPG key export previously and click the **Add GPG Key** button.

See [Adding a new GPG key to your GitHub account](https://help.github.com/articles/adding-a-new-gpg-key-to-your-github-account/) for more details.

### Use the GPG key to sign commit and tags locally

If you want to use this GPG to also sign the commits and tags you create on your local machine you can follow the instruction at [Git Tools - Signing Your Work](https://git-scm.com/book/id/v2/Git-Tools-Signing-Your-Work)
This step is optional and unrelated to Semantic-release.

#### Add the GPG keys to your CI environment

Make the public and private GPG key available on the CI environment. Encrypt the keys, commit it to your repository and configure the CI environment to decrypt it.

##### Add the GPG keys to Travis CI

Install the [Travis CLI](https://github.com/travis-ci/travis.rb#installation):

```bash
$ gem install travis
```

[Login](https://github.com/travis-ci/travis.rb#login) to Travis with the CLI:

```bash
$ travis login
```

Add the following [environment](https://github.com/travis-ci/travis.rb#env) variables to Travis:
- `GPG_PASSPHRASE` to Travis with the value set during the [GPG keys generation](#generate-the-gpg-keys) step
- `GPG_KEY_ID` to Travis with the value of your GPG key ID retrieved during the [GPG keys generation](#generate-the-gpg-keys) (replace XXXXXXXXXXXXXXXX with your key ID)
- `GIT_EMAIL` with the email address you set during the [GPG keys generation](#generate-the-gpg-keys) step
- `GIT_USERNAME` with the name you set during the [GPG keys generation](#generate-the-gpg-keys) step

```bash
$ travis env set GPG_PASSPHRASE <gpg_passphrase>
$ travis env set GPG_KEY_ID XXXXXXXXXXXXXXXX
$ travis env set GIT_EMAIL <your_email>
$ travis env set GIT_USERNAME <your_name>
```

From your repository root export your public and private GPG keys in the `git_gpg_keys.asc` (replace XXXXXXXXXXXXXXXX with your key ID):

```bash
$ gpg --export -a XXXXXXXXXXXXXXXX > git_gpg_keys.asc
$ gpg --export-secret-key -a XXXXXXXXXXXXXXXX >> git_gpg_keys.asc
```

[Encrypt](https://github.com/travis-ci/travis.rb#encrypt) the `git_gpg_keys.asc` (public and private key) using a symmetric encryption (AES-256), and store the secret in a secure environment variable in the Travis environment:

```bash
$ travis encrypt-file git_gpg_keys.asc
```
The `travis encrypt-file` will encrypt the keys into the `git_gpg_keys.asc.enc` file and output in the console the command to add to your `.travis.yml` file. It should look like `openssl aes-256-cbc -K $encrypted_AAAAAAAAAAAA_key -iv $encrypted_BBBBBBBBBBBB_iv -in git_gpg_keys.asc.enc -out git_gpg_keys.asc -d`.

Copy this command to your `.travis.yml` file in the `before_install` step. Change the output path to write the unencrypted key in `/tmp`: `-out git_gpg_keys.asc` => `/tmp/git_gpg_keys.asc`. This will avoid to commit / modify / delete the unencrypted keys by mistake on the CI. Then add the commands to decrypt the GPG keys and make it available to `git`:

```yaml
before_install:
  # Decrypt the git_gpg_keys.asc.enc key into /tmp/git_gpg_keys.asc
  - openssl aes-256-cbc -K $encrypted_AAAAAAAAAAAA_key -iv $encrypted_BBBBBBBBBBBB_iv -in git_gpg_keys.asc.enc -out /tmp/git_gpg_keys.asc -d
  # Make sure only the current user can read the keys
  - chmod 600 /tmp/git_gpg_keys.asc
  # Import the gpg key
  - gpg --batch --yes --import /tmp/git_gpg_keys.asc
  # Create a script that pass the passphrase to the gpg CLI called by git
  - echo '/usr/bin/gpg2 --passphrase ${GPG_PASSPHRASE} --batch --no-tty "$@"' > /tmp/gpg-with-passphrase && chmod +x /tmp/gpg-with-passphrase
  # Configure git to use the script that passes the passphrase
  - git config gpg.program "/tmp/gpg-with-passphrase"
  # Configure git to sign the commits and tags
  - git config commit.gpgsign true
  # Configure git to use your GPG key
  - git config --global user.signingkey ${GPG_KEY_ID}
```

See [Encrypting Files](https://docs.travis-ci.com/user/encrypting-files/) for more details.

Delete the local keys as it won't be used anymore:

```bash
$ rm git_gpg_keys.asc
```

Commit the encrypted keys and the `.travis.yml` file to your repository:

```bash
$ git add git_gpg_keys.asc.enc .travis.yml
$ git commit -m "ci(travis): Add the encrypted GPG keys"
$ git push
```
