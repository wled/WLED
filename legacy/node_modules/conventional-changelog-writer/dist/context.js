import semver from 'semver';
import { stringify } from './utils.js';
export function getCommitGroups(commits, options) {
    const { groupBy, commitGroupsSort, commitsSort } = options;
    const commitGroups = [];
    const commitGroupsObj = commits.reduce((groups, commit) => {
        const key = commit[groupBy] || '';
        if (groups[key]) {
            groups[key].push(commit);
        }
        else {
            groups[key] = [commit];
        }
        return groups;
    }, {});
    Object.entries(commitGroupsObj).forEach(([title, commits]) => {
        if (commitsSort) {
            commits.sort(commitsSort);
        }
        commitGroups.push({
            title,
            commits
        });
    });
    if (commitGroupsSort) {
        commitGroups.sort(commitGroupsSort);
    }
    return commitGroups;
}
export function getNoteGroups(notes, options) {
    const { noteGroupsSort, notesSort } = options;
    const retGroups = [];
    notes.forEach((note) => {
        const { title } = note;
        let titleExists = false;
        retGroups.forEach((group) => {
            if (group.title === title) {
                titleExists = true;
                group.notes.push(note);
            }
        });
        if (!titleExists) {
            retGroups.push({
                title,
                notes: [note]
            });
        }
    });
    if (noteGroupsSort) {
        retGroups.sort(noteGroupsSort);
    }
    if (notesSort) {
        retGroups.forEach((group) => {
            group.notes.sort(notesSort);
        });
    }
    return retGroups;
}
export function getExtraContext(commits, notes, options) {
    return {
        // group `commits` by `options.groupBy`
        commitGroups: getCommitGroups(commits, options),
        // group `notes` for footer
        noteGroups: getNoteGroups(notes, options)
    };
}
/**
 * Get final context with default values.
 * @param context
 * @param options
 * @returns Final context with default values.
 */
export function getFinalContext(context, options) {
    const finalContext = {
        commit: 'commits',
        issue: 'issues',
        date: options.formatDate(new Date()),
        ...context
    };
    if (typeof finalContext.linkReferences !== 'boolean'
        && (finalContext.repository || finalContext.repoUrl)
        && finalContext.commit
        && finalContext.issue) {
        finalContext.linkReferences = true;
    }
    return finalContext;
}
/**
 * Get context prepared for template.
 * @param keyCommit
 * @param commits
 * @param filteredCommits
 * @param notes
 * @param context
 * @param options
 * @returns Context prepared for template.
 */
