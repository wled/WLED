import {
  isString,
  isPlainObject,
  isNil,
  isArray,
  isNumber,
  isBoolean,
} from "lodash-es";
import urlJoin from "url-join";
import AggregateError from "aggregate-error";

import parseGithubUrl from "./parse-github-url.js";
import resolveConfig from "./resolve-config.js";
import { toOctokitOptions } from "./octokit.js";
import getError from "./get-error.js";

const isNonEmptyString = (value) => isString(value) && value.trim();
const oneOf = (enumArray) => (value) => enumArray.includes(value);
const isStringOrStringArray = (value) =>
  isNonEmptyString(value) ||
  (isArray(value) && value.every((string) => isNonEmptyString(string)));
const isArrayOf = (validator) => (array) =>
  isArray(array) && array.every((value) => validator(value));
const canBeDisabled = (validator) => (value) =>
  value === false || validator(value);

const VALIDATORS = {
  proxy: canBeDisabled(
    (proxy) =>
      isNonEmptyString(proxy) ||
      (isPlainObject(proxy) &&
        isNonEmptyString(proxy.host) &&
        isNumber(proxy.port)),
  ),
  assets: isArrayOf(
    (asset) =>
      isStringOrStringArray(asset) ||
      (isPlainObject(asset) && isStringOrStringArray(asset.path)),
  ),
  successComment: canBeDisabled(isNonEmptyString),
  successCommentCondition: canBeDisabled(isNonEmptyString),
  failTitle: canBeDisabled(isNonEmptyString),
  failComment: canBeDisabled(isNonEmptyString),
  failCommentCondition: canBeDisabled(isNonEmptyString),
  labels: canBeDisabled(isArrayOf(isNonEmptyString)),
  assignees: isArrayOf(isNonEmptyString),
  releasedLabels: canBeDisabled(isArrayOf(isNonEmptyString)),
  addReleases: canBeDisabled(oneOf(["bottom", "top"])),
  draftRelease: isBoolean,
  releaseBodyTemplate: isNonEmptyString,
  releaseNameTemplate: isNonEmptyString,
  discussionCategoryName: canBeDisabled(isNonEmptyString),
};

export default async function verify(pluginConfig, context, { Octokit }) {
  const {
    env,
    options: { repositoryUrl },
    logger,
  } = context;
  const {
    githubToken,
    githubUrl,
    githubApiPathPrefix,
    githubApiUrl,
    proxy,
    ...options
  } = resolveConfig(pluginConfig, context);

  const errors = Object.entries({ ...options, proxy }).reduce(
    (errors, [option, value]) =>
      !isNil(value) && !VALIDATORS[option](value)
        ? [
            ...errors,
            getError(`EINVALID${option.toUpperCase()}`, { [option]: value }),
          ]
        : errors,
    [],
  );

  if (githubApiUrl) {
    logger.log("Verify GitHub authentication (%s)", githubApiUrl);
  } else if (githubUrl) {
    logger.log(
      "Verify GitHub authentication (%s)",
      urlJoin(githubUrl, githubApiPathPrefix),
    );
  } else {
    logger.log("Verify GitHub authentication");
  }

  const { repo, owner } = parseGithubUrl(repositoryUrl);
  if (!owner || !repo) {
    errors.push(getError("EINVALIDGITHUBURL"));
  } else if (
    githubToken &&
    !errors.find(({ code }) => code === "EINVALIDPROXY")
  ) {
    const octokit = new Octokit(
      toOctokitOptions({
        githubToken,
        githubUrl,
        githubApiPathPrefix,
        githubApiUrl,
        proxy,
      }),
    );
    try {
      const {
        data: { permissions, clone_url },
      } = await octokit.request("GET /repos/{owner}/{repo}", { repo, owner });
      // Verify if Repository Name wasn't changed
      const parsedCloneUrl = parseGithubUrl(clone_url);
      if (
        `${owner}/${repo}`.toLowerCase() !==
        `${parsedCloneUrl.owner}/${parsedCloneUrl.repo}`.toLowerCase()
      ) {
        errors.push(
          getError("EMISMATCHGITHUBURL", { repositoryUrl, clone_url }),
        );
      }

      // https://github.com/semantic-release/github/issues/182
      // Do not check for permissions in GitHub actions, as the provided token is an installation access token.
      // octokit.request("GET /repos/{owner}/{repo}", {repo, owner}) does not return the "permissions" key in that case.
      // But GitHub Actions have all permissions required for @semantic-release/github to work
      if (!env.GITHUB_ACTION && !permissions?.push) {
        // If authenticated as GitHub App installation, `push` will always be false.
        // We send another request to check if current authentication is an installation.
        // Note: we cannot check if the installation has all required permissions, it's
        // up to the user to make sure it has
        if (
          !(await octokit
            .request("HEAD /installation/repositories", { per_page: 1 })
            .catch(() => false))
        ) {
          errors.push(getError("EGHNOPERMISSION", { owner, repo }));
        }
      }
    } catch (error) {
      if (error.status === 401) {
        errors.push(getError("EINVALIDGHTOKEN", { owner, repo }));
      } else if (error.status === 404) {
        errors.push(getError("EMISSINGREPO", { owner, repo }));
      } else {
        throw error;
      }
    }
  }

  if (!githubToken) {
    errors.push(getError("ENOGHTOKEN", { owner, repo }));
  }

  if (errors.length > 0) {
    throw new AggregateError(errors);
  }
}
