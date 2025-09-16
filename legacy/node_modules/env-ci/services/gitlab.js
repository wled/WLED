// https://docs.gitlab.com/ce/ci/variables/README.html

export default {
  detect({ env }) {
    return Boolean(env.GITLAB_CI);
  },
  configuration({ env }) {
    const pr = env.CI_MERGE_REQUEST_ID;
    const isPr = Boolean(pr);

    return {
      name: "GitLab CI/CD",
      service: "gitlab",
      commit: env.CI_COMMIT_SHA,
      tag: env.CI_COMMIT_TAG,
      build: env.CI_PIPELINE_ID,
      buildUrl: `${env.CI_PROJECT_URL}/pipelines/${env.CI_PIPELINE_ID}`,
      job: env.CI_JOB_ID,
      jobUrl: `${env.CI_PROJECT_URL}/-/jobs/${env.CI_JOB_ID}`,
      branch: isPr
        ? env.CI_MERGE_REQUEST_TARGET_BRANCH_NAME
        : env.CI_COMMIT_REF_NAME,
      pr,
      isPr,
      prBranch: env.CI_MERGE_REQUEST_SOURCE_BRANCH_NAME,
      slug: env.CI_PROJECT_PATH,
      root: env.CI_PROJECT_DIR,
    };
  },
};
