// https://scrutinizer-ci.com/docs/build/environment-variables

export default {
  detect({ env }) {
    return Boolean(env.SCRUTINIZER);
  },
  configuration({ env }) {
    const pr = env.SCRUTINIZER_PR_NUMBER;
    const isPr = Boolean(pr);

    return {
      name: "Scrutinizer",
      service: "scrutinizer",
      commit: env.SCRUTINIZER_SHA1,
      build: env.SCRUTINIZER_INSPECTION_UUID,
      branch: env.SCRUTINIZER_BRANCH,
      pr,
      isPr,
      prBranch: env.SCRUTINIZER_PR_SOURCE_BRANCH,
    };
  },
};
