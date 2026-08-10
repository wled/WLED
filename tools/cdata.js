/**
 * Writes compressed C arrays of data files (web interface)
 * How to use it?
 *
 * 1) Install Node 20+ and npm
 * 2) npm install
 * 3) npm run build
 *
 * If you change data folder often, you can run it in monitoring mode (it will recompile and update *.h on every file change)
 *
 * > npm run dev
 *
 * How it works?
 *
 * It uses NodeJS packages to inline, minify and GZIP files. See the mainJobs table and writeChunks/writeHtmlGzipped below.
 *
 * Command line:
 *   node cdata.js                       build the main web UI (skips up-to-date outputs)
 *   node cdata.js -f | --force          rebuild everything unconditionally
 *   node cdata.js --manifest <file>...  additionally build these usermod manifests in the same run
 *   node cdata.js --depfile <file>      write a Makefile-style dependency graph for the build system
 *   node cdata.js --emit-deps --depfile <file>   only write the dependency graph, build nothing
 *   node cdata.js <manifest>            legacy single-manifest mode (build just that manifest)
 */

const fs = require("node:fs");
const path = require("path");
const zlib = require("node:zlib");
const packageJson = require("../package.json");

// The heavy third-party packages (web-resource-inliner, clean-css,
// html-minifier-terser) are require()d lazily inside the functions that use
// them.  This keeps the --emit-deps dependency-graph pass working before
// `npm ci` has populated node_modules, and keeps node startup cheap.

// Generate build timestamp as UNIX timestamp (seconds since epoch)
function generateBuildTime() {
  return Math.floor(Date.now() / 1000);
}

const singleHeader = `/*
 * Binary array for the Web UI.
 * gzip is used for smaller size and improved speeds.
 *
 * Please see https://kno.wled.ge/advanced/custom-features/#changing-web-ui
 * to find out how to easily modify the web UI source!
 */

// Automatically generated build time for cache busting (UNIX timestamp)
#define WEB_BUILD_TIME ${generateBuildTime()}

`;

const multiHeader = `/*
 * More web UI HTML source arrays.
 * This file is auto generated, please don't make any changes manually.
 *
 * Instead, see https://kno.wled.ge/advanced/custom-features/#changing-web-ui
 * to find out how to easily modify the web UI source!
 */
`;

function hexdump(buffer, isHex = false) {
  let lines = [];

  for (let i = 0; i < buffer.length; i += (isHex ? 32 : 16)) {
    var block;
    let hexArray = [];
    if (isHex) {
      block = buffer.slice(i, i + 32)
      for (let j = 0; j < block.length; j += 2) {
        hexArray.push("0x" + block.slice(j, j + 2))
      }
    } else {
      block = buffer.slice(i, i + 16); // cut buffer into blocks of 16
      for (let value of block) {
        hexArray.push("0x" + value.toString(16).padStart(2, "0"));
      }
    }

    let hexString = hexArray.join(", ");
    let line = `  ${hexString}`;
    lines.push(line);
  }

  return lines.join(",\n");
}

function adoptVersionAndRepo(html) {
  let repoUrl = packageJson.repository ? packageJson.repository.url : undefined;
  if (repoUrl) {
    repoUrl = repoUrl.replace(/^git\+/, "");
    repoUrl = repoUrl.replace(/\.git$/, "");
    html = html.replaceAll("https://github.com/atuline/WLED", repoUrl);
    html = html.replaceAll("https://github.com/wled-dev/WLED", repoUrl);
  }
  let version = packageJson.version;
  if (version) {
    html = html.replaceAll("##VERSION##", version);
  }
  return html;
}

