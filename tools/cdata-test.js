'use strict';

const assert = require('node:assert');
const { describe, it, before, after } = require('node:test');
const fs = require('fs');
const os = require('os');
const path = require('path');
const child_process = require('child_process');
const util = require('util');
const execPromise = util.promisify(child_process.exec);

// Importing the build script must be side-effect free: with NODE_ENV=test it
// defines its helpers but must not run a build.
process.env.NODE_ENV = 'test';
require('./cdata.js');

// The CLI decides whether to build from NODE_ENV, so child processes must not
// inherit the test harness's NODE_ENV=test (which suppresses the build).
const buildEnv = { ...process.env, NODE_ENV: 'production' };
const runCdata = (args = '') => execPromise('node tools/cdata.js ' + args, { env: buildEnv });

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
    assert(Date.now() - modifiedTime < 850, file + ' was not modified');
  }

  async function testFileModification(sourceFilePath, resultFile) {
    // run cdata.js to ensure html_*.h files are created
    await execPromise('node tools/cdata.js');

    // modify file
    fs.appendFileSync(sourceFilePath, ' ');
    // delay for 1 second to ensure the modified time is different
    await new Promise(resolve => setTimeout(resolve, 1400));

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
      await testFileModification(path.join(dataPath, 'settings_leds.htm'), 'html_settings.h');
    });

    it('common.js changes', async () => {
      await testFileModification(path.join(dataPath, 'common.js'), 'html_settings.h');
    });

    // this testcase currently fails - might be due to npm updates (maybe "faking" a favicon.ico change is harder now), or a real regression
    // see https://github.com/wled/WLED/issues/5581
    // it('the favicon changes', async () => {
    //   await testFileModification(path.join(dataPath, 'favicon.ico'), 'html_other.h');
    // });

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

describe('Dependency graph (--emit-deps / --depfile)', () => {
  const depfile = path.join(os.tmpdir(), `cdata-deps-${process.pid}.d`);

  after(() => {
    try { fs.rmSync(depfile); } catch { /* ignore */ }
  });

  it('emits a depfile listing every generated header, without building', async () => {
    const { stdout } = await runCdata(`--emit-deps --depfile "${depfile}"`);
    assert.match(stdout, /Wrote dependency graph/);
    assert.doesNotMatch(stdout, /Minified/); // nothing was actually built

    const dep = fs.readFileSync(depfile, 'utf8');
    for (const header of ['wled00/html_ui.h', 'wled00/html_settings.h', 'wled00/js_iro.h']) {
      assert.match(dep, new RegExp('^' + header.replace(/\./g, '\\.') + ':', 'm'),
        `depfile is missing a rule for ${header}`);
    }
  });

  it('records cdata.js, package.json and package-lock.json as inputs of every header', async () => {
    await runCdata(`--emit-deps --depfile "${depfile}"`);
    const dep = fs.readFileSync(depfile, 'utf8');
    for (const line of dep.split('\n')) {
      if (!line || line.startsWith('#') || !line.includes(':')) continue;
      assert.match(line, /tools\/cdata\.js/, 'every rule should depend on cdata.js');
      assert.match(line, /(^|\s)package\.json(\s|$)/, 'every rule should depend on package.json');
      // The lock file pins minifier versions, so an npm install that changes them
      // (without touching package.json) must still invalidate every header.
      assert.match(line, /package-lock\.json/, 'every rule should depend on package-lock.json');
    }
  });

  it("records an html job's whole source folder, not a snapshot of its files", async () => {
    // An html job inlines arbitrary resources from its source folder, so the
    // folder itself is the dependency.  That is what lets a file added to or
    // removed from wled00/data (e.g. wled00/data/icons-ui/Read Me.txt) count as a
    // dependency change -- rather than listing today's files and missing the next
    // one that appears.
    await runCdata(`--emit-deps --depfile "${depfile}"`);
    const dep = fs.readFileSync(depfile, 'utf8');
    const uiRule = dep.split('\n').find(l => l.startsWith('wled00/html_ui.h:'));
    assert(uiRule, 'no dependency rule for wled00/html_ui.h');
    assert.match(uiRule, /(^|\s)wled00\/data(\s|$)/, 'html_ui.h should depend on its source folder');
    // The folder is listed as one token, so individual files inside it -- and any
    // spaces in their names -- are not enumerated here.
    assert.doesNotMatch(uiRule, /Read/);
  });
});

