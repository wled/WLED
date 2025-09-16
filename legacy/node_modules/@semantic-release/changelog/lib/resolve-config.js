const {isNil} = require('lodash');

module.exports = ({changelogFile, changelogTitle}) => ({
  changelogFile: isNil(changelogFile) ? 'CHANGELOG.md' : changelogFile,
  changelogTitle,
});
