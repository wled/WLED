import type { ParserOptions, ParserRegexes } from './types.js';
/**
 * Make the regexes used to parse a commit.
 * @param options
 * @returns Regexes.
 */
export declare function getParserRegexes(options?: Pick<ParserOptions, 'noteKeywords' | 'notesPattern' | 'issuePrefixes' | 'issuePrefixesCaseSensitive' | 'referenceActions'>): ParserRegexes;
//# sourceMappingURL=regex.d.ts.map