// https://codefresh.io/docs/docs/codefresh-yaml/variables#system-provided-variables

export default {
  detect({ env }) {
    return Boolean(env.CF_BUILD_ID);
  },
  configuration({ env }) {
    const pr = env.CF_PULL_REQUEST_NUMBER;
    const isPr = Boolean(pr);

    return {
      name: "Codefresh",
      service: "codefresh",
      commit: env.CF_REVISION,
      build: env.CF_BUILD_ID,
      buildUrl: env.CF_BUILD_URL,
      branch: isPr ? env.CF_PULL_REQUEST_TARGET : env.CF_BRANCH,
      pr,
      isPr,
      prBranch: isPr ? env.CF_BRANCH : undefined,
      slug: `${env.CF_REPO_OWNER}/${env.CF_REPO_NAME}`,
      root: env.CF_VOLUME_PATH,
    };
  },
};
