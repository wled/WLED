import type { Octokit } from "@octokit/core";
import type { RequestError } from "@octokit/request-error";
export { VERSION } from "./version.js";
export declare function retry(octokit: Octokit, octokitOptions: any): {
    retry: {
        retryRequest: (error: RequestError, retries: number, retryAfter: number) => RequestError;
    };
};
export declare namespace retry {
    var VERSION: string;
}
