import {type Buffer} from 'node:buffer';
import {type MergeExclusive, type TypedArray} from 'type-fest';

export type FileOptions = MergeExclusive<
{
	/**
	File extension.

	Mutually exclusive with the `name` option.

	_You usually won't need this option. Specify it only when actually needed._
	*/
	readonly extension?: string;
},
{
	/**
	Filename.

	Mutually exclusive with the `extension` option.

	_You usually won't need this option. Specify it only when actually needed._
	*/
	readonly name?: string;
}
>;

export type DirectoryOptions = {
	/**
	Directory prefix.

	_You usually won't need this option. Specify it only when actually needed._

	Useful for testing by making it easier to identify cache directories that are created.
	*/
	readonly prefix?: string;
};

/**
The temporary path created by the function. Can be asynchronous.
*/
export type TaskCallback<ReturnValueType> = (temporaryPath: string) => Promise<ReturnValueType> | ReturnValueType;

/**
Get a temporary file path you can write to.

@example
```
import {temporaryFile, temporaryDirectory} from 'tempy';

temporaryFile();
//=> '/private/var/folders/3x/jf5977fn79jbglr7rk0tq4d00000gn/T/4f504b9edb5ba0e89451617bf9f971dd'

temporaryFile({extension: 'png'});
//=> '/private/var/folders/3x/jf5977fn79jbglr7rk0tq4d00000gn/T/a9fb0decd08179eb6cf4691568aa2018.png'

temporaryFile({name: 'unicorn.png'});
//=> '/private/var/folders/3x/jf5977fn79jbglr7rk0tq4d00000gn/T/f7f62bfd4e2a05f1589947647ed3f9ec/unicorn.png'

temporaryDirectory();
//=> '/private/var/folders/3x/jf5977fn79jbglr7rk0tq4d00000gn/T/2f3d094aec2cb1b93bb0f4cffce5ebd6'
```
*/
export function temporaryFile(options?: FileOptions): string;

/**
The `callback` resolves with a temporary file path you can write to. The file is automatically cleaned up after the callback is executed.

@returns A promise that resolves after the callback is executed and the file is cleaned up.

@example
```
import {temporaryFileTask} from 'tempy';

await temporaryFileTask(tempFile => {
	console.log(tempFile);
	//=> '/private/var/folders/3x/jf5977fn79jbglr7rk0tq4d00000gn/T/4f504b9edb5ba0e89451617bf9f971dd'
});
```
*/
export function temporaryFileTask<ReturnValueType>(callback: TaskCallback<ReturnValueType>, options?: FileOptions): Promise <ReturnValueType>;

/**
Get a temporary directory path. The directory is created for you.

@example
```
import {temporaryDirectory} from 'tempy';

temporaryDirectory();
//=> '/private/var/folders/3x/jf5977fn79jbglr7rk0tq4d00000gn/T/2f3d094aec2cb1b93bb0f4cffce5ebd6'

temporaryDirectory({prefix: 'a'});
//=> '/private/var/folders/3x/jf5977fn79jbglr7rk0tq4d00000gn/T/name_3c085674ad31223b9653c88f725d6b41'
```
*/
export function temporaryDirectory(options?: DirectoryOptions): string;

/**
The `callback` resolves with a temporary directory path you can write to. The directory is automatically cleaned up after the callback is executed.

@returns A promise that resolves after the callback is executed and the directory is cleaned up.

@example
```
import {temporaryDirectoryTask} from 'tempy';

await temporaryDirectoryTask(tempDirectory => {
	//=> '/private/var/folders/3x/jf5977fn79jbglr7rk0tq4d00000gn/T/2f3d094aec2cb1b93bb0f4cffce5ebd6'
})
```
*/
export function temporaryDirectoryTask<ReturnValueType>(callback: TaskCallback<ReturnValueType>, options?: DirectoryOptions): Promise<ReturnValueType>;

/**
Write data to a random temp file.

@example
```
import {temporaryWrite} from 'tempy';

await temporaryWrite('🦄');
//=> '/private/var/folders/3x/jf5977fn79jbglr7rk0tq4d00000gn/T/2f3d094aec2cb1b93bb0f4cffce5ebd6'
```
*/
export function temporaryWrite(fileContent: string | Buffer | TypedArray | DataView | NodeJS.ReadableStream, options?: FileOptions): Promise<string>;

/**
Write data to a random temp file. The file is automatically cleaned up after the callback is executed.

@returns A promise that resolves after the callback is executed and the file is cleaned up.

@example
```
import {temporaryWriteTask} from 'tempy';

await temporaryWriteTask('🦄', tempFile => {
	//=> '/private/var/folders/3x/jf5977fn79jbglr7rk0tq4d00000gn/T/4f504b9edb5ba0e89451617bf9f971dd'
});
```
*/
export function temporaryWriteTask<ReturnValueType>(fileContent: string | Buffer | TypedArray | DataView | NodeJS.ReadableStream, callback: TaskCallback<ReturnValueType>, options?: FileOptions): Promise<ReturnValueType>;

/**
Synchronously write data to a random temp file.

@example
```
import {temporaryWriteSync} from 'tempy';

temporaryWriteSync('🦄');
//=> '/private/var/folders/3x/jf5977fn79jbglr7rk0tq4d00000gn/T/2f3d094aec2cb1b93bb0f4cffce5ebd6'
```
*/
export function temporaryWriteSync(fileContent: string | Buffer | TypedArray | DataView, options?: FileOptions): string;

export {default as rootTemporaryDirectory} from 'temp-dir';
