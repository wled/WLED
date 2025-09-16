import path from "path";
import rc from "rc";
import fs from "fs-extra";
import getAuthToken from "registry-auth-token";
import nerfDart from "nerf-dart";
import AggregateError from "aggregate-error";
import getError from "./get-error.js";

export default async function (npmrc, registry, { cwd, env: { NPM_TOKEN, NPM_CONFIG_USERCONFIG }, logger }) {
  logger.log("Verify authentication for registry %s", registry);
  const { configs, ...rcConfig } = rc(
    "npm",
    { registry: "https://registry.npmjs.org/" },
    { config: NPM_CONFIG_USERCONFIG || path.resolve(cwd, ".npmrc") }
  );

  if (configs) {
    logger.log("Reading npm config from %s", configs.join(", "));
  }

  const currentConfig = configs ? (await Promise.all(configs.map((config) => fs.readFile(config)))).join("\n") : "";

  if (getAuthToken(registry, { npmrc: rcConfig })) {
    await fs.outputFile(npmrc, currentConfig);
    return;
  }

  if (NPM_TOKEN) {
    await fs.outputFile(
      npmrc,
      `${currentConfig ? `${currentConfig}\n` : ""}${nerfDart(registry)}:_authToken = \${NPM_TOKEN}`
    );
    logger.log(`Wrote NPM_TOKEN to ${npmrc}`);
  } else {
    throw new AggregateError([getError("ENONPMTOKEN", { registry })]);
  }
}
