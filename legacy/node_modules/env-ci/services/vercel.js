// https://vercel.com/docs/environment-variables

export default {
  detect({ env }) {
    return Boolean(env.VERCEL) || Boolean(env.NOW_GITHUB_DEPLOYMENT);
  },
  configuration({ env }) {
    const name = "Vercel";
    const service = "vercel";

    if (env.VERCEL) {
      return {
        name,
        service,
        commit: env.VERCEL_GIT_COMMIT_SHA,
        branch: env.VERCEL_GIT_COMMIT_REF,
        slug: `${env.VERCEL_GIT_REPO_OWNER}/${env.VERCEL_GIT_REPO_SLUG}`,
      };
    }

    return {
      name,
      service,
      commit: env.NOW_GITHUB_COMMIT_SHA,
      branch: env.NOW_GITHUB_COMMIT_REF,
      slug: `${env.NOW_GITHUB_ORG}/${env.NOW_GITHUB_REPO}`,
    };
  },
};
