import { resolve, basename, extname } from "node:path";
import { stat, readFile } from "node:fs/promises";

import { isPlainObject, template } from "lodash-es";
import mime from "mime";
import debugFactory from "debug";

import { RELEASE_NAME } from "./definitions/constants.js";
import parseGithubUrl from "./parse-github-url.js";
import globAssets from "./glob-assets.js";
import resolveConfig from "./resolve-config.js";
import { toOctokitOptions } from "./octokit.js";
import isPrerelease from "./is-prerelease.js";

const debug = debugFactory("semantic-release:github");

export default async function publish(pluginConfig, context, { Octokit }) {
  const {
    cwd,
    options: { repositoryUrl },
    branch,
    nextRelease: { gitTag },
    logger,
  } = context;
  const {
    githubToken,
    githubUrl,
    githubApiPathPrefix,
    githubApiUrl,
    proxy,
    assets,
    draftRelease,
    releaseNameTemplate,
    releaseBodyTemplate,
    discussionCategoryName,
  } = resolveConfig(pluginConfig, context);
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
  const release = {
    owner,
    repo,
    tag_name: gitTag,
    target_commitish: branch.name,
    name: template(releaseNameTemplate)(context),
    body: template(releaseBodyTemplate)(context),
    prerelease: isPrerelease(branch),
  };

  debug("release object: %O", release);

  const draftReleaseOptions = { ...release, draft: true };

  // When there are no assets, we publish a release directly.
  if (!assets || assets.length === 0) {
    // If draftRelease is true we create a draft release instead.
    if (draftRelease) {
      const {
        data: { html_url: url, id: releaseId },
      } = await octokit.request(
        "POST /repos/{owner}/{repo}/releases",
        draftReleaseOptions,
      );

      logger.log("Created GitHub draft release: %s", url);
      return { url, name: RELEASE_NAME, id: releaseId };
    }

    // add discussion_category_name if discussionCategoryName is not undefined or false
    if (discussionCategoryName) {
      release.discussion_category_name = discussionCategoryName;
    }

    const {
      data: { html_url: url, id: releaseId, discussion_url },
    } = await octokit.request("POST /repos/{owner}/{repo}/releases", release);

    logger.log("Published GitHub release: %s", url);

    if (discussionCategoryName) {
      logger.log("Created GitHub release discussion: %s", discussion_url);
    }

    return { url, name: RELEASE_NAME, id: releaseId, discussion_url };
  }

  // We'll create a draft release, append the assets to it, and then publish it.
  // This is so that the assets are available when we get a Github release event.
  const {
    data: { upload_url: uploadUrl, html_url: draftUrl, id: releaseId },
  } = await octokit.request(
    "POST /repos/{owner}/{repo}/releases",
    draftReleaseOptions,
  );

  // Append assets to the release
  const globbedAssets = await globAssets(context, assets);
  debug("globed assets: %o", globbedAssets);

  await Promise.all(
    globbedAssets.map(async (asset) => {
      const filePath = isPlainObject(asset) ? asset.path : asset;
      let file;

      try {
        file = await stat(resolve(cwd, filePath));
      } catch {
        logger.error(
          "The asset %s cannot be read, and will be ignored.",
          filePath,
        );
        return;
      }

      if (!file || !file.isFile()) {
        logger.error(
          "The asset %s is not a file, and will be ignored.",
          filePath,
        );
        return;
      }

      const fileName = template(asset.name || basename(filePath))(context);
      const upload = {
        method: "POST",
        url: uploadUrl,
        data: await readFile(resolve(cwd, filePath)),
        name: fileName,
        headers: {
          "content-type": mime.getType(extname(fileName)) || "text/plain",
          "content-length": file.size,
        },
      };

      debug("file path: %o", filePath);
      debug("file name: %o", fileName);

      if (isPlainObject(asset) && asset.label) {
        upload.label = template(asset.label)(context);
      }

      const {
        data: { browser_download_url: downloadUrl },
      } = await octokit.request(upload);
      logger.log("Published file %s", downloadUrl);
    }),
  );

  // If we want to create a draft we don't need to update the release again
  if (draftRelease) {
    logger.log("Created GitHub draft release: %s", draftUrl);
    return { url: draftUrl, name: RELEASE_NAME, id: releaseId };
  }

  const patchRelease = {
    owner,
    repo,
    release_id: releaseId,
    draft: false,
  };

  // add discussion_category_name if discussionCategoryName is not undefined or false
  if (discussionCategoryName) {
    patchRelease.discussion_category_name = discussionCategoryName;
  }

  const {
    data: { html_url: url, discussion_url },
  } = await octokit.request(
    "PATCH /repos/{owner}/{repo}/releases/{release_id}",
    patchRelease,
  );

  logger.log("Published GitHub release: %s", url);

  if (discussionCategoryName) {
    logger.log("Created GitHub release discussion: %s", discussion_url);
  }

  return { url, name: RELEASE_NAME, id: releaseId, discussion_url };
}
