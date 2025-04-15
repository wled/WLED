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
 * It uses NodeJS packages to inline, minify and GZIP files. See writeHtmlGzipped and writeChunks invocations at the bottom of the page.
 */

const fs = require("node:fs");
const path = require("path");
const inline = require("web-resource-inliner");
const zlib = require("node:zlib");
const CleanCSS = require("clean-css");
const minifyHtml = require("html-minifier-terser").minify;
const packageJson = require("../package.json");

// Export functions for testing
module.exports = { isFileNewerThan, isAnyFileInFolderNewerThan };

const output = ["wled00/html_ui.h", "wled00/html_pixart.h", "wled00/html_cpal.h", "wled00/html_pxmagic.h", "wled00/html_gifplayer.h", "wled00/html_settings.h", "wled00/html_other.h"]

// \x1b[34m is blue, \x1b[36m is cyan, \x1b[0m is reset
const wledBanner = `
\t\x1b[34m  ##  ##      ##        ######    ######
\t\x1b[34m##      ##    ##      ##        ##  ##  ##
\t\x1b[34m##  ##  ##  ##        ######        ##  ##
\t\x1b[34m##  ##  ##  ##        ##            ##  ##
\t\x1b[34m  ##  ##      ######    ######    ######
\t\t\x1b[36m build script for web UI
\x1b[0m`;

const singleHeader = `/*
 * Binary array for the Web UI.
 * gzip is used for smaller size and improved speeds.
 * 
 * Please see https://kno.wled.ge/advanced/custom-features/#changing-web-ui
 * to find out how to easily modify the web UI source!
 */
 
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
    return new CleanCSS({}).minify(str).styles;
  } else if (type == "js-minify") {
    let js = await minifyHtml('<script>' + str + '</script>', options);
    return js.replace(/<[\/]*script>/g, '');
  } else if (type == "html-minify") {
    return await minifyHtml(str, options);
  }

  throw new Error("Unknown filter: " + type);
}

async function writeHtmlGzipped(sourceFile, resultFile, page) {
  console.info("Reading " + sourceFile);
  inline.html({
    fileContent: fs.readFileSync(sourceFile, "utf8"),
    relativeTo: path.dirname(sourceFile),
    strict: true,
  },
    async function (error, html) {
      if (error) throw error;

      html = adoptVersionAndRepo(html);
      const originalLength = html.length;
      html = await minify(html, "html-minify");
      const result = zlib.gzipSync(html, { level: zlib.constants.Z_BEST_COMPRESSION });
      console.info("Minified and compressed " + sourceFile + " from " + originalLength + " to " + result.length + " bytes");
      const array = hexdump(result);
      let src = singleHeader;
      src += `const uint16_t PAGE_${page}_L = ${result.length};\n`;
      src += `const uint8_t PAGE_${page}[] PROGMEM = {\n${array}\n};\n\n`;
      console.info("Writing " + resultFile);
      fs.writeFileSync(resultFile, src);
    });
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
      chunk += `const char ${s.name}[] PROGMEM = R"${s.prepend || ""}${minified}${s.append || ""}";\n\n`;
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

// Check if a file is newer than a given time
function isFileNewerThan(filePath, time) {
  const stats = fs.statSync(filePath);
  return stats.mtimeMs > time;
}

// Check if any file in a folder (or its subfolders) is newer than a given time
function isAnyFileInFolderNewerThan(folderPath, time) {
  const files = fs.readdirSync(folderPath, { withFileTypes: true });
  for (const file of files) {
    const filePath = path.join(folderPath, file.name);
    if (isFileNewerThan(filePath, time)) {
      return true;
    }
    if (file.isDirectory() && isAnyFileInFolderNewerThan(filePath, time)) {
      return true;
    }
  }
  return false;
}

// Check if the web UI is already built
function isAlreadyBuilt(webUIPath, packageJsonPath = "package.json") {
  let lastBuildTime = Infinity;

  for (const file of output) {
    try {
      lastBuildTime = Math.min(lastBuildTime, fs.statSync(file).mtimeMs);
    } catch (e) {
      if (e.code !== 'ENOENT') throw e;
      console.info("File " + file + " does not exist. Rebuilding...");
      return false;
    }
  }

  return !isAnyFileInFolderNewerThan(webUIPath, lastBuildTime) && !isFileNewerThan(packageJsonPath, lastBuildTime) && !isFileNewerThan(__filename, lastBuildTime);
}

// Don't run this script if we're in a test environment
if (process.env.NODE_ENV === 'test') {
  return;
}

console.info(wledBanner);

if (isAlreadyBuilt("wled00/data") && process.argv[2] !== '--force' && process.argv[2] !== '-f') {
  console.info("Web UI is already built");
  return;
}

writeHtmlGzipped("wled00/data/index.htm", "wled00/html_ui.h", 'index');
writeHtmlGzipped("wled00/data/pixart/pixart.htm", "wled00/html_pixart.h", 'pixart');
writeHtmlGzipped("wled00/data/cpal/cpal.htm", "wled00/html_cpal.h", 'cpal');
writeHtmlGzipped("wled00/data/pxmagic/pxmagic.htm", "wled00/html_pxmagic.h", 'pxmagic');
writeHtmlGzipped("wled00/data/gifplayer/gifplayer.htm", "wled00/html_gifplayer.h", 'gifplayer');

writeChunks(
  "wled00/data",
  [
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
    }
  ],
  "wled00/html_settings.h"
);

writeChunks(
  "wled00/data",
  [
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
      mangle: (str) =>
        str
          .replace(
            /function GetV().*\<\/script\>/gms,
            "</script><script src=\"/settings/s.js?p=9\"></script>"
          )
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
  "wled00/html_other.h"
);
