#!/usr/bin/env python3
"""
Split Large UI Update File

This script splits a large UI update file into smaller, manageable chunks
that can be processed by the ESP32 without running out of memory.
"""

import os
import re

def split_ui_update_file(input_file, max_size=20000):
    """Split a large UI update file into smaller chunks"""
    print(f"🔧 Splitting {input_file} into smaller chunks...")
    
    if not os.path.exists(input_file):
        print(f"❌ File not found: {input_file}")
        return []
    
    # Read the file
    with open(input_file, 'r', encoding='utf-8') as f:
        content = f.read()
    
    print(f"📊 Original file size: {len(content):,} characters")
    
    # Split by FILENAME markers
    sections = re.split(r'(FILENAME:[^:]+:)', content)
    
    # Remove empty first section if it exists
    if sections and not sections[0].strip():
        sections = sections[1:]
    
    # Group sections into chunks
    chunks = []
    current_chunk = ""
    chunk_num = 1
    
    for i in range(0, len(sections), 2):
        if i + 1 < len(sections):
            filename_section = sections[i]
            file_content = sections[i + 1]
            
            # Check if adding this section would exceed max size
            test_chunk = current_chunk + filename_section + file_content
            
            if len(test_chunk) > max_size and current_chunk:
                # Save current chunk
                output_file = f"arklights_ui_update_part{chunk_num}.txt"
                with open(output_file, 'w', encoding='utf-8') as f:
                    f.write(current_chunk)
                
                print(f"✅ Created {output_file} ({len(current_chunk):,} characters)")
                chunks.append(output_file)
                
                # Start new chunk
                current_chunk = filename_section + file_content
                chunk_num += 1
            else:
                # Add to current chunk
                current_chunk = test_chunk
    
    # Save the last chunk
    if current_chunk:
        output_file = f"arklights_ui_update_part{chunk_num}.txt"
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(current_chunk)
        
        print(f"✅ Created {output_file} ({len(current_chunk):,} characters)")
        chunks.append(output_file)
    
    print(f"\n📋 Summary:")
    print(f"  Original file: {len(content):,} characters")
    print(f"  Split into: {len(chunks)} parts")
    print(f"  Max size per part: {max_size:,} characters")
    
    return chunks

def create_individual_files(input_file):
    """Create individual update files for each UI component"""
    print(f"\n🔧 Creating individual update files...")
    
    if not os.path.exists(input_file):
        print(f"❌ File not found: {input_file}")
        return []
    
    # Read the file
    with open(input_file, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Split by FILENAME markers
    sections = re.split(r'(FILENAME:[^:]+:)', content)
    
    # Remove empty first section if it exists
    if sections and not sections[0].strip():
        sections = sections[1:]
    
    individual_files = []
    
    for i in range(0, len(sections), 2):
        if i + 1 < len(sections):
            filename_section = sections[i]
            file_content = sections[i + 1]
            
            # Extract filename from FILENAME: section
            filename_match = re.match(r'FILENAME:([^:]+):', filename_section)
            if filename_match:
                ui_filename = filename_match.group(1)
                # Convert /ui/filename to just filename
                base_filename = os.path.basename(ui_filename)
                
                # Create individual update file
                individual_file = f"update_{base_filename}.txt"
                update_content = filename_section + file_content
                
                with open(individual_file, 'w', encoding='utf-8') as f:
                    f.write(update_content)
                
                print(f"✅ Created {individual_file} ({len(update_content):,} characters)")
                individual_files.append(individual_file)
    
    return individual_files

def main():
    print("🔧 ArkLights UI Update File Splitter")
    print("=" * 50)
    
    input_file = "arklights_ui_update.txt"
    
    if not os.path.exists(input_file):
        print(f"❌ Input file not found: {input_file}")
        return
    
    # Split into smaller chunks
    chunks = split_ui_update_file(input_file, max_size=20000)
    
    # Create individual files
    individual_files = create_individual_files(input_file)
    
    print(f"\n🚀 Usage Instructions:")
    print(f"1. **Upload chunks in order:**")
    for i, chunk in enumerate(chunks, 1):
        print(f"   {i}. Upload {chunk}")
    
    print(f"\n2. **Or upload individual files:**")
    for individual_file in individual_files:
        print(f"   • Upload {individual_file}")
    
    print(f"\n3. **After uploading:**")
    print(f"   • Run 'ls' command to verify files")
    print(f"   • Visit root URL to test interface")
    
    print(f"\n💡 **Pro Tip:**")
    print(f"Start with individual files - they're smaller and easier to debug!")

if __name__ == "__main__":
    main()
