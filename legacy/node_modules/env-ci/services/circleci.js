// https://circleci.com/docs/2.0/env-vars/#built-in-environment-variables

import { prNumber } from "../lib/utils.js";

export default {
  detect({ env }) {
    return Boolean(env.CIRCLECI);
  },
  configuration({ env }) {
    const pr =
      env.CIRCLE_PR_NUMBER ||
      prNumber(env.CIRCLE_PULL_REQUEST || env.CI_PULL_REQUEST);
    const isPr = Boolean(pr);

    return {
      name: "CircleCI",
      service: "circleci",
      build: env.CIRCLE_BUILD_NUM,
      buildUrl: env.CIRCLE_BUILD_URL,
      job: `${env.CIRCLE_BUILD_NUM}.${env.CIRCLE_NODE_INDEX}`,
      commit: env.CIRCLE_SHA1,
      tag: env.CIRCLE_TAG,
      branch: isPr ? undefined : env.CIRCLE_BRANCH,
      pr,
      isPr,
      prBranch: isPr ? env.CIRCLE_BRANCH : undefined,
      slug: `${env.CIRCLE_PROJECT_USERNAME}/${env.CIRCLE_PROJECT_REPONAME}`,
    };
  },
};
