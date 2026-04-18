#include "xlz_unzip.h"
#include "usermod_fseq.h" // Contains FSEQ playback logic and getter methods for pins

namespace {
constexpr size_t XLZ_BUFFER_SIZE = 8192;

// IMPORTANT: unzipLIB uses a fixed internal structure of roughly 41 KB.
// Do NOT put UNZIP on the task stack (e.g. as a local variable in loop()).
// Keeping one static instance avoids stack-canary panics in loopTask.
static UNZIP g_xlzZip;

static bool endsWithIgnoreCase(const String& value, const char* suffix) {
  const size_t n = strlen(suffix);
  if (value.length() < n) return false;
  return value.substring(value.length() - n).equalsIgnoreCase(suffix);
}

static String normalizePath(const String& path) {
  if (path.isEmpty()) return String("/");
  if (path[0] == '/') return path;
  return String("/") + path;
}
} // namespace

bool XLZUnzip::hasXLZExtension(const String& path) {
  return endsWithIgnoreCase(path, ".xlz");
}

void* XLZUnzip::openZip(const char* filename, int32_t* size) {
  if (size) *size = 0;

  String path = filename ? String(filename) : String();
  path = normalizePath(path);
  DEBUG_PRINTF("[XLZ] openZip('%s')\n", path.c_str());

  FsHandle* h = new FsHandle();
  h->file = SD_ADAPTER.open(path.c_str(), FILE_READ);

  if (!h->file) {
    delete h;
    DEBUG_PRINTF("[XLZ] Failed to open archive: %s\n", path.c_str());
    return nullptr;
  }

  if (size) *size = static_cast<int32_t>(h->file.size());
  h->pos = 0;
  return h;
}

void XLZUnzip::closeZip(void* p) {
  if (!p) return;
  ZIPFILE* zf = static_cast<ZIPFILE*>(p);
  FsHandle* h = static_cast<FsHandle*>(zf->fHandle);
  if (h) {
    if (h->file) h->file.close();
    delete h;
    zf->fHandle = nullptr;
  }
}

int32_t XLZUnzip::readZip(void* p, uint8_t* buffer, int32_t length) {
  if (!p || !buffer || length <= 0) return 0;

  ZIPFILE* zf = static_cast<ZIPFILE*>(p);
  FsHandle* h = static_cast<FsHandle*>(zf->fHandle);
  if (!h || !h->file) return 0;

  if (!h->file.seek(h->pos)) return 0;

  const int32_t bytesRead = static_cast<int32_t>(h->file.read(buffer, length));
  if (bytesRead > 0) h->pos += bytesRead;
  return bytesRead;
}

int32_t XLZUnzip::seekZip(void* p, int32_t position, int iType) {
  if (!p) return -1;

  ZIPFILE* zf = static_cast<ZIPFILE*>(p);
  FsHandle* h = static_cast<FsHandle*>(zf->fHandle);
  if (!h || !h->file) return -1;

  int32_t newPos = position;
  switch (iType) {
    case SEEK_SET:
      newPos = position;
      break;
    case SEEK_CUR:
      newPos = h->pos + position;
      break;
    case SEEK_END:
      newPos = static_cast<int32_t>(h->file.size()) + position;
      break;
    default:
      return -1;
  }

  if (newPos < 0) newPos = 0;
  if (!h->file.seek(newPos)) return -1;
  h->pos = newPos;
  return h->pos;
}

String XLZUnzip::sanitizeEntryName(const char* rawName) {
  String name = rawName ? String(rawName) : String();
  name.replace('\\', '/');
  name.trim();

  while (name.startsWith("/")) {
    name.remove(0, 1);
  }
  while (name.startsWith("./")) {
    name.remove(0, 2);
  }

  // prevent path traversal on extraction
  if (name.indexOf("../") >= 0 || name == "..") {
    return String();
  }

  return name;
}

