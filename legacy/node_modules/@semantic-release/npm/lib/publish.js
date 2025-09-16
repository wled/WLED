import path from "path";
import { execa } from "execa";
import getRegistry from "./get-registry.js";
import getChannel from "./get-channel.js";
import getReleaseInfo from "./get-release-info.js";

export default async function (npmrc, { npmPublish, pkgRoot }, pkg, context) {
  const {
    cwd,
    env,
    stdout,
    stderr,
    nextRelease: { version, channel },
    logger,
  } = context;

  if (npmPublish !== false && pkg.private !== true) {
    const basePath = pkgRoot ? path.resolve(cwd, pkgRoot) : cwd;
    const registry = getRegistry(pkg, context);
    const distTag = getChannel(channel);

    logger.log(`Publishing version ${version} to npm registry on dist-tag ${distTag}`);
    const result = execa(
      "npm",
      ["publish", basePath, "--userconfig", npmrc, "--tag", distTag, "--registry", registry],
      { cwd, env, preferLocal: true }
    );
    result.stdout.pipe(stdout, { end: false });
    result.stderr.pipe(stderr, { end: false });
    await result;

    logger.log(`Published ${pkg.name}@${version} to dist-tag @${distTag} on ${registry}`);

    return getReleaseInfo(pkg, context, distTag, registry);
  }

  logger.log(
    `Skip publishing to npm registry as ${npmPublish === false ? "npmPublish" : "package.json's private property"} is ${
      npmPublish !== false
    }`
  );

  return false;
}
