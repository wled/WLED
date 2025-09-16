import type { CommitKnownProps, CommitTransformFunction, FinalContext, FinalOptions, TransformedCommit } from './types/index.js';
/**
 * Apply transformation to commit.
 * @param commit
 * @param transform
 * @param context
 * @param options
 * @returns Transformed commit.
 */
export declare function transformCommit<Commit extends CommitKnownProps = CommitKnownProps>(commit: Commit, transform: CommitTransformFunction<Commit> | null | undefined, context: FinalContext<Commit>, options: FinalOptions<Commit>): Promise<TransformedCommit<Commit> | null>;
//# sourceMappingURL=commit.d.ts.map