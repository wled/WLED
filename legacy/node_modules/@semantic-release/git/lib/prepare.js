const {isPlainObject, isArray, template, castArray, uniq} = require('lodash');
const micromatch = require('micromatch');
const dirGlob = require('dir-glob');
const pReduce = require('p-reduce');
const debug = require('debug')('semantic-release:git');
const resolveConfig = require('./resolve-config');
const {getModifiedFiles, add, commit, push} = require('./git');

/**
 * Prepare a release commit including configurable files.
 *
 * @param {Object} pluginConfig The plugin configuration.
 * @param {String|Array<String>} [pluginConfig.assets] Files to include in the release commit. Can be files path or globs.
 * @param {String} [pluginConfig.message] The message for the release commit.
 * @param {Object} context semantic-release context.
 * @param {Object} context.options `semantic-release` configuration.
 * @param {Object} context.lastRelease The last release.
 * @param {Object} context.nextRelease The next release.
 * @param {Object} logger Global logger.
 */
module.exports = async (pluginConfig, context) => {
  const {
    env,
    cwd,
    branch,
    options: {repositoryUrl},
    lastRelease,
    nextRelease,
    logger,
  } = context;
  const {message, assets} = resolveConfig(pluginConfig, logger);

  const modifiedFiles = await getModifiedFiles({env, cwd});

  const filesToCommit = uniq(
    await pReduce(
      assets.map(asset => (!isArray(asset) && isPlainObject(asset) ? asset.path : asset)),
      async (result, asset) => {
        const glob = castArray(asset);
        let nonegate;
        // Skip solo negated pattern (avoid to include every non js file with `!**/*.js`)
        if (glob.length <= 1 && glob[0].startsWith('!')) {
          nonegate = true;
          debug(
            'skipping the negated glob %o as its alone in its group and would retrieve a large amount of files ',
            glob[0]
          );
        }

        return [
          ...result,
          ...micromatch(modifiedFiles, await dirGlob(glob, {cwd}), {dot: true, nonegate, cwd, expand: true}),
        ];
      },
      []
    )
  );

  if (filesToCommit.length > 0) {
    logger.log('Found %d file(s) to commit', filesToCommit.length);
    await add(filesToCommit, {env, cwd});
    debug('commited files: %o', filesToCommit);
    await commit(
      message
        ? template(message)({branch: branch.name, lastRelease, nextRelease})
        : `chore(release): ${nextRelease.version} [skip ci]\n\n${nextRelease.notes}`,
      {env, cwd}
    );
    await push(repositoryUrl, branch.name, {env, cwd});
    logger.log('Prepared Git release: %s', nextRelease.gitTag);
  }
};
