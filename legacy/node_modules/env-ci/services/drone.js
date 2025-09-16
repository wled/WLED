// https://readme.drone.io/reference/environ

export default {
  detect({ env }) {
    return Boolean(env.DRONE);
  },
  configuration({ env }) {
    const isPr = env.DRONE_BUILD_EVENT === "pull_request";

    return {
      name: "Drone",
      service: "drone",
      commit: env.DRONE_COMMIT_SHA,
      tag: env.DRONE_TAG,
      build: env.DRONE_BUILD_NUMBER,
      buildUrl: env.DRONE_BUILD_LINK,
      branch: isPr ? env.DRONE_TARGET_BRANCH : env.DRONE_BRANCH,
      job: env.DRONE_JOB_NUMBER,
      jobUrl: env.DRONE_BUILD_LINK,
      pr: env.DRONE_PULL_REQUEST,
      isPr,
      prBranch: isPr ? env.DRONE_SOURCE_BRANCH : undefined,
      slug: `${env.DRONE_REPO_OWNER}/${env.DRONE_REPO_NAME}`,
      root: env.DRONE_WORKSPACE,
    };
  },
};
