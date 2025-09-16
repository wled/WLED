import { dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { isUndefined } from "lodash-es";
import importFrom from "import-from-esm";
import RELEASE_TYPES from "./default-release-types.js";

/**
 * Load and validate the `releaseRules` rules.
 *
 * If `releaseRules` parameter is a `string` then load it as an external module with `require`.
 * Verifies that the loaded/parameter `releaseRules` is an `Array` and each element has a valid `release` attribute.
 *
 * @param {Object} pluginConfig The plugin configuration.
 * @param {String|Array} pluginConfig.releaseRules A `String` to load an external module or an `Array` of rules.
 * @param {Object} context The semantic-release context.
 * @param {String} context.cwd The current working directory.
 *
 * @return {Promise<Array>} the loaded and validated `releaseRules`.
 */
export default async ({ releaseRules }, { cwd }) => {
  let loadedReleaseRules;
  const __dirname = dirname(fileURLToPath(import.meta.url));

  if (releaseRules) {
    loadedReleaseRules =
      typeof releaseRules === "string"
        ? (await importFrom.silent(__dirname, releaseRules)) || (await importFrom(cwd, releaseRules))
        : releaseRules;

    if (!Array.isArray(loadedReleaseRules)) {
      throw new TypeError('Error in commit-analyzer configuration: "releaseRules" must be an array of rules');
    }

    loadedReleaseRules.forEach((rule) => {
      if (!rule || isUndefined(rule.release)) {
        throw new Error('Error in commit-analyzer configuration: rules must be an object with a "release" property');
      } else if (!RELEASE_TYPES.includes(rule.release) && rule.release !== null && rule.release !== false) {
        throw new Error(
          `Error in commit-analyzer configuration: "${
            rule.release
          }" is not a valid release type. Valid values are: ${JSON.stringify(RELEASE_TYPES)}`
        );
      }
    });
  }

  return loadedReleaseRules;
};
