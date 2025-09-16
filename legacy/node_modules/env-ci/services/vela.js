// https://go-vela.github.io/docs/reference/environment/variables/

export default {
  detect({ env }) {
    return Boolean(env.VELA);
  },
  configuration({ env }) {
    const isPr = env.VELA_BUILD_EVENT === "pull_request";

    return {
      name: "Vela",
      service: "vela",
      branch: isPr ? env.VELA_PULL_REQUEST_TARGET : env.VELA_BUILD_BRANCH,
      commit: env.VELA_BUILD_COMMIT,
      tag: env.VELA_BUILD_TAG,
      build: env.VELA_BUILD_NUMBER,
      buildUrl: env.VELA_BUILD_LINK,
      job: undefined,
      jobUrl: undefined,
      isPr,
      pr: env.VELA_BUILD_PULL_REQUEST,
      prBranch: env.VELA_PULL_REQUEST_SOURCE,
      slug: env.VELA_REPO_FULL_NAME,
      root: env.VELA_BUILD_WORKSPACE,
    };
  },
};
