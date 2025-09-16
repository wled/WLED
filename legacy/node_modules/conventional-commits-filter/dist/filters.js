import { Transform } from 'stream';
import { RevertedCommitsFilter } from './RevertedCommitsFilter.js';
/**
 * Filter reverted commits.
 * @param commits
 * @yields Commits without reverted commits.
 */
export async function* filterRevertedCommits(commits) {
    const filter = new RevertedCommitsFilter();
    for await (const commit of commits) {
        yield* filter.process(commit);
    }
    yield* filter.flush();
}
/**
 * Filter reverted commits synchronously.
 * @param commits
 * @yields Commits without reverted commits.
 */
export function* filterRevertedCommitsSync(commits) {
    const filter = new RevertedCommitsFilter();
    for (const commit of commits) {
        yield* filter.process(commit);
    }
    yield* filter.flush();
}
/**
 * Filter reverted commits stream.
 * @returns Reverted commits filter stream.
 */
export function filterRevertedCommitsStream() {
    return Transform.from(filterRevertedCommits);
}
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoiZmlsdGVycy5qcyIsInNvdXJjZVJvb3QiOiIiLCJzb3VyY2VzIjpbIi4uL3NyYy9maWx0ZXJzLnRzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiJBQUFBLE9BQU8sRUFBRSxTQUFTLEVBQUUsTUFBTSxRQUFRLENBQUE7QUFFbEMsT0FBTyxFQUFFLHFCQUFxQixFQUFFLE1BQU0sNEJBQTRCLENBQUE7QUFFbEU7Ozs7R0FJRztBQUNILE1BQU0sQ0FBQyxLQUFLLFNBQVMsQ0FBQyxDQUFDLHFCQUFxQixDQUcxQyxPQUF1QztJQUV2QyxNQUFNLE1BQU0sR0FBRyxJQUFJLHFCQUFxQixFQUFLLENBQUE7SUFFN0MsSUFBSSxLQUFLLEVBQUUsTUFBTSxNQUFNLElBQUksT0FBTyxFQUFFO1FBQ2xDLEtBQUssQ0FBQyxDQUFDLE1BQU0sQ0FBQyxPQUFPLENBQUMsTUFBTSxDQUFDLENBQUE7S0FDOUI7SUFFRCxLQUFLLENBQUMsQ0FBQyxNQUFNLENBQUMsS0FBSyxFQUFFLENBQUE7QUFDdkIsQ0FBQztBQUVEOzs7O0dBSUc7QUFDSCxNQUFNLFNBQVMsQ0FBQyxDQUFDLHlCQUF5QixDQUd4QyxPQUFvQjtJQUVwQixNQUFNLE1BQU0sR0FBRyxJQUFJLHFCQUFxQixFQUFLLENBQUE7SUFFN0MsS0FBSyxNQUFNLE1BQU0sSUFBSSxPQUFPLEVBQUU7UUFDNUIsS0FBSyxDQUFDLENBQUMsTUFBTSxDQUFDLE9BQU8sQ0FBQyxNQUFNLENBQUMsQ0FBQTtLQUM5QjtJQUVELEtBQUssQ0FBQyxDQUFDLE1BQU0sQ0FBQyxLQUFLLEVBQUUsQ0FBQTtBQUN2QixDQUFDO0FBRUQ7OztHQUdHO0FBQ0gsTUFBTSxVQUFVLDJCQUEyQjtJQUN6QyxPQUFPLFNBQVMsQ0FBQyxJQUFJLENBQUMscUJBQXFCLENBQUMsQ0FBQTtBQUM5QyxDQUFDIn0=