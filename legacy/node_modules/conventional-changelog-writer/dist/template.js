import { join } from 'path';
import { fileURLToPath } from 'url';
import { readFile } from 'fs/promises';
import Handlebars from 'handlebars';
// @todo Drop import and ignoreReverted option
import { filterRevertedCommitsSync } from 'conventional-commits-filter';
import { getTemplateContext } from './context.js';
const dirname = fileURLToPath(new URL('.', import.meta.url));
/**
 * Load templates from files.
 * @param options
 * @returns Templates strings object.
 */
export async function loadTemplates(options = {}) {
    const [mainTemplate, headerPartial, commitPartial, footerPartial] = await Promise.all([
        options.mainTemplate || readFile(join(dirname, '..', 'templates', 'template.hbs'), 'utf-8'),
        options.headerPartial || readFile(join(dirname, '..', 'templates', 'header.hbs'), 'utf-8'),
        options.commitPartial || readFile(join(dirname, '..', 'templates', 'commit.hbs'), 'utf-8'),
        options.footerPartial || readFile(join(dirname, '..', 'templates', 'footer.hbs'), 'utf-8')
    ]);
    return {
        mainTemplate,
        headerPartial,
        commitPartial,
        footerPartial
    };
}
/**
 * Compile Handlebars templates.
 * @param templates
 * @returns Handlebars template instance.
 */
export function compileTemplates(templates) {
    const { mainTemplate, headerPartial, commitPartial, footerPartial, partials } = templates;
    Handlebars.registerPartial('header', headerPartial);
    Handlebars.registerPartial('commit', commitPartial);
    Handlebars.registerPartial('footer', footerPartial);
    if (partials) {
        Object.entries(partials).forEach(([name, partial]) => {
            if (typeof partial === 'string') {
                Handlebars.registerPartial(name, partial);
            }
        });
    }
    return Handlebars.compile(mainTemplate, {
        noEscape: true
    });
}
/**
 * Create template renderer.
 * @param context
 * @param options
 * @returns Template render function.
 */
export function createTemplateRenderer(context, options) {
    const { ignoreReverted } = options;
    const template = compileTemplates(options);
    return async (commits, keyCommit) => {
        const notes = [];
        const commitsForTemplate = (ignoreReverted
            ? Array.from(filterRevertedCommitsSync(commits))
            : commits).map(commit => ({
            ...commit,
            notes: commit.notes.map((note) => {
                const commitNote = {
                    ...note,
                    commit
                };
                notes.push(commitNote);
                return commitNote;
            })
        }));
        const templateContext = await getTemplateContext(keyCommit, commits, commitsForTemplate, notes, context, options);
        return template(templateContext);
    };
}
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoidGVtcGxhdGUuanMiLCJzb3VyY2VSb290IjoiIiwic291cmNlcyI6WyIuLi9zcmMvdGVtcGxhdGUudHMiXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6IkFBQUEsT0FBTyxFQUFFLElBQUksRUFBRSxNQUFNLE1BQU0sQ0FBQTtBQUMzQixPQUFPLEVBQUUsYUFBYSxFQUFFLE1BQU0sS0FBSyxDQUFBO0FBQ25DLE9BQU8sRUFBRSxRQUFRLEVBQUUsTUFBTSxhQUFhLENBQUE7QUFDdEMsT0FBTyxVQUFVLE1BQU0sWUFBWSxDQUFBO0FBQ25DLDhDQUE4QztBQUM5QyxPQUFPLEVBQUUseUJBQXlCLEVBQUUsTUFBTSw2QkFBNkIsQ0FBQTtBQVV2RSxPQUFPLEVBQUUsa0JBQWtCLEVBQUUsTUFBTSxjQUFjLENBQUE7QUFFakQsTUFBTSxPQUFPLEdBQUcsYUFBYSxDQUFDLElBQUksR0FBRyxDQUFDLEdBQUcsRUFBRSxNQUFNLENBQUMsSUFBSSxDQUFDLEdBQUcsQ0FBQyxDQUFDLENBQUE7QUFFNUQ7Ozs7R0FJRztBQUNILE1BQU0sQ0FBQyxLQUFLLFVBQVUsYUFBYSxDQUFDLFVBQTRCLEVBQUU7SUFDaEUsTUFBTSxDQUNKLFlBQVksRUFDWixhQUFhLEVBQ2IsYUFBYSxFQUNiLGFBQWEsQ0FDZCxHQUFHLE1BQU0sT0FBTyxDQUFDLEdBQUcsQ0FBQztRQUNwQixPQUFPLENBQUMsWUFBWSxJQUFJLFFBQVEsQ0FBQyxJQUFJLENBQUMsT0FBTyxFQUFFLElBQUksRUFBRSxXQUFXLEVBQUUsY0FBYyxDQUFDLEVBQUUsT0FBTyxDQUFDO1FBQzNGLE9BQU8sQ0FBQyxhQUFhLElBQUksUUFBUSxDQUFDLElBQUksQ0FBQyxPQUFPLEVBQUUsSUFBSSxFQUFFLFdBQVcsRUFBRSxZQUFZLENBQUMsRUFBRSxPQUFPLENBQUM7UUFDMUYsT0FBTyxDQUFDLGFBQWEsSUFBSSxRQUFRLENBQUMsSUFBSSxDQUFDLE9BQU8sRUFBRSxJQUFJLEVBQUUsV0FBVyxFQUFFLFlBQVksQ0FBQyxFQUFFLE9BQU8s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