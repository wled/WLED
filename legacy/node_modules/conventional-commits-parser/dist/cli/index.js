#!/usr/bin/env node
import { pipeline } from 'stream/promises';
import meow from 'meow';
import { parseCommits } from '../index.js';
import { parseOptions } from './options.js';
import { readRawCommitsFromFiles, readRawCommitsFromLine, readRawCommitsFromStdin, stringify } from './utils.js';
const DEFAULT_SEPARATOR = '\n\n\n';
const cli = meow(`
    Practice writing commit messages or parse messages from files.
    If used without specifying a text file path, you will enter an interactive shell.
    Otherwise the commit messages in the files are parsed and printed
    By default, commits will be split by three newlines ('\\n\\n\\n') or you can specify a separator.

    Usage
      conventional-commits-parser [-s <commit-separator>]
      conventional-commits-parser [-s <commit-separator>] <path> [<path> ...]
      cat <path> | conventional-commits-parser [-s <commit-separator>]

    Example
      conventional-commits-parser
      conventional-commits-parser log.txt
      cat log.txt | conventional-commits-parser
      conventional-commits-parser log2.txt -s '===' >> parsed.txt

    Options
      -s, --separator                   Commit separator
      -p, --header-pattern              Regex to match header pattern
      -c, --header-correspondence       Comma separated parts used to define what capturing group of 'headerPattern' captures what
      -r, --reference-actions           Comma separated keywords that used to reference issues
      -i, --issue-prefixes              Comma separated prefixes of an issue
      --issue-prefixes-case-sensitive   Treat issue prefixes as case sensitive
      -n, --note-keywords               Comma separated keywords for important notes
      -f, --field-pattern               Regex to match other fields
      --revert-pattern                  Regex to match revert pattern
      --revert-correspondence           Comma separated fields used to define what the commit reverts
      -v, --verbose                     Verbose output
`, {
    importMeta: import.meta,
    flags: {
        separator: {
            shortFlag: 's',
            type: 'string',
            default: DEFAULT_SEPARATOR
        },
        headerPattern: {
            shortFlag: 'p',
            type: 'string'
        },
        headerCorrespondence: {
            shortFlag: 'c',
            type: 'string'
        },
        referenceActions: {
            shortFlag: 'r',
            type: 'string'
        },
        issuePrefixes: {
            shortFlag: 'i',
            type: 'string'
        },
        issuePrefixesCaseSensitive: {
            type: 'boolean'
        },
        noteKeywords: {
            shortFlag: 'n',
            type: 'string'
        },
        fieldPattern: {
            shortFlag: 'f',
            type: 'string'
        },
        revertPattern: {
            type: 'string'
        },
        revertCorrespondence: {
            type: 'string'
        },
        verbose: {
            shortFlag: 'v',
            type: 'boolean'
        }
    }
});
const { separator } = cli.flags;
const options = parseOptions(cli.flags);
let inputStream;
try {
    if (cli.input.length) {
        inputStream = readRawCommitsFromFiles(cli.input, separator);
    }
    else if (process.stdin.isTTY) {
        inputStream = readRawCommitsFromLine(separator);
    }
    else {
        inputStream = readRawCommitsFromStdin(separator);
    }
    await pipeline(inputStream, parseCommits(options), stringify, process.stdout);
}
catch (err) {
    console.error(err);
    process.exit(1);
}
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoiaW5kZXguanMiLCJzb3VyY2VSb290IjoiIiwic291cmNlcyI6WyIuLi8uLi9zcmMvY2xpL2luZGV4LnRzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiI7QUFDQSxPQUFPLEVBQUUsUUFBUSxFQUFFLE1BQU0saUJBQWlCLENBQUE7QUFDMUMsT0FBTyxJQUFJLE1BQU0sTUFBTSxDQUFBO0FBQ3ZCLE9BQU8sRUFBRSxZQUFZLEVBQUUsTUFBTSxhQUFhLENBQUE7QUFDMUMsT0FBTyxFQUFFLFlBQVksRUFBRSxNQUFNLGNBQWMsQ0FBQTtBQUMzQyxPQUFPLEVBQ0wsdUJBQXVCLEVBQ3ZCLHNCQUFzQixFQUN0Qix1QkFBdUIsRUFDdkIsU0FBUyxFQUNWLE1BQU0sWUFBWSxDQUFBO0FBRW5CLE1BQU0saUJBQWlCLEdBQUcsUUFBUSxDQUFBO0FBQ2xDLE1BQU0sR0FBRyxHQUFHLElBQUksQ0FBQzs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Q0E2QmhCLEVBQUU7SUFDRCxVQUFVLEVBQUUsTUFBTSxDQUFDLElBQUk7SUFDdkIsS0FBSyxFQUFFO1FBQ0wsU0FBUyxFQUFFO1lBQ1QsU0FBUyxFQUFFLEdBQUc7WUFDZCxJQUFJLEVBQUUsUUFBUTtZQUNkLE9BQU8sRUFBRSxpQkFBaUI7U0FDM0I7UUFDRCxhQUFhLEVBQUU7WUFDYixTQUFTLEVBQUUsR0FBRztZQUNkLElBQUksRUFBRSxRQUFRO1NBQ2Y7UUFDRCxvQkFBb0IsRUFBRTtZQUNwQixTQUFTLEVBQUUsR0FBRztZQUNkLElBQUksRUFBRSxRQUFRO1NBQ2Y7UUFDRCxnQkFBZ0IsRUFBRTtZQUNoQixTQUFTLEVBQUUsR0FBRztZQUNkLElBQUksRUFBRSxRQUFRO1NBQ2Y7UUFDRCxhQUFhLEVBQUU7WUFDYixTQUFTLEVBQUUsR0FBRztZQUNkLElBQUksRUFBRSxRQUFRO1NBQ2Y7UUFDRCwwQkFBMEIsRUFBRTtZQUMxQixJQUFJLEVBQUUsU0FBUztTQUNoQjtRQUNELFlBQVksRUFBRTtZQUNaLFNBQVMsRUFBRSxHQUFHO1lBQ2QsSUFBSSxFQUFFLFFBQVE7U0FDZjtRQUNELFlBQVksRUFBRTtZQUNaLFNBQVMsRUFBRSxHQUFHO1lBQ2QsSUFBSSxFQUFFLFFBQVE7U0FDZjtRQUNELGFBQWEsRUFBRTtZQUNiLElBQUksRUFBRSxRQUFRO1NBQ2Y7UUFDRCxvQkFBb0IsRUFBRTtZQUNwQixJQUFJLEVBQUUsUUFBUTtTQUNmO1FBQ0QsT0FBTyxFQUFFO1lBQ1AsU0FBUyxFQUFFLEdBQUc7WUFDZCxJQUFJLEVBQUUsU0FBUztTQUNoQjtLQUNGO0NBQ0YsQ0FBQyxDQUFBO0FBQ0YsTUFBTSxFQUFFLFNBQVMsRUFBRSxHQUFHLEdBQUcsQ0FBQyxLQUFLLENBQUE7QUFDL0IsTUFBTSxPQUFPLEdBQUcsWUFBWSxDQUFDLEdBQUcsQ0FBQyxLQUFLLENBQUMsQ0FBQTtBQUN2QyxJQUFJLFdBQWtDLENBQUE7QUFFdEMsSUFBSTtJQUNGLElBQUksR0FBRyxDQUFDLEtBQUssQ0FBQyxNQUFNLEVBQUU7UUFDcEIsV0FBVyxHQUFHLHVCQUF1QixDQUFDLEdBQUcsQ0FBQyxLQUFLLEVBQUUsU0FBUyxDQUFDLENBQUE7S0FDNUQ7U0FDQyxJQUFJLE9BQU8sQ0FBQyxLQUFLLENBQUMsS0FBSyxFQUFFO1FBQ3ZCLFdBQVcsR0FBRyxzQkFBc0IsQ0FBQyxTQUFTLENBQUMsQ0FBQTtLQUNoRDtTQUFNO1FBQ0wsV0FBVyxHQUFHLHVCQUF1QixDQUFDLFNBQVMsQ0FBQyxDQUFBO0tBQ2pEO0lBRUgsTUFBTSxRQUFRLENBQ1osV0FBVyxFQUNYLFlBQVksQ0FBQyxPQUFPLENBQUMsRUFDckIsU0FBUyxFQUNULE9BQU8sQ0FBQyxNQUFNLENBQ2YsQ0FBQTtDQUNGO0FBQUMsT0FBTyxHQUFHLEVBQUU7SUFDWixPQUFPLENBQUMsS0FBSyxDQUFDLEdBQUcsQ0FBQyxDQUFBO0lBQ2xCLE9BQU8sQ0FBQyxJQUFJLENBQUMsQ0FBQyxDQUFDLENBQUE7Q0FDaEIifQ==