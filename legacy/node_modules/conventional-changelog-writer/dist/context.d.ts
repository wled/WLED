import type { CommitKnownProps, CommitGroup, CommitNote, NoteGroup, FinalOptions, Context, FinalContext } from './types/index.js';
export declare function getCommitGroups<Commit extends CommitKnownProps = CommitKnownProps>(commits: Commit[], options: Pick<FinalOptions<Commit>, 'groupBy' | 'commitGroupsSort' | 'commitsSort'>): CommitGroup<Commit>[];
export declare function getNoteGroups<Commit extends CommitKnownProps = CommitKnownProps>(notes: CommitNote[], options: Pick<FinalOptions<Commit>, 'noteGroupsSort' | 'notesSort'>): NoteGroup[];
export declare function getExtraContext<Commit extends CommitKnownProps = CommitKnownProps>(commits: Commit[], notes: CommitNote[], options: Pick<FinalOptions<Commit>, 'groupBy' | 'commitGroupsSort' | 'commitsSort' | 'noteGroupsSort' | 'notesSort'>): {
    commitGroups: CommitGroup<Commit>[];
    noteGroups: NoteGroup[];
};
/**
 * Get final context with default values.
 * @param context
 * @param options
 * @returns Final context with default values.
 */
export declare function getFinalContext<Commit extends CommitKnownProps = CommitKnownProps>(context: Context<Commit>, options: Pick<FinalOptions<Commit>, 'formatDate'>): FinalContext<Commit>;
/**
 * Get context prepared for template.
 * @param keyCommit
 * @param commits
 * @param filteredCommits
 * @param notes
 * @param context
 * @param options
 * @returns Context prepared for template.
 */
export declare function getTemplateContext<Commit extends CommitKnownProps = CommitKnownProps>(keyCommit: Commit | null, commits: Commit[], filteredCommits: Commit[], notes: CommitNote[], context: FinalContext<Commit>, options: FinalOptions<Commit>): Promise<FinalContext<Commit>>;
//# sourceMappingURL=context.d.ts.map