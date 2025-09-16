import type { Octokit } from "@octokit/core";
import type { EndpointDefaults } from "@octokit/types";
import type Bottleneck from "bottleneck";
type LimitHandler = (retryAfter: number, options: Required<EndpointDefaults>, octokit: Octokit, retryCount: number) => void;
export type SecondaryLimitHandler = {
    onSecondaryRateLimit: LimitHandler;
};
export type ThrottlingOptionsBase = {
    enabled?: boolean;
    Bottleneck?: typeof Bottleneck;
    id?: string;
    timeout?: number;
    connection?: Bottleneck.RedisConnection | Bottleneck.IORedisConnection;
    /**
     * @deprecated use `fallbackSecondaryRateRetryAfter`
     */
    minimalSecondaryRateRetryAfter?: number;
    fallbackSecondaryRateRetryAfter?: number;
    retryAfterBaseValue?: number;
    write?: Bottleneck.Group;
    search?: Bottleneck.Group;
    notifications?: Bottleneck.Group;
    onRateLimit: LimitHandler;
};
export type ThrottlingOptions = (ThrottlingOptionsBase & SecondaryLimitHandler) | (Partial<ThrottlingOptionsBase & SecondaryLimitHandler> & {
    enabled: false;
});
export type Groups = {
    global?: Bottleneck.Group;
    write?: Bottleneck.Group;
    search?: Bottleneck.Group;
    notifications?: Bottleneck.Group;
};
export type State = {
    clustering: boolean;
    triggersNotification: (pathname: string) => boolean;
    fallbackSecondaryRateRetryAfter: number;
    retryAfterBaseValue: number;
    retryLimiter: Bottleneck;
    id: string;
} & Required<Groups> & ThrottlingOptions;
export type CreateGroupsCommon = {
    connection?: Bottleneck.RedisConnection | Bottleneck.IORedisConnection;
    timeout: number;
};
export {};
