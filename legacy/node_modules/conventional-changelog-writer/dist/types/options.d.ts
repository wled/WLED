import type { Comparator, PickStringsKeys } from './utils.js';
import type { CommitKnownProps, CommitGroup, CommitNote, NoteGroup } from './commit.js';
import type { FinalContext } from './context.js';
export type SortBy<T> = PickStringsKeys<T> | PickStringsKeys<T>[] | Comparator<T>;
export type CommitTransformFunction<Commit extends CommitKnownProps = CommitKnownProps> = (commit: Commit, context: FinalContext<Commit>, options: FinalOptions<Commit>) => Partial<Commit> | null | Promise<Partial<Commit> | null>;
export type GenerateOnFunction<Commit extends CommitKnownProps = CommitKnownProps> = (keyCommit: Commit, commitsGroup: Commit[], context: FinalContext<Commit>, options: FinalOptions<Commit>) => boolean;
export interface TemplatesOptions {
    mainTemplate?: string;
    headerPartial?: string;
    commitPartial?: string;
    footerPartial?: string;
    partials?: Record<string, string | null>;
}
type RequiredTemplatesOptions = Required<TemplatesOptions>;
export interface FinalTemplatesOptions extends TemplatesOptions {
    mainTemplate: RequiredTemplatesOptions['mainTemplate'];
    headerPartial: RequiredTemplatesOptions['headerPartial'];
    commitPartial: RequiredTemplatesOptions['commitPartial'];
    footerPartial: RequiredTemplatesOptions['footerPartial'];
}
export interface Options<Commit extends CommitKnownProps = CommitKnownProps> extends TemplatesOptions {
    /**
     * Key to group commits by.
     * If this value is falsy, commits are not grouped.
     */
    groupBy?: keyof Commit;
    /**
     * Key to sort commits by or sort function.
     * If this value is falsy, commits are not sorted.
     */
    commitsSort?: SortBy<Commit>;
    /**
     * Key to sort commit groups by or sort function.
     * If this value is falsy, commit groups are not sorted.
     */
    commitGroupsSort?: SortBy<CommitGroup<Commit>>;
    /**
     * Key to sort notes by or sort function.
     * If this value is falsy, notes are not sorted.
     */
    notesSort?: SortBy<CommitNote>;
    /**
     * Key to sort note groups by or sort function.
     * If this value is falsy, note groups are not sorted.
     */
    noteGroupsSort?: SortBy<NoteGroup>;
    /**
     * If `true`, reverted commits will be ignored.
     */
    ignoreReverted?: boolean;
    /**
     * The normal order means reverse chronological order.
     * `reverse` order means chronological order.
     */
    reverse?: boolean;
    /**
     * If `true`, the stream will flush out the last bit of commits (could be empty) to changelog.
     */
    doFlush?: boolean;
    /**
     * A function to transform commits.
     * Should return diff object which will be merged with the original commit.
     */
    transform?: CommitTransformFunction<Commit>;
    /**
     * Function or key to detect whether a changelog block should be generated.
     */
    generateOn?: GenerateOnFunction<Commit> | keyof Commit | null;
    /**
     * Last chance to modify your context before generating a changelog.
     * @param context
     * @param options
     * @param filteredCommits
     * @param keyCommit
     * @param commits
     */
    finalizeContext?(context: FinalContext<Commit>, options: FinalOptions<Commit>, filteredCommits: Commit[], keyCommit: Commit | null, commits: Commit[]): FinalContext<Commit> | Promise<FinalContext<Commit>>;
    /**
     * A function to get debug information.
     * @param message - The debug message.
     */
    debug?(message: string): void;
    /**
     * A function to format date.
     * @param date - Date string or Date object.
     * @returns Final date string.
     */
    formatDate?(date: string | Date): string;
}
type RequiredOptions<Commit extends CommitKnownProps = CommitKnownProps> = Required<Options<Commit>>;
export interface FinalOptions<Commit extends CommitKnownProps = CommitKnownProps> extends Options<Commit> {
    groupBy: RequiredOptions<Commit>['groupBy'];
    generateOn: RequiredOptions<Commit>['generateOn'];
    finalizeContext: RequiredOptions<Commit>['finalizeContext'];
    debug: RequiredOptions<Commit>['debug'];
    formatDate: RequiredOptions<Commit>['formatDate'];
    transform: RequiredOptions<Commit>['transform'];
    commitsSort?: Comparator<Commit>;
    commitGroupsSort?: Comparator<CommitGroup<Commit>>;
    notesSort?: Comparator<CommitNote>;
    noteGroupsSort?: Comparator<NoteGroup>;
    mainTemplate: FinalTemplatesOptions['mainTemplate'];
    headerPartial: FinalTemplatesOptions['headerPartial'];
    commitPartial: FinalTemplatesOptions['commitPartial'];
    footerPartial: FinalTemplatesOptions['footerPartial'];
}
export {};
//# sourceMappingURL=options.d.ts.map