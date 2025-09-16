export interface ParserOptions {
    /**
     * Character used to comment out a line.
     */
    commentChar?: string;
    /**
     * Pattern to match merge headers. EG: branch merge, GitHub or GitLab like pull requests headers.
     * When a merge header is parsed, the next line is used for conventional header parsing.
     */
    mergePattern?: RegExp;
    /**
     * Used to define what capturing group of `mergePattern`.
     */
    mergeCorrespondence?: string[];
    /**
     * Used to match header pattern.
     */
    headerPattern?: RegExp;
    /**
     * Breaking changes header pattern.
     */
    breakingHeaderPattern?: RegExp;
    /**
     * Used to define what capturing group of `headerPattern` captures what header part.
     * The order of the array should correspond to the order of `headerPattern`'s capturing group.
     */
    headerCorrespondence?: string[];
    /**
     * Pattern to match what this commit reverts.
     */
    revertPattern?: RegExp;
    /**
     * Used to define what capturing group of `revertPattern` captures what reverted commit fields.
     * The order of the array should correspond to the order of `revertPattern`'s capturing group.
     */
    revertCorrespondence?: string[];
    /**
     * Pattern to match other fields.
     */
    fieldPattern?: RegExp;
    /**
     * Keywords for important notes. This value is case **insensitive**.
     */
    noteKeywords?: string[];
    /**
     * A function that takes `noteKeywordsSelection` and returns a `RegExp` to be matched against the notes.
     */
    notesPattern?(text: string): RegExp;
    /**
     * The prefixes of an issue. EG: In `gh-123` `gh-` is the prefix.
     */
    issuePrefixes?: string[];
    /**
     * Used to define if `issuePrefixes` should be considered case sensitive.
     */
    issuePrefixesCaseSensitive?: boolean;
    /**
     * Keywords to reference an issue. This value is case **insensitive**.
     */
    referenceActions?: string[];
}
export interface ParserStreamOptions extends ParserOptions {
    /**
     * What warn function to use. For example, `console.warn.bind(console)`. By default, it's a noop. If it is `true`, it will error if commit cannot be parsed (strict).
     */
    warn?: boolean | ((message: string) => void);
}
export interface ParserRegexes {
    notes: RegExp;
    referenceParts: RegExp;
    references: RegExp;
    mentions: RegExp;
    url: RegExp;
}
export interface CommitReference {
    raw: string;
    action: string | null;
    owner: string | null;
    repository: string | null;
    issue: string;
    prefix: string;
}
export interface CommitNote {
    title: string;
    text: string;
}
export type CommitMeta = Record<string, string | null>;
export interface CommitBase {
    merge: string | null;
    revert: CommitMeta | null;
    header: string | null;
    body: string | null;
    footer: string | null;
    notes: CommitNote[];
    mentions: string[];
    references: CommitReference[];
}
export type Commit = CommitBase & CommitMeta;
//# sourceMappingURL=types.d.ts.map