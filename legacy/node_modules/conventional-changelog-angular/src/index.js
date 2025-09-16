import { createParserOpts } from './parser.js'
import { createWriterOpts } from './writer.js'
import { whatBump } from './whatBump.js'

export default async function createPreset () {
  return {
    parser: createParserOpts(),
    writer: await createWriterOpts(),
    whatBump
  }
}
