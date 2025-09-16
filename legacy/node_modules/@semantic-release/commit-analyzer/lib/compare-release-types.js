import RELEASE_TYPES from "./default-release-types.js";

/**
 * Test if a realease type is of higher level than a given one.
 *
 * @param {string} currentReleaseType the current release type.
 * @param {string} releaseType the release type to compare with.
 * @return {Boolean} true if `releaseType` is higher than `currentReleaseType`.
 */
export default (currentReleaseType, releaseType) =>
  !currentReleaseType || RELEASE_TYPES.indexOf(releaseType) < RELEASE_TYPES.indexOf(currentReleaseType);
