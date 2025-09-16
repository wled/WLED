import type { Comparator, StringsRecord } from './types/index.js';
/**
 * Formats date to yyyy-mm-dd format.
 * @param date - Date string or Date object.
 * @returns Date string in yyyy-mm-dd format.
 */
export declare function formatDate(date: string | Date): string;
/**
 * Safe JSON.stringify with circular reference support.
 * @param obj
 * @returns Stringified object with circular references.
 */
export declare function stringify(obj: unknown): string;
/**
 * Creates a compare function for sorting from object keys.
 * @param strings - String or array of strings of object keys to compare.
 * @returns Compare function.
 */
export declare function createComparator<K extends string, T extends StringsRecord<K>>(strings: K | K[] | Comparator<T> | undefined): Comparator<T> | undefined;
//# sourceMappingURL=utils.d.ts.map