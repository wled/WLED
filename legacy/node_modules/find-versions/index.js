import semverRegex from 'semver-regex';
import {matches} from 'super-regex';

export default function findVersions(stringWithVersions, {loose = false} = {}) {
	if (typeof stringWithVersions !== 'string') {
		throw new TypeError(`Expected a string, got ${typeof stringWithVersions}`);
	}

	const regex = loose ? new RegExp(`(?:${semverRegex().source})|(?:v?(?:\\d+\\.\\d+)(?:\\.\\d+)?)`, 'g') : semverRegex();
	const versions = [...matches(regex, stringWithVersions)].map(({match}) => match.trim().replace(/^v/, '').replace(/^\d+\.\d+$/, '$&.0')); // TODO: Remove the `...` when https://github.com/tc39/proposal-iterator-helpers is available.

	return [...new Set(versions)];
}
