#!/usr/bin/env python3
"""
Apply UI Update Patch

This script applies the improved UI update function to main.cpp
to handle large files by processing them in chunks.
"""

import os
import shutil
from datetime import datetime

def backup_file(filename):
    """Create a backup of the original file"""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    backup_name = f"{filename}.backup_{timestamp}"
    shutil.copy2(filename, backup_name)
    print(f"✅ Created backup: {backup_name}")
    return backup_name

def apply_patch():
    """Apply the UI update patch to main.cpp"""
    main_cpp = "src/main.cpp"
    patch_file = "ui_update_patch.cpp"
    
    if not os.path.exists(main_cpp):
        print(f"❌ {main_cpp} not found")
        return False
    
    if not os.path.exists(patch_file):
        print(f"❌ {patch_file} not found")
        return False
    
    # Create backup
    backup_file(main_cpp)
    
    # Read the patch content
    with open(patch_file, 'r') as f:
        patch_content = f.read()
    
    # Read the main.cpp content
    with open(main_cpp, 'r') as f:
        main_content = f.read()
    
    # Check if patch is already applied
    if "processUIUpdateStreaming" in main_content:
        print("⚠️ Patch appears to already be applied")
        return True
    
    # Find the processUIUpdate function and replace it
    # Look for the function definition
    start_marker = "bool processUIUpdate(const String& updatePath) {"
    end_marker = "    return false;\n}"
  #  end_marker = "    return false;\n}"
    
    start_pos = main_content.find(start_marker)
    if start_pos == -1:
        print("❌ Could not find processUIUpdate function to replace")
        return False
    
    # Find the end of the function (look for the closing brace)
    # This is a bit tricky, so we'll use a simpler approach
    print("🔧 Applying patch...")
    
    # Add the new function declarations near the other declarations
    declaration_pos = main_content.find("bool processUIUpdate(const String& updatePath);")
    if declaration_pos != -1:
        # Add new declarations after the existing one
        new_declarations = "\nbool processUIUpdateStreaming(const String& updatePath);\nbool saveUIFile(const String& filename, const String& content);"
        main_content = main_content[:declaration_pos + len("bool processUIUpdate(const String& updatePath);")] + new_declarations + main_content[declaration_pos + len("bool processUIUpdate(const String& updatePath);"):]
    
    # Replace the function implementation
    # Find the start of the function
    func_start = main_content.find("bool processUIUpdate(const String& updatePath) {")
    if func_start == -1:
        print("❌ Could not find function to replace")
        return False
    
    # Find the end of the function by looking for the next function
    # Look for the next function declaration
    next_func_pos = main_content.find("\nbool ", func_start + 1)
    if next_func_pos == -1:
        next_func_pos = main_content.find("\nvoid ", func_start + 1)
    if next_func_pos == -1:
        next_func_pos = len(main_content)
    
    # Extract the new function implementation from the patch
    patch_lines = patch_content.split('\n')
    new_function = []
    in_function = False
    
    for line in patch_lines:
        if "bool processUIUpdate(const String& updatePath) {" in line:
            in_function = True
        if in_function:
            new_function.append(line)
            if line.strip() == "}" and "processUIUpdate" in line:
                break
    
    new_function_text = '\n'.join(new_function)
    
    # Replace the function
    main_content = main_content[:func_start] + new_function_text + main_content[next_func_pos:]
    
    # Add the helper functions at the end
    helper_functions = []
    in_helpers = False
    
    for line in patch_lines:
        if "bool processUIUpdateStreaming" in line and "{" in line:
            in_helpers = True
        if in_helpers:
            helper_functions.append(line)
    
    helper_text = '\n'.join(helper_functions)
    
    # Add helpers before the last closing brace
    last_brace = main_content.rfind('}')
    if last_brace != -1:
        main_content = main_content[:last_brace] + '\n' + helper_text + '\n' + main_content[last_brace:]
    
    # Write the modified content
    with open(main_cpp, 'w') as f:
        f.write(main_content)
    
    print("✅ Patch applied successfully!")
    print("🔧 The UI update function now supports large files")
    print("📝 Recompile and upload the firmware to use the improved version")
    
    return True

def main():
    print("🔧 ArkLights UI Update Patch Applicator")
    print("=" * 50)
    
    if apply_patch():
        print("\n🚀 Next Steps:")
        print("1. Recompile the firmware with the patched code")
        print("2. Upload the new firmware to your device")
        print("3. Try uploading your large arklights_ui_update.txt file")
        print("4. The system will automatically use streaming mode for large files")
        
        print("\n💡 Benefits:")
        print("• Can handle files up to several MB in size")
        print("• Processes files in chunks to avoid memory issues")
        print("• Automatically detects large files and switches modes")
        print("• Maintains backward compatibility with small files")
    else:
        print("❌ Patch application failed")

if __name__ == "__main__":
    main()
