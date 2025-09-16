# @semantic-release/npm

[**semantic-release**](https://github.com/semantic-release/semantic-release) plugin to publish a [npm](https://www.npmjs.com) package.

[![Build Status](https://github.com/semantic-release/npm/workflows/Test/badge.svg)](https://github.com/semantic-release/npm/actions?query=workflow%3ATest+branch%3Amaster) [![npm latest version](https://img.shields.io/npm/v/@semantic-release/npm/latest.svg)](https://www.npmjs.com/package/@semantic-release/npm)
[![npm next version](https://img.shields.io/npm/v/@semantic-release/npm/next.svg)](https://www.npmjs.com/package/@semantic-release/npm)
[![npm beta version](https://img.shields.io/npm/v/@semantic-release/npm/beta.svg)](https://www.npmjs.com/package/@semantic-release/npm)

| Step               | Description                                                                                                                      |
| ------------------ | -------------------------------------------------------------------------------------------------------------------------------- |
| `verifyConditions` | Verify the presence of the `NPM_TOKEN` environment variable, or an `.npmrc` file, and verify the authentication method is valid. |
| `prepare`          | Update the `package.json` version and [create](https://docs.npmjs.com/cli/pack) the npm package tarball.                         |
| `addChannel`       | [Add a release to a dist-tag](https://docs.npmjs.com/cli/dist-tag).                                                              |
| `publish`          | [Publish the npm package](https://docs.npmjs.com/cli/publish) to the registry.                                                   |

## Install

```bash
$ npm install @semantic-release/npm -D
```

## Usage

The plugin can be configured in the [**semantic-release** configuration file](https://github.com/semantic-release/semantic-release/blob/master/docs/usage/configuration.md#configuration):

```json
{
  "plugins": ["@semantic-release/commit-analyzer", "@semantic-release/release-notes-generator", "@semantic-release/npm"]
}
```

## Configuration

### npm registry authentication

The npm [token](https://docs.npmjs.com/about-access-tokens) authentication configuration is **required** and can be set via [environment variables](#environment-variables).

Automation tokens are recommended since they can be used for an automated workflow, even when your account is configured to use the [`auth-and-writes` level of 2FA](https://docs.npmjs.com/about-two-factor-authentication#authorization-and-writes).

### npm provenance

If you are publishing to the official registry and your pipeline is on a [provider that is supported by npm for provenance](https://docs.npmjs.com/generating-provenance-statements#provenance-limitations), npm can be configured to [publish with provenance](https://docs.npmjs.com/generating-provenance-statements).

Since semantic-release wraps the npm publish command, configuring provenance is not exposed directly.
Instead, provenance can be configured through the [other configuration options exposed by npm](https://docs.npmjs.com/generating-provenance-statements#using-third-party-package-publishing-tools).
Provenance applies specifically to publishing, so our recommendation is to configure under `publishConfig` within the `package.json`.

#### npm provenance on GitHub Actions

For package provenance to be signed on the GitHub Actions CI the following permission is required
to be enabled on the job:

```yaml
permissions:
  id-token: write # to enable use of OIDC for npm provenance
```

It's worth noting that if you are using semantic-release to its fullest with a GitHub release, GitHub comments,
and other features, then [more permissions are required](https://github.com/semantic-release/github#github-authentication) to be enabled on this job:

```yaml
permissions:
  contents: write # to be able to publish a GitHub release
  issues: write # to be able to comment on released issues
  pull-requests: write # to be able to comment on released pull requests
  id-token: write # to enable use of OIDC for npm provenance
```

Refer to the [GitHub Actions recipe for npm package provenance](https://semantic-release.gitbook.io/semantic-release/recipes/ci-configurations/github-actions#.github-workflows-release.yml-configuration-for-node-projects) for the full CI job's YAML code example.

### Environment variables

| Variable    | Description                                                                                                                   |
| ----------- | ----------------------------------------------------------------------------------------------------------------------------- |
| `NPM_TOKEN` | Npm token created via [npm token create](https://docs.npmjs.com/getting-started/working_with_tokens#how-to-create-new-tokens) |

### Options

| Options      | Description                                                                                                        | Default                                                                                                                          |
| ------------ | ------------------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------------- |
| `npmPublish` | Whether to publish the `npm` package to the registry. If `false` the `package.json` version will still be updated. | `false` if the `package.json` [private](https://docs.npmjs.com/files/package.json#private) property is `true`, `true` otherwise. |
| `pkgRoot`    | Directory path to publish.                                                                                         | `.`                                                                                                                              |
| `tarballDir` | Directory path in which to write the package tarball. If `false` the tarball is not be kept on the file system.    | `false`                                                                                                                          |

**Note**: The `pkgRoot` directory must contain a `package.json`. The version will be updated only in the `package.json` and `npm-shrinkwrap.json` within the `pkgRoot` directory.

**Note**: If you use a [shareable configuration](https://github.com/semantic-release/semantic-release/blob/master/docs/usage/shareable-configurations.md#shareable-configurations) that defines one of these options you can set it to `false` in your [**semantic-release** configuration](https://github.com/semantic-release/semantic-release/blob/master/docs/usage/configuration.md#configuration) in order to use the default value.

### npm configuration

The plugin uses the [`npm` CLI](https://github.com/npm/cli) which will read the configuration from [`.npmrc`](https://docs.npmjs.com/files/npmrc). See [`npm config`](https://docs.npmjs.com/misc/config) for the option list.

The [`registry`](https://docs.npmjs.com/misc/registry) can be configured via the npm environment variable `NPM_CONFIG_REGISTRY` and will take precedence over the configuration in `.npmrc`.

The [`registry`](https://docs.npmjs.com/misc/registry) and [`dist-tag`](https://docs.npmjs.com/cli/dist-tag) can be configured under `publishConfig` in the `package.json`:

```json
{
  "publishConfig": {
    "registry": "https://registry.npmjs.org/",
    "tag": "latest"
  }
}
```

**Notes**:

- The presence of an `.npmrc` file will override any specified environment variables.
- The presence of `registry` or `dist-tag` under `publishConfig` in the `package.json` will take precedence over the configuration in `.npmrc` and `NPM_CONFIG_REGISTRY`

### Examples

The `npmPublish` and `tarballDir` option can be used to skip the publishing to the `npm` registry and instead, release the package tarball with another plugin. For example with the [@semantic-release/github](https://github.com/semantic-release/github) plugin:

```json
{
  "plugins": [
    "@semantic-release/commit-analyzer",
    "@semantic-release/release-notes-generator",
    [
      "@semantic-release/npm",
      {
        "npmPublish": false,
        "tarballDir": "dist"
      }
    ],
    [
      "@semantic-release/github",
      {
        "assets": "dist/*.tgz"
      }
    ]
  ]
}
```

When publishing from a sub-directory with the `pkgRoot` option, the `package.json` and `npm-shrinkwrap.json` updated with the new version can be moved to another directory with a `postversion`. For example with the [@semantic-release/git](https://github.com/semantic-release/git) plugin:

```json
{
  "plugins": [
    "@semantic-release/commit-analyzer",
    "@semantic-release/release-notes-generator",
    [
      "@semantic-release/npm",
      {
        "pkgRoot": "dist"
      }
    ],
    [
      "@semantic-release/git",
      {
        "assets": ["package.json", "npm-shrinkwrap.json"]
      }
    ]
  ]
}
```

```json
{
  "scripts": {
    "postversion": "cp -r package.json .. && cp -r npm-shrinkwrap.json .."
  }
}
```
