// https://buildkite.com/docs/builds/environment-variables
import { getSlugFromGitURL } from "../lib/git.js";

export default {
  detect({ env }) {
    return Boolean(env.BUILDKITE);
  },
  configuration({ env }) {
    const pr =
      env.BUILDKITE_PULL_REQUEST === "false"
        ? undefined
        : env.BUILDKITE_PULL_REQUEST;
    const isPr = Boolean(pr);

    return {
      name: "Buildkite",
      service: "buildkite",
      build: env.BUILDKITE_BUILD_NUMBER,
      buildUrl: env.BUILDKITE_BUILD_URL,
      commit: env.BUILDKITE_COMMIT,
      tag: env.BUILDKITE_TAG,
      branch: isPr
        ? env.BUILDKITE_PULL_REQUEST_BASE_BRANCH
        : env.BUILDKITE_BRANCH,
      slug: getSlugFromGitURL(env.BUILDKITE_REPO),
      pr,
      isPr,
      prBranch: isPr ? env.BUILDKITE_BRANCH : undefined,
      root: env.BUILDKITE_BUILD_CHECKOUT_PATH,
    };
  },
};
