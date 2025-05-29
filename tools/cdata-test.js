'use strict';

const assert = require('node:assert');
const { describe, it, before, after } = require('node:test');
const fs = require('fs');
const path = require('path');
const child_process = require('child_process');
const util = require('util');
const execPromise = util.promisify(child_process.exec);

process.env.NODE_ENV = 'test'; // Set the environment to testing
const cdata = require('./cdata.js');

describe('Function', () => {
  const testFolderPath = path.join(__dirname, 'testFolder');
  const oldFilePath = path.join(testFolderPath, 'oldFile.txt');
  const newFilePath = path.join(testFolderPath, 'newFile.txt');

  // Create a temporary file before the test
  before(() => {
    // Create test folder
    if (!fs.existsSync(testFolderPath)) {
      fs.mkdirSync(testFolderPath);
    }

    // Create an old file
    fs.writeFileSync(oldFilePath, 'This is an old file.');
    // Modify the 'mtime' to simulate an old file
    const oldTime = new Date();
    oldTime.setFullYear(oldTime.getFullYear() - 1);
    fs.utimesSync(oldFilePath, oldTime, oldTime);

    // Create a new file
    fs.writeFileSync(newFilePath, 'This is a new file.');
  });

  // delete the temporary files after the test
  after(() => {
    fs.rmSync(testFolderPath, { recursive: true });
  });

  describe('isFileNewerThan', async () => {
    it('should return true if the file is newer than the provided time', async () => {
      const pastTime = Date.now() - 10000; // 10 seconds ago
      assert.strictEqual(cdata.isFileNewerThan(newFilePath, pastTime), true);
    });

    it('should return false if the file is older than the provided time', async () => {
      assert.strictEqual(cdata.isFileNewerThan(oldFilePath, Date.now()), false);
    });

    it('should throw an exception if the file does not exist', async () => {
      assert.throws(() => {
        cdata.isFileNewerThan('nonexistent.txt', Date.now());
      });
    });
  });

  describe('isAnyFileInFolderNewerThan', async () => {
    it('should return true if a file in the folder is newer than the given time', async () => {
      const time = fs.statSync(path.join(testFolderPath, 'oldFile.txt')).mtime;
      assert.strictEqual(cdata.isAnyFileInFolderNewerThan(testFolderPath, time), true);
    });

    it('should return false if no files in the folder are newer than the given time', async () => {
      assert.strictEqual(cdata.isAnyFileInFolderNewerThan(testFolderPath, new Date()), false);
    });

    it('should throw an exception if the folder does not exist', async () => {
      assert.throws(() => {
        cdata.isAnyFileInFolderNewerThan('nonexistent', new Date());
      });
    });
  });
});

describe('Script', () => {
  const folderPath = 'wled00';
  const dataPath = path.join(folderPath, 'data');

  before(() => {
    process.env.NODE_ENV = 'production';
    // Backup files
    fs.cpSync("wled00/data", "wled00Backup", { recursive: true });
    fs.cpSync("tools/cdata.js", "cdata.bak.js");
    fs.cpSync("package.json", "package.bak.json");
  });
  after(() => {
    // Restore backup
    fs.rmSync("wled00/data", { recursive: true });
    fs.renameSync("wled00Backup", "wled00/data");
    fs.rmSync("tools/cdata.js");
    fs.renameSync("cdata.bak.js", "tools/cdata.js");
    fs.rmSync("package.json");
    fs.renameSync("package.bak.json", "package.json");
  });

  // delete all html_*.h files
  async function deleteBuiltFiles() {
    const files = await fs.promises.readdir(folderPath);
    await Promise.all(files.map(file => {
      if (file.startsWith('html_') && path.extname(file) === '.h') {
        return fs.promises.unlink(path.join(folderPath, file));
      }
    }));
  }

  // check if html_*.h files were created
  async function checkIfBuiltFilesExist() {
    const files = await fs.promises.readdir(folderPath);
    const htmlFiles = files.filter(file => file.startsWith('html_') && path.extname(file) === '.h');
    assert(htmlFiles.length > 0, 'html_*.h files were not created');
  }

  async function runAndCheckIfBuiltFilesExist() {
    await execPromise('node tools/cdata.js');
    await checkIfBuiltFilesExist();
  }

  async function checkIfFileWasNewlyCreated(file) {
    const modifiedTime = fs.statSync(file).mtimeMs;
    assert(Date.now() - modifiedTime < 500, file + ' was not modified');
  }

  async function testFileModification(sourceFilePath, resultFile) {
    // run cdata.js to ensure html_*.h files are created
    await execPromise('node tools/cdata.js');

    // modify file
    fs.appendFileSync(sourceFilePath, ' ');
    // delay for 1 second to ensure the modified time is different
    await new Promise(resolve => setTimeout(resolve, 1000));

    // run script cdata.js again and wait for it to finish
    await execPromise('node tools/cdata.js');

    await checkIfFileWasNewlyCreated(path.join(folderPath, resultFile));
  }

  describe('should build if', () => {
    it('html_*.h files are missing', async () => {
      await deleteBuiltFiles();
      await runAndCheckIfBuiltFilesExist();
    });

    it('only one html_*.h file is missing', async () => {
      // run script cdata.js and wait for it to finish
      await execPromise('node tools/cdata.js');

      // delete a random html_*.h file
      let files = await fs.promises.readdir(folderPath);
      let htmlFiles = files.filter(file => file.startsWith('html_') && path.extname(file) === '.h');
      const randomFile = htmlFiles[Math.floor(Math.random() * htmlFiles.length)];
      await fs.promises.unlink(path.join(folderPath, randomFile));

      await runAndCheckIfBuiltFilesExist();
    });

    it('script was executed with -f or --force', async () => {
      await execPromise('node tools/cdata.js');
      await new Promise(resolve => setTimeout(resolve, 1000));
      await execPromise('node tools/cdata.js --force');
      await checkIfFileWasNewlyCreated(path.join(folderPath, 'html_ui.h'));
      await new Promise(resolve => setTimeout(resolve, 1000));
      await execPromise('node tools/cdata.js -f');
      await checkIfFileWasNewlyCreated(path.join(folderPath, 'html_ui.h'));
    });

    it('a file changes', async () => {
      await testFileModification(path.join(dataPath, 'index.htm'), 'html_ui.h');
    });

    it('a inlined file changes', async () => {
      await testFileModification(path.join(dataPath, 'index.js'), 'html_ui.h');
    });

    it('a settings file changes', async () => {
      await testFileModification(path.join(dataPath, 'settings_leds.htm'), 'html_ui.h');
    });

    it('the favicon changes', async () => {
      await testFileModification(path.join(dataPath, 'favicon.ico'), 'html_ui.h');
    });

    it('cdata.js changes', async () => {
      await testFileModification('tools/cdata.js', 'html_ui.h');
    });

    it('package.json changes', async () => {
      await testFileModification('package.json', 'html_ui.h');
    });
  });

  describe('should not build if', () => {
    it('the files are already built', async () => {
      await deleteBuiltFiles();

      // run script cdata.js and wait for it to finish
      let startTime = Date.now();
      await execPromise('node tools/cdata.js');
      const firstRunTime = Date.now() - startTime;

      // run script cdata.js and wait for it to finish
      startTime = Date.now();
      await execPromise('node tools/cdata.js');
      const secondRunTime = Date.now() - startTime;

      // check if second run was faster than the first (must be at least 2x faster)
      assert(secondRunTime < firstRunTime / 2, 'html_*.h files were rebuilt');
    });
  });
});