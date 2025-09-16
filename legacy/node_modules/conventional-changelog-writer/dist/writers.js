import { Transform } from 'stream';
import { loadTemplates, createTemplateRenderer } from './template.js';
import { getFinalContext } from './context.js';
import { getFinalOptions, getGenerateOnFunction } from './options.js';
import { transformCommit } from './commit.js';
async function getRequirements(context = {}, options = {}) {
    const templates = await loadTemplates(options);
    const finalOptions = getFinalOptions(options, templates);
    const finalContext = getFinalContext(context, finalOptions);
    const generateOn = getGenerateOnFunction(finalContext, finalOptions);
    const renderTemplate = createTemplateRenderer(finalContext, finalOptions);
    return {
        finalContext,
        finalOptions,
        generateOn,
        renderTemplate
    };
}
export function writeChangelog(context = {}, options = {}, includeDetails = false) {
    const requirementsPromise = getRequirements(context, options);
    const prepResult = includeDetails
        ? (log, keyCommit) => ({
            log,
            keyCommit
        })
        : (log) => log;
    return async function* write(commits) {
        const { finalContext, finalOptions, generateOn, renderTemplate } = await requirementsPromise;
        const { transform, reverse, doFlush } = finalOptions;
        let chunk;
        let commit;
        let keyCommit;
        let commitsGroup = [];
        let neverGenerated = true;
        let result;
        let savedKeyCommit = null;
        let firstRelease = true;
        for await (chunk of commits) {
            commit = await transformCommit(chunk, transform, finalContext, finalOptions);
            keyCommit = commit || chunk;
            // previous blocks of logs
            if (reverse) {
                if (commit) {
                    commitsGroup.push(commit);
                }
                if (generateOn(keyCommit, commitsGroup)) {
                    neverGenerated = false;
                    result = await renderTemplate(commitsGroup, keyCommit);
                    commitsGroup = [];
                    yield prepResult(result, keyCommit);
                }
            }
            else {
                if (generateOn(keyCommit, commitsGroup)) {
                    neverGenerated = false;
                    result = await renderTemplate(commitsGroup, savedKeyCommit);
                    commitsGroup = [];
                    if (!firstRelease || doFlush) {
                        yield prepResult(result, savedKeyCommit);
                    }
                    firstRelease = false;
                    savedKeyCommit = keyCommit;
                }
                if (commit) {
                    commitsGroup.push(commit);
                }
            }
        }
        if (!doFlush && (reverse || neverGenerated)) {
            return;
        }
        result = await renderTemplate(commitsGroup, savedKeyCommit);
        yield prepResult(result, savedKeyCommit);
    };
}
/**
 * Creates a transform stream which takes commits and outputs changelog entries.
 * @param context - Context for changelog template.
 * @param options - Options for changelog template.
 * @param includeDetails - Whether to emit details object instead of changelog entry.
 * @returns Transform stream which takes commits and outputs changelog entries.
 */
export function writeChangelogStream(context, options, includeDetails = false) {
    return Transform.from(writeChangelog(context, options, includeDetails));
}
/**
 * Create a changelog string from commits.
 * @param commits - Commits to generate changelog from.
 * @param context - Context for changelog template.
 * @param options - Options for changelog template.
 * @returns Changelog string.
 */
export async function writeChangelogString(commits, context, options) {
    const changelogAsyncIterable = writeChangelog(context, options)(commits);
    let changelog = '';
    let chunk;
    for await (chunk of changelogAsyncIterable) {
        changelog += chunk;
    }
    return changelog;
}
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoid3JpdGVycy5qcyIsInNvdXJjZVJvb3QiOiIiLCJzb3VyY2VzIjpbIi4uL3NyYy93cml0ZXJzLnRzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiJBQUFBLE9BQU8sRUFBRSxTQUFTLEVBQUUsTUFBTSxRQUFRLENBQUE7QUFRbEMsT0FBTyxFQUNMLGFBQWEsRUFDYixzQkFBc0IsRUFDdkIsTUFBTSxlQUFlLENBQUE7QUFDdEIsT0FBTyxFQUFFLGVBQWUsRUFBRSxNQUFNLGNBQWMsQ0FBQTtBQUM5QyxPQUFPLEVBQ0wsZUFBZSxFQUNmLHFCQUFxQixFQUN0QixNQUFNLGNBQWMsQ0FBQTtBQUNyQixPQUFPLEVBQUUsZUFBZSxFQUFFLE1BQU0sYUFBYSxDQUFBO0FBRTdDLEtBQUssVUFBVSxlQUFlLENBRzVCLFVBQTJCLEVBQUUsRUFDN0IsVUFBMkIsRUFBRTtJQUU3QixNQUFNLFNBQVMsR0FBRyxNQUFNLGFBQWEsQ0FBQyxPQUFPLENBQUMsQ0FBQTtJQUM5QyxNQUFNLFlBQVksR0FBRyxlQUFlLENBQUMsT0FBTyxFQUFFLFNBQVMsQ0FBQyxDQUFBO0lBQ3hELE1BQU0sWUFBWSxHQUFHLGVBQWUsQ0FBQyxPQUFPLEVBQUUsWUFBWSxDQUFDLENBQUE7SUFDM0QsTUFBTSxVQUFVLEdBQUcscUJBQXFCLENBQUMsWUFBWSxFQUFFLFlBQVksQ0FBQyxDQUFBO0lBQ3BFLE1BQU0sY0FBYyxHQUFHLHNCQUFzQixDQUFDLFlBQVksRUFBRSxZQUFZLENBQUMsQ0FBQTtJQUV6RSxPQUFPO1FBQ0wsWUFBWTtRQUNaLFlBQVk7UUFDWixVQUFVO1FBQ1YsY0FBYztLQUNmLENBQUE7QUFDSCxDQUFDO0FBeUJELE1BQU0sVUFBVSxjQUFjLENBQzVCLFVBQTJCLEVBQUUsRUFDN0IsVUFBMkIsRUFBRSxFQUM3QixjQUFjLEdBQUcsS0FBSztJQUV0QixNQUFNLG1CQUFtQixHQUFHLGVBQWUsQ0FBQyxPQUFPLEVBQUUsT0FBTyxDQUFDLENBQUE7SUFDN0QsTUFBTSxVQUFVLEdBQUcsY0FBYztR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