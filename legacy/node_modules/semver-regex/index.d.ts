/**
Regular expression for matching [semver](https://github.com/npm/node-semver) versions.

@example
```
import semverRegex from 'semver-regex';

semverRegex().test('v1.0.0');
//=> true

semverRegex().test('1.2.3-alpha.10.beta.0+build.unicorn.rainbow');
//=> true

semverRegex().exec('unicorn 1.0.0 rainbow')[0];
//=> '1.0.0'

'unicorn 1.0.0 and rainbow 2.1.3'.match(semverRegex());
//=> ['1.0.0', '2.1.3']
```
*/
export default function semverRegex(): RegExp;
