#!/usr/bin/env node
import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import mime from './index.js';
const __dirname = path.dirname(fileURLToPath(import.meta.url));
export default async function () {
    process.title = 'mime';
    const json = await fs.readFile(path.join(__dirname, '../../package.json'), 'utf-8');
    const pkg = JSON.parse(json);
    const args = process.argv.splice(2);
    if (args.includes('--version') ||
        args.includes('-v') ||
        args.includes('--v')) {
        console.log(pkg.version);
        process.exit(0);
    }
    else if (args.includes('--name') ||
        args.includes('-n') ||
        args.includes('--n')) {
        console.log(pkg.name);
        process.exit(0);
    }
    else if (args.includes('--help') ||
        args.includes('-h') ||
        args.includes('--h')) {
        console.log(pkg.name + ' - ' + pkg.description + '\n');
        console.log(`Usage:

  mime [flags] [path_or_extension]

  Flags:
    --help, -h                     Show this message
    --version, -v                  Display the version
    --name, -n                     Print the name of the program
    --reverse, -r                  Print the extension of the mime type

  Note: the command will exit after it executes if a command is specified
  The path_or_extension is the path to the file or the extension of the file.

  Examples:
    mime --help
    mime --version
    mime --name
    mime -v
    mime --reverse application/text
    mime src/log.js
    mime new.py
    mime foo.sh
  `);
        process.exit(0);
    }
    else if (args.includes('--reverse') || args.includes('-r')) {
        const mimeType = args[args.length - 1];
        const extension = mime.getExtension(mimeType);
        if (!extension)
            process.exit(1);
        process.stdout.write(extension + '\n');
        process.exit(0);
    }
    const file = args[0];
    const type = mime.getType(file);
    if (!type)
        process.exit(1);
    process.stdout.write(type + '\n');
}
