// https://developers.cloudflare.com/pages/platform/build-configuration#environment-variables

export default {
  detect({ env }) {
    return env.CF_PAGES === "1";
  },
  configuration({ env }) {
    return {
      name: "Cloudflare Pages",
      service: "cloudflarePages",
      commit: env.CF_PAGES_COMMIT_SHA,
      branch: env.CF_PAGES_BRANCH,
      root: env.PWD,
    };
  },
};
