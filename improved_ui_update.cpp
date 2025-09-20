// Improved UI Update Function - Stream Processing
// This version processes the update file in chunks instead of loading it all into memory

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

// Alternative: Process file in fixed-size chunks
bool processUIUpdateChunked(const String& updatePath) {
    Serial.printf("Processing UI update (chunked): %s\n", updatePath.c_str());
    
    File updateFile = SPIFFS.open(updatePath, "r");
    if (!updateFile) {
        Serial.println("Failed to open update file");
        return false;
    }
    
    const size_t CHUNK_SIZE = 1024; // 1KB chunks
    char buffer[CHUNK_SIZE + 1];
    String accumulatedContent = "";
    int filesProcessed = 0;
    
    Serial.println("Processing text-based UI update (chunked mode)");
    
    while (updateFile.available()) {
        size_t bytesRead = updateFile.readBytes(buffer, CHUNK_SIZE);
        buffer[bytesRead] = '\0';
        
        accumulatedContent += String(buffer);
        
        // Process complete files in the accumulated content
        while (accumulatedContent.indexOf(":ENDFILE") != -1) {
            int filenameStart = accumulatedContent.indexOf("FILENAME:");
            if (filenameStart == -1) break;
            
            int filenameEnd = accumulatedContent.indexOf(":", filenameStart + 9);
            if (filenameEnd == -1) break;
            
            String filename = accumulatedContent.substring(filenameStart + 9, filenameEnd);
            
            int contentStart = filenameEnd + 1;
            int contentEnd = accumulatedContent.indexOf(":ENDFILE", contentStart);
            if (contentEnd == -1) break;
            
            String fileContent = accumulatedContent.substring(contentStart, contentEnd);
            
            if (saveUIFile(filename, fileContent)) {
                filesProcessed++;
            }
            
            // Remove processed content
            accumulatedContent = accumulatedContent.substring(contentEnd + 8);
        }
    }
    
    updateFile.close();
    
    Serial.printf("UI update completed successfully - %d files processed\n", filesProcessed);
    return filesProcessed > 0;
}
