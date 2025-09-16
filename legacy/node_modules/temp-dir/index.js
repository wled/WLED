import {promises as fs} from 'node:fs';
import os from 'node:os';

const temporaryDirectory = await fs.realpath(os.tmpdir());

export default temporaryDirectory;
