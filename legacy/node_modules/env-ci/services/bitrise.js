// https://devcenter.bitrise.io/builds/available-environment-variables/#exposed-by-bitriseio

export default {
  detect({ env }) {
    return Boolean(env.BITRISE_IO);
  },
  configuration({ env }) {
    const pr =
      env.BITRISE_PULL_REQUEST === "false"
        ? undefined
        : env.BITRISE_PULL_REQUEST;
    const isPr = Boolean(pr);

    return {
      name: "Bitrise",
      service: "bitrise",
      commit: env.BITRISE_GIT_COMMIT,
      tag: env.BITRISE_GIT_TAG,
      build: env.BITRISE_BUILD_NUMBER,
      buildUrl: env.BITRISE_BUILD_URL,
      branch: isPr ? env.BITRISEIO_GIT_BRANCH_DEST : env.BITRISE_GIT_BRANCH,
      pr,
      isPr,
      prBranch: isPr ? env.BITRISE_GIT_BRANCH : undefined,
      slug: env.BITRISE_APP_SLUG,
    };
  },
};
