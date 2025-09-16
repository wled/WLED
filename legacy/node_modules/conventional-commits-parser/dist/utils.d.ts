/**
 * Remove leading and trailing newlines.
 * @param input
 * @returns String without leading and trailing newlines.
 */
export declare function trimNewLines(input: string): string;
/**
 * Append a newline to a string.
 * @param src
 * @param line
 * @returns String with appended newline.
 */
export declare function appendLine(src: string | null, line: string | undefined): string;
/**
 * Creates a function that filters out comments lines.
 * @param char
 * @returns Comment filter function.
 */
export declare function getCommentFilter(char: string | undefined): (line: string) => boolean;
/**
 * Select lines before the scissor.
 * @param lines
 * @returns Lines before the scissor.
 */
export declare function truncateToScissor(lines: string[]): string[];
/**
 * Filter out GPG sign lines.
 * @param line
 * @returns True if the line is not a GPG sign line.
 */
export declare function gpgFilter(line: string): boolean;
//# sourceMappingURL=utils.d.ts.map