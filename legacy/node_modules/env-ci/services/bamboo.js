// https://confluence.atlassian.com/bamboo/bamboo-variables-289277087.html

export default {
  detect({ env }) {
    return Boolean(env.bamboo_agentId);
  },
  configuration({ env }) {
    return {
      name: "Bamboo",
      service: "bamboo",
      commit: env.bamboo_planRepository_1_revision,
      build: env.bamboo_buildNumber,
      buildUrl: env.bamboo_buildResultsUrl,
      branch: env.bamboo_planRepository_1_branchName,
      job: env.bamboo_buildKey,
      root: env.bamboo_build_working_directory,
    };
  },
};
