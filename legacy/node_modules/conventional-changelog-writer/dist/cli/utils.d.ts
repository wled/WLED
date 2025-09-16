/// <reference types="node" resolution-mode="require"/>
import type { Readable } from 'stream';
export declare function parseJsonStream<T>(stream: Readable): AsyncGenerator<Awaited<T>, void, unknown>;
export declare function readCommitsFromFiles<T>(files: string[]): AsyncGenerator<Awaited<T>, void, unknown>;
export declare function readCommitsFromStdin<T>(): AsyncGenerator<Awaited<T>, void, unknown>;
export declare function loadDataFile(filePath: string): Promise<object>;
//# sourceMappingURL=utils.d.ts.map