/// <reference types="node" resolution-mode="require"/>
import type { CommitKnownProps, Context, Options, Details } from './types/index.js';
/**
 * Creates an async generator function to generate changelog entries from commits.
 * @param context - Context for changelog template.
 * @param options - Options for changelog template.
 * @param includeDetails - Whether to yield details object instead of changelog entry.
 * @returns Async generator function to generate changelog entries from commits.
 */
export declare function writeChangelog<Commit extends CommitKnownProps = CommitKnownProps>(context?: Context<Commit>, options?: Options<Commit>, includeDetails?: false): (commits: Iterable<Commit> | AsyncIterable<Commit>) => AsyncGenerator<string, void>;
export declare function writeChangelog<Commit extends CommitKnownProps = CommitKnownProps>(context: Context<Commit>, options: Options<Commit>, includeDetails: true): (commits: Iterable<Commit> | AsyncIterable<Commit>) => AsyncGenerator<Details<Commit>, void>;
export declare function writeChangelog<Commit extends CommitKnownProps = CommitKnownProps>(context?: Context<Commit>, options?: Options<Commit>, includeDetails?: boolean): (commits: Iterable<Commit> | AsyncIterable<Commit>) => AsyncGenerator<string | Details<Commit>, void>;
/**
 * Creates a transform stream which takes commits and outputs changelog entries.
 * @param context - Context for changelog template.
 * @param options - Options for changelog template.
 * @param includeDetails - Whether to emit details object instead of changelog entry.
 * @returns Transform stream which takes commits and outputs changelog entries.
 */
export declare function writeChangelogStream<Commit extends CommitKnownProps = CommitKnownProps>(context?: Context<Commit>, options?: Options<Commit>, includeDetails?: boolean): import("stream").Duplex;
/**
 * Create a changelog string from commits.
 * @param commits - Commits to generate changelog from.
 * @param context - Context for changelog template.
 * @param options - Options for changelog template.
 * @returns Changelog string.
 */
export declare function writeChangelogString<Commit extends CommitKnownProps = CommitKnownProps>(commits: Iterable<Commit> | AsyncIterable<Commit>, context?: Context<Commit>, options?: Options<Commit>): Promise<string>;
//# sourceMappingURL=writers.d.ts.map