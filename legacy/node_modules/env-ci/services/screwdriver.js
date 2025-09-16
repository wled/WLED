// https://docs.screwdriver.cd/user-guide/environment-variables

export default {
  detect({ env }) {
    return Boolean(env.SCREWDRIVER);
  },
  configuration({ env }) {
    const pr = env.SD_PULL_REQUEST;
    const isPr = Boolean(pr);

    return {
      name: "Screwdriver.cd",
      service: "screwdriver",
      branch: isPr ? env.PR_BASE_BRANCH_NAME : env.GIT_BRANCH,
      prBranch: isPr ? env.PR_BRANCH_NAME : undefined,
      commit: env.SD_BUILD_SHA,
      build: env.SD_BUILD_ID,
      buildUrl: env.SD_UI_BUILD_URL,
      job: env.SD_JOB_ID,
      pr,
      isPr,
      slug: env.SD_PIPELINE_NAME,
      root: env.SD_ROOT_DIR,
    };
  },
};
