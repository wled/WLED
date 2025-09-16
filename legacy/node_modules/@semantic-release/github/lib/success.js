import { isNil, uniqBy, template, flatten, isEmpty, merge } from "lodash-es";
import pFilter from "p-filter";
import AggregateError from "aggregate-error";
import issueParser from "issue-parser";
import debugFactory from "debug";

import parseGithubUrl from "./parse-github-url.js";
import resolveConfig from "./resolve-config.js";
import { toOctokitOptions } from "./octokit.js";
import getSuccessComment from "./get-success-comment.js";
import findSRIssues from "./find-sr-issues.js";
import { RELEASE_NAME } from "./definitions/constants.js";
import getReleaseLinks from "./get-release-links.js";

const debug = debugFactory("semantic-release:github");

export default async function success(pluginConfig, context, { Octokit }) {
  const {
    options: { repositoryUrl },
    commits,
    nextRelease,
    releases,
    logger,
  } = context;
  const {
    githubToken,
    githubUrl,
    githubApiPathPrefix,
    githubApiUrl,
    proxy,
    labels,
    successComment,
    successCommentCondition,
    failTitle,
    failComment,
    failCommentCondition,
    releasedLabels,
    addReleases,
  } = resolveConfig(pluginConfig, context);

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

  const errors = [];

  if (successComment === false || isEmpty(commits)) {
    if (isEmpty(commits)) {
      logger.log("No commits found in release");
    }
    logger.log("Skip commenting on issues and pull requests.");
    logger.warn(
      `DEPRECATION: 'false' for 'successComment' is deprecated and will be removed in a future major version. Use 'successCommentCondition' instead.`,
    );
  } else if (successCommentCondition === false) {
    logger.log("Skip commenting on issues and pull requests.");
  } else {
    const parser = issueParser(
      "github",
      githubUrl ? { hosts: [githubUrl] } : {},
    );
    const releaseInfos = releases.filter((release) => Boolean(release.name));
    const shas = commits.map(({ hash }) => hash);

    // Get associatedPRs
    const associatedPRs = await inChunks(shas, 100, async (chunk) => {
      const responsePRs = [];
      const { repository } = await octokit.graphql(
        buildAssociatedPRsQuery(chunk),
        { owner, repo },
      );
      const responseAssociatedPRs = Object.values(repository).map(
        (item) => item.associatedPullRequests,
      );
      for (const { nodes, pageInfo } of responseAssociatedPRs) {
        if (nodes.length === 0) continue;

        responsePRs.push(...buildIssuesOrPRsFromResponseNode(nodes));
        if (pageInfo.hasNextPage) {
          let cursor = pageInfo.endCursor;
          let hasNextPage = true;
          while (hasNextPage) {
            const { repository } = await octokit.graphql(
              loadSingleCommitAssociatedPRs,
              { owner, repo, sha: response.commit.oid, cursor },
            );
            const { associatedPullRequests } = repository.commit;
            responsePRs.push(
              ...buildIssuesOrPRsFromResponseNode(
                associatedPullRequests.nodes,
                "PR",
              ),
            );
            if (associatedPullRequests.pageInfo.hasNextPage) {
              cursor = associatedPullRequests.pageInfo.endCursor;
            } else {
              hasNextPage = false;
            }
          }
        }
      }
      return responsePRs;
    });

    const uniqueAssociatedPRs = uniqBy(flatten(associatedPRs), "number");

    const prs = await pFilter(uniqueAssociatedPRs, async ({ number }) => {
      const commits = await octokit.paginate(
        "GET /repos/{owner}/{repo}/pulls/{pull_number}/commits",
        {
          owner,
          repo,
          pull_number: number,
        },
      );
      const matchingCommit = commits.find(({ sha }) => shas.includes(sha));
      if (matchingCommit) return matchingCommit;

      const { data: pullRequest } = await octokit.request(
        "GET /repos/{owner}/{repo}/pulls/{pull_number}",
        {
          owner,
          repo,
          pull_number: number,
        },
      );
      return shas.includes(pullRequest.merge_commit_sha);
    });

    debug(
      "found pull requests: %O",
      prs.map((pr) => pr.number),
    );

    // Parse the release commits message and PRs body to find resolved issues/PRs via comment keyworkds
    const parsedIssues = [
      ...prs.map((pr) => pr.body),
      ...commits.map((commit) => commit.message),
    ].reduce(
      (issues, message) =>
        message
          ? issues.concat(
              parser(message)
                .actions.close.filter(
                  (action) =>
                    isNil(action.slug) || action.slug === `${owner}/${repo}`,
                )
                .map((action) => ({
                  number: Number.parseInt(action.issue, 10),
                })),
            )
          : issues,
      [],
    );

    let issues = [];

    if (!isEmpty(parsedIssues)) {
      const uniqueParsedIssues = uniqBy(flatten(parsedIssues), "number");

      // Get relatedIssues (or relatedPRs i.e. Issues/PRs that are closed by an associatedPR)
      issues = await inChunks(uniqueParsedIssues, 100, async (chunk) => {
        const { repository } = await octokit.graphql(
          buildRelatedIssuesQuery(chunk.map((issue) => issue.number)),
          { owner, repo },
        );
        const responseRelatedIssues = Object.values(repository).map(
          (issue) => issue,
        );
        return buildIssuesOrPRsFromResponseNode(responseRelatedIssues);
      });

      debug(
        "found related issues via PRs and Commits: %O",
        issues.map((issue) => issue.number),
      );
    }

    await Promise.all(
      uniqBy([...prs, ...issues], "number").map(async (issue) => {
        const issueOrPR = issue.pull_request ? "PR" : "issue";

        const canCommentOnIssue = successCommentCondition
          ? template(successCommentCondition)({ ...context, issue })
          : true;

        if (!canCommentOnIssue) {
          logger.log(`Skip commenting on ${issueOrPR} #%d.`, issue.number);
          return;
        }

        const body = successComment
          ? template(successComment)({ ...context, issue })
          : getSuccessComment(issue, releaseInfos, nextRelease);
        try {
          const comment = { owner, repo, issue_number: issue.number, body };
          debug("create comment: %O", comment);
          const {
            data: { html_url: url },
          } = await octokit.request(
            "POST /repos/{owner}/{repo}/issues/{issue_number}/comments",
            comment,
          );
          logger.log(
            `Added comment to ${issueOrPR} #%d: %s`,
            issue.number,
            url,
          );

          if (releasedLabels) {
            const labels = releasedLabels.map((label) =>
              template(label)(context),
            );
            await octokit.request(
              "POST /repos/{owner}/{repo}/issues/{issue_number}/labels",
              {
                owner,
                repo,
                issue_number: issue.number,
                data: labels,
              },
            );
            logger.log(
              `Added labels %O to ${issueOrPR} #%d`,
              labels,
              issue.number,
            );
          }
        } catch (error) {
          if (error.status === 403) {
            logger.error(
              `Not allowed to add a comment to the issue/PR #%d.`,
              issue.number,
            );
          } else if (error.status === 404) {
            logger.error(
              `Failed to add a comment to the issue/PR #%d as it doesn't exist.`,
              issue.number,
            );
          } else {
            errors.push(error);
            logger.error(
              `Failed to add a comment to the issue/PR #%d.`,
              issue.number,
            );
            // Don't throw right away and continue to update other issues
          }
        }
      }),
    );
  }

  if (failComment === false || failTitle === false) {
    logger.log("Skip closing issue.");
  } else {
    const srIssues = await findSRIssues(
      octokit,
      logger,
      failTitle,
      labels,
      owner,
      repo,
    );

    debug("found semantic-release issues: %O", srIssues);

    await Promise.all(
      srIssues.map(async (issue) => {
        debug("close issue: %O", issue);
        try {
          const updateIssue = {
            owner,
            repo,
            issue_number: issue.number,
            state: "closed",
          };
          debug("closing issue: %O", updateIssue);
          const {
            data: { html_url: url },
          } = await octokit.request(
            "PATCH /repos/{owner}/{repo}/issues/{issue_number}",
            updateIssue,
          );
          logger.log("Closed issue #%d: %s.", issue.number, url);
        } catch (error) {
          errors.push(error);
          logger.error("Failed to close the issue #%d.", issue.number);
          // Don't throw right away and continue to close other issues
        }
      }),
    );
  }

  if (addReleases !== false && errors.length === 0) {
    const ghRelease = releases.find(
      (release) => release.name && release.name === RELEASE_NAME,
    );
    if (!isNil(ghRelease)) {
      const ghRelaseId = ghRelease.id;
      const additionalReleases = getReleaseLinks(releases);
      if (!isEmpty(additionalReleases) && !isNil(ghRelaseId)) {
        const newBody =
          addReleases === "top"
            ? additionalReleases.concat("\n---\n", nextRelease.notes)
            : nextRelease.notes.concat("\n---\n", additionalReleases);
        await octokit.request(
          "PATCH /repos/{owner}/{repo}/releases/{release_id}",
          {
            owner,
            repo,
            release_id: ghRelaseId,
            body: newBody,
          },
        );
      }
    }
  }

  if (errors.length > 0) {
    throw new AggregateError(errors);
  }
}

