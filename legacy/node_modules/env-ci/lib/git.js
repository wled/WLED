import { execaSync } from "execa";

export function head(options) {
  try {
    return execaSync("git", ["rev-parse", "HEAD"], options).stdout;
  } catch {
    return undefined;
  }
}

export function branch(options) {
  try {
    const headRef = execaSync(
      "git",
      ["rev-parse", "--abbrev-ref", "HEAD"],
      options,
    ).stdout;

    if (headRef === "HEAD") {
      const branch = execaSync(
        "git",
        ["show", "-s", "--pretty=%d", "HEAD"],
        options,
      )
        .stdout.replace(/^\(|\)$/g, "")
        .split(", ")
        .find((branch) => branch.startsWith("origin/"));
      return branch ? branch.match(/^origin\/(?<branch>.+)/)[1] : undefined;
    }

    return headRef;
  } catch {
    return undefined;
  }
}

// Match slug on SSH URLs (ex: `USER@HOST:PORT/ORG/REPO.git`)
const GIT_SSH_URL_SLUG_PATTERN = /^(?:.*)@(?:.*):(?:\d+\/)?(.*)\.git$/;

// Match slug on HTTP(S) URLs `https://github.com/semantic-release/env-ci.git`
const GIT_PATHNAME_SLUG_PATTERN = /^\/(.*)\.git$/;

/**
 * Extract repository slug(owner/repo) from a repository URL
 *
 * @param {String} repositoryURL
 * @returns {String | undefined}
 */
export function getSlugFromGitURL(repositoryURL) {
  if (!repositoryURL) {
    return undefined;
  }

  if (repositoryURL.match(GIT_SSH_URL_SLUG_PATTERN)) {
    return repositoryURL.replace(GIT_SSH_URL_SLUG_PATTERN, "$1");
  }

  try {
    const url = new URL(repositoryURL);
    return url.pathname.replace(GIT_PATHNAME_SLUG_PATTERN, "$1");
  } catch {
    return undefined;
  }
}
