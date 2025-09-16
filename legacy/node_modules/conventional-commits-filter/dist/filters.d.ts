/// <reference types="node" resolution-mode="require"/>
import type { Commit } from './types.js';
/**
 * Filter reverted commits.
 * @param commits
 * @yields Commits without reverted commits.
 */
export declare function filterRevertedCommits<T extends Commit = Commit>(commits: Iterable<T> | AsyncIterable<T>): AsyncGenerator<T, void, undefined>;
/**
 * Filter reverted commits synchronously.
 * @param commits
 * @yields Commits without reverted commits.
 */
export declare function filterRevertedCommitsSync<T extends Commit = Commit>(commits: Iterable<T>): Generator<T, void, undefined>;
/**
 * Filter reverted commits stream.
 * @returns Reverted commits filter stream.
 */
export declare function filterRevertedCommitsStream(): import("stream").Duplex;
//# sourceMappingURL=filters.d.ts.map