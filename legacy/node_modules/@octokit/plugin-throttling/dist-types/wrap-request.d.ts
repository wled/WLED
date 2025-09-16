import type { EndpointDefaults, OctokitResponse } from "@octokit/types";
import type { State } from "./types.js";
export declare function wrapRequest(state: State, request: ((options: Required<EndpointDefaults>) => Promise<OctokitResponse<any>>) & {
    retryCount: number;
}, options: Required<EndpointDefaults>): Promise<OctokitResponse<any, number>>;
