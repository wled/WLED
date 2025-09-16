/**
 * Join all arguments together and normalize the resulting url.
 * This works similar to `path.join` but you shouldn't use `path.join` for urls since it works
 * differently depending on the operating system and also doesn't work for some cases.
 */
declare function urlJoin(...parts: string[]): string;
declare function urlJoin(parts: string[]): string;
 
export default urlJoin;
