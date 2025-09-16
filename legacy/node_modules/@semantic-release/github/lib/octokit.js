/* c8 ignore start */
// @ts-check

import { createRequire } from "node:module";

// If maintaining @octokit/core and the separate plugins gets to cumbersome
// then the `octokit` package can be used which has all these plugins included.
// However the `octokit` package has a lot of other things we don't care about.
// We use only the bits we need to minimize the size of the package.
import { Octokit } from "@octokit/core";
import { paginateRest } from "@octokit/plugin-paginate-rest";
import { retry } from "@octokit/plugin-retry";
import { throttling } from "@octokit/plugin-throttling";
import urljoin from "url-join";
import { HttpProxyAgent } from "http-proxy-agent";
import { HttpsProxyAgent } from "https-proxy-agent";

import { RETRY_CONF } from "./definitions/retry.js";
import { THROTTLE_CONF } from "./definitions/throttle.js";

// NOTE: replace with import ... assert { type: 'json' } once supported
const require = createRequire(import.meta.url);
const pkg = require("../package.json");

const onRetry = (retryAfter, options, octokit, retryCount) => {
  octokit.log.warn(
    `Request quota exhausted for request ${options.method} ${options.url}`,
  );

  if (retryCount <= RETRY_CONF.retries) {
    octokit.log.debug(`Will retry after ${retryAfter}.`);
    return true;
  }
};

export const SemanticReleaseOctokit = Octokit.plugin(
  paginateRest,
  retry,
  throttling,
).defaults({
  userAgent: `@semantic-release/github v${pkg.version}`,
  retry: RETRY_CONF,
  throttle: {
    ...THROTTLE_CONF,
    onRateLimit: onRetry,
    onSecondaryRateLimit: onRetry,
  },
});
/* c8 ignore stop */

/**
 * @param {{githubToken: string, proxy: any} | {githubUrl: string, githubApiPathPrefix: string, githubApiUrl: string,githubToken: string, proxy: any}} options
 * @returns {{ auth: string, baseUrl?: string, request: { agent?: any } }}
 */
export function toOctokitOptions(options) {
  const baseUrl =
    "githubApiUrl" in options && options.githubApiUrl
      ? // Use `urljoin` to normalize the provided URL
        urljoin(options.githubApiUrl, "")
      : "githubUrl" in options && options.githubUrl
        ? urljoin(options.githubUrl, options.githubApiPathPrefix)
        : undefined;

  const agent = options.proxy
    ? baseUrl && new URL(baseUrl).protocol.replace(":", "") === "http"
      ? // Some `proxy.headers` need to be passed as second arguments since version 6 or 7
        // For simplicity, we just pass the same proxy object twice. It works 🤷🏻
        new HttpProxyAgent(options.proxy, options.proxy)
      : new HttpsProxyAgent(options.proxy, options.proxy)
    : undefined;

  return {
    ...(baseUrl ? { baseUrl } : {}),
    auth: options.githubToken,
    request: {
      agent,
    },
  };
}
