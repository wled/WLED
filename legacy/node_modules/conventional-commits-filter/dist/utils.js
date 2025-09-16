/**
 * Match commit with revert data
 * @param object - Commit object
 * @param source - Revert data
 * @returns `true` if commit matches revert data, otherwise `false`
 */
export function isMatch(object, source) {
    let aValue;
    let bValue;
    for (const key in source) {
        aValue = object[key];
        bValue = source[key];
        if (typeof aValue === 'string') {
            aValue = aValue.trim();
        }
        if (typeof bValue === 'string') {
            bValue = bValue.trim();
        }
        if (aValue !== bValue) {
            return false;
        }
    }
    return true;
}
/**
 * Find revert commit in set
 * @param commit
 * @param reverts
 * @returns Revert commit if found, otherwise `null`
 */
export function findRevertCommit(commit, reverts) {
    if (!reverts.size) {
        return null;
    }
    const rawCommit = commit.raw || commit;
    for (const revertCommit of reverts) {
        if (revertCommit.revert && isMatch(rawCommit, revertCommit.revert)) {
            return revertCommit;
        }
    }
    return null;
}
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoidXRpbHMuanMiLCJzb3VyY2VSb290IjoiIiwic291cmNlcyI6WyIuLi9zcmMvdXRpbHMudHMiXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6IkFBS0E7Ozs7O0dBS0c7QUFDSCxNQUFNLFVBQVUsT0FBTyxDQUNyQixNQUFpQixFQUNqQixNQUFpQjtJQUVqQixJQUFJLE1BQWUsQ0FBQTtJQUNuQixJQUFJLE1BQWUsQ0FBQTtJQUVuQixLQUFLLE1BQU0sR0FBRyxJQUFJLE1BQU0sRUFBRTtRQUN4QixNQUFNLEdBQUcsTUFBTSxDQUFDLEdBQUcsQ0FBQyxDQUFBO1FBQ3BCLE1BQU0sR0FBRyxNQUFNLENBQUMsR0FBRyxDQUFDLENBQUE7UUFFcEIsSUFBSSxPQUFPLE1BQU0sS0FBSyxRQUFRLEVBQUU7WUFDOUIsTUFBTSxHQUFHLE1BQU0sQ0FBQyxJQUFJLEVBQUUsQ0FBQTtTQUN2QjtRQUVELElBQUksT0FBTyxNQUFNLEtBQUssUUFBUSxFQUFFO1lBQzlCLE1BQU0sR0FBRyxNQUFNLENBQUMsSUFBSSxFQUFFLENBQUE7U0FDdkI7UUFFRCxJQUFJLE1BQU0sS0FBSyxNQUFNLEVBQUU7WUFDckIsT0FBTyxLQUFLLENBQUE7U0FDYjtLQUNGO0lBRUQsT0FBTyxJQUFJLENBQUE7QUFDYixDQUFDO0FBRUQ7Ozs7O0dBS0c7QUFDSCxNQUFNLFVBQVUsZ0JBQWdCLENBQW1CLE1BQVMsRUFBRSxPQUFlO0lBQzNFLElBQUksQ0FBQyxPQUFPLENBQUMsSUFBSSxFQUFFO1FBQ2pCLE9BQU8sSUFBSSxDQUFBO0tBQ1o7SUFFRCxNQUFNLFNBQVMsR0FBRyxNQUFNLENBQUMsR0FBRyxJQUFJLE1BQU0sQ0FBQTtJQUV0QyxLQUFLLE1BQU0sWUFBWSxJQUFJLE9BQU8sRUFBRTtRQUNsQyxJQUFJLFlBQVksQ0FBQyxNQUFNLElBQUksT0FBTyxDQUFDLFNBQVMsRUFBRSxZQUFZLENBQUMsTUFBTSxDQUFDLEVBQUU7WUFDbEUsT0FBTyxZQUFZLENBQUE7U0FDcEI7S0FDRjtJQUVELE9BQU8sSUFBSSxDQUFBO0FBQ2IsQ0FBQyJ9