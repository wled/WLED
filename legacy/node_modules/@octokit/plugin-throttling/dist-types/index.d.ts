import type { Octokit, OctokitOptions } from "@octokit/core";
import type { ThrottlingOptions } from "./types.js";
export declare function throttling(octokit: Octokit, octokitOptions: OctokitOptions): {};
export declare namespace throttling {
    var VERSION: string;
    var triggersNotification: (string: string) => boolean;
}
declare module "@octokit/core" {
    interface OctokitOptions {
        throttle?: ThrottlingOptions;
    }
}
declare module "@octokit/types" {
    interface OctokitResponse<T, S extends number = number> {
        retryCount: number;
    }
}
export type { ThrottlingOptions };
