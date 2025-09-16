import { head } from "../lib/git.js";

// 1.0: https://semaphoreci.com/docs/available-environment-variables.html
// 2.0: https://docs.semaphoreci.com/article/12-environment-variables

export default {
  detect({ env }) {
    return Boolean(env.SEMAPHORE);
  },
  configuration({ env, cwd }) {
    const pr = env.SEMAPHORE_GIT_PR_NUMBER || env.PULL_REQUEST_NUMBER;
    const isPr = Boolean(pr);

    return {
      name: "Semaphore",
      service: "semaphore",
      commit: env.SEMAPHORE_GIT_SHA || head({ env, cwd }),
      tag: env.SEMAPHORE_GIT_TAG_NAME,
      build: env.SEMAPHORE_JOB_ID || env.SEMAPHORE_BUILD_NUMBER,
      branch: env.SEMAPHORE_GIT_BRANCH || (isPr ? undefined : env.BRANCH_NAME),
      pr,
      isPr,
      prBranch:
        env.SEMAPHORE_GIT_PR_BRANCH || (isPr ? env.BRANCH_NAME : undefined),
      slug: env.SEMAPHORE_GIT_REPO_SLUG || env.SEMAPHORE_REPO_SLUG,
      root: env.SEMAPHORE_GIT_DIR || env.SEMAPHORE_PROJECT_DIR,
    };
  },
};
