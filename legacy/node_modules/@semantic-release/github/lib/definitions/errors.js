import { inspect } from "node:util";
import { createRequire } from "node:module";
const require = createRequire(import.meta.url);

import { isString } from "lodash-es";

const pkg = require("../../package.json");
const HOMEPAGE = pkg.homepage;

const stringify = (object) =>
  isString(object)
    ? object
    : inspect(object, {
        breakLength: Number.POSITIVE_INFINITY,
        depth: 2,
        maxArrayLength: 5,
      });
const linkify = (file) => `${HOMEPAGE}/blob/master/${file}`;

export function EINVALIDASSETS({ assets }) {
  return {
    message: "Invalid `assets` option.",
    details: `The [assets option](${linkify(
      "README.md#assets",
    )}) must be an \`Array\` of \`Strings\` or \`Objects\` with a \`path\` property.

Your configuration for the \`assets\` option is \`${stringify(assets)}\`.`,
  };
}

export function EINVALIDSUCCESSCOMMENT({ successComment }) {
  return {
    message: "Invalid `successComment` option.",
    details: `The [successComment option](${linkify(
      "README.md#successcomment",
    )}) if defined, must be a non empty \`String\`.

Your configuration for the \`successComment\` option is \`${stringify(
      successComment,
    )}\`.`,
  };
}

export function EINVALIDFAILTITLE({ failTitle }) {
  return {
    message: "Invalid `failTitle` option.",
    details: `The [failTitle option](${linkify(
      "README.md#failtitle",
    )}) if defined, must be a non empty \`String\`.

Your configuration for the \`failTitle\` option is \`${stringify(
      failTitle,
    )}\`.`,
  };
}

export function EINVALIDFAILCOMMENT({ failComment }) {
  return {
    message: "Invalid `failComment` option.",
    details: `The [failComment option](${linkify(
      "README.md#failcomment",
    )}) if defined, must be a non empty \`String\`.

Your configuration for the \`failComment\` option is \`${stringify(
      failComment,
    )}\`.`,
  };
}

export function EINVALIDLABELS({ labels }) {
  return {
    message: "Invalid `labels` option.",
    details: `The [labels option](${linkify(
      "README.md#options",
    )}) if defined, must be an \`Array\` of non empty \`String\`.

Your configuration for the \`labels\` option is \`${stringify(labels)}\`.`,
  };
}

export function EINVALIDASSIGNEES({ assignees }) {
  return {
    message: "Invalid `assignees` option.",
    details: `The [assignees option](${linkify(
      "README.md#options",
    )}) must be an \`Array\` of non empty \`Strings\`.

Your configuration for the \`assignees\` option is \`${stringify(
      assignees,
    )}\`.`,
  };
}

export function EINVALIDRELEASEDLABELS({ releasedLabels }) {
  return {
    message: "Invalid `releasedLabels` option.",
    details: `The [releasedLabels option](${linkify(
      "README.md#options",
    )}) if defined, must be an \`Array\` of non empty \`String\`.

Your configuration for the \`releasedLabels\` option is \`${stringify(
      releasedLabels,
    )}\`.`,
  };
}

export function EINVALIDADDRELEASES({ addReleases }) {
  return {
    message: "Invalid `addReleases` option.",
    details: `The [addReleases option](${linkify(
      "README.md#options",
    )}) if defined, must be one of \`false|top|bottom\`.

Your configuration for the \`addReleases\` option is \`${stringify(
      addReleases,
    )}\`.`,
  };
}

export function EINVALIDDRAFTRELEASE({ draftRelease }) {
  return {
    message: "Invalid `draftRelease` option.",
    details: `The [draftRelease option](${linkify(
      "README.md#options",
    )}) if defined, must be a \`Boolean\`.

Your configuration for the \`draftRelease\` option is \`${stringify(
      draftRelease,
    )}\`.`,
  };
}

export function EINVALIDGITHUBURL() {
  return {
    message: "The git repository URL is not a valid GitHub URL.",
    details: `The **semantic-release** \`repositoryUrl\` option must a valid GitHub URL with the format \`<GitHub_or_GHE_URL>/<owner>/<repo>.git\`.

By default the \`repositoryUrl\` option is retrieved from the \`repository\` property of your \`package.json\` or the [git origin url](https://git-scm.com/book/en/v2/Git-Basics-Working-with-Remotes) of the repository cloned by your CI environment.`,
  };
}

export function EMISMATCHGITHUBURL({ repositoryUrl, clone_url }) {
  return {
    message: "The git repository URL mismatches the GitHub URL.",
    details: `The **semantic-release** \`repositoryUrl\` option must have the same repository name and owner as the GitHub repo.

Your configuration for the \`repositoryUrl\` option is \`${stringify(repositoryUrl)}\` and the \`clone_url\` of your GitHub repo is \`${stringify(clone_url)}\`.

By default the \`repositoryUrl\` option is retrieved from the \`repository\` property of your \`package.json\` or the [git origin url](https://git-scm.com/book/en/v2/Git-Basics-Working-with-Remotes) of the repository cloned by your CI environment.

Note: If you have recently changed your GitHub repository name or owner, update the value in **semantic-release** \`repositoryUrl\` option and the \`repository\` property of your \`package.json\` respectively to match the new GitHub URL.`,
  };
}

