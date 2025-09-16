import type { AnyObject, Commit } from './types.js';
/**
 * Match commit with revert data
 * @param object - Commit object
 * @param source - Revert data
 * @returns `true` if commit matches revert data, otherwise `false`
 */
export declare function isMatch(object: AnyObject, source: AnyObject): boolean;
/**
 * Find revert commit in set
 * @param commit
 * @param reverts
 * @returns Revert commit if found, otherwise `null`
 */
export declare function findRevertCommit<T extends Commit>(commit: T, reverts: Set<T>): T | null;
//# sourceMappingURL=utils.d.ts.map