import { head, branch } from "../lib/git.js";

// https://docs.aws.amazon.com/codebuild/latest/userguide/build-env-ref-env-vars.html

export default {
  detect({ env }) {
    return Boolean(env.CODEBUILD_BUILD_ID);
  },
  configuration({ env, cwd }) {
    return {
      name: "AWS CodeBuild",
      service: "codebuild",
      commit: head({ env, cwd }),
      build: env.CODEBUILD_BUILD_ID,
      branch: branch({ env, cwd }),
      buildUrl: `https://console.aws.amazon.com/codebuild/home?region=${env.AWS_REGION}#/builds/${env.CODEBUILD_BUILD_ID}/view/new`,
      root: env.PWD,
    };
  },
};
