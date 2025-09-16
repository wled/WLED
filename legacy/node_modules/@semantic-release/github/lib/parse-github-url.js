export default function parseGitHubUrl(repositoryUrl) {
  const [match, auth, host, path] =
    /^(?!.+:\/\/)(?:(?<auth>.*)@)?(?<host>.*?):(?<path>.*)$/.exec(
      repositoryUrl,
    ) || [];
  try {
    const [, owner, repo] =
      /^\/(?<owner>[^/]+)?\/?(?<repo>.+?)(?:\.git)?$/.exec(
        new URL(
          match
            ? `ssh://${auth ? `${auth}@` : ""}${host}/${path}`
            : repositoryUrl,
        ).pathname,
      );
    return { owner, repo };
  } catch {
    return {};
  }
}