async function minify(str, type = "plain") {
  const options = {
    collapseWhitespace: true,
    conservativeCollapse: true, // preserve spaces in text
    collapseBooleanAttributes: true,
    collapseInlineTagWhitespace: true,
    minifyCSS: true,
    minifyJS: true,
    removeAttributeQuotes: true,
    removeComments: true,
    sortAttributes: true,
    sortClassName: true,
  };

  if (type == "plain") {
    return str;
  } else if (type == "css-minify") {
    const CleanCSS = require("clean-css");
    return new CleanCSS({}).minify(str).styles;
  } else if (type == "js-minify") {
    const minifyHtml = require("html-minifier-terser").minify;
    let js = await minifyHtml('<script>' + str + '</script>', options);
    return js.replace(/<[\/]*script>/g, '');
  } else if (type == "html-minify") {
    const minifyHtml = require("html-minifier-terser").minify;
    return await minifyHtml(str, options);
  }

  throw new Error("Unknown filter: " + type);
}

// Promisified wrapper around web-resource-inliner's callback API.  Resolves with
// the inlined HTML (css/js pulled in) or rejects on error.  Without this, the
// enclosing async function used to resolve *before* the callback ran, so callers
// could not await the real work and a thrown error vanished into an unhandled
// rejection instead of propagating.
function inlineHtml(sourceFile, inlineCss) {
  const inline = require("web-resource-inliner");
  return new Promise((resolve, reject) => {
    inline.html({
      fileContent: fs.readFileSync(sourceFile, "utf8"),
      relativeTo: path.dirname(sourceFile),
      strict: inlineCss,     // when not inlining css, ignore errors (enables linking style.css from subfolder htm files)
      stylesheets: inlineCss // when true (default), css is inlined
    }, (error, html) => error ? reject(error) : resolve(html));
  });
}

async function writeHtmlGzipped(sourceFile, resultFile, page, inlineCss = true) {
  console.info("Reading " + sourceFile);
  let html = await inlineHtml(sourceFile, inlineCss);
  html = adoptVersionAndRepo(html);
  const originalLength = html.length;
  html = await minify(html, "html-minify");
  const result = zlib.gzipSync(html, { level: zlib.constants.Z_BEST_COMPRESSION });
  console.info("Minified and compressed " + sourceFile + " from " + originalLength + " to " + result.length + " bytes");
  const array = hexdump(result);
  let src = singleHeader;
  src += `const uint16_t PAGE_${page}_length = ${result.length};\n`;
  src += `const uint8_t PAGE_${page}[] PROGMEM = {\n${array}\n};\n\n`;
  console.info("Writing " + resultFile);
  fs.writeFileSync(resultFile, src);
}

async function specToChunk(srcDir, s) {
  const buf = fs.readFileSync(srcDir + "/" + s.file);
  let chunk = `\n// Autogenerated from ${srcDir}/${s.file}, do not edit!!\n`

  if (s.method == "plaintext" || s.method == "gzip") {
    let str = buf.toString("utf-8");
    str = adoptVersionAndRepo(str);
    const originalLength = str.length;
    if (s.method == "gzip") {
      if (s.mangle) str = s.mangle(str);
      const zip = zlib.gzipSync(await minify(str, s.filter), { level: zlib.constants.Z_BEST_COMPRESSION });
      console.info("Minified and compressed " + s.file + " from " + originalLength + " to " + zip.length + " bytes");
      const result = hexdump(zip);
      chunk += `const uint16_t ${s.name}_length = ${zip.length};\n`;
      chunk += `const uint8_t ${s.name}[] PROGMEM = {\n${result}\n};\n\n`;
      return chunk;
    } else {
      const minified = await minify(str, s.filter);
      console.info("Minified " + s.file + " from " + originalLength + " to " + minified.length + " bytes");
      // prepend/append together delimit the C++ raw string: R"delim(...)delim".
      // Default to a safe matching pair when the spec supplies neither, so a
      // plaintext spec is never emitted as a bare R"..." -- which is invalid C++
      // because it has no parentheses.  (manifestToJobs rejects supplying only one
      // of the two, so an unmatched delimiter can't slip through here.)
      const prepend = s.prepend !== undefined ? s.prepend : "=====(";
      const append  = s.append  !== undefined ? s.append  : ")=====";
      chunk += `const char ${s.name}[] PROGMEM = R"${prepend}${minified}${append}";\n\n`;
      return s.mangle ? s.mangle(chunk) : chunk;
    }
  } else if (s.method == "binary") {
    const result = hexdump(buf);
    chunk += `const uint16_t ${s.name}_length = ${buf.length};\n`;
    chunk += `const uint8_t ${s.name}[] PROGMEM = {\n${result}\n};\n\n`;
    return chunk;
  }

  throw new Error("Unknown method: " + s.method);
}

