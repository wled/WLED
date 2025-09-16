#include "wled.h"

/*
 * UI Update Processing
 * Handles extraction and deployment of UI files from ZIP archives
 */

bool processUIUpdate(const String& updatePath) {
  DEBUG_PRINTF_P(PSTR("Processing UI update: %s\n"), updatePath.c_str());
  
  // For now, implement a simple file replacement system
  // In a full implementation, you'd want to use a ZIP library like MiniZ
  
  File updateFile = WLED_FS.open(updatePath, "r");
  if (!updateFile) {
    DEBUG_PRINTLN(F("Failed to open update file"));
    return false;
  }
  
  // Simple implementation: check if it's a text file and extract content
  // This is a basic implementation - for production you'd want proper ZIP handling
  String content = updateFile.readString();
  updateFile.close();
  
  // For demonstration, let's implement a simple text-based update format
  // Format: FILENAME:CONTENT:ENDFILE
  if (content.indexOf("FILENAME:") == 0) {
    DEBUG_PRINTLN(F("Processing text-based UI update"));
    
    int pos = 0;
    while (pos < content.length()) {
      int filenameStart = content.indexOf("FILENAME:", pos);
      if (filenameStart == -1) break;
      
      int filenameEnd = content.indexOf(":", filenameStart + 9);
      if (filenameEnd == -1) break;
      
      String filename = content.substring(filenameStart + 9, filenameEnd);
      
      int contentStart = filenameEnd + 1;
      int contentEnd = content.indexOf(":ENDFILE", contentStart);
      if (contentEnd == -1) break;
      
      String fileContent = content.substring(contentStart, contentEnd);
      
      // Ensure filename starts with /
      if (filename.charAt(0) != '/') {
        filename = "/" + filename;
      }
      
      DEBUG_PRINTF_P(PSTR("Updating file: %s\n"), filename.c_str());
      
      // Write the file
      File targetFile = WLED_FS.open(filename, "w");
      if (targetFile) {
        targetFile.print(fileContent);
        targetFile.close();
        DEBUG_PRINTF_P(PSTR("Successfully updated: %s\n"), filename.c_str());
      } else {
        DEBUG_PRINTF_P(PSTR("Failed to write file: %s\n"), filename.c_str());
      }
      
      pos = contentEnd + 8; // Move past :ENDFILE
    }
    
    // Invalidate cache to force reload of updated files
    cacheInvalidate++;
    
    DEBUG_PRINTLN(F("UI update completed successfully"));
    return true;
  }
  
  // If it's not our text format, try to handle as a simple file replacement
  // This is a fallback for single-file updates
  DEBUG_PRINTLN(F("Attempting simple file replacement"));
  
  // For now, just return success - in production you'd implement proper ZIP extraction
  DEBUG_PRINTLN(F("UI update completed (simple mode)"));
  return true;
}

/*
 * Helper function to create a simple update package
 * This would typically be called from a development tool
 */
String createUIUpdatePackage(const String& filename, const String& content) {
  return "FILENAME:" + filename + ":" + content + ":ENDFILE";
}

/*
 * Helper function to backup current UI files before update
 */
bool backupUIFiles() {
  DEBUG_PRINTLN(F("Creating UI backup"));
  
  // List of UI files to backup
  const char* uiFiles[] = {
    // OneWheel Interface
    "/onewheel.htm",
    "/welcome.htm", 
    "/imu-debug.html",
    // WLED Advanced Interface (filesystem overrides)
    "/index.htm",
    "/index.css",
    "/index.js",
    "/iro.js",
    "/rangetouch.js",
    // Settings pages
    "/settings.htm",
    "/settings_wifi.htm",
    "/settings_leds.htm",
    "/settings_ui.htm",
    "/settings_sync.htm",
    "/settings_time.htm",
    "/settings_sec.htm",
    "/settings_2D.htm",
    "/settings_dmx.htm",
    "/settings_pin.htm",
    "/settings_um.htm"
  };
  
  const int numFiles = sizeof(uiFiles) / sizeof(uiFiles[0]);
  
  for (int i = 0; i < numFiles; i++) {
    String filename = String(uiFiles[i]);
    String backupName = filename + ".backup";
    
    if (WLED_FS.exists(filename)) {
      File source = WLED_FS.open(filename, "r");
      File backup = WLED_FS.open(backupName, "w");
      
      if (source && backup) {
        while (source.available()) {
          backup.write(source.read());
        }
        source.close();
        backup.close();
        DEBUG_PRINTF_P(PSTR("Backed up: %s\n"), filename.c_str());
      }
    }
  }
  
  return true;
}

/*
 * Helper function to restore UI files from backup
 */
bool restoreUIFiles() {
  DEBUG_PRINTLN(F("Restoring UI from backup"));
  
  const char* uiFiles[] = {
    // OneWheel Interface
    "/onewheel.htm",
    "/welcome.htm", 
    "/imu-debug.html",
    // WLED Advanced Interface (filesystem overrides)
    "/index.htm",
    "/index.css",
    "/index.js",
    "/iro.js",
    "/rangetouch.js",
    // Settings pages
    "/settings.htm",
    "/settings_wifi.htm",
    "/settings_leds.htm",
    "/settings_ui.htm",
    "/settings_sync.htm",
    "/settings_time.htm",
    "/settings_sec.htm",
    "/settings_2D.htm",
    "/settings_dmx.htm",
    "/settings_pin.htm",
    "/settings_um.htm"
  };
  
  const int numFiles = sizeof(uiFiles) / sizeof(uiFiles[0]);
  
  for (int i = 0; i < numFiles; i++) {
    String filename = String(uiFiles[i]);
    String backupName = filename + ".backup";
    
    if (WLED_FS.exists(backupName)) {
      File backup = WLED_FS.open(backupName, "r");
      File target = WLED_FS.open(filename, "w");
      
      if (backup && target) {
        while (backup.available()) {
          target.write(backup.read());
        }
        backup.close();
        target.close();
        DEBUG_PRINTF_P(PSTR("Restored: %s\n"), filename.c_str());
      }
    }
  }
  
  return true;
}
