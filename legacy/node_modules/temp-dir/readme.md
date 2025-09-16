# temp-dir

> Get the real path of the system temp directory

[The `os.tmpdir()` built-in doesn't return the real path.](https://github.com/nodejs/node/issues/11422) That can cause problems when the returned path is a symlink, which is the case on macOS. Use this module to get the resolved path.

## Install

```sh
npm install temp-dir
```

## Usage

```js
import temporaryDirectory from 'temp-dir';

console.log(temporaryDirectory);
//=> '/private/var/folders/3x/jf5977fn79jbglr7rk0tq4d00000gn/T'
```

```js
import os from 'node:os';

console.log(os.tmpdir());
//=> '/var/folders/3x/jf5977fn79jbglr7rk0tq4d00000gn/T' // <= Symlink
```
