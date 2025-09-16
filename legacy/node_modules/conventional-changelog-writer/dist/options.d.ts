import type { Options, FinalTemplatesOptions, FinalOptions, FinalContext, CommitKnownProps } from './types/index.js';
/**
 * Default commit transform function.
 * @param commit
 * @param _context
 * @param options
 * @param options.formatDate - Date formatter function.
 * @returns Patch object for commit.
 */
export declare function defaultCommitTransform<Commit extends CommitKnownProps = CommitKnownProps>(commit: Commit, _context: unknown, options: Pick<FinalOptions<Commit>, 'formatDate'>): Partial<Commit>;
/**
 * Get final options object.
 * @param options
 * @param templates
 * @returns Final options object.
 */
export declare function getFinalOptions<Commit extends CommitKnownProps = CommitKnownProps>(options: Options<Commit>, templates: FinalTemplatesOptions): FinalOptions<Commit>;
/**
 * Get final context object.
 * @param context
 * @param options
 * @returns Final context object.
 */
export declare function getGenerateOnFunction<Commit extends CommitKnownProps = CommitKnownProps>(context: FinalContext<Commit>, options: FinalOptions<Commit>): (keyCommit: Commit, commitsGroup: Commit[]) => boolean;
//# sourceMappingURL=options.d.ts.map