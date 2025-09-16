import { RELEASE_NAME } from "./definitions/constants.js";

const linkify = (releaseInfo) =>
  `${
    releaseInfo.url
      ? releaseInfo.url.startsWith("http")
        ? `[${releaseInfo.name}](${releaseInfo.url})`
        : `${releaseInfo.name}: \`${releaseInfo.url}\``
      : `\`${releaseInfo.name}\``
  }`;

const filterReleases = (releaseInfos) =>
  releaseInfos.filter(
    (releaseInfo) => releaseInfo.name && releaseInfo.name !== RELEASE_NAME,
  );

export default function getReleaseLinks(releaseInfos) {
  return `${
    filterReleases(releaseInfos).length > 0
      ? `This release is also available on:\n${filterReleases(releaseInfos)
          .map((releaseInfo) => `- ${linkify(releaseInfo)}`)
          .join("\n")}`
      : ""
  }`;
}
