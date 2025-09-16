/* eslint require-atomic-updates: off */

import { defaultTo, castArray } from "lodash-es";

import verifyGitHub from "./lib/verify.js";
import addChannelGitHub from "./lib/add-channel.js";
import publishGitHub from "./lib/publish.js";
import successGitHub from "./lib/success.js";
import failGitHub from "./lib/fail.js";
import { SemanticReleaseOctokit } from "./lib/octokit.js";

let verified;

export async function verifyConditions(
  pluginConfig,
  context,
  { Octokit = SemanticReleaseOctokit } = {},
) {
  const { options } = context;
  // If the GitHub publish plugin is used and has `assets`, `successComment`, `failComment`, `failTitle`, `labels`, `discussionCategoryName` or `assignees` configured, validate it now in order to prevent any release if the configuration is wrong
  if (options.publish) {
    const publishPlugin =
      castArray(options.publish).find(
        (config) => config.path && config.path === "@semantic-release/github",
      ) || {};

    pluginConfig.assets = defaultTo(pluginConfig.assets, publishPlugin.assets);
    pluginConfig.successComment = defaultTo(
      pluginConfig.successComment,
      publishPlugin.successComment,
    );
    pluginConfig.failComment = defaultTo(
      pluginConfig.failComment,
      publishPlugin.failComment,
    );
    pluginConfig.failTitle = defaultTo(
      pluginConfig.failTitle,
      publishPlugin.failTitle,
    );
    pluginConfig.labels = defaultTo(pluginConfig.labels, publishPlugin.labels);
    pluginConfig.assignees = defaultTo(
      pluginConfig.assignees,
      publishPlugin.assignees,
    );
    pluginConfig.discussionCategoryName = defaultTo(
      pluginConfig.discussionCategoryName,
      publishPlugin.discussionCategoryName,
    );
  }

  await verifyGitHub(pluginConfig, context, { Octokit });
  verified = true;
}

export async function publish(
  pluginConfig,
  context,
  { Octokit = SemanticReleaseOctokit } = {},
) {
  if (!verified) {
    await verifyGitHub(pluginConfig, context, { Octokit });
    verified = true;
  }

  return publishGitHub(pluginConfig, context, { Octokit });
}

export async function addChannel(
  pluginConfig,
  context,
  { Octokit = SemanticReleaseOctokit } = {},
) {
  if (!verified) {
    await verifyGitHub(pluginConfig, context, { Octokit });
    verified = true;
  }

  return addChannelGitHub(pluginConfig, context, { Octokit });
}

export async function success(
  pluginConfig,
  context,
  { Octokit = SemanticReleaseOctokit } = {},
) {
  if (!verified) {
    await verifyGitHub(pluginConfig, context, { Octokit });
    verified = true;
  }

  await successGitHub(pluginConfig, context, { Octokit });
}

export async function fail(
  pluginConfig,
  context,
  { Octokit = SemanticReleaseOctokit } = {},
) {
  if (!verified) {
    await verifyGitHub(pluginConfig, context, { Octokit });
    verified = true;
  }

  await failGitHub(pluginConfig, context, { Octokit });
}