bool XLZUnzip::unpackCurrentFile(UNZIP& zip, const String& outputPath, uint32_t expectedSize) {
  if (zip.openCurrentFile() != UNZ_OK) {
    DEBUG_PRINTLN(F("[XLZ] openCurrentFile() failed"));
    return false;
  }

  if (SD_ADAPTER.exists(outputPath.c_str())) {
    SD_ADAPTER.remove(outputPath.c_str());
  }

  File out = SD_ADAPTER.open(outputPath.c_str(), FILE_WRITE);
  if (!out) {
    DEBUG_PRINTF("[XLZ] Failed to create output file: %s\n", outputPath.c_str());
    zip.closeCurrentFile();
    return false;
  }

  uint8_t* buffer = static_cast<uint8_t*>(malloc(XLZ_BUFFER_SIZE));
  if (!buffer) {
    DEBUG_PRINTLN(F("[XLZ] Failed to allocate unzip buffer"));
    out.close();
    SD_ADAPTER.remove(outputPath.c_str());
    zip.closeCurrentFile();
    return false;
  }

  bool ok = true;
  uint32_t written = 0;

  while (true) {
    const int rc = zip.readCurrentFile(buffer, XLZ_BUFFER_SIZE);
    if (rc < 0) {
      DEBUG_PRINTF("[XLZ] readCurrentFile() failed: %d\n", rc);
      ok = false;
      break;
    }
    if (rc == 0) break;

    if (out.write(buffer, static_cast<size_t>(rc)) != static_cast<size_t>(rc)) {
      DEBUG_PRINTLN(F("[XLZ] Failed while writing decompressed data"));
      ok = false;
      break;
    }

    written += static_cast<uint32_t>(rc);
    yield();
  }

  free(buffer);
  out.flush();
  out.close();

  const int closeRc = zip.closeCurrentFile();
  if (closeRc != UNZ_OK) {
    DEBUG_PRINTF("[XLZ] closeCurrentFile() failed: %d\n", closeRc);
    ok = false;
  }

  if (ok && expectedSize > 0 && written != expectedSize) {
    DEBUG_PRINTF("[XLZ] Size mismatch. expected=%lu actual=%lu\n",
                 static_cast<unsigned long>(expectedSize),
                 static_cast<unsigned long>(written));
    ok = false;
  }

  if (!ok) {
    SD_ADAPTER.remove(outputPath.c_str());
  }

  return ok;
}

