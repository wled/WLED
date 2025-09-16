import type { CommitKnownProps, CommitGroup, NoteGroup } from './commit.js';
export interface Context<Commit extends CommitKnownProps = CommitKnownProps> {
    /**
     * Version number of the up-coming release.
     * If `version` is found in the last commit before generating logs, it will be overwritten.
     */
    version?: string;
    /**
     * Is this a patch release?
     */
    isPatch?: boolean;
    /**
     * The title of the release.
     */
    title?: string;
    /**
     * The release date.
     */
    date?: string;
    /**
     * Should all references be linked?
     */
    linkReferences?: boolean;
    /**
     * Commit base url path.
     */
    commit?: string;
    /**
     * Issue base url.
     */
    issue?: string;
    /**
     * Repository name.
     * Eg: `'conventional-changelog-writer'`
     */
    repository?: string;
    /**
     * Repository host.
     * Eg: `'https://github.com'` or `'https://bitbucket.org'`
     */
    host?: string;
    /**
     * Repository owner.
     * Eg: `'conventional-changelog'`
     */
    owner?: string;
    /**
     * Full repository url.
     * Eg: `'https://github.com/conventional-changelog/conventional-changelog-writer'`.
     * Should be used as a fallback when `context.repository` doesn't exist.
     */
    repoUrl?: string;
    /**
     * Commit groups.
     */
    commitGroups?: CommitGroup<Commit>[];
    /**
     * Note groups.
     */
    noteGroups?: NoteGroup[];
}
type RequiredContext<Commit extends CommitKnownProps = CommitKnownProps> = Required<Context<Commit>>;
export interface FinalContext<Commit extends CommitKnownProps = CommitKnownProps> extends Context<Commit> {
    commit: RequiredContext<Commit>['commit'];
    issue: RequiredContext<Commit>['issue'];
    date: RequiredContext<Commit>['date'];
}
export {};
//# sourceMappingURL=context.d.ts.map