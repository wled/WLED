const {defaultTo, castArray} = require('lodash');
const verifyGit = require('./lib/verify');
const prepareGit = require('./lib/prepare');

let verified;

function verifyConditions(pluginConfig, context) {
  const {options} = context;
  // If the Git prepare plugin is used and has `assets` or `message` configured, validate them now in order to prevent any release if the configuration is wrong
  if (options.prepare) {
    const preparePlugin =
      castArray(options.prepare).find(config => config.path && config.path === '@semantic-release/git') || {};

    pluginConfig.assets = defaultTo(pluginConfig.assets, preparePlugin.assets);
    pluginConfig.message = defaultTo(pluginConfig.message, preparePlugin.message);
  }

  verifyGit(pluginConfig);
  verified = true;
}

async function prepare(pluginConfig, context) {
  if (!verified) {
    verifyGit(pluginConfig);
    verified = true;
  }

  await prepareGit(pluginConfig, context);
}

module.exports = {verifyConditions, prepare};
