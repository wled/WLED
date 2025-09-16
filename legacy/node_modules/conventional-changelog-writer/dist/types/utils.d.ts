export type AnyObject = Record<string, any>;
export type Comparator<T> = (a: T, b: T) => number;
export type StringsRecord<T extends string> = {
    [K in T]?: string | undefined;
};
export type PickStringsKeys<T> = {
    [P in keyof T]: P extends string ? T[P] extends string ? (string extends T[P] ? P : never) : never : never;
}[keyof T];
export type PickStrings<T> = StringsRecord<PickStringsKeys<T>>;
//# sourceMappingURL=utils.d.ts.map