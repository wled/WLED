import { template } from "lodash-es";
import debugFactory from "debug";

import parseGithubUrl from "./parse-github-url.js";
import { ISSUE_ID, RELEASE_FAIL_LABEL } from "./definitions/constants.js";
import resolveConfig from "./resolve-config.js";
import { toOctokitOptions } from "./octokit.js";
import findSRIssues from "./find-sr-issues.js";
import getFailComment from "./get-fail-comment.js";

const debug = debugFactory("semantic-release:github");

export default async function fail(pluginConfig, context, { Octokit }) {
  const {
    options: { repositoryUrl },
    branch,
    errors,
    logger,
  } = context;
  const {
    githubToken,
    githubUrl,
    githubApiPathPrefix,
    githubApiUrl,
    proxy,
    failTitle,
    failComment,
    failCommentCondition,
    labels,
    assignees,
  } = resolveConfig(pluginConfig, context);

  if (failComment === false || failTitle === false) {
    logger.log("Skip issue creation.");
    logger.warn(
      `DEPRECATION: 'false' for 'failComment' or 'failTitle' is deprecated and will be removed in a future major version. Use 'failCommentCondition' instead.`,
    );
  } else if (failCommentCondition === false) {
    logger.log("Skip issue creation.");
  } else {
    const octokit = new Octokit(
      toOctokitOptions({
        githubToken,
        githubUrl,
        githubApiPathPrefix,
        githubApiUrl,
        proxy,
      }),
    );
    // In case the repo changed name, get the new `repo`/`owner` as the search API will not follow redirects
    const { data: repoData } = await octokit.request(
      "GET /repos/{owner}/{repo}",
      parseGithubUrl(repositoryUrl),
    );
    const [owner, repo] = repoData.full_name.split("/");
    const body = failComment
      ? template(failComment)({ branch, errors })
      : getFailComment(branch, errors);
    const [srIssue] = await findSRIssues(
      octokit,
      logger,
      failTitle,
      labels,
      owner,
      repo,
    );

    const canCommentOnOrCreateIssue = failCommentCondition
      ? template(failCommentCondition)({ ...context, issue: srIssue })
      : true;

    if (!canCommentOnOrCreateIssue) {
      logger.log("Skip commenting on or creating an issue.");
      return;
    }

    if (srIssue) {
      logger.log("Found existing semantic-release issue #%d.", srIssue.number);
      const comment = { owner, repo, issue_number: srIssue.number, body };
      debug("create comment: %O", comment);
      const {
        data: { html_url: url },
      } = await octokit.request(
        "POST /repos/{owner}/{repo}/issues/{issue_number}/comments",
        comment,
      );
      logger.log("Added comment to issue #%d: %s.", srIssue.number, url);
    } else {
      const newIssue = {
        owner,
        repo,
        title: failTitle,
        body: `${body}\n\n${ISSUE_ID}`,
        labels: (labels || []).concat([RELEASE_FAIL_LABEL]),
        assignees,
      };
      debug("create issue: %O", newIssue);
      const {
        data: { html_url: url, number },
      } = await octokit.request("POST /repos/{owner}/{repo}/issues", newIssue);
      logger.log("Created issue #%d: %s.", number, url);
    }
  }
}
