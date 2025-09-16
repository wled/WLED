// https://cirrus-ci.org/guide/writing-tasks/#environment-variables

const CIRRUS_CI_DASHBOARD = "https://cirrus-ci.com";

export default {
  detect({ env }) {
    return Boolean(env.CIRRUS_CI);
  },
  configuration({ env }) {
    const pr = env.CIRRUS_PR;
    const isPr = Boolean(pr);

    return {
      name: "Cirrus CI",
      service: "cirrus",
      commit: env.CIRRUS_CHANGE_IN_REPO,
      tag: env.CIRRUS_TAG,
      build: env.CIRRUS_BUILD_ID,
      buildUrl: `${CIRRUS_CI_DASHBOARD}/build/${env.CIRRUS_BUILD_ID}`,
      job: env.CIRRUS_TASK_ID,
      jobUrl: `${CIRRUS_CI_DASHBOARD}/task/${env.CIRRUS_TASK_ID}`,
      branch: isPr ? env.CIRRUS_BASE_BRANCH : env.CIRRUS_BRANCH,
      pr,
      isPr,
      prBranch: isPr ? env.CIRRUS_BRANCH : undefined,
      slug: env.CIRRUS_REPO_FULL_NAME,
      root: env.CIRRUS_WORKING_DIR,
    };
  },
};