export function EINVALIDPROXY({ proxy }) {
  return {
    message: "Invalid `proxy` option.",
    details: `The [proxy option](${linkify(
      "README.md#proxy",
    )}) must be a \`String\`  or an \`Objects\` with a \`host\` and a \`port\` property.

Your configuration for the \`proxy\` option is \`${stringify(proxy)}\`.`,
  };
}

export function EMISSINGREPO({ owner, repo }) {
  return {
    message: `The repository ${owner}/${repo} doesn't exist.`,
    details: `The **semantic-release** \`repositoryUrl\` option must refer to your GitHub repository. The repository must be accessible with the [GitHub API](https://developer.github.com/v3).

By default the \`repositoryUrl\` option is retrieved from the \`repository\` property of your \`package.json\` or the [git origin url](https://git-scm.com/book/en/v2/Git-Basics-Working-with-Remotes) of the repository cloned by your CI environment.

If you are using [GitHub Enterprise](https://enterprise.github.com) please make sure to configure the \`githubUrl\` and \`githubApiPathPrefix\` [options](${linkify(
      "README.md#options",
    )}).`,
  };
}

export function EGHNOPERMISSION({ owner, repo }) {
  return {
    message: `The GitHub token doesn't allow to push on the repository ${owner}/${repo}.`,
    details: `The user associated with the [GitHub token](${linkify(
      "README.md#github-authentication",
    )}) configured in the \`GH_TOKEN\` or \`GITHUB_TOKEN\` environment variable must allows to push to the repository ${owner}/${repo}.

Please make sure the GitHub user associated with the token is an [owner](https://help.github.com/articles/permission-levels-for-a-user-account-repository/#owner-access-on-a-repository-owned-by-a-user-account) or a [collaborator](https://help.github.com/articles/permission-levels-for-a-user-account-repository/#collaborator-access-on-a-repository-owned-by-a-user-account) if the repository belong to a user account or has [write permissions](https://help.github.com/articles/managing-team-access-to-an-organization-repository) if the repository [belongs to an organization](https://help.github.com/articles/repository-permission-levels-for-an-organization).`,
  };
}

export function EINVALIDGHTOKEN({ owner, repo }) {
  return {
    message: "Invalid GitHub token.",
    details: `The [GitHub token](${linkify(
      "README.md#github-authentication",
    )}) configured in the \`GH_TOKEN\` or \`GITHUB_TOKEN\` environment variable must be a valid [personal token](https://help.github.com/articles/creating-a-personal-access-token-for-the-command-line) allowing to push to the repository ${owner}/${repo}.

Please make sure to set the \`GH_TOKEN\` or \`GITHUB_TOKEN\` environment variable in your CI with the exact value of the GitHub personal token.`,
  };
}

export function ENOGHTOKEN({ owner, repo }) {
  return {
    message: "No GitHub token specified.",
    details: `A [GitHub personal token](${linkify(
      "README.md#github-authentication",
    )}) must be created and set in the \`GH_TOKEN\` or \`GITHUB_TOKEN\` environment variable on your CI environment.

Please make sure to create a [GitHub personal token](https://help.github.com/articles/creating-a-personal-access-token-for-the-command-line) and to set it in the \`GH_TOKEN\` or \`GITHUB_TOKEN\` environment variable on your CI environment. The token must allow to push to the repository ${owner}/${repo}.`,
  };
}

export function EINVALIDRELEASEBODYTEMPLATE({ releaseBodyTemplate }) {
  return {
    message: "Invalid `releaseBodyTemplate` option.",
    details: `The [releaseBodyTemplate option](${linkify(
      "README.md#releaseBodyTemplate",
    )}) must be a non empty \`String\`.

Your configuration for the \`releaseBodyTemplate\` option is \`${stringify(
      releaseBodyTemplate,
    )}\`.`,
  };
}

export function EINVALIDRELEASENAMETEMPLATE({ releaseNameTemplate }) {
  return {
    message: "Invalid `releaseNameTemplate` option.",
    details: `The [releaseNameTemplate option](${linkify(
      "README.md#releaseNameTemplate",
    )}) must be a non empty \`String\`.

Your configuration for the \`releaseNameTemplate\` option is \`${stringify(
      releaseNameTemplate,
    )}\`.`,
  };
}

export function EINVALIDDISCUSSIONCATEGORYNAME({ discussionCategoryName }) {
  return {
    message: "Invalid `discussionCategoryName` option.",
    details: `The [discussionCategoryName option](${linkify(
      "README.md#discussionCategoryName",
    )}) if defined, must be a non empty \`String\`.

Your configuration for the \`discussionCategoryName\` option is \`${stringify(
      discussionCategoryName,
    )}\`.`,
  };
}