async function writeChunks(srcDir, specs, resultFile) {
  let src = multiHeader;
  for (const s of specs) {
    console.info("Reading " + srcDir + "/" + s.file + " as " + s.name);
    src += await specToChunk(srcDir, s);
  }
  console.info("Writing " + src.length + " characters into " + resultFile);
  fs.writeFileSync(resultFile, src);
}

// List every file under a folder, recursively (absolute or srcDir-relative paths).
function listFilesRecursive(folderPath) {
  const out = [];
  for (const entry of fs.readdirSync(folderPath, { withFileTypes: true })) {
    const p = path.join(folderPath, entry.name);
    if (entry.isDirectory()) out.push(...listFilesRecursive(p));
    else out.push(p);
  }
  return out;
}

// List every directory under a folder, including the folder itself, recursively.
// A directory's mtime changes when an entry is added or removed, so these are the
// nodes that make folder-membership changes (a new or deleted file) observable to
// the mtime-based staleness check -- listing the surviving files alone cannot.
function listDirsRecursive(folderPath) {
  const out = [folderPath];
  for (const entry of fs.readdirSync(folderPath, { withFileTypes: true })) {
    if (entry.isDirectory()) out.push(...listDirsRecursive(path.join(folderPath, entry.name)));
  }
  return out;
}

