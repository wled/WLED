import { Transform } from 'stream';
import { CommitParser } from './CommitParser.js';
/**
 * Create async generator function to parse async iterable of raw commits.
 * @param options - CommitParser options.
 * @returns Async generator function to parse async iterable of raw commits.
 */
export function parseCommits(options = {}) {
    const warnOption = options.warn;
    const warn = warnOption === true
        ? (err) => {
            throw err;
        }
        : warnOption
            ? (err) => warnOption(err.toString())
            : () => { };
    return async function* parse(rawCommits) {
        const parser = new CommitParser(options);
        let rawCommit;
        for await (rawCommit of rawCommits) {
            try {
                yield parser.parse(rawCommit.toString());
            }
            catch (err) {
                warn(err);
            }
        }
    };
}
/**
 * Create stream to parse commits.
 * @param options - CommitParser options.
 * @returns Stream of parsed commits.
 */
export function parseCommitsStream(options = {}) {
    return Transform.from(parseCommits(options));
}
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoic3RyZWFtLmpzIiwic291cmNlUm9vdCI6IiIsInNvdXJjZXMiOlsiLi4vc3JjL3N0cmVhbS50cyJdLCJuYW1lcyI6W10sIm1hcHBpbmdzIjoiQUFBQSxPQUFPLEVBQUUsU0FBUyxFQUFFLE1BQU0sUUFBUSxDQUFBO0FBRWxDLE9BQU8sRUFBRSxZQUFZLEVBQUUsTUFBTSxtQkFBbUIsQ0FBQTtBQUVoRDs7OztHQUlHO0FBQ0gsTUFBTSxVQUFVLFlBQVksQ0FDMUIsVUFBK0IsRUFBRTtJQUVqQyxNQUFNLFVBQVUsR0FBRyxPQUFPLENBQUMsSUFBSSxDQUFBO0lBQy9CLE1BQU0sSUFBSSxHQUFHLFVBQVUsS0FBSyxJQUFJO1FBQzlCLENBQUMsQ0FBQyxDQUFDLEdBQVUsRUFBRSxFQUFFO1lBQ2YsTUFBTSxHQUFHLENBQUE7UUFDWCxDQUFDO1FBQ0QsQ0FBQyxDQUFDLFVBQVU7WUFDVixDQUFDLENBQUMsQ0FBQyxHQUFVLEVBQUUsRUFBRSxDQUFDLFVBQVUsQ0FBQyxHQUFHLENBQUMsUUFBUSxFQUFFLENBQUM7WUFDNUMsQ0FBQyxDQUFDLEdBQUcsRUFBRSxHQUFjLENBQUMsQ0FBQTtJQUUxQixPQUFPLEtBQUssU0FBUyxDQUFDLENBQUMsS0FBSyxDQUMxQixVQUFzRTtRQUV0RSxNQUFNLE1BQU0sR0FBRyxJQUFJLFlBQVksQ0FBQyxPQUFPLENBQUMsQ0FBQTtRQUN4QyxJQUFJLFNBQTBCLENBQUE7UUFFOUIsSUFBSSxLQUFLLEVBQUUsU0FBUyxJQUFJLFVBQVUsRUFBRTtZQUNsQyxJQUFJO2dCQUNGLE1BQU0sTUFBTSxDQUFDLEtBQUssQ0FBQyxTQUFTLENBQUMsUUFBUSxFQUFFLENBQUMsQ0FBQTthQUN6QztZQUFDLE9BQU8sR0FBRyxFQUFFO2dCQUNaLElBQUksQ0FBQyxHQUFZLENBQUMsQ0FBQTthQUNuQjtTQUNGO0lBQ0gsQ0FBQyxDQUFBO0FBQ0gsQ0FBQztBQUVEOzs7O0dBSUc7QUFDSCxNQUFNLFVBQVUsa0JBQWtCLENBQUMsVUFBK0IsRUFBRTtJQUNsRSxPQUFPLFNBQVMsQ0FBQyxJQUFJLENBQUMsWUFBWSxDQUFDLE9BQU8sQ0FBQyxDQUFDLENBQUE7QUFDOUMsQ0FBQyJ9