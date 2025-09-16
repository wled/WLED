import { valid as semverValid } from 'semver';
import { formatDate, createComparator } from './utils.js';
const HASH_SHORT_LENGTH = 7;
const HEADER_MAX_LENGTH = 100;
/**
 * Default commit transform function.
 * @param commit
 * @param _context
 * @param options
 * @param options.formatDate - Date formatter function.
 * @returns Patch object for commit.
 */
export function defaultCommitTransform(commit, _context, options) {
    const { hash, header, committerDate } = commit;
    return {
        hash: typeof hash === 'string'
            ? hash.substring(0, HASH_SHORT_LENGTH)
            : hash,
        header: typeof header === 'string'
            ? header.substring(0, HEADER_MAX_LENGTH)
            : header,
        committerDate: committerDate
            ? options.formatDate(committerDate)
            : committerDate
    };
}
/**
 * Get final options object.
 * @param options
 * @param templates
 * @returns Final options object.
 */
export function getFinalOptions(options, templates) {
    const prefinalOptions = {
        groupBy: 'type',
        commitsSort: 'header',
        noteGroupsSort: 'title',
        notesSort: 'text',
        transform: defaultCommitTransform,
        generateOn: (commit) => Boolean(semverValid(commit.version)),
        finalizeContext: (context) => context,
        debug: () => { },
        formatDate,
        reverse: false,
        ignoreReverted: true,
        doFlush: true,
        ...templates,
        ...options
    };
    const finalOptions = {
        ...prefinalOptions,
        commitGroupsSort: createComparator(prefinalOptions.commitGroupsSort),
        commitsSort: createComparator(prefinalOptions.commitsSort),
        noteGroupsSort: createComparator(prefinalOptions.noteGroupsSort),
        notesSort: createComparator(prefinalOptions.notesSort)
    };
    return finalOptions;
}
/**
 * Get final context object.
 * @param context
 * @param options
 * @returns Final context object.
 */