// Every generated header, described as data so a single code path can both build
// it and report its input dependencies.
//   kind:'html'   — a single .htm inlined + gzipped into a PAGE_<page> array
//   kind:'chunks' — one or more specs concatenated into a header
const mainJobs = [
  { kind: 'html', src: "wled00/data/index.htm", out: "wled00/html_ui.h", page: 'index' },
  { kind: 'html', src: "wled00/data/pixart/pixart.htm", out: "wled00/html_pixart.h", page: 'pixart' },
  { kind: 'html', src: "wled00/data/pxmagic/pxmagic.htm", out: "wled00/html_pxmagic.h", page: 'pxmagic' },
  { kind: 'html', src: "wled00/data/pixelforge/pixelforge.htm", out: "wled00/html_pixelforge.h", page: 'pixelforge', inlineCss: false }, // do not inline css
  //{ kind: 'html', src: "wled00/data/edit.htm", out: "wled00/html_edit.h", page: 'edit' },

  {
    kind: 'chunks',
    srcDir: "wled00/data/",
    out: "wled00/js_iro.h",
    specs: [
      {
        file: "iro.js",
        name: "JS_iro",
        method: "gzip",
        filter: "plain", // no minification, it is already minified
        mangle: (s) => s.replace(/^\/\*![\s\S]*?\*\//, '') // remove license comment at the top
      }
    ],
  },

  {
    kind: 'chunks',
    srcDir: "wled00/data/pixelforge",
    out: "wled00/js_omggif.h",
    specs: [
      {
        file: "omggif.js",
        name: "JS_omggif",
        method: "gzip",
        filter: "js-minify",
        mangle: (s) => s.replace(/^\/\*![\s\S]*?\*\//, '') // remove license comment at the top
      }
    ],
  },

  {
    kind: 'chunks',
    srcDir: "wled00/data",
    out: "wled00/html_edit.h",
    specs: [
      {
        file: "edit.htm",
        name: "PAGE_edit",
        method: "gzip",
        filter: "html-minify"
      }
    ],
  },

  {
    kind: 'chunks',
    srcDir: "wled00/data/cpal",
    out: "wled00/html_cpal.h",
    specs: [
      {
        file: "cpal.htm",
        name: "PAGE_cpal",
        method: "gzip",
        filter: "html-minify"
      }
    ],
  },

  {
    kind: 'chunks',
    srcDir: "wled00/data",
    out: "wled00/html_settings.h",
    specs: [
      {
        file: "style.css",
        name: "PAGE_settingsCss",
        method: "gzip",
        filter: "css-minify",
        mangle: (str) =>
          str
            .replace("%%", "%")
      },
      {
        file: "common.js",
        name: "JS_common",
        method: "gzip",
        filter: "js-minify",
      },
      {
        file: "settings.htm",
        name: "PAGE_settings",
        method: "gzip",
        filter: "html-minify",
      },
      {
        file: "settings_wifi.htm",
        name: "PAGE_settings_wifi",
        method: "gzip",
        filter: "html-minify",
      },
      {
        file: "settings_leds.htm",
        name: "PAGE_settings_leds",
        method: "gzip",
        filter: "html-minify",
      },
      {
        file: "settings_dmx.htm",
        name: "PAGE_settings_dmx",
        method: "gzip",
        filter: "html-minify",
      },
      {
        file: "settings_ui.htm",
        name: "PAGE_settings_ui",
        method: "gzip",
        filter: "html-minify",
      },
      {
        file: "settings_sync.htm",
        name: "PAGE_settings_sync",
        method: "gzip",
        filter: "html-minify",
      },
      {
        file: "settings_time.htm",
        name: "PAGE_settings_time",
        method: "gzip",
        filter: "html-minify",
      },
      {
        file: "settings_sec.htm",
        name: "PAGE_settings_sec",
        method: "gzip",
        filter: "html-minify",
      },
      {
        file: "settings_um.htm",
        name: "PAGE_settings_um",
        method: "gzip",
        filter: "html-minify",
      },
      {
        file: "settings_2D.htm",
        name: "PAGE_settings_2D",
        method: "gzip",
        filter: "html-minify",
      },
      {
        file: "settings_pin.htm",
        name: "PAGE_settings_pin",
        method: "gzip",
        filter: "html-minify"
      },
      {
        file: "settings_pininfo.htm",
        name: "PAGE_settings_pininfo",
        method: "gzip",
        filter: "html-minify"
      }
    ],
  },

  {
    kind: 'chunks',
    srcDir: "wled00/data",
    out: "wled00/html_other.h",
    specs: [
      {
        file: "usermod.htm",
        name: "PAGE_usermod",
        method: "gzip",
        filter: "html-minify",
        mangle: (str) =>
          str.replace(/fetch\("http\:\/\/.*\/win/gms, 'fetch("/win'),
      },
      {
        file: "msg.htm",
        name: "PAGE_msg",
        prepend: "=====(",
        append: ")=====",
        method: "plaintext",
        filter: "html-minify",
        mangle: (str) => str.replace(/\<h2\>.*\<\/body\>/gms, "<h2>%MSG%</body>"),
      },
      {
        file: "dmxmap.htm",
        name: "PAGE_dmxmap",
        prepend: "=====(",
        append: ")=====",
        method: "plaintext",
        filter: "html-minify",
        mangle: (str) => `
#ifdef WLED_ENABLE_DMX
${str.replace(/function FM\(\)[ ]?\{/gms, "function FM() {%DMXVARS%\n")}
#else
const char PAGE_dmxmap[] PROGMEM = R"=====()=====";
#endif
`,
      },
      {
        file: "update.htm",
        name: "PAGE_update",
        method: "gzip",
        filter: "html-minify",
      },
      {
        file: "welcome.htm",
        name: "PAGE_welcome",
        method: "gzip",
        filter: "html-minify",
      },
      {
        file: "liveview.htm",
        name: "PAGE_liveview",
        method: "gzip",
        filter: "html-minify",
      },
      {
        file: "liveviewws2D.htm",
        name: "PAGE_liveviewws2D",
        method: "gzip",
        filter: "html-minify",
      },
      {
        file: "404.htm",
        name: "PAGE_404",
        method: "gzip",
        filter: "html-minify",
      },
      {
        file: "favicon.ico",
        name: "favicon",
        method: "binary",
      }
    ],
  },
];

// Top-level keys a usermod manifest (cdata.json) is allowed to carry.  Any other
// key is a validation error: the schema is closed so typos and not-yet-supported
// features (e.g. 'inject', 'defaults') fail loudly instead of being ignored.
const MANIFEST_ALLOWED_KEYS = new Set(['header', 'schemaVersion', '$schema']);

// The processing a spec's 'method' selects, and (for text methods) the optional
// 'filter' minifier.  A manifest is validated against these up front so a typo
// fails loudly with manifest context, rather than surfacing as an "Unknown
// method/filter" throw deep in the build (or, worse, silently generating invalid
// C++).  SPEC_FILTERS must stay in sync with the branches in minify().
const SPEC_METHODS = new Set(['plaintext', 'gzip', 'binary']);
const SPEC_FILTERS = new Set(['plain', 'css-minify', 'js-minify', 'html-minify']);

// Resolve `rel` (a manifest-supplied path) against `baseDir`, failing via `fail`
// if it lands outside `baseDir`.  Usermod manifests are third-party build inputs,
// so a stray `..` -- or an absolute path -- in an output/srcDir/spec file must not
// let the build read from, or write to, anywhere outside the usermod's own folder.
// path.resolve (not path.join) is used deliberately: it treats an absolute `rel`
// as absolute, so "/etc/passwd" is caught by the escape check rather than being
// quietly reinterpreted as a subpath.
function resolveWithin(baseDir, rel, fail, what) {
  const abs = path.resolve(baseDir, rel);
  const rp = path.relative(baseDir, abs);
  if (path.isAbsolute(rp) || rp === '..' || rp.startsWith('..' + path.sep)) {
    fail(what + " '" + rel + "' resolves outside the usermod folder");
  }
  return abs;
}

// Turn a usermod manifest (cdata.json) into one chunks job per header op.
// The manifest is an externally-tagged object: `header` is an array of
// header-operations, each of which produces one generated header file.
function manifestToJobs(manifestArg) {
  const manifestPath = path.resolve(manifestArg);
  const modDir = path.dirname(manifestPath);
  const fail = (msg) => {
    console.error("Invalid manifest " + manifestPath + ": " + msg);
    process.exit(1);
  };

  let manifest;
  try {
    manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
  } catch (e) {
    console.error("Could not read manifest " + manifestPath + ": " + e.message);
    process.exit(1);
  }

  if (typeof manifest !== 'object' || manifest === null || Array.isArray(manifest)) {
    fail("top-level value must be an object");
  }
  for (const key of Object.keys(manifest)) {
    if (!MANIFEST_ALLOWED_KEYS.has(key)) fail("unknown top-level key '" + key + "'");
  }
  // Absent schemaVersion means the baseline contract (implicit v1); only absent
  // or an explicit 1 is understood by this tooling.
  if ('schemaVersion' in manifest && manifest.schemaVersion !== 1) {
    fail("unsupported schemaVersion " + JSON.stringify(manifest.schemaVersion) + " (expected 1 or absent)");
  }
  if (!Array.isArray(manifest.header) || manifest.header.length === 0) {
    fail("missing or empty 'header' array");
  }

  const seenOut = new Set();
  return manifest.header.map((op, i) => {
    if (typeof op !== 'object' || op === null || Array.isArray(op)) {
      fail("header[" + i + "] must be an object");
    }
    if (typeof op.output !== 'string' || op.output.length === 0) {
      fail("header[" + i + "] is missing a string 'output'");
    }
    if (!Array.isArray(op.specs) || op.specs.length === 0) {
      fail("header[" + i + "] is missing a non-empty 'specs' array");
    }
    // Constrain every manifest-supplied path to the usermod folder (see
    // resolveWithin).  srcDir is resolved first so spec files can be checked
    // against it.
    const srcDir = resolveWithin(modDir, op.srcDir || 'data', fail, "header[" + i + "] srcDir");
    op.specs.forEach((s, j) => {
      const where = "header[" + i + "].specs[" + j + "]";
      if (typeof s !== 'object' || s === null || Array.isArray(s)) {
        fail(where + " must be an object");
      }
      if (typeof s.file !== 'string' || s.file.length === 0) {
        fail(where + " is missing a string 'file'");
      }
      resolveWithin(srcDir, s.file, fail, where + " file");
      // 'name' is emitted verbatim as a C identifier (const char <name>[] ...); a
      // value with spaces or punctuation would break -- or inject into -- the
      // generated header, so require a strict identifier.
      if (typeof s.name !== 'string' || !/^[A-Za-z_][A-Za-z0-9_]*$/.test(s.name)) {
        fail(where + " needs a 'name' that is a valid C identifier");
      }
      if (!SPEC_METHODS.has(s.method)) {
        fail(where + " has an invalid 'method' " + JSON.stringify(s.method) +
             " (expected one of: " + [...SPEC_METHODS].join(', ') + ")");
      }
      // filter is optional (absent means no minification); reject only a value
      // that minify() would not understand.
      if (s.filter !== undefined && !SPEC_FILTERS.has(s.filter)) {
        fail(where + " has an invalid 'filter' " + JSON.stringify(s.filter) +
             " (expected one of: " + [...SPEC_FILTERS].join(', ') + ")");
      }
      // A plaintext spec's raw-string delimiters must be supplied as a matching
      // pair; supplying only one would emit an unbalanced R"delim(...)" .  Neither
      // is fine -- specToChunk fills in a safe default pair.
      if (s.method === 'plaintext' && (s.prepend === undefined) !== (s.append === undefined)) {
        fail(where + " must set both 'prepend' and 'append' raw-string delimiters, or neither");
      }
    });
    // Two ops writing the same file would declare two SCons builders for one
    // target (an error); catch it here with a clear message instead.
    const out = resolveWithin(modDir, op.output, fail, "header[" + i + "] output");
    if (seenOut.has(out)) {
      fail("header[" + i + "] output '" + op.output + "' collides with an earlier header op; each must write a distinct file");
    }
    seenOut.add(out);
    return {
      kind: 'chunks',
      srcDir: srcDir,
      out: out,
      specs: op.specs,
      manifestPath: manifestPath, // the manifest itself is an input
    };
  });
}

// A job's inputs, split by how a change to them is detected:
//   files - exact files whose *content* feeds the output: this script,
//           package.json and package-lock.json (they affect every generated
//           output), the manifest, and a chunks job's listed specs.
//   dirs  - folders an html job depends on wholesale.  It inlines arbitrary
//           resources from its source folder, so a file appearing or disappearing
//           there changes the result.  We depend on the folder itself, not a
//           snapshot of its current files, so that added/removed files count as a
//           dependency change rather than being silently ignored.
// package-lock.json pins the exact minifier/inliner versions, so an `npm install`
// that changes them (without touching package.json) must still invalidate every
// header -- the compressed bytes they emit can change.  A missing lock file is
// tolerated downstream (isJobStale ignores absent inputs; the build system drops
// non-existent depfile sources).
function jobInputs(job) {
  const files = [__filename, 'package.json', 'package-lock.json'];
  const dirs = [];
  if (job.kind === 'html') {
    dirs.push(path.dirname(job.src));
  } else {
    for (const s of job.specs) files.push(path.join(job.srcDir, s.file));
  }
  if (job.manifestPath) files.push(job.manifestPath);
  return { files, dirs };
}

// A job is stale if its output is missing or older than any input.  For a folder
// dependency that means both every file *within* it (a content edit) and every
// directory in the tree (a bumped directory mtime, which is how an added or
// removed entry -- including a removed file we can no longer stat -- shows up).
function isJobStale(job) {
  let outMs;
  try {
    outMs = fs.statSync(job.out).mtimeMs;
  } catch (e) {
    if (e.code === 'ENOENT') return true;
    throw e;
  }
  const { files, dirs } = jobInputs(job);
  const inputs = files.slice();
  for (const d of dirs) {
    try {
      inputs.push(...listDirsRecursive(d), ...listFilesRecursive(d));
    } catch (e) {
      if (e.code === 'ENOENT') return true; // the whole source folder vanished
      throw e;
    }
  }
  for (const input of inputs) {
    try {
      if (fs.statSync(input).mtimeMs > outMs) return true;
    } catch (e) {
      if (e.code !== 'ENOENT') throw e; // a missing input can't make us stale
    }
  }
  return false;
}

async function buildJob(job) {
  if (job.kind === 'html') {
    await writeHtmlGzipped(job.src, job.out, job.page, job.inlineCss !== false);
  } else {
    await writeChunks(job.srcDir, job.specs, job.out);
  }
}

// Emit a Makefile-style depfile: one `output: input1 input2 ...` rule per job.
// Paths are written relative to the current directory (the project root) with
// forward slashes, so the file is portable and free of Windows drive-letter
// colons that would confuse a depfile parser.  Spaces in file names are escaped
// as "\ " (Makefile style) so a name like "Read Me.txt" stays a single token.
function toDepPath(p) {
  return path.relative(process.cwd(), path.resolve(p)).split(path.sep).join('/');
}

function escapeDepPath(p) {
  return toDepPath(p).replace(/ /g, '\\ ');
}

function emitDepfile(depfilePath, jobs) {
  let out = "# Auto-generated by tools/cdata.js -- web UI dependency graph. Do not edit.\n";
  for (const job of jobs) {
    const target = escapeDepPath(job.out);
    // A folder input is emitted as the directory itself (not its current files):
    // the build system re-reads the folder each build, so files added there later
    // are picked up without the depfile having to be regenerated first.
    const { files, dirs } = jobInputs(job);
    const deps = [...files, ...dirs].map(escapeDepPath);
    out += `${target}: ${deps.join(' ')}\n`;
  }
  fs.mkdirSync(path.dirname(path.resolve(depfilePath)), { recursive: true });
  fs.writeFileSync(depfilePath, out);
  console.info("Wrote dependency graph (" + jobs.length + " targets) to " + depfilePath);
}

// Await every task; if any fail, report them all and throw once.  Using
// allSettled rather than Promise.all means one failing conversion cannot abort
// its siblings mid-write and leave a truncated header behind.
async function runAll(tasks) {
  const results = await Promise.allSettled(tasks);
  const errors = results.filter(r => r.status === 'rejected').map(r => r.reason);
  if (errors.length > 0) {
    for (const err of errors) console.error(err);
    throw new Error(errors.length + " web UI build task(s) failed");
  }
}

function parseArgs(args) {
  const opts = { force: false, depfile: null, emitDepsOnly: false, manifests: [], legacyManifest: null };
  for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if (a === '-f' || a === '--force') opts.force = true;
    else if (a === '--emit-deps') opts.emitDepsOnly = true;
    else if (a === '--depfile') {
      opts.depfile = args[++i];
      if (opts.depfile === undefined) { console.error("--depfile requires a path"); process.exit(1); }
    }
    else if (a === '--manifest') {
      const m = args[++i];
      if (m === undefined) { console.error("--manifest requires a path"); process.exit(1); }
      opts.manifests.push(m);
    }
    else if (!a.startsWith('-')) opts.legacyManifest = a;
    else { console.error("Unknown option: " + a); process.exit(1); }
  }
  return opts;
}

async function main() {
  const opts = parseArgs(process.argv.slice(2));

  // Assemble the job list.  Legacy single-manifest mode builds only that
  // manifest; otherwise we build the main UI plus any --manifest usermods.
  const jobs = opts.legacyManifest
    ? manifestToJobs(opts.legacyManifest)
    : mainJobs.concat(opts.manifests.flatMap(manifestToJobs));

  if (opts.emitDepsOnly) {
    if (!opts.depfile) {
      console.error("--emit-deps requires --depfile <path>");
      process.exit(1);
    }
    emitDepfile(opts.depfile, jobs);
    return;
  }

  const stale = opts.force ? jobs : jobs.filter(isJobStale);
  if (stale.length === 0) {
    console.info("Web UI is already built");
  } else {
    await runAll(stale.map(buildJob));
  }

  // Refresh the dependency graph for the build system, if requested.
  if (opts.depfile) emitDepfile(opts.depfile, jobs);
}

// Don't run the build when imported by the test harness.
if (process.env.NODE_ENV !== 'test') {
  main().catch((err) => {
    console.error(err);
    process.exitCode = 1;
  });
}
