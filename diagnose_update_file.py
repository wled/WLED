#!/usr/bin/env python3
"""
Diagnose ArkLights UI Update File

This script analyzes the update file format and identifies potential issues.
"""

import os
import re

def analyze_update_file(filename):
    """Analyze an update file for format issues"""
    print(f"🔍 Analyzing: {filename}")
    print("=" * 50)
    
    if not os.path.exists(filename):
        print(f"❌ File not found: {filename}")
        return False
    
    # Get file info
    size = os.path.getsize(filename)
    print(f"📊 File size: {size:,} bytes")
    
    # Read file content
    with open(filename, 'r', encoding='utf-8') as f:
        content = f.read()
    
    print(f"📝 Content length: {len(content):,} characters")
    
    # Check for FILENAME markers
    filename_matches = re.findall(r'FILENAME:([^:]+):', content)
    print(f"📁 FILENAME markers found: {len(filename_matches)}")
    for i, filename_match in enumerate(filename_matches, 1):
        print(f"  {i}. {filename_match}")
    
    # Check for ENDFILE markers
    endfile_matches = re.findall(r':ENDFILE', content)
    print(f"🏁 ENDFILE markers found: {len(endfile_matches)}")
    
    # Check if markers match
    if len(filename_matches) == len(endfile_matches):
        print("✅ FILENAME and ENDFILE markers match")
    else:
        print(f"❌ Mismatch: {len(filename_matches)} FILENAME vs {len(endfile_matches)} ENDFILE")
    
    # Parse each file section
    print(f"\n📋 File Sections:")
    sections = re.split(r'FILENAME:([^:]+):', content)
    
    # Remove empty first section if it exists
    if sections and not sections[0].strip():
        sections = sections[1:]
    
    for i in range(0, len(sections), 2):
        if i + 1 < len(sections):
            filename = sections[i]
            file_content = sections[i + 1]
            
            # Remove ENDFILE marker
            if file_content.endswith(':ENDFILE'):
                file_content = file_content[:-8]
            
            print(f"  📄 {filename}")
            print(f"     Content length: {len(file_content):,} characters")
            start_preview = file_content[:50].replace('\n', '\\n')
            end_preview = file_content[-50:].replace('\n', '\\n')
            print(f"     Starts with: {start_preview}...")
            print(f"     Ends with: ...{end_preview}")
    
    # Check for potential issues
    print(f"\n🔧 Potential Issues:")
    
    # Check for encoding issues
    try:
        content.encode('utf-8')
        print("✅ UTF-8 encoding OK")
    except UnicodeEncodeError as e:
        print(f"❌ UTF-8 encoding issue: {e}")
    
    # Check for very large files
    if size > 100000:  # 100KB
        print(f"⚠️ Large file size ({size:,} bytes) - might cause memory issues")
    
    # Check for missing newlines
    if not content.endswith('\n'):
        print("⚠️ File doesn't end with newline")
    
    # Check for special characters
    special_chars = re.findall(r'[^\x20-\x7E\n\r\t]', content)
    if special_chars:
        print(f"⚠️ Found {len(special_chars)} special characters")
        unique_chars = set(special_chars)
        print(f"   Unique special chars: {[ord(c) for c in unique_chars]}")
    
    return True

def compare_files(file1, file2):
    """Compare two update files"""
    print(f"\n🔄 Comparing {file1} vs {file2}")
    print("=" * 50)
    
    if not os.path.exists(file1) or not os.path.exists(file2):
        print("❌ One or both files not found")
        return
    
    size1 = os.path.getsize(file1)
    size2 = os.path.getsize(file2)
    
    print(f"📊 Size comparison:")
    print(f"  {file1}: {size1:,} bytes")
    print(f"  {file2}: {size2:,} bytes")
    print(f"  Difference: {abs(size1 - size2):,} bytes")
    
    # Read both files
    with open(file1, 'r', encoding='utf-8') as f:
        content1 = f.read()
    
    with open(file2, 'r', encoding='utf-8') as f:
        content2 = f.read()
    
    # Count markers
    filename1 = len(re.findall(r'FILENAME:', content1))
    filename2 = len(re.findall(r'FILENAME:', content2))
    endfile1 = len(re.findall(r':ENDFILE', content1))
    endfile2 = len(re.findall(r':ENDFILE', content2))
    
    print(f"📁 FILENAME markers: {file1}={filename1}, {file2}={filename2}")
    print(f"🏁 ENDFILE markers: {file1}={endfile1}, {file2}={endfile2}")

def main():
    print("🔍 ArkLights UI Update File Diagnostic")
    print("=" * 50)
    
    # Analyze the main update file
    analyze_update_file("arklights_ui_update.txt")
    
    # Analyze the debug file
    analyze_update_file("debug_ui_update.txt")
    
    # Compare them
    compare_files("arklights_ui_update.txt", "debug_ui_update.txt")
    
    print(f"\n💡 Recommendations:")
    print("1. Check if the file is too large for the ESP32 to process")
    print("2. Verify the file format matches exactly")
    print("3. Try uploading a smaller version first")
    print("4. Check for special characters or encoding issues")

if __name__ == "__main__":
    main()
