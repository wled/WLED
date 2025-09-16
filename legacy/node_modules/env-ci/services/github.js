import { readFileSync } from "node:fs";

// https://docs.github.com/en/actions/learn-github-actions/environment-variables#default-environment-variables
import { parseBranch } from "../lib/utils.js";

const getPrEvent = ({ env }) => {
  try {
    const event = env.GITHUB_EVENT_PATH
      ? JSON.parse(readFileSync(env.GITHUB_EVENT_PATH, "utf-8"))
      : undefined;

    if (event && event.pull_request) {
      return {
        branch: event.pull_request.base
          ? parseBranch(event.pull_request.base.ref)
          : undefined,
        pr: event.pull_request.number,
      };
    }
  } catch {
    // Noop
  }

  return { pr: undefined, branch: undefined };
};

const getPrNumber = (env) => {
  const event = env.GITHUB_EVENT_PATH
    ? JSON.parse(readFileSync(env.GITHUB_EVENT_PATH, "utf-8"))
    : undefined;

  return event && event.pull_request ? event.pull_request.number : undefined;
};

export default {
  detect({ env }) {
    return Boolean(env.GITHUB_ACTIONS);
  },
  configuration({ env, cwd }) {
    const isPr =
      env.GITHUB_EVENT_NAME === "pull_request" ||
      env.GITHUB_EVENT_NAME === "pull_request_target";
    const branch = parseBranch(
      env.GITHUB_EVENT_NAME === "pull_request_target"
        ? `refs/pull/${getPrNumber(env)}/merge`
        : env.GITHUB_REF,
    );

    return {
      name: "GitHub Actions",
      service: "github",
      commit: env.GITHUB_SHA,
      build: env.GITHUB_RUN_ID,
      isPr,
      branch,
      prBranch: isPr ? branch : undefined,
      slug: env.GITHUB_REPOSITORY,
      root: env.GITHUB_WORKSPACE,
      ...(isPr ? getPrEvent({ env, cwd }) : undefined),
    };
  },
};
