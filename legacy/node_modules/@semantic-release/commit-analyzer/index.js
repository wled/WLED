import { isUndefined } from "lodash-es";
import { CommitParser } from "conventional-commits-parser";
import { filterRevertedCommitsSync } from "conventional-commits-filter";
import debugFactory from "debug";
import loadParserConfig from "./lib/load-parser-config.js";
import loadReleaseRules from "./lib/load-release-rules.js";
import analyzeCommit from "./lib/analyze-commit.js";
import compareReleaseTypes from "./lib/compare-release-types.js";
import RELEASE_TYPES from "./lib/default-release-types.js";
import DEFAULT_RELEASE_RULES from "./lib/default-release-rules.js";

const debug = debugFactory("semantic-release:commit-analyzer");

/**
 * Determine the type of release to create based on a list of commits.
 *
 * @param {Object} pluginConfig The plugin configuration.
 * @param {String} pluginConfig.preset conventional-changelog preset ('angular', 'atom', 'codemirror', 'ember', 'eslint', 'express', 'jquery', 'jscs', 'jshint')
 * @param {String} pluginConfig.config Requireable npm package with a custom conventional-changelog preset
 * @param {String|Array} pluginConfig.releaseRules A `String` to load an external module or an `Array` of rules.
 * @param {Object} pluginConfig.parserOpts Additional `conventional-changelog-parser` options that will overwrite ones loaded by `preset` or `config`.
 * @param {Object} context The semantic-release context.
 * @param {Array<Object>} context.commits The commits to analyze.
 * @param {String} context.cwd The current working directory.
 *
 * @returns {Promise<String|null>} the type of release to create based on the list of commits or `null` if no release has to be done.
 */
export async function analyzeCommits(pluginConfig, context) {
  const { commits, logger } = context;
  const releaseRules = await loadReleaseRules(pluginConfig, context);
  const config = await loadParserConfig(pluginConfig, context);
  let releaseType = null;

  const parser = new CommitParser(config);
  const filteredCommits = filterRevertedCommitsSync(
    commits
      .filter(({ message, hash }) => {
        if (!message.trim()) {
          debug("Skip commit %s with empty message", hash);
          return false;
        }

        return true;
      })
      .map(({ message, ...commitProps }) => ({
        rawMsg: message,
        message,
        ...commitProps,
        ...parser.parse(message),
      }))
  );

  for (const { rawMsg, ...commit } of filteredCommits) {
    logger.log(`Analyzing commit: %s`, rawMsg);
    let commitReleaseType;

    // Determine release type based on custom releaseRules
    if (releaseRules) {
      debug("Analyzing with custom rules");
      commitReleaseType = analyzeCommit(releaseRules, commit);
    }

    // If no custom releaseRules or none matched the commit, try with default releaseRules
    if (isUndefined(commitReleaseType)) {
      debug("Analyzing with default rules");
      commitReleaseType = analyzeCommit(DEFAULT_RELEASE_RULES, commit);
    }

    if (commitReleaseType) {
      logger.log("The release type for the commit is %s", commitReleaseType);
    } else {
      logger.log("The commit should not trigger a release");
    }

    // Set releaseType if commit's release type is higher
    if (commitReleaseType && compareReleaseTypes(releaseType, commitReleaseType)) {
      releaseType = commitReleaseType;
    }

    // Break loop if releaseType is the highest
    if (releaseType === RELEASE_TYPES[0]) {
      break;
    }
  }

  logger.log("Analysis of %s commits complete: %s release", commits.length, releaseType || "no");

  return releaseType;
}