bool XLZUnzip::unpackArchive(const String& archivePath, String& finalOutputPath) {
  const String zipPath = normalizePath(archivePath);
  DEBUG_PRINTF("[XLZ] unpackArchive('%s')\n", zipPath.c_str());

  const int openRc = g_xlzZip.openZIP(zipPath.c_str(), openZip, closeZip, readZip, seekZip);
  if (openRc != UNZ_OK) {
    DEBUG_PRINTF("[XLZ] openZIP() failed for %s: %d\n", zipPath.c_str(), openRc);
    return false;
  }

  bool ok = false;
  unz_file_info fileInfo{};
  char entryName[256] = {0};
  char comment[64] = {0};

  if (g_xlzZip.gotoFirstFile() != UNZ_OK) {
    DEBUG_PRINTLN(F("[XLZ] Archive contains no files"));
    g_xlzZip.closeZIP();
    return false;
  }

  const int infoRc = g_xlzZip.getFileInfo(&fileInfo, entryName, sizeof(entryName),
                                          nullptr, 0, comment, sizeof(comment));
  if (infoRc != UNZ_OK) {
    DEBUG_PRINTF("[XLZ] getFileInfo() failed: %d\n", infoRc);
    g_xlzZip.closeZIP();
    return false;
  }

  String safeName = sanitizeEntryName(entryName);
  if (safeName.isEmpty()) {
    DEBUG_PRINTLN(F("[XLZ] Invalid filename inside archive"));
    g_xlzZip.closeZIP();
    return false;
  }

  finalOutputPath = zipPath;
  if (hasXLZExtension(finalOutputPath)) {
    finalOutputPath.remove(finalOutputPath.length() - 4);
    finalOutputPath += ".fseq";
  } else {
    finalOutputPath = normalizePath(safeName);
    if (!endsWithIgnoreCase(finalOutputPath, ".fseq")) {
      finalOutputPath += ".fseq";
    }
  }

  const uint64_t totalBytes = SD_ADAPTER.totalBytes();
  const uint64_t usedBytes = SD_ADAPTER.usedBytes();
  const uint64_t freeBytes = (totalBytes >= usedBytes) ? (totalBytes - usedBytes) : 0;
  if (fileInfo.uncompressed_size > freeBytes) {
    DEBUG_PRINTF("[XLZ] Not enough free space. need=%lu free=%lu\n",
                 static_cast<unsigned long>(fileInfo.uncompressed_size),
                 static_cast<unsigned long>(freeBytes));
    g_xlzZip.closeZIP();
    return false;
  }

  DEBUG_PRINTF("[XLZ] Extracting %s -> %s\n", zipPath.c_str(), finalOutputPath.c_str());
  ok = unpackCurrentFile(g_xlzZip, finalOutputPath, static_cast<uint32_t>(fileInfo.uncompressed_size));

  const int nextRc = g_xlzZip.gotoNextFile();
  if (ok && nextRc == UNZ_OK) {
    DEBUG_PRINTLN(F("[XLZ] Warning: archive contains more than one file; only the first file was extracted"));
  }

  g_xlzZip.closeZIP();
  return ok;
}

bool XLZUnzip::unpackAndDelete(const String& archivePath, String* outFile) {
  DEBUG_PRINTF("[XLZ] raw archivePath='%s'\n", archivePath.c_str());

  const String zipPath = normalizePath(archivePath);
  DEBUG_PRINTF("[XLZ] normalized archivePath='%s'\n", zipPath.c_str());

  if (!hasXLZExtension(zipPath)) {
    DEBUG_PRINTF("[XLZ] Not an .xlz file: %s\n", zipPath.c_str());
    return false;
  }

  if (!SD_ADAPTER.exists(zipPath.c_str())) {
    DEBUG_PRINTF("[XLZ] Archive not found: %s\n", zipPath.c_str());
    return false;
  }

  String finalOutputPath;
  const bool ok = unpackArchive(zipPath, finalOutputPath);
  if (!ok) return false;

  if (!SD_ADAPTER.remove(zipPath.c_str())) {
    DEBUG_PRINTF("[XLZ] Extracted, but could not delete archive: %s\n", zipPath.c_str());
  }

  if (outFile) {
    *outFile = finalOutputPath;
  }

  DEBUG_PRINTF("[XLZ] Done: %s\n", finalOutputPath.c_str());
  return true;
}

uint8_t XLZUnzip::processAllPendingXLZ() {
  DEBUG_PRINTLN("[XLZ] processAllPendingXLZ() entered");

  File root = SD_ADAPTER.open("/");
  if (!root || !root.isDirectory()) {
    DEBUG_PRINTLN("[XLZ] failed to open root directory");
    return 0;
  }

  uint8_t count = 0;
  File file = root.openNextFile();
  while (file) {
    String path = String(file.name());
    path = normalizePath(path);
    DEBUG_PRINTF("[XLZ] found entry: %s\n", path.c_str());

    const bool isDir = file.isDirectory();
    file.close();

    if (!isDir && hasXLZExtension(path)) {
      DEBUG_PRINTF("[XLZ] unpacking: %s\n", path.c_str());
      if (unpackAndDelete(path, nullptr)) {
        ++count;
      }
    }

    file = root.openNextFile();
    yield();
  }

  root.close();
  DEBUG_PRINTF("[XLZ] processAllPendingXLZ() done, count=%u\n", count);
  return count;
}
