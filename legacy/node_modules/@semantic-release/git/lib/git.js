const execa = require('execa');
const debug = require('debug')('semantic-release:git');

/**
 * Retrieve the list of files modified on the local repository.
 *
 * @param {Object} [execaOpts] Options to pass to `execa`.
 *
 * @return {Array<String>} Array of modified files path.
 */
async function getModifiedFiles(execaOptions) {
  return (await execa('git', ['ls-files', '-m', '-o'], execaOptions)).stdout
    .split('\n')
    .map(file => file.trim())
    .filter(file => Boolean(file));
}

/**
 * Add a list of file to the Git index. `.gitignore` will be ignored.
 *
 * @param {Array<String>} files Array of files path to add to the index.
 * @param {Object} [execaOpts] Options to pass to `execa`.
 */
async function add(files, execaOptions) {
  const shell = await execa('git', ['add', '--force', '--ignore-errors', ...files], {...execaOptions, reject: false});
  debug('add file to git index', shell);
}

/**
 * Commit to the local repository.
 *
 * @param {String} message Commit message.
 * @param {Object} [execaOpts] Options to pass to `execa`.
 *
 * @throws {Error} if the commit failed.
 */
async function commit(message, execaOptions) {
  await execa('git', ['commit', '-m', message], execaOptions);
}

/**
 * Push to the remote repository.
 *
 * @param {String} origin The remote repository URL.
 * @param {String} branch The branch to push.
 * @param {Object} [execaOpts] Options to pass to `execa`.
 *
 * @throws {Error} if the push failed.
 */
async function push(origin, branch, execaOptions) {
  await execa('git', ['push', '--tags', origin, `HEAD:${branch}`], execaOptions);
}

/**
 * Get the HEAD sha.
 *
 * @param {Object} [execaOpts] Options to pass to `execa`.
 *
 * @return {String} The sha of the head commit on the local repository
 */
async function gitHead(execaOptions) {
  return (await execa('git', ['rev-parse', 'HEAD'], execaOptions)).stdout;
}

module.exports = {getModifiedFiles, add, gitHead, commit, push};
