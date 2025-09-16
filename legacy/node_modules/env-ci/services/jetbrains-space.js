// https://jetbrains.com/help/space/automation-environment-variables.html#automation
import { parseBranch } from "../lib/utils.js";

export default {
  detect({ env }) {
    return Boolean(env.JB_SPACE_EXECUTION_NUMBER);
  },
  configuration({ env }) {
    const projectKey = env.JB_SPACE_PROJECT_KEY;
    const repositoryName = env.JB_SPACE_GIT_REPOSITORY_NAME;
    return {
      name: "JetBrains Space",
      service: "jetbrainsSpace",
      commit: env.JB_SPACE_GIT_REVISION,
      build: env.JB_SPACE_EXECUTION_NUMBER,
      branch: parseBranch(env.JB_SPACE_GIT_BRANCH),
      slug:
        projectKey && repositoryName
          ? `${projectKey.toLowerCase()}/${repositoryName}`
          : undefined,
    };
  },
};
