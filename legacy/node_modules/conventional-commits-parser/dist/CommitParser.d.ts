import type { ParserOptions, Commit } from './types.js';
/**
 * Helper to create commit object.
 * @param initialData - Initial commit data.
 * @returns Commit object with empty data.
 */
export declare function createCommitObject(initialData?: Partial<Commit>): Commit;
/**
 * Commit message parser.
 */
export declare class CommitParser {
    private readonly options;
    private readonly regexes;
    private lines;
    private lineIndex;
    private commit;
    constructor(options?: ParserOptions);
    private currentLine;
    private nextLine;
    private isLineAvailable;
    private parseReference;
    private parseReferences;
    private skipEmptyLines;
    private parseMerge;
    private parseHeader;
    private parseMeta;
    private parseNotes;
    private parseBodyAndFooter;
    private parseBreakingHeader;
    private parseMentions;
    private parseRevert;
    private cleanupCommit;
    /**
     * Parse commit message string into an object.
     * @param input - Commit message string.
     * @returns Commit object.
     */
    parse(input: string): Commit;
}
//# sourceMappingURL=CommitParser.d.ts.map