export function getGenerateOnFunction(context, options) {
    const { generateOn } = options;
    if (typeof generateOn === 'string') {
        return (commit) => typeof commit[generateOn] !== 'undefined';
    }
    else if (typeof generateOn !== 'function') {
        return () => false;
    }
    return (keyCommit, commitsGroup) => generateOn(keyCommit, commitsGroup, context, options);
}
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoib3B0aW9ucy5qcyIsInNvdXJjZVJvb3QiOiIiLCJzb3VyY2VzIjpbIi4uL3NyYy9vcHRpb25zLnRzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiJBQUFBLE9BQU8sRUFBRSxLQUFLLElBQUksV0FBVyxFQUFFLE1BQU0sUUFBUSxDQUFBO0FBVTdDLE9BQU8sRUFDTCxVQUFVLEVBQ1YsZ0JBQWdCLEVBQ2pCLE1BQU0sWUFBWSxDQUFBO0FBRW5CLE1BQU0saUJBQWlCLEdBQUcsQ0FBQyxDQUFBO0FBQzNCLE1BQU0saUJBQWlCLEdBQUcsR0FBRyxDQUFBO0FBRTdCOzs7Ozs7O0dBT0c7QUFDSCxNQUFNLFVBQVUsc0JBQXNCLENBQ3BDLE1BQWMsRUFDZCxRQUFpQixFQUNqQixPQUFpRDtJQUVqRCxNQUFNLEVBQ0osSUFBSSxFQUNKLE1BQU0sRUFDTixhQUFhLEVBQ2QsR0FBRyxNQUFNLENBQUE7SUFFVixPQUFPO1FBQ0wsSUFBSSxFQUFFLE9BQU8sSUFBSSxLQUFLLFFBQVE7WUFDNUIsQ0FBQyxDQUFDLElBQUksQ0FBQyxTQUFTLENBQUMsQ0FBQyxFQUFFLGlCQUFpQixDQUFDO1lBQ3RDLENBQUMsQ0FBQyxJQUFJO1FBQ1IsTUFBTSxFQUFFLE9BQU8sTUFBTSxLQUFLLFFBQVE7WUFDaEMsQ0FBQyxDQUFDLE1BQU0sQ0FBQyxTQUFTLENBQUMsQ0FBQyxFQUFFLGlCQUFpQixDQUFDO1lBQ3hDLENBQUMsQ0FBQyxNQUFNO1FBQ1YsYUFBYSxFQUFFLGFBQWE7WUFDMUIsQ0FBQyxDQUFDLE9BQU8sQ0FBQyxVQUFVLENBQUMsYUFBYSxDQUFDO1lBQ25DLENBQUMsQ0FBQyxhQUFhO0tBQ0MsQ0FBQTtBQUN0QixDQUFDO0FBRUQ7Ozs7O0dBS0c7QUFDSCxNQUFNLFVBQVUsZUFBZSxDQUM3QixPQUF3QixFQUN4QixTQUFnQztJQUVoQyxNQUFNLGVBQWUsR0FBRztRQUN0QixPQUFPLEVBQUUsTUFBZTtRQUN4QixXQUFXLEVBQUUsUUFBaUI7UUFDOUIsY0FBYyxFQUFFLE9BQWdCO1FBQ2hDLFNBQVMsRUFBRSxNQUFlO1FBQzFCLFNBQVMsRUFBRSxzQkFBc0I7UUFDakMsVUFBVSxFQUFFLENBQUMsTUFBYyxFQUFFLEVBQUUsQ0FBQyxPQUFPLENBQUMsV0FBVyxDQUFDLE1BQU0sQ0FBQyxPQUFPLENBQUMsQ0FBQztRQUNwRSxlQUFlLEVBQUUsQ0FBQyxPQUE2QixFQUFFLEVBQUUsQ0FBQyxPQUFPO1FBQzNELEtBQUssRUFBRSxHQUFHLEVBQUUsR0FBYyxDQUFDO1FBQzNCLFVBQVU7UUFDVixPQUFPLEVBQUUsS0FBSztRQUNkLGNBQWMsRUFBRSxJQUFJO1FBQ3BCLE9BQU8sRUFBRSxJQUFJO1FBQ2IsR0FBRyxTQUFTO1FBQ1osR0FBRyxPQUFPO0tBQ1gsQ0FBQTtJQUNELE1BQU0sWUFBWSxHQUFHO1FBQ25CLEdBQUcsZUFBZTtRQUNsQixnQkFBZ0IsRUFBRSxnQkFBZ0IsQ0FBQyxlQUFlLENBQUMsZ0JBQWdCLENBQUM7UUFDcEUsV0FBVyxFQUFFLGdCQUFnQixDQUFDLGVBQWUsQ0FBQyxXQUEwQyxDQUFDO1FBQ3pGLGNBQWMsRUFBRSxnQkFBZ0IsQ0FBQyxlQUFlLENBQUMsY0FBYyxDQUFDO1FBQ2hFLFNBQVMsRUFBRSxnQkFBZ0IsQ0FBQyxlQUFlLENBQUMsU0FBUyxDQUFDO0tBQy9CLENBQUE7SUFFekIsT0FBTyxZQUFZLENBQUE7QUFDckIsQ0FBQztBQUVEOzs7OztHQUtHO0FBQ0gsTUFBTSxVQUFVLHFCQUFxQixDQUNuQyxPQUE2QixFQUM3QixPQUE2QjtJQUU3QixNQUFNLEVBQUUsVUFBVSxFQUFFLEdBQUcsT0FBTyxDQUFBO0lBRTlCLElBQUksT0FBTyxVQUFVLEtBQUssUUFBUSxFQUFFO1FBQ2xDLE9BQU8sQ0FBQyxNQUFjLEVBQUUsRUFBRSxDQUFDLE9BQU8sTUFBTSxDQUFDLFVBQVUsQ0FBQyxLQUFLLFdBQVcsQ0FBQTtLQUNyRTtTQUFNLElBQUksT0FBTyxVQUFVLEtBQUssVUFBVSxFQUFFO1FBQzNDLE9BQU8sR0FBRyxFQUFFLENBQUMsS0FBSyxDQUFBO0tBQ25CO0lBRUQsT0FBTyxDQUFDLFNBQWlCLEVBQUUsWUFBc0IsRUFBRSxFQUFFLENBQUMsVUFBVSxDQUFDLFNBQVMsRUFBRSxZQUFZLEVBQUUsT0FBTyxFQUFFLE9BQU8sQ0FBQyxDQUFBO0FBQzdHLENBQUMifQ==