import { uniqBy } from "lodash-es";
import { ISSUE_ID, RELEASE_FAIL_LABEL } from "./definitions/constants.js";

export default async (octokit, logger, title, labels, owner, repo) => {
  let issues = [];

  const {
    repository: {
      issues: { nodes: issueNodes },
    },
  } = await octokit.graphql(loadGetSRIssuesQuery, {
    owner,
    repo,
    filter: {
      labels: (labels || []).concat([RELEASE_FAIL_LABEL]),
    },
  });

  issues.push(...issueNodes);

  /**
   * BACKWARD COMPATIBILITY: Fallback to the search API if the issue was not found in the GraphQL response.
   * This fallback will be removed in a future release
   */
  if (issueNodes.length === 0) {
    try {
      const {
        data: { items: backwardIssues },
      } = await octokit.request("GET /search/issues", {
        q: `in:title+repo:${owner}/${repo}+type:issue+state:open+${title}`,
      });
      issues.push(...backwardIssues);
    } catch (error) {
      logger.log(
        "An error occured fetching issue via fallback (with GH SearchAPI)",
      );
    }
  }

  const uniqueSRIssues = uniqBy(
    issues.filter((issue) => issue.body && issue.body.includes(ISSUE_ID)),
    "number",
  );

  return uniqueSRIssues;
};

/**
 * GraphQL Query to et the semantic-release issues for a repository.
 */
const loadGetSRIssuesQuery = `#graphql
  query getSRIssues($owner: String!, $repo: String!, $filter: IssueFilters) {
    repository(owner: $owner, name: $repo) {
      issues(first: 100, states: OPEN, filterBy: $filter) {
        nodes {
          number
          title
          body
        }
      }
    }
  }
`;
