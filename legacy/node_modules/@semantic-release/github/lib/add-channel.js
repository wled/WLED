import debugFactory from "debug";

import { RELEASE_NAME } from "./definitions/constants.js";
import parseGithubUrl from "./parse-github-url.js";
import resolveConfig from "./resolve-config.js";
import isPrerelease from "./is-prerelease.js";
import { toOctokitOptions } from "./octokit.js";

const debug = debugFactory("semantic-release:github");

export default async function addChannel(pluginConfig, context, { Octokit }) {
  const {
    options: { repositoryUrl },
    branch,
    nextRelease: { name, gitTag, notes },
    logger,
  } = context;
  const { githubToken, githubUrl, githubApiPathPrefix, githubApiUrl, proxy } =
    resolveConfig(pluginConfig, context);
  const { owner, repo } = parseGithubUrl(repositoryUrl);
  const octokit = new Octokit(
    toOctokitOptions({
      githubToken,
      githubUrl,
      githubApiPathPrefix,
      githubApiUrl,
      proxy,
    }),
  );
  let releaseId;

  const release = {
    owner,
    repo,
    name,
    prerelease: isPrerelease(branch),
    tag_name: gitTag,
  };

  debug("release object: %O", release);

  try {
    ({
      data: { id: releaseId },
    } = await octokit.request("GET /repos/{owner}/{repo}/releases/tags/{tag}", {
      owner,
      repo,
      tag: gitTag,
    }));
  } catch (error) {
    if (error.status === 404) {
      logger.log("There is no release for tag %s, creating a new one", gitTag);

      const {
        data: { html_url: url },
      } = await octokit.request("POST /repos/{owner}/{repo}/releases", {
        ...release,
        body: notes,
      });

      logger.log("Published GitHub release: %s", url);
      return { url, name: RELEASE_NAME };
    }

    throw error;
  }

  debug("release release_id: %o", releaseId);

  const {
    data: { html_url: url },
  } = await octokit.request(
    "PATCH /repos/{owner}/{repo}/releases/{release_id}",
    { ...release, release_id: releaseId },
  );

  logger.log("Updated GitHub release: %s", url);

  return { url, name: RELEASE_NAME };
}
