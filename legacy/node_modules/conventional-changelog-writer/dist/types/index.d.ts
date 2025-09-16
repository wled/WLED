import type { CommitKnownProps } from './commit.js';
export * from './commit.js';
export * from './context.js';
export * from './options.js';
export * from './utils.js';
export interface Details<Commit extends CommitKnownProps = CommitKnownProps> {
    log: string;
    keyCommit: Commit | null;
}
//# sourceMappingURL=index.d.ts.map