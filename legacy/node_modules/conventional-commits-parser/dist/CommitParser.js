import { getParserRegexes } from './regex.js';
import { trimNewLines, appendLine, getCommentFilter, gpgFilter, truncateToScissor } from './utils.js';
import { defaultOptions } from './options.js';
/**
 * Helper to create commit object.
 * @param initialData - Initial commit data.
 * @returns Commit object with empty data.
 */
export function createCommitObject(initialData = {}) {
    // @ts-expect-error: You can read properties from `Commit` without problems, but you can't assign object to this type. So here is helper for that.
    return {
        merge: null,
        revert: null,
        header: null,
        body: null,
        footer: null,
        notes: [],
        mentions: [],
        references: [],
        ...initialData
    };
}
/**
 * Commit message parser.
 */
export class CommitParser {
    options;
    regexes;
    lines = [];
    lineIndex = 0;
    commit = createCommitObject();
    constructor(options = {}) {
        this.options = {
            ...defaultOptions,
            ...options
        };
        this.regexes = getParserRegexes(this.options);
    }
    currentLine() {
        return this.lines[this.lineIndex];
    }
    nextLine() {
        return this.lines[this.lineIndex++];
    }
    isLineAvailable() {
        return this.lineIndex < this.lines.length;
    }
    parseReference(input, action) {
        const { regexes } = this;
        if (regexes.url.test(input)) {
            return null;
        }
        const matches = regexes.referenceParts.exec(input);
        if (!matches) {
            return null;
        }
        let [raw, repository = null, prefix, issue] = matches;
        let owner = null;
        if (repository) {
            const slashIndex = repository.indexOf('/');
            if (slashIndex !== -1) {
                owner = repository.slice(0, slashIndex);
                repository = repository.slice(slashIndex + 1);
            }
        }
        return {
            raw,
            action,
            owner,
            repository,
            prefix,
            issue
        };
    }
    parseReferences(input) {
        const { regexes } = this;
        const regex = input.match(regexes.references)
            ? regexes.references
            : /()(.+)/gi;
        const references = [];
        let matches;
        let action;
        let sentence;
        let reference;
        while (true) {
            matches = regex.exec(input);
            if (!matches) {
                break;
            }
            action = matches[1] || null;
            sentence = matches[2] || '';
            while (true) {
                reference = this.parseReference(sentence, action);
                if (!reference) {
                    break;
                }
                references.push(reference);
            }
        }
        return references;
    }
    skipEmptyLines() {
        let line = this.currentLine();
        while (line !== undefined && !line.trim()) {
            this.nextLine();
            line = this.currentLine();
        }
    }
    parseMerge() {
        const { commit, options } = this;
        const correspondence = options.mergeCorrespondence || [];
        const merge = this.currentLine();
        const matches = merge && options.mergePattern
            ? merge.match(options.mergePattern)
            : null;
        if (matches) {
            this.nextLine();
            commit.merge = matches[0] || null;
            correspondence.forEach((key, index) => {
                commit[key] = matches[index + 1] || null;
            });
            return true;
        }
        return false;
    }
    parseHeader(isMergeCommit) {
        if (isMergeCommit) {
            this.skipEmptyLines();
        }
        const { commit, options } = this;
        const correspondence = options.headerCorrespondence || [];
        const header = commit.header ?? this.nextLine();
        let matches = null;
        if (header) {
            if (options.breakingHeaderPattern) {
                matches = header.match(options.breakingHeaderPattern);
            }
            if (!matches && options.headerPattern) {
                matches = header.match(options.headerPattern);
            }
        }
        if (header) {
            commit.header = header;
        }
        if (matches) {
            correspondence.forEach((key, index) => {
                commit[key] = matches[index + 1] || null;
            });
        }
    }
    parseMeta() {
        const { options, commit } = this;
        if (!options.fieldPattern || !this.isLineAvailable()) {
            return false;
        }
        let matches;
        let field = null;
        let parsed = false;
        while (this.isLineAvailable()) {
            matches = this.currentLine().match(options.fieldPattern);
            if (matches) {
                field = matches[1] || null;
                this.nextLine();
                continue;
            }
            if (field) {
                parsed = true;
                commit[field] = appendLine(commit[field], this.currentLine());
                this.nextLine();
            }
            else {
                break;
            }
        }
        return parsed;
    }
    parseNotes() {
        const { regexes, commit } = this;
        if (!this.isLineAvailable()) {
            return false;
        }
        const matches = this.currentLine().match(regexes.notes);
        let references = [];
        if (matches) {
            const note = {
                title: matches[1],
                text: matches[2]
            };
            commit.notes.push(note);
            commit.footer = appendLine(commit.footer, this.currentLine());
            this.nextLine();
            while (this.isLineAvailable()) {
                if (this.parseMeta()) {
                    return true;
                }
                if (this.parseNotes()) {
                    return true;
                }
                references = this.parseReferences(this.currentLine());
                if (references.length) {
                    commit.references.push(...references);
                }
                else {
                    note.text = appendLine(note.text, this.currentLine());
                }
                commit.footer = appendLine(commit.footer, this.currentLine());
                this.nextLine();
                if (references.length) {
                    break;
                }
            }
            return true;
        }
        return false;
    }
    parseBodyAndFooter(isBody) {
        const { commit } = this;
        if (!this.isLineAvailable()) {
            return isBody;
        }
        const references = this.parseReferences(this.currentLine());
        const isStillBody = !references.length && isBody;
        if (isStillBody) {
            commit.body = appendLine(commit.body, this.currentLine());
        }
        else {
            commit.references.push(...references);
            commit.footer = appendLine(commit.footer, this.currentLine());
        }
        this.nextLine();
        return isStillBody;
    }
    parseBreakingHeader() {
        const { commit, options } = this;
        if (!options.breakingHeaderPattern || commit.notes.length || !commit.header) {
            return;
        }
        const matches = commit.header.match(options.breakingHeaderPattern);
        if (matches) {
            commit.notes.push({
                title: 'BREAKING CHANGE',
                text: matches[3]
            });
        }
    }
    parseMentions(input) {
        const { commit, regexes } = this;
        let matches;
        for (;;) {
            matches = regexes.mentions.exec(input);
            if (!matches) {
                break;
            }
            commit.mentions.push(matches[1]);
        }
    }
    parseRevert(input) {
        const { commit, options } = this;
        const correspondence = options.revertCorrespondence || [];
        const matches = options.revertPattern
            ? input.match(options.revertPattern)
            : null;
        if (matches) {
            commit.revert = correspondence.reduce((meta, key, index) => {
                meta[key] = matches[index + 1] || null;
                return meta;
            }, {});
        }
    }
    cleanupCommit() {
        const { commit } = this;
        if (commit.body) {
            commit.body = trimNewLines(commit.body);
        }
        if (commit.footer) {
            commit.footer = trimNewLines(commit.footer);
        }
        commit.notes.forEach((note) => {
            note.text = trimNewLines(note.text);
        });
    }
    /**
     * Parse commit message string into an object.
     * @param input - Commit message string.
     * @returns Commit object.
     */
    parse(input) {
        if (!input.trim()) {
            throw new TypeError('Expected a raw commit');
        }
        const commentFilter = getCommentFilter(this.options.commentChar);
        const rawLines = trimNewLines(input).split(/\r?\n/);
        const lines = truncateToScissor(rawLines).filter(line => commentFilter(line) && gpgFilter(line));
        const commit = createCommitObject();
        this.lines = lines;
        this.lineIndex = 0;
        this.commit = commit;
        const isMergeCommit = this.parseMerge();
        this.parseHeader(isMergeCommit);
        if (commit.header) {
            commit.references = this.parseReferences(commit.header);
        }
        let isBody = true;
        while (this.isLineAvailable()) {
            this.parseMeta();
            if (this.parseNotes()) {
                isBody = false;
            }
            if (!this.parseBodyAndFooter(isBody)) {
                isBody = false;
            }
        }
        this.parseBreakingHeader();
        this.parseMentions(input);
        this.parseRevert(input);
        this.cleanupCommit();
        return commit;
    }
}
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoiQ29tbWl0UGFyc2VyLmpzIiwic291cmNlUm9vdCI6IiIsInNvdXJjZXMiOlsiLi4vc3JjL0NvbW1pdFBhcnNlci50cyJdLCJuYW1lcyI6W10sIm1hcHBpbmdzIjoiQUFRQSxPQUFPLEVBQUUsZ0JBQWdCLEVBQUUsTUFBTSxZQUFZLENBQUE7QUFDN0MsT0FBTyxFQUNMLFlBQVksRUFDWixVQUFVLEVBQ1YsZ0JBQWdCLEVBQ2hCLFNBQVMsRUFDVCxpQkFBaUIsRUFDbEIsTUFBTSxZQUFZLENBQUE7QUFDbkIsT0FBTyxFQUFFLGNBQWMsRUFBRSxNQUFNLGNBQWMsQ0FBQTtBQUU3Qzs7OztHQUlHO0FBQ0gsTUFBTSxVQUFVLGtCQUFrQixDQUFDLGNBQStCLEVBQUU7SUFDbEUsa0pBQWtKO0lBQ2xKLE9BQU87UUFDTCxLQUFLLEVBQUUsSUFBSTtRQUNYLE1BQU0sRUFBRSxJQUFJO1FBQ1osTUFBTSxFQUFFLElBQUk7UUFDWixJQUFJLEVBQUUsSUFBSTtRQUNWLE1BQU0sRUFBRSxJQUFJO1FBQ1osS0FBSyxFQUFFLEVBQUU7UUFDVCxRQUFRLEVBQUUsRUFBRTtRQUNaLFVBQVUsRUFBRSxFQUFFO1FBQ2QsR0FBRyxXQUFXO0tBQ2YsQ0FBQTtBQUNILENBQUM7QUFFRDs7R0FFRztBQUNILE1BQU0sT0FBTyxZQUFZO0lBQ04sT0FBTyxDQUFlO0lBQ3RCLE9BQU8sQ0FBZTtJQUMvQixLQUFLLEdBQWEsRUFBRSxDQUFBO0lBQ3BCLFNBQVMsR0FBRyxDQUFDLENBQUE7SUFDYixNQUFNLEdBQUcsa0JBQWtCLEVBQUUsQ0FBQTtJQUVyQyxZQUFZLFVBQXlCLEVBQUU7UUFDckMsSUFBSSxDQUFDLE9BQU8sR0FBRztZQUNiLEdBQUcsY0FBYztZQUNqQixHQUFHLE9BQU87U0FDWCxDQUFBO1FBQ0QsSUFBSSxDQUFDLE9BQU8sR0FBRyxnQkFBZ0IsQ0FBQyxJQUFJLENBQUMsT0FBTyxDQUFDLENBQUE7SUFDL0MsQ0FBQztJQUVPLFdBQVc7UUFDakIsT0FBTyxJQUFJLENBQUMsS0FBSyxD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