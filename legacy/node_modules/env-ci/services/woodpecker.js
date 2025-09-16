// https://woodpecker-ci.org/docs/usage/environment#built-in-environment-variables

export default {
  detect({ env }) {
    return env.CI && env.CI === "woodpecker";
  },
  configuration({ env }) {
    const isPr = env.CI_PIPELINE_EVENT === "pull_request";

    return {
      name: "Woodpecker CI",
      service: "woodpecker",
      commit: env.CI_COMMIT_SHA,
      tag: env.CI_COMMIT_TAG,
      build: env.CI_PIPELINE_NUMBER,
      buildUrl: env.CI_PIPELINE_URL,
      branch: isPr ? env.CI_COMMIT_TARGET_BRANCH : env.CI_COMMIT_BRANCH,
      job: env.CI_STEP_NUMBER,
      jobUrl: env.CI_STEP_URL,
      pr: env.CI_COMMIT_PULL_REQUEST,
      isPr,
      prBranch: isPr ? env.CI_COMMIT_SOURCE_BRANCH : undefined,
      slug: `${env.CI_REPO_OWNER}/${env.CI_REPO_NAME}`,
      root: env.CI_WORKSPACE,
    };
  },
};
