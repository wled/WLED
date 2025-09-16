import { castArray, defaultTo } from "lodash-es";
import AggregateError from "aggregate-error";
import { temporaryFile } from "tempy";
import getPkg from "./lib/get-pkg.js";
import verifyNpmConfig from "./lib/verify-config.js";
import verifyNpmAuth from "./lib/verify-auth.js";
import addChannelNpm from "./lib/add-channel.js";
import prepareNpm from "./lib/prepare.js";
import publishNpm from "./lib/publish.js";

let verified;
let prepared;
const npmrc = temporaryFile({ name: ".npmrc" });

export async function verifyConditions(pluginConfig, context) {
  // If the npm publish plugin is used and has `npmPublish`, `tarballDir` or `pkgRoot` configured, validate them now in order to prevent any release if the configuration is wrong
  if (context.options.publish) {
    const publishPlugin =
      castArray(context.options.publish).find((config) => config.path && config.path === "@semantic-release/npm") || {};

    pluginConfig.npmPublish = defaultTo(pluginConfig.npmPublish, publishPlugin.npmPublish);
    pluginConfig.tarballDir = defaultTo(pluginConfig.tarballDir, publishPlugin.tarballDir);
    pluginConfig.pkgRoot = defaultTo(pluginConfig.pkgRoot, publishPlugin.pkgRoot);
  }

  const errors = verifyNpmConfig(pluginConfig);

  try {
    const pkg = await getPkg(pluginConfig, context);

    // Verify the npm authentication only if `npmPublish` is not false and `pkg.private` is not `true`
    if (pluginConfig.npmPublish !== false && pkg.private !== true) {
      await verifyNpmAuth(npmrc, pkg, context);
    }
  } catch (error) {
    errors.push(...error.errors);
  }

  if (errors.length > 0) {
    throw new AggregateError(errors);
  }

  verified = true;
}

export async function prepare(pluginConfig, context) {
  const errors = verified ? [] : verifyNpmConfig(pluginConfig);

  try {
    // Reload package.json in case a previous external step updated it
    const pkg = await getPkg(pluginConfig, context);
    if (!verified && pluginConfig.npmPublish !== false && pkg.private !== true) {
      await verifyNpmAuth(npmrc, pkg, context);
    }
  } catch (error) {
    errors.push(...error.errors);
  }

  if (errors.length > 0) {
    throw new AggregateError(errors);
  }

  await prepareNpm(npmrc, pluginConfig, context);
  prepared = true;
}

export async function publish(pluginConfig, context) {
  let pkg;
  const errors = verified ? [] : verifyNpmConfig(pluginConfig);

  try {
    // Reload package.json in case a previous external step updated it
    pkg = await getPkg(pluginConfig, context);
    if (!verified && pluginConfig.npmPublish !== false && pkg.private !== true) {
      await verifyNpmAuth(npmrc, pkg, context);
    }
  } catch (error) {
    errors.push(...error.errors);
  }

  if (errors.length > 0) {
    throw new AggregateError(errors);
  }

  if (!prepared) {
    await prepareNpm(npmrc, pluginConfig, context);
  }

  return publishNpm(npmrc, pluginConfig, pkg, context);
}

export async function addChannel(pluginConfig, context) {
  let pkg;
  const errors = verified ? [] : verifyNpmConfig(pluginConfig);

  try {
    // Reload package.json in case a previous external step updated it
    pkg = await getPkg(pluginConfig, context);
    if (!verified && pluginConfig.npmPublish !== false && pkg.private !== true) {
      await verifyNpmAuth(npmrc, pkg, context);
    }
  } catch (error) {
    errors.push(...error.errors);
  }

  if (errors.length > 0) {
    throw new AggregateError(errors);
  }

  return addChannelNpm(npmrc, pluginConfig, pkg, context);
}
