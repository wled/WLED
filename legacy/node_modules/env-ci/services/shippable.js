// http://docs.shippable.com/ci/env-vars/#stdEnv

export default {
  detect({ env }) {
    return Boolean(env.SHIPPABLE);
  },
  configuration({ env }) {
    const pr = env.IS_PULL_REQUEST === "true" ? env.PULL_REQUEST : undefined;
    const isPr = Boolean(pr);

    return {
      name: "Shippable",
      service: "shippable",
      commit: env.COMMIT,
      tag: env.GIT_TAG_NAME,
      build: env.BUILD_NUMBER,
      buildUrl: env.BUILD_URL,
      branch: isPr ? env.BASE_BRANCH : env.BRANCH,
      job: env.JOB_NUMBER,
      pr,
      isPr,
      prBranch: isPr ? env.HEAD_BRANCH : undefined,
      slug: env.SHIPPABLE_REPO_SLUG,
      root: env.SHIPPABLE_BUILD_DIR,
    };
  },
};
