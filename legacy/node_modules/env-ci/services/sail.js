// https://sail.ci/docs/environment-variables

export default {
  detect({ env }) {
    return Boolean(env.SAILCI);
  },
  configuration({ env }) {
    const pr = env.SAIL_PULL_REQUEST_NUMBER;
    const isPr = Boolean(pr);

    return {
      name: "Sail CI",
      service: "sail",
      commit: env.SAIL_COMMIT_SHA,
      branch: isPr ? undefined : env.SAIL_COMMIT_BRANCH,
      pr,
      isPr,
      slug: `${env.SAIL_REPO_OWNER}/${env.SAIL_REPO_NAME}`,
      root: env.SAIL_CLONE_DIR,
    };
  },
};
