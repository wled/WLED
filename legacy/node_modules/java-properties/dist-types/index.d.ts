declare class PropertiesFile {
    objs: {
        [key: string]: any;
    };
    constructor(...args: string[]);
    makeKeys(line: string): void;
    addFile(file: string): void;
    of(...args: string[]): void;
    get(key: string, defaultValue?: string): string | string[] | undefined;
    getLast(key: string, defaultValue?: string): string | undefined;
    getFirst(key: string, defaultValue?: string): string | undefined;
    getInt(key: string, defaultIntValue?: number): number | undefined;
    getFloat(key: string, defaultFloatValue?: number): number | undefined;
    getBoolean(key: string, defaultBooleanValue?: boolean): boolean;
    set(key: string, value: string): void;
    interpolate(s: string): string;
    getKeys(): string[];
    getMatchingKeys(matchstr: string): string[];
    reset(): void;
}
declare let of: (...args: any[]) => PropertiesFile;
export { PropertiesFile, of };
