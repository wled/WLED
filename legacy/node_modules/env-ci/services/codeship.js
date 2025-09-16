// https://documentation.codeship.com/basic/builds-and-configuration/set-environment-variables/#default-environment-variables

export default {
  detect({ env }) {
    return env.CI_NAME && env.CI_NAME === "codeship";
  },
  configuration({ env }) {
    return {
      name: "Codeship",
      service: "codeship",
      build: env.CI_BUILD_NUMBER,
      buildUrl: env.CI_BUILD_URL,
      commit: env.CI_COMMIT_ID,
      branch: env.CI_BRANCH,
      slug: env.CI_REPO_NAME,
    };
  },
};
