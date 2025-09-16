import BottleneckLight from "bottleneck/light.js";
import { VERSION } from "./version.js";
import { wrapRequest } from "./wrap-request.js";
import triggersNotificationPaths from "./generated/triggers-notification-paths.js";
import { routeMatcher } from "./route-matcher.js";
const regex = routeMatcher(triggersNotificationPaths);
const triggersNotification = regex.test.bind(regex);
const groups = {};
const createGroups = function(Bottleneck, common) {
  groups.global = new Bottleneck.Group({
    id: "octokit-global",
    maxConcurrent: 10,
    ...common
  });
  groups.search = new Bottleneck.Group({
    id: "octokit-search",
    maxConcurrent: 1,
    minTime: 2e3,
    ...common
  });
  groups.write = new Bottleneck.Group({
    id: "octokit-write",
    maxConcurrent: 1,
    minTime: 1e3,
    ...common
  });
  groups.notifications = new Bottleneck.Group({
    id: "octokit-notifications",
    maxConcurrent: 1,
    minTime: 3e3,
    ...common
  });
};
function throttling(octokit, octokitOptions) {
  const {
    enabled = true,
    Bottleneck = BottleneckLight,
    id = "no-id",
    timeout = 1e3 * 60 * 2,
    // Redis TTL: 2 minutes
    connection
  } = octokitOptions.throttle || {};
  if (!enabled) {
    return {};
  }
  const common = { timeout };
  if (typeof connection !== "undefined") {
    common.connection = connection;
  }
  if (groups.global == null) {
    createGroups(Bottleneck, common);
  }
  const state = Object.assign(
    {
      clustering: connection != null,
      triggersNotification,
      fallbackSecondaryRateRetryAfter: 60,
      retryAfterBaseValue: 1e3,
      retryLimiter: new Bottleneck(),
      id,
      ...groups
    },
    octokitOptions.throttle
  );
  if (typeof state.onSecondaryRateLimit !== "function" || typeof state.onRateLimit !== "function") {
    throw new Error(`octokit/plugin-throttling error:
        You must pass the onSecondaryRateLimit and onRateLimit error handlers.
        See https://octokit.github.io/rest.js/#throttling

        const octokit = new Octokit({
          throttle: {
            onSecondaryRateLimit: (retryAfter, options) => {/* ... */},
            onRateLimit: (retryAfter, options) => {/* ... */}
          }
        })
    `);
  }
  const events = {};
  const emitter = new Bottleneck.Events(events);
  events.on("secondary-limit", state.onSecondaryRateLimit);
  events.on("rate-limit", state.onRateLimit);
  events.on(
    "error",
    (e) => octokit.log.warn("Error in throttling-plugin limit handler", e)
  );
  state.retryLimiter.on("failed", async function(error, info) {
    const [state2, request, options] = info.args;
    const { pathname } = new URL(options.url, "http://github.test");
    const shouldRetryGraphQL = pathname.startsWith("/graphql") && error.status !== 401;
    if (!(shouldRetryGraphQL || error.status === 403 || error.status === 429)) {
      return;
    }
    const retryCount = ~~request.retryCount;
    request.retryCount = retryCount;
    options.request.retryCount = retryCount;
    const { wantRetry, retryAfter = 0 } = await async function() {
      if (/\bsecondary rate\b/i.test(error.message)) {
        const retryAfter2 = Number(error.response.headers["retry-after"]) || state2.fallbackSecondaryRateRetryAfter;
        const wantRetry2 = await emitter.trigger(
          "secondary-limit",
          retryAfter2,
          options,
          octokit,
          retryCount
        );
        return { wantRetry: wantRetry2, retryAfter: retryAfter2 };
      }
      if (error.response.headers != null && error.response.headers["x-ratelimit-remaining"] === "0" || (error.response.data?.errors ?? []).some(
        (error2) => error2.type === "RATE_LIMITED"
      )) {
        const rateLimitReset = new Date(
          ~~error.response.headers["x-ratelimit-reset"] * 1e3
        ).getTime();
        const retryAfter2 = Math.max(
          // Add one second so we retry _after_ the reset time
          // https://docs.github.com/en/rest/overview/resources-in-the-rest-api?apiVersion=2022-11-28#exceeding-the-rate-limit
          Math.ceil((rateLimitReset - Date.now()) / 1e3) + 1,
          0
        );
        const wantRetry2 = await emitter.trigger(
          "rate-limit",
          retryAfter2,
          options,
          octokit,
          retryCount
        );
        return { wantRetry: wantRetry2, retryAfter: retryAfter2 };
      }
      return {};
    }();
    if (wantRetry) {
      request.retryCount++;
      return retryAfter * state2.retryAfterBaseValue;
    }
  });
  octokit.hook.wrap("request", wrapRequest.bind(null, state));
  return {};
}
throttling.VERSION = VERSION;
throttling.triggersNotification = triggersNotification;
export {
  throttling
};
