const HOME_URL = "https://github.com/semantic-release/semantic-release";
const linkify = (releaseInfo) =>
  `${
    releaseInfo.url
      ? `[${releaseInfo.name}](${releaseInfo.url})`
      : `\`${releaseInfo.name}\``
  }`;

export default function getSuccessComment(issue, releaseInfos, nextRelease) {
  return `:tada: This ${
    issue.pull_request ? "PR is included" : "issue has been resolved"
  } in version ${nextRelease.version} :tada:${
    releaseInfos.length > 0
      ? `\n\nThe release is available on${
          releaseInfos.length === 1
            ? ` ${linkify(releaseInfos[0])}`
            : `:\n${releaseInfos
                .map((releaseInfo) => `- ${linkify(releaseInfo)}`)
                .join("\n")}`
        }`
      : ""
  }

Your **[semantic-release](${HOME_URL})** bot :package::rocket:`;
}