export async function getTemplateContext(keyCommit, commits, filteredCommits, notes, context, options) {
    let templateContext = {
        ...context,
        ...keyCommit,
        ...getExtraContext(filteredCommits, notes, options)
    };
    if (keyCommit?.committerDate) {
        templateContext.date = keyCommit.committerDate;
    }
    if (templateContext.version && semver.valid(templateContext.version)) {
        templateContext.isPatch = templateContext.isPatch || semver.patch(templateContext.version) !== 0;
    }
    templateContext = await options.finalizeContext(templateContext, options, filteredCommits, keyCommit, commits);
    options.debug(`Your final context is:\n${stringify(templateContext)}`);
    return templateContext;
}
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoiY29udGV4dC5qcyIsInNvdXJjZVJvb3QiOiIiLCJzb3VyY2VzIjpbIi4uL3NyYy9jb250ZXh0LnRzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiJBQUFBLE9BQU8sTUFBTSxNQUFNLFFBQVEsQ0FBQTtBQVUzQixPQUFPLEVBQUUsU0FBUyxFQUFFLE1BQU0sWUFBWSxDQUFBO0FBRXRDLE1BQU0sVUFBVSxlQUFlLENBQzdCLE9BQWlCLEVBQ2pCLE9BQW1GO0lBRW5GLE1BQU0sRUFDSixPQUFPLEVBQ1AsZ0JBQWdCLEVBQ2hCLFdBQVcsRUFDWixHQUFHLE9BQU8sQ0FBQTtJQUNYLE1BQU0sWUFBWSxHQUEwQixFQUFFLENBQUE7SUFDOUMsTUFBTSxlQUFlLEdBQUcsT0FBTyxDQUFDLE1BQU0sQ0FBMkIsQ0FBQyxNQUFNLEVBQUUsTUFBTSxFQUFFLEVBQUU7UUFDbEYsTUFBTSxHQUFHLEdBQUcsTUFBTSxDQUFDLE9BQU8sQ0FBVyxJQUFJLEVBQUUsQ0FBQTtRQUUzQyxJQUFJLE1BQU0sQ0FBQyxHQUFHLENBQUMsRUFBRTtZQUNmLE1BQU0sQ0FBQyxHQUFHLENBQUMsQ0FBQyxJQUFJLENBQUMsTUFBTSxDQUFDLENBQUE7U0FDekI7YUFBTTtZQUNMLE1BQU0sQ0FBQyxHQUFHLENBQUMsR0FBRyxDQUFDLE1BQU0sQ0FBQyxDQUFBO1NBQ3ZCO1FBRUQsT0FBTyxNQUFNLENBQUE7SUFDZixDQUFDLEVBQUUsRUFBRSxDQUFDLENBQUE7SUFFTixNQUFNLENBQUMsT0FBTyxDQUFDLGVBQWUsQ0FBQyxDQUFDLE9BQU8sQ0FBQyxDQUFDLENBQUMsS0FBSyxFQUFFLE9BQU8sQ0FBQyxFQUFFLEVBQUU7UUFDM0QsSUFBSSxXQUFXLEVBQUU7WUFDZixPQUFPLENBQUMsSUFBSSxDQUFDLFdBQVcsQ0FBQyxDQUFBO1NBQzFCO1FBRUQsWUFBWSxDQUFDLElBQUksQ0FBQztZQUNoQixLQUFLO1lBQ0wsT0FBTztTQUNSLENBQUMsQ0FBQTtJQUNKLENBQUMsQ0FBQyxDQUFBO0lBRUYsSUFBSSxnQkFBZ0IsRUFBRTtRQUNwQixZQUFZLENBQUMsSUFBSSxDQUFDLGdCQUFnQixDQUFDLENBQUE7S0FDcEM7SUFFRCxPQUFPLFlBQVksQ0FBQTtBQUNyQixDQUFDO0FBRUQsTUFBTSxVQUFVLGFBQWEsQ0FDM0IsS0FBbUIsRUFDbkIsT0FBbUU7SUFFbkUsTUFBTSxFQUNKLGNBQWMsRUFDZCxTQUFTLEVBQ1YsR0FBRyxPQUFPLENBQUE7SUFDWCxNQUFNLFNBQVMsR0FBZ0IsRUFBRSxDQUFBO0lBRWpDLEtBQUssQ0FBQyxPQUFPLENBQUMsQ0FBQyxJQUFJLEVBQUUsRUFBRTtRQUNyQixNQUFNLEVBQUUsS0FBSyxFQUFFLEdBQUcsSUFBSSxDQUFBO1FBQ3RCLElBQUksV0FBVyxHQUFHLEtBQUssQ0FBQTtRQUV2QixTQUFTLENBQUMsT0FBTyxDQUFDLENBQUMsS0FBSyxFQUFFLEVBQUU7WUFDMUIsSUFBSSxLQUFLLENBQUMsS0FBSyxLQUFLLEtBQUssRUFBRTtnQkFDekIsV0FBVyxHQUFHLElBQUksQ0FBQTtnQkFDbEIsS0FBSyxDQUFDLEtBQUssQ0FBQyxJQUFJLENBQUMsSUFBSSxDQUFDLENBQUE7YUFDdkI7UUFDSCxDQUFDLENBQUMsQ0FBQTtRQUVGLElBQUksQ0FBQyxXQUFXLEVBQUU7WUFDaEIsU0FBUyxDQUFDLElBQUksQ0FBQztnQkFDYixLQUFLO2dCQUNMLEtBQUssRUFBRSxDQUFDLElBQUksQ0FBQzthQUNkLENBQUMsQ0FBQTtTQUNIO0lBQ0gsQ0FBQyxDQUFDLENBQUE7SUFFRixJQUFJLGNBQWMsRUFBRTtRQUNsQixTQUFTLENBQUMsSUFBSSxDQUFDLGNBQWMsQ0FBQyxDQUFBO0tBQy9CO0lBRUQsSUFBSSxTQUFTLEVBQUU7UUFDYixTQUFTLENBQUMsT0FBTyxDQUFDLENBQUMsS0FBSyxFQUFFLEVBQUU7WUFDMUIsS0FBSyxDQUFDLEtBQUssQ0FBQyxJQUFJLENBQUMsU0FBUyxDQUFDLENBQUE7UUFDN0IsQ0FBQyxDQUFDLENBQUE7S0FDSDtJQUVELE9BQU8sU0FBUyxDQUFBO0FBQ2xCLENBQUM7QUFFRCxNQUFNLFVBQVUsZUFBZSxDQUM3QixPQUFpQixFQUNqQixLQUFtQixFQUNuQixPQUFvSDtJQUVwSCxPQUFPO1FBQ0wsdUNBQXVDO1FBQ3ZDLFlBQVksRUFBRSxlQUFlLENBQUMsT0FBTyxFQUFFLE9BQU8sQ0FBQztRQUMvQywyQkFBMkI7UUFDM0IsVUFBVSxFQUFFLGFBQWEsQ0FBQyxLQUFLLEVBQUUsT0FBTyxDQUFDO0tBQzFDLENBQUE7QUFDSCxDQUFDO0FBRUQ7Ozs7O0dBS0c7QUFDSCxNQUFNLFVBQVUsZUFBZSxDQUM3QixPQUF3QixFQUN4QixPQUFpRDtJQUVqRCxNQUFNLFlBQVksR0FBeUI7UUFDekMsTUFBTSxFQUFFLFNBQVM7UUFDakIsS0FBSyxFQUFFLFFBQVE7UUFDZixJQUFJLEVBQUUsT0FBTyxDQUFDLFVBQVUsQ0FBQyxJQUFJLElBQUksRUFBRSxDQUFDO1FBQ3BDLEdBQUcsT0FBTztLQUNYLENBQUE7SUFFRCxJQUNFLE9BQU8sWUFBWSxDQUFDLGNBQWMsS0FBSyxTQUFTO1dBQzdDLENBQUMsWUFBWSxDQUFDLFVBQVUsSUFBSSxZQUFZLENBQUMsT0FBTyxDQUFDO1dBQ2pELFlBQVksQ0FBQyxNQUFNO1dBQ25CLFlBQVksQ0FBQyxLQUFLLEVBQ3JCO1FBQ0EsWUFBWSxDQUFDLGNBQWMsR0FBRyxJQUFJLENBQUE7S0FDbkM7SUFFRCxPQUFPLFlBQVksQ0FBQTtBQUNyQixDQUFDO0FBRUQ7Ozs7Ozs7OztHQVNHO0FBQ0gsTUFBTSxDQUFDLEtBQUssVUFBVSxrQkFBa0IsQ0FDdEMsU0FBd0IsRUFDeEIsT0FBaUIsRUFDakIsZUFBeUIsRUFDekIsS0FBbUIsRUFDbkIsT0FBNkIsRUFDN0IsT0FBNkI7SUFFN0IsSUFBSSxlQUFlLEdBQXlCO1FBQzFDLEdBQUcsT0FBTztRQUNWLEdBQUcsU0FBbUI7UUFDdEIsR0FBRyxlQUFlLENBQUMsZUFBZSxFQUFFLEtBQUssRUFBRSxPQUFPLENBQUM7S0FDcEQsQ0FBQTtJQUVELElBQUksU0FBUyxFQUFFLGFBQWEsRUFBRTtRQUM1QixlQUFlLENBQUMsSUFBSSxHQUFHLFNBQVMsQ0FBQyxhQUFhLENBQUE7S0FDL0M7SUFFRCxJQUFJLGVBQWUsQ0FBQyxPQUFPLElBQUksTUFBTSxDQUFDLEtBQUssQ0FBQyxlQUFlLENBQUMsT0FBTyxDQUFDLEVBQUU7UUFDcEUsZUFBZSxDQUFDLE9BQU8sR0FBRyxlQUFlLENBQUMsT0FBTyxJQUFJLE1BQU0sQ0FBQyxLQUFLLENBQUMsZUFBZSxDQUFDLE9BQU8sQ0FBQyxLQUFLLENBQUMsQ0FBQTtLQUNqRztJQUVELGVBQWUsR0FBRyxNQUFNLE9BQU8sQ0FBQyxlQUFlLENBQUMsZUFBZSxFQUFFLE9BQU8sRUFBRSxlQUFlLEVBQUUsU0FBUyxFQUFFLE9BQU8sQ0FBQyxDQUFBO0lBRTlHLE9BQU8sQ0FBQyxLQUFLLENBQUMsMkJBQTJCLFNBQVMsQ0FBQyxlQUFlLENBQUMsRUFBRSxDQUFDLENBQUE7SUFFdEUsT0FBTyxlQUFlLENBQUE7QUFDeEIsQ0FBQyJ9