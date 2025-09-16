type TypeMap = {
    [key: string]: string[];
};
export default class Mime {
    #private;
    constructor(...args: TypeMap[]);
    define(typeMap: TypeMap, force?: boolean): this;
    getType(path: string): string | null;
    getExtension(type: string): string | null;
    getAllExtensions(type: string): Set<string> | null;
    _freeze(): this;
    _getTestState(): {
        types: Map<string, string>;
        extensions: Map<string, string>;
    };
}
export {};
