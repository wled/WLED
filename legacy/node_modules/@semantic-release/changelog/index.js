/* eslint require-atomic-updates: off */

const {defaultTo, castArray} = require('lodash');
const verifyChangelog = require('./lib/verify.js');
const prepareChangelog = require('./lib/prepare.js');

let verified;

async function verifyConditions(pluginConfig, context) {
  const {options} = context;
  // If the Changelog prepare plugin is used and has `changelogFile` configured, validate them now in order to prevent any release if the configuration is wrong
  if (options.prepare) {
    const preparePlugin =
      castArray(options.prepare).find((config) => config.path && config.path === '@semantic-release/changelog') || {};

    pluginConfig.changelogFile = defaultTo(pluginConfig.changelogFile, preparePlugin.changelogFile);
  }

  await verifyChangelog(pluginConfig);
  verified = true;
}

async function prepare(pluginConfig, context) {
  if (!verified) {
    await verifyChangelog(pluginConfig);
    verified = true;
  }

  await prepareChangelog(pluginConfig, context);
}

module.exports = {verifyConditions, prepare};
