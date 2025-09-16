// https://docs.netlify.com/configure-builds/environment-variables/#netlify-configuration-variables

export default {
  detect({ env }) {
    return env.NETLIFY === "true";
  },
  configuration({ env }) {
    const isPr = env.PULL_REQUEST === "true";

    return {
      name: "Netlify",
      service: "netlify",
      commit: env.COMMIT_REF,
      build: env.DEPLOY_ID,
      buildUrl: `https://app.netlify.com/sites/${env.SITE_NAME}/deploys/${env.DEPLOY_ID}`,
      branch: isPr ? undefined : env.HEAD,
      pr: env.REVIEW_ID,
      isPr,
      prBranch: isPr ? env.HEAD : undefined,
      slug: env.REPOSITORY_URL.match(/[^/:]+\/[^/]+?$/)[0],
      root: env.PWD,
    };
  },
};
