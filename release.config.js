const fs = require("fs");

module.exports = {
  branches: [
    "main",
    {
      name: "dev",
      channel: "beta",
      prerelease: true,
    },
  ],
  plugins: [
    "@semantic-release/commit-analyzer",
    "@semantic-release/release-notes-generator",
    [
      "@semantic-release/changelog",
      {
        changelogFile: "CHANGELOG.md",
        changelogTitle: "# Changelog",
      },
    ],
    [
      "@semantic-release/exec",
      {
        // Write version to custom VERSION file
        prepareCmd: "echo ${nextRelease.version} > VERSION",
      },
    ],
    [
      "@semantic-release/git",
      {
        assets: ["VERSION", "CHANGELOG.md"],
        message: "chore(release): ${nextRelease.version} [skip ci]\n\n${nextRelease.notes}",
      },
    ],
    "@semantic-release/github",
  ],
};