describe('Usermod manifests', () => {
  let modDir;
  const manifest = () => path.join(modDir, 'cdata.json');

  before(() => {
    modDir = fs.mkdtempSync(path.join(os.tmpdir(), 'cdata-mod-'));
    fs.mkdirSync(path.join(modDir, 'data'));
    fs.writeFileSync(path.join(modDir, 'data', 'page.htm'),
      '<!doctype html><html><body><h1>Hi ##VERSION##</h1></body></html>');
    fs.writeFileSync(manifest(), JSON.stringify({
      header: [{
        output: 'html_mod.h',
        srcDir: 'data',
        specs: [{ file: 'page.htm', name: 'PAGE_mod', method: 'gzip', filter: 'html-minify' }],
      }],
    }));
  });

  after(() => {
    fs.rmSync(modDir, { recursive: true, force: true });
  });

  it('legacy single-manifest mode builds only that header', async () => {
    const out = path.join(modDir, 'html_mod.h');
    const { stdout } = await runCdata(`"${manifest()}"`);
    assert(fs.existsSync(out), 'usermod header was not built');
    assert.match(fs.readFileSync(out, 'utf8'), /PAGE_mod/);
    assert.doesNotMatch(stdout, /index\.htm|html_ui\.h/); // main UI is not part of this run
  });

  it('--manifest adds the usermod header to the graph alongside the main UI', async () => {
    const depfile = path.join(modDir, 'deps.d');
    await runCdata(`--emit-deps --depfile "${depfile}" --manifest "${manifest()}"`);
    const dep = fs.readFileSync(depfile, 'utf8');
    assert.match(dep, /html_mod\.h:/);        // the usermod output
    assert.match(dep, /wled00\/html_ui\.h:/); // together with the main UI
  });

  it('rejects a manifest with an unknown top-level key', async () => {
    const bad = path.join(modDir, 'bad.json');
    fs.writeFileSync(bad, JSON.stringify({
      inject: {}, // reserved for the future, not accepted by current tooling
      header: [{
        output: 'html_bad.h',
        specs: [{ file: 'page.htm', name: 'PAGE_bad', method: 'gzip', filter: 'html-minify' }],
      }],
    }));
    await assert.rejects(runCdata(`"${bad}"`), /unknown top-level key/);
    fs.rmSync(bad);
  });

  it('builds one header per header op in a multi-header manifest', async () => {
    const multi = path.join(modDir, 'multi.json');
    fs.writeFileSync(multi, JSON.stringify({
      header: [
        { output: 'html_a.h', srcDir: 'data',
          specs: [{ file: 'page.htm', name: 'PAGE_a', method: 'gzip', filter: 'html-minify' }] },
        { output: 'html_b.h', srcDir: 'data',
          specs: [{ file: 'page.htm', name: 'PAGE_b', method: 'gzip', filter: 'html-minify' }] },
      ],
    }));
    await runCdata(`"${multi}"`);
    assert.match(fs.readFileSync(path.join(modDir, 'html_a.h'), 'utf8'), /PAGE_a/);
    assert.match(fs.readFileSync(path.join(modDir, 'html_b.h'), 'utf8'), /PAGE_b/);
    fs.rmSync(multi);
  });

  it('emits a valid raw string for a plaintext spec with no delimiters', async () => {
    const pt = path.join(modDir, 'plain.json');
    fs.writeFileSync(pt, JSON.stringify({
      header: [{ output: 'html_plain.h', srcDir: 'data',
        specs: [{ file: 'page.htm', name: 'PAGE_plain', method: 'plaintext' }] }],
    }));
    await runCdata(`"${pt}"`);
    const out = fs.readFileSync(path.join(modDir, 'html_plain.h'), 'utf8');
    // A safe matching delimiter pair is filled in, so the literal is R"=====(...)====="
    // rather than an invalid bare R"...".
    assert.match(out, /const char PAGE_plain\[\] PROGMEM = R"=====\(/);
    assert.match(out, /\)=====";/);
    assert.doesNotMatch(out, /R"[^=]/); // never a bare/undelimited raw string
    fs.rmSync(pt);
    fs.rmSync(path.join(modDir, 'html_plain.h'));
  });

  // The build hard-fails on any invalid manifest (cdata.js is the single
  // validator; the Python layer only forwards paths).  Each case asserts the
  // CLI exits non-zero with a message identifying the specific problem.
  const spec = { file: 'page.htm', name: 'PAGE_x', method: 'gzip', filter: 'html-minify' };
  const invalidManifests = {
    'malformed JSON': { raw: '{ "header": [ }', error: /Could not read manifest/ },
    'a header op missing output': {
      json: { header: [{ specs: [spec] }] }, error: /missing a string 'output'/ },
    'an empty header array': {
      json: { header: [] }, error: /missing or empty 'header' array/ },
    'no header key at all': {
      json: { schemaVersion: 1 }, error: /missing or empty 'header' array/ },
    'an unsupported schemaVersion': {
      json: { schemaVersion: 2, header: [{ output: 'x.h', specs: [spec] }] },
      error: /unsupported schemaVersion 2/ },
    'two header ops writing the same output': {
      json: { header: [
        { output: 'dup.h', srcDir: 'data', specs: [{ ...spec, name: 'PAGE_a' }] },
        { output: 'dup.h', srcDir: 'data', specs: [{ ...spec, name: 'PAGE_b' }] },
      ] },
      error: /collides with an earlier header op/ },
    // Path traversal: no manifest-supplied path may escape the usermod folder.
    'an output escaping the usermod folder': {
      json: { header: [{ output: '../escape.h', specs: [spec] }] },
      error: /output '\.\.\/escape\.h' resolves outside the usermod folder/ },
    'an absolute output path': {
      json: { header: [{ output: '/tmp/evil.h', specs: [spec] }] },
      error: /resolves outside the usermod folder/ },
    'a srcDir escaping the usermod folder': {
      json: { header: [{ output: 'x.h', srcDir: '../../elsewhere', specs: [spec] }] },
      error: /srcDir '\.\.\/\.\.\/elsewhere' resolves outside the usermod folder/ },
    'a spec file escaping the usermod folder': {
      json: { header: [{ output: 'x.h', srcDir: 'data', specs: [{ ...spec, file: '../../../../etc/hosts' }] }] },
      error: /file '.*etc\/hosts' resolves outside the usermod folder/ },
    // Spec value validation: bad values must fail loudly, not reach codegen.
    'a spec name that is not a C identifier': {
      json: { header: [{ output: 'x.h', srcDir: 'data', specs: [{ ...spec, name: 'not a name!' }] }] },
      error: /needs a 'name' that is a valid C identifier/ },
    'an unknown spec method': {
      json: { header: [{ output: 'x.h', srcDir: 'data', specs: [{ ...spec, method: 'zip' }] }] },
      error: /invalid 'method' "zip"/ },
    'an unknown spec filter': {
      json: { header: [{ output: 'x.h', srcDir: 'data', specs: [{ ...spec, filter: 'htlm-minify' }] }] },
      error: /invalid 'filter' "htlm-minify"/ },
    'a plaintext spec with only one delimiter': {
      json: { header: [{ output: 'x.h', srcDir: 'data',
        specs: [{ file: 'page.htm', name: 'PAGE_x', method: 'plaintext', prepend: '=====(' }] }] },
      error: /must set both 'prepend' and 'append'/ },
  };

  for (const [desc, { raw, json, error }] of Object.entries(invalidManifests)) {
    it(`rejects ${desc}`, async () => {
      const bad = path.join(modDir, 'invalid.json');
      fs.writeFileSync(bad, raw !== undefined ? raw : JSON.stringify(json));
      await assert.rejects(runCdata(`"${bad}"`), error);
      fs.rmSync(bad);
    });
  }
});
