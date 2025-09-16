import { isNil, castArray } from "lodash-es";

export default function resolveConfig(
  {
    githubUrl,
    githubApiUrl,
    githubApiPathPrefix,
    proxy,
    assets,
    successComment,
    successCommentCondition,
    failTitle,
    failComment,
    failCommentCondition,
    labels,
    assignees,
    releasedLabels,
    addReleases,
    draftRelease,
    releaseNameTemplate,
    releaseBodyTemplate,
    discussionCategoryName,
  },
  { env },
) {
  return {
    githubToken: env.GH_TOKEN || env.GITHUB_TOKEN,
    githubUrl: githubUrl || env.GH_URL || env.GITHUB_URL,
    githubApiPathPrefix:
      githubApiPathPrefix || env.GH_PREFIX || env.GITHUB_PREFIX || "",
    githubApiUrl: githubApiUrl || env.GITHUB_API_URL,
    proxy: isNil(proxy) ? env.http_proxy || env.HTTP_PROXY || false : proxy,
    assets: assets ? castArray(assets) : assets,
    successComment,
    successCommentCondition,
    failTitle: isNil(failTitle)
      ? "The automated release is failing 🚨"
      : failTitle,
    failComment,
    failCommentCondition,
    labels: isNil(labels)
      ? ["semantic-release"]
      : labels === false
        ? false
        : castArray(labels),
    assignees: assignees ? castArray(assignees) : assignees,
    releasedLabels: isNil(releasedLabels)
      ? [
          `released<%= nextRelease.channel ? \` on @\${nextRelease.channel}\` : "" %>`,
        ]
      : releasedLabels === false
        ? false
        : castArray(releasedLabels),
    addReleases: isNil(addReleases) ? false : addReleases,
    draftRelease: isNil(draftRelease) ? false : draftRelease,
    releaseBodyTemplate: !isNil(releaseBodyTemplate)
      ? releaseBodyTemplate
      : "<%= nextRelease.notes %>",
    releaseNameTemplate: !isNil(releaseNameTemplate)
      ? releaseNameTemplate
      : "<%= nextRelease.name %>",
    discussionCategoryName: isNil(discussionCategoryName)
      ? false
      : discussionCategoryName,
  };
}
