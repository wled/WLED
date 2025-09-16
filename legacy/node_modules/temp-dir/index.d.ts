/**
Get the real path of the system temp directory.

@example
```
import temporaryDirectory from 'temp-dir';

console.log(temporaryDirectory);
//=> '/private/var/folders/3x/jf5977fn79jbglr7rk0tq4d00000gn/T'
```

@example
```
import os from 'node:os';

console.log(os.tmpdir());
//=> '/var/folders/3x/jf5977fn79jbglr7rk0tq4d00000gn/T' // <= Symlink
```
*/
declare const temporaryDirectory: string;

export default temporaryDirectory;
