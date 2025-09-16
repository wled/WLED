/// <reference types="node" resolution-mode="require"/>
import type { Readable } from 'stream';
export declare function splitStream(stream: Readable, separator: string): AsyncGenerator<string, void, undefined>;
export declare function readRawCommitsFromFiles(files: string[], separator: string): AsyncGenerator<string, void, undefined>;
export declare function readRawCommitsFromLine(separator: string): AsyncGenerator<string, void, unknown>;
export declare function readRawCommitsFromStdin(separator: string): AsyncGenerator<string, void, undefined>;
export declare function stringify(commits: AsyncIterable<Record<string, unknown>>): AsyncGenerator<string, void, unknown>;
//# sourceMappingURL=utils.d.ts.map