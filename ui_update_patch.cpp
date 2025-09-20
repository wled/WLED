// Patch for main.cpp - Replace the existing processUIUpdate function
// This version can handle large files by processing them in chunks

// Add this function declaration near the other function declarations (around line 420)
bool processUIUpdateStreaming(const String& updatePath);
bool saveUIFile(const String& filename, const String& content);

// Replace the existing processUIUpdate function (around line 3178) with this:
bool processUIUpdate(const String& updatePath) {
    Serial.printf("Processing UI update: %s\n", updatePath.c_str());
    
    File updateFile = SPIFFS.open(updatePath, "r");
    if (!updateFile) {
        Serial.println("Failed to open update file");
        return false;
    }
    
    // Check file size first
    size_t fileSize = updateFile.size();
    Serial.printf("Update file size: %d bytes\n", fileSize);
    
    updateFile.close();
    
    // If file is large (>30KB), use streaming mode
    if (fileSize > 30000) {
        Serial.println("Large file detected, using streaming mode");
        return processUIUpdateStreaming(updatePath);
    }
    
    // For smaller files, use the original method
    Serial.println("Small file, using original method");
    
    updateFile = SPIFFS.open(updatePath, "r");
    String content = updateFile.readString();
    updateFile.close();
    
    // Process text-based update format: FILENAME:CONTENT:ENDFILE
    if (content.indexOf("FILENAME:") == 0) {
        Serial.println("Processing text-based UI update");
        
        int pos = 0;
        int filesProcessed = 0;
        
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
            
            if (saveUIFile(filename, fileContent)) {
                filesProcessed++;
            }
            
            pos = contentEnd + 8; // Move past :ENDFILE
        }
        
        Serial.printf("UI update completed successfully - %d files processed\n", filesProcessed);
        return filesProcessed > 0;
    }
    
    // For ZIP files, we'd need a ZIP library - for now just return success
    Serial.println("ZIP update format not yet implemented - use text format");
    return false;
}

// Add these helper functions at the end of the file (before the last closing brace):

bool processUIUpdateStreaming(const String& updatePath) {
    Serial.printf("Processing UI update (streaming): %s\n", updatePath.c_str());
    
    File updateFile = SPIFFS.open(updatePath, "r");
    if (!updateFile) {
        Serial.println("Failed to open update file");
        return false;
    }
    
    // Process text-based update format: FILENAME:CONTENT:ENDFILE
    String line;
    String currentFilename = "";
    String currentContent = "";
    bool inFileContent = false;
    int filesProcessed = 0;
    
    Serial.println("Processing text-based UI update (streaming mode)");
    
    while (updateFile.available()) {
        line = updateFile.readStringUntil('\n');
        line.trim();
        
        if (line.startsWith("FILENAME:")) {
            // Save previous file if we have one
            if (currentFilename.length() > 0 && currentContent.length() > 0) {
                if (saveUIFile(currentFilename, currentContent)) {
                    filesProcessed++;
                }
            }
            
            // Start new file
            int colonPos = line.indexOf(':', 9); // Find second colon
            if (colonPos > 0) {
                currentFilename = line.substring(9, colonPos);
                currentContent = line.substring(colonPos + 1);
                inFileContent = true;
                Serial.printf("Starting file: %s\n", currentFilename.c_str());
            }
        }
        else if (line == ":ENDFILE") {
            // End of current file
            if (currentFilename.length() > 0) {
                if (saveUIFile(currentFilename, currentContent)) {
                    filesProcessed++;
                }
                currentFilename = "";
                currentContent = "";
                inFileContent = false;
            }
        }
        else if (inFileContent && line.length() > 0) {
            // Add line to current file content
            if (currentContent.length() > 0) {
                currentContent += "\n";
            }
            currentContent += line;
        }
    }
    
    // Save last file if we have one
    if (currentFilename.length() > 0 && currentContent.length() > 0) {
        if (saveUIFile(currentFilename, currentContent)) {
            filesProcessed++;
        }
    }
    
    updateFile.close();
    
    Serial.printf("UI update completed successfully - %d files processed\n", filesProcessed);
    return filesProcessed > 0;
}

bool saveUIFile(const String& filename, const String& content) {
    // Ensure filename starts with / (avoid double slashes)
    String cleanFilename = filename;
    if (cleanFilename.charAt(0) != '/') {
        cleanFilename = "/" + cleanFilename;
    }
    
    Serial.printf("Saving file: %s (%d bytes)\n", cleanFilename.c_str(), content.length());
    
    // Write the file
    File targetFile = SPIFFS.open(cleanFilename, "w");
    if (targetFile) {
        size_t bytesWritten = targetFile.print(content);
        targetFile.close();
        Serial.printf("Successfully saved: %s (%d bytes written)\n", cleanFilename.c_str(), bytesWritten);
        
        // Verify the file was written correctly
        File verifyFile = SPIFFS.open(cleanFilename, "r");
        if (verifyFile) {
            Serial.printf("✅ Verification: %s exists (%d bytes)\n", cleanFilename.c_str(), verifyFile.size());
            verifyFile.close();
            return true;
        } else {
            Serial.printf("❌ Verification failed: %s not found after write\n", cleanFilename.c_str());
            return false;
        }
    } else {
        Serial.printf("Failed to write file: %s\n", cleanFilename.c_str());
        return false;
    }
}
