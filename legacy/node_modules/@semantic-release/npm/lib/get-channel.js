import semver from "semver";

export default function (channel) {
  return channel ? (semver.validRange(channel) ? `release-${channel}` : channel) : "latest";
}