/**
 * In order to speed up a function call that handles a big array of items, we split up the
 * array in chunks and call the function for each chunk in parallel. At the end we combine the
 * results again.
 *
 * @template TItem
 * @template TCallbackResult
 * @param {TItem[]} items
 * @param {number} chunkSize
 * @param {(items: TItem[]) => TCallbackResult} callback
 * @returns TCallbackResult
 */
async function inChunks(items, chunkSize, callback) {
  const chunkCalls = [];
  for (let i = 0; i < items.length; i += chunkSize) {
    chunkCalls.push(callback(items.slice(i, i + chunkSize)));
  }
  const results = await Promise.all(chunkCalls);

  return results.flat();
}

/**
 * Fields common accross PRs and Issue
 */
const baseFields = `
  __typename
  id
  title
  body
  url
  number
  createdAt
  updatedAt
  closedAt
  comments {
    totalCount
  }
  state
  author {
    login
    url
    avatarUrl
    __typename
  }
  authorAssociation
  activeLockReason
  labels(first: 40) {
    nodes {
      id
      url
      name
      color
      description
      isDefault
    }
  }
  milestone {
    url
    id
    number
    state
    title
    description
    creator {
      login
      url
      avatarUrl
    }
    createdAt
    closedAt
    updatedAt
  }
  locked
`;

