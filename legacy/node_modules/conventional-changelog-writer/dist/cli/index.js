#!/usr/bin/env node
import { pipeline } from 'stream/promises';
import meow from 'meow';
import { writeChangelog } from '../index.js';
import { loadDataFile, readCommitsFromFiles, readCommitsFromStdin } from './utils.js';
const cli = meow(`
    Usage
      conventional-changelog-writer <path> [<path> ...]
      cat <path> | conventional-changelog-writer
    ,
    Example
      conventional-changelog-writer commits.ldjson
      cat commits.ldjson | conventional-changelog-writer
    ,
    Options
      -c, --context    A filepath of a json that is used to define template variables
      -o, --options    A filepath of a javascript object that is used to define options
`, {
    importMeta: import.meta,
    flags: {
        context: {
            shortFlag: 'c',
            type: 'string'
        },
        options: {
            shortFlag: 'o',
            type: 'string'
        }
    }
});
const { context: contextPath, options: optionsPath } = cli.flags;
let context;
let options;
if (contextPath) {
    try {
        context = await loadDataFile(contextPath);
    }
    catch (err) {
        console.error(`Failed to get context from file ${contextPath}:\n  ${err}`);
        process.exit(1);
    }
}
if (optionsPath) {
    try {
        options = await loadDataFile(optionsPath);
    }
    catch (err) {
        console.error(`Failed to get options from file ${optionsPath}:\n  ${err}`);
        process.exit(1);
    }
}
let inputStream;
try {
    if (cli.input.length) {
        inputStream = readCommitsFromFiles(cli.input);
    }
    else if (process.stdin.isTTY) {
        console.error('You must specify at least one line delimited json file');
        process.exit(1);
    }
    else {
        inputStream = readCommitsFromStdin();
    }
    await pipeline(inputStream, writeChangelog(context, options), process.stdout);
}
catch (err) {
    console.error(err);
    process.exit(1);
}
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoiaW5kZXguanMiLCJzb3VyY2VSb290IjoiIiwic291cmNlcyI6WyIuLi8uLi9zcmMvY2xpL2luZGV4LnRzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiI7QUFDQSxPQUFPLEVBQUUsUUFBUSxFQUFFLE1BQU0saUJBQWlCLENBQUE7QUFDMUMsT0FBTyxJQUFJLE1BQU0sTUFBTSxDQUFBO0FBTXZCLE9BQU8sRUFBRSxjQUFjLEVBQUUsTUFBTSxhQUFhLENBQUE7QUFDNUMsT0FBTyxFQUNMLFlBQVksRUFDWixvQkFBb0IsRUFDcEIsb0JBQW9CLEVBQ3JCLE1BQU0sWUFBWSxDQUFBO0FBRW5CLE1BQU0sR0FBRyxHQUFHLElBQUksQ0FBQzs7Ozs7Ozs7Ozs7O0NBWWhCLEVBQUU7SUFDRCxVQUFVLEVBQUUsTUFBTSxDQUFDLElBQUk7SUFDdkIsS0FBSyxFQUFFO1FBQ0wsT0FBTyxFQUFFO1lBQ1AsU0FBUyxFQUFFLEdBQUc7WUFDZCxJQUFJLEVBQUUsUUFBUTtTQUNmO1FBQ0QsT0FBTyxFQUFFO1lBQ1AsU0FBUyxFQUFFLEdBQUc7WUFDZCxJQUFJLEVBQUUsUUFBUTtTQUNmO0tBQ0Y7Q0FDRixDQUFDLENBQUE7QUFDRixNQUFNLEVBQ0osT0FBTyxFQUFFLFdBQVcsRUFDcEIsT0FBTyxFQUFFLFdBQVcsRUFDckIsR0FBRyxHQUFHLENBQUMsS0FBSyxDQUFBO0FBQ2IsSUFBSSxPQUE0QixDQUFBO0FBQ2hDLElBQUksT0FBNEIsQ0FBQTtBQUVoQyxJQUFJLFdBQVcsRUFBRTtJQUNmLElBQUk7UUFDRixPQUFPLEdBQUcsTUFBTSxZQUFZLENBQUMsV0FBVyxDQUFDLENBQUE7S0FDMUM7SUFBQyxPQUFPLEdBQUcsRUFBRTtRQUNaLE9BQU8sQ0FBQyxLQUFLLENBQUMsbUNBQW1DLFdBQVcsUUFBUSxHQUFhLEVBQUUsQ0FBQyxDQUFBO1FBQ3BGLE9BQU8sQ0FBQyxJQUFJLENBQUMsQ0FBQyxDQUFDLENBQUE7S0FDaEI7Q0FDRjtBQUVELElBQUksV0FBVyxFQUFFO0lBQ2YsSUFBSTtRQUNGLE9BQU8sR0FBRyxNQUFNLFlBQVksQ0FBQyxXQUFXLENBQUMsQ0FBQTtLQUMxQztJQUFDLE9BQU8sR0FBRyxFQUFFO1FBQ1osT0FBTyxDQUFDLEtBQUssQ0FBQyxtQ0FBbUMsV0FBVyxRQUFRLEdBQWEsRUFBRSxDQUFDLENBQUE7UUFDcEYsT0FBTyxDQUFDLElBQUksQ0FBQyxDQUFDLENBQUMsQ0FBQTtLQUNoQjtDQUNGO0FBRUQsSUFBSSxXQUE0QyxDQUFBO0FBRWhELElBQUk7SUFDRixJQUFJLEdBQUcsQ0FBQyxLQUFLLENBQUMsTUFBTSxFQUFFO1FBQ3BCLFdBQVcsR0FBRyxvQkFBb0IsQ0FBQyxHQUFHLENBQUMsS0FBSyxDQUFDLENBQUE7S0FDOUM7U0FDQyxJQUFJLE9BQU8sQ0FBQyxLQUFLLENBQUMsS0FBSyxFQUFFO1FBQ3ZCLE9BQU8sQ0FBQyxLQUFLLENBQUMsd0RBQXdELENBQUMsQ0FBQTtRQUN2RSxPQUFPLENBQUMsSUFBSSxDQUFDLENBQUMsQ0FBQyxDQUFBO0tBQ2hCO1NBQU07UUFDTCxXQUFXLEdBQUcsb0JBQW9CLEVBQUUsQ0FBQTtLQUNyQztJQUVILE1BQU0sUUFBUSxDQUNaLFdBQVcsRUFDWCxjQUFjLENBQUMsT0FBTyxFQUFFLE9BQU8sQ0FBQyxFQUNoQyxPQUFPLENBQUMsTUFBTSxDQUNmLENBQUE7Q0FDRjtBQUFDLE9BQU8sR0FBRyxFQUFFO0lBQ1osT0FBTyxDQUFDLEtBQUssQ0FBQyxHQUFHLENBQUMsQ0FBQTtJQUNsQixPQUFPLENBQUMsSUFBSSxDQUFDLENBQUMsQ0FBQyxDQUFBO0NBQ2hCIn0=