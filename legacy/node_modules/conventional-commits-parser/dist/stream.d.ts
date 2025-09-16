/// <reference types="node" resolution-mode="require"/>
/// <reference types="node" resolution-mode="require"/>
/// <reference types="node" resolution-mode="require"/>
import type { ParserStreamOptions } from './types.js';
/**
 * Create async generator function to parse async iterable of raw commits.
 * @param options - CommitParser options.
 * @returns Async generator function to parse async iterable of raw commits.
 */
export declare function parseCommits(options?: ParserStreamOptions): (rawCommits: Iterable<string | Buffer> | AsyncIterable<string | Buffer>) => AsyncGenerator<import("./types.js").Commit, void, unknown>;
/**
 * Create stream to parse commits.
 * @param options - CommitParser options.
 * @returns Stream of parsed commits.
 */
export declare function parseCommitsStream(options?: ParserStreamOptions): import("stream").Duplex;
//# sourceMappingURL=stream.d.ts.map