/**
 * Builds GraphQL query for fetching PRs/Commits related Issues to a list of commit hash (sha)
 * @param {Array<number>} numbers
 * @returns {string}
 */
function buildRelatedIssuesQuery(numbers) {
  return `#graphql
    query getRelatedIssues($owner: String!, $repo: String!) {
      repository(owner: $owner, name: $repo) {
        ${numbers
          .map((num) => {
            return `issue${num}: issueOrPullRequest(number: ${num}) {
              ... on Issue {
                ${baseFields}
              }
              ... on PullRequest {
                ${baseFields}
                mergeable
                changedFiles
                mergedAt
                isDraft
                mergedBy {
                  login
                  avatarUrl
                  url
                }
                commits {
                  totalCount
                }
              }
            }`;
          })
          .join("")}
      }
    }
  `;
}

/**
 * Builds GraphQL query for fetching associated PRs to a list of commit hash (sha)
 * @param {Array<string>} shas
 * @returns {string}
 */
function buildAssociatedPRsQuery(shas) {
  return `#graphql
    query getAssociatedPRs($owner: String!, $repo: String!) {
      repository(owner: $owner, name: $repo) {
        ${shas
          .map((sha) => {
            return `commit${sha.slice(0, 6)}: object(oid: "${sha}") {
            ...on Commit {
              oid
              associatedPullRequests(first: 100) {
                pageInfo {
                  endCursor
                  hasNextPage
                }
                nodes {
                  ${baseFields}
                  mergeable
                  changedFiles
                  mergedAt
                  isDraft
                  mergedBy {
                    login
                    avatarUrl
                    url
                  }
                  commits {
                    totalCount
                  }
                }
              }
            }
          }`;
          })
          .join("")}
      }
    }
  `;
}

/**
 * GraphQL Query to fetch additional associatedPR for commits that has more than 100 associatedPRs
 */
const loadSingleCommitAssociatedPRs = `#graphql
  query getCommitAssociatedPRs($owner: String!, $repo: String!, $sha: String!, $cursor: String) {
    repository(owner: $owner, name: $repo) {
      commit: object(oid: $sha) {
        ...on Commit {
          oid
          associatedPullRequests(after: $cursor, first: 100) {
            pageInfo {
              endCursor
              hasNextPage
            }
            nodes {
              ${baseFields}
              mergeable
              changedFiles
              mergedAt
              isDraft
              mergedBy {
                login
                avatarUrl
                url
              }
              commits {
                totalCount
              }
            }
          }
        }
      }
    }
  }
`;

/**
 * Build associatedPRs or RelatedIssues object (into issue-like object with `pull_request` property) from the GraphQL repository response
 * @param {object} responseNodes
 * @returns {object[]}
 */
function buildIssuesOrPRsFromResponseNode(responseNodes) {
  const resultArray = [];
  for (const node of responseNodes) {
    let baseProps = {
      number: node.number,
      title: node.title,
      body: node.body,
      labels: node.labels?.nodes.map((label) => {
        return {
          id: label.id,
          url: label.url,
          name: label.name,
          color: label.color,
          description: label.description,
          default: label.isDefault,
        };
      }),
      html_url: node.url,
      created_at: node.createdAt,
      updated_at: node.updatedAt,
      user: {
        login: node.author?.login,
        html_url: node.author?.url,
        avatar_url: node.author?.avatarUrl,
        type: node.author?.__typename,
      },
      comments: node.comments?.totalCount,
      state: node.state,
      milestone: node.milestone
        ? {
            url: node.milestone.url,
            id: node.milestone.id,
            number: node.milestone.number,
            state: node.milestone.state,
            title: node.milestone.title,
            description: node.milestone.description,
            creator: {
              login: node.milestone.creator.login,
              html_url: node.milestone.creator.url,
              avatar_url: node.milestone.creator.avatarUrl,
            },
            created_at: node.milestone.createdAt,
            closed_at: node.milestone.closedAt,
            updated_at: node.milestone.updatedAt,
          }
        : null,
      locked: node.locked,
      active_lock_reason: node.activeLockReason,
      closed_at: node.closedAt,
    };

    let result = baseProps;

    if (node.__typename === "PullRequest") {
      const prProps = {
        pull_request: true,
        mergeable: node.mergeable,
        changed_files: node.changedFiles,
        commits: node.commits?.totalCount,
        merged_at: node.mergedAt,
        draft: node.isDraft,
        merged_by: {
          login: node.mergedBy?.login,
          avatar_url: node.mergedBy?.avatarUrl,
          html_url: node.mergedBy?.url,
        },
      };
      result = merge(baseProps, prProps);
    }

    resultArray.push(result);
  }
  return resultArray;
}
