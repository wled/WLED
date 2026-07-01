#!/usr/bin/env python3
"""Regenerate en_template with new format keys

Regenerates all template files in en_template/ using the new extract.py format.
This ensures consistency between en_template/ and en_template_new/.

Usage:
    python3 regenerate_en_template.py --wled-path ~/WLED --translations-path ~/WLED-translations
"""

import sys
import json
import subprocess
import argparse
from pathlib import Path
from datetime import datetime

from common import load_json, save_json, run_command


def main():
    parser = argparse.ArgumentParser(
        description='Regenerate en_template with new format keys')
    parser.add_argument('--wled-path', required=True,
                        help='Path to WLED repository')
    parser.add_argument('--translations-path', required=True,
                        help='Path to WLED-translations repository')
    args = parser.parse_args()
    
    wled_path = Path(args.wled_path)
    translations_path = Path(args.translations_path)
    scripts_dir = translations_path / 'scripts'
    en_template_dir = translations_path / 'en_template'
    
    # Step 1: Extract HTML/JS strings
    print("Step 1: Extracting HTML/JS strings...")
    extract_script = wled_path / 'tools' / 'i18n' / 'extract.py'
    strings_file = en_template_dir / 'strings.json'
    
    if not extract_script.exists():
        print(f"Error: extract.py not found at {extract_script}")
        return 1
    
    cmd = [sys.executable, str(extract_script), '--output', str(strings_file)]
    success, output = run_command(cmd, cwd=wled_path)
    if not success:
        print(f"Error: HTML/JS extraction failed: {output}")
        return 1
    
    print("✓ Extracted HTML/JS strings")
    
    # Step 2: Split into static.json and js.json
    print("Step 2: Splitting into static.json and js.json...")
    try:
        strings = load_json(strings_file)
    except (json.JSONDecodeError, IOError) as e:
        print(f"Error: Failed to load {strings_file}: {e}")
        return 1
    
    static = {}
    js = {}
    
    for filename, entries in strings.items():
        if not isinstance(entries, dict):
            continue
        for key, value in entries.items():
            if not isinstance(value, dict):
                continue
            entry_with_file = {
                'en': value.get('en', ''),
                'file': filename,
                'type': value.get('type', 'html_text'),
                'context': value.get('context', f"{filename}: (html_text)")
            }
            if key.startswith('js:'):
                js[key] = entry_with_file
            else:
                static[key] = entry_with_file
    
    save_json(en_template_dir / 'static.json', static)
    save_json(en_template_dir / 'js.json', js)
    print(f"✓ Split: {len(static)} static, {len(js)} js")
    
    # Step 3: Extract effects
    print("Step 3: Extracting effects...")
    effects_script = scripts_dir / 'extract_effects_palettes.py'
    effects_file = en_template_dir / 'effects.json'
    
    if effects_script.exists():
        cmd = [sys.executable, str(effects_script), 'effects',
               '--wled-path', str(wled_path), '--output', str(effects_file)]
        success, output = run_command(cmd, cwd=wled_path)
        if success:
            print("✓ Extracted effects")
        else:
            print(f"Error: Effects extraction failed: {output}")
            return 1
    else:
        print(f"Warning: {effects_script} not found, skipping effects extraction")
    
    # Step 4: Extract palettes
    print("Step 4: Extracting palettes...")
    palettes_file = en_template_dir / 'palettes.json'
    
    if effects_script.exists():
        cmd = [sys.executable, str(effects_script), 'palettes',
               '--wled-path', str(wled_path), '--output', str(palettes_file)]
        success, output = run_command(cmd, cwd=wled_path)
        if success:
            print("✓ Extracted palettes")
        else:
            print(f"Error: Palettes extraction failed: {output}")
            return 1
    else:
        print(f"Warning: {effects_script} not found, skipping palettes extraction")
    
    # Step 5: Extract index.js strings
    print("Step 5: Extracting index.js strings...")
    index_js_script = scripts_dir / 'extract_index_js.py'
    index_js_file = en_template_dir / 'index_js.json'
    
    if index_js_script.exists():
        cmd = [sys.executable, str(index_js_script),
               '--wled-path', str(wled_path), '--output', str(index_js_file)]
        success, output = run_command(cmd, cwd=wled_path)
        if success:
            print("✓ Extracted index.js strings")
        else:
            print(f"Error: index.js extraction failed: {output}")
            return 1
    else:
        print(f"Warning: {index_js_script} not found, skipping index.js extraction")
    
    # Step 6: Merge index.js into js.json
    print("Step 6: Merging index.js into js.json...")
    if index_js_file.exists():
        try:
            index_js = load_json(index_js_file)
            js.update(index_js)
            save_json(en_template_dir / 'js.json', js)
            print(f"✓ Merged: {len(js)} total js strings")
        except (json.JSONDecodeError, IOError) as e:
            print(f"Warning: Failed to merge index.js: {e}")
    
    # Step 7: Create metadata
    print("Step 7: Creating metadata...")
    commit_hash = ''
    commit_msg = ''
    for ref in ['upstream/main', 'upstream/master', 'origin/main', 'origin/master']:
        result = subprocess.run(
            ['git', 'log', '--oneline', '-1', ref],
            cwd=wled_path,
            capture_output=True,
            text=True,
            timeout=10
        )
        if result.returncode == 0 and result.stdout.strip():
            parts = result.stdout.strip().split(' ', 1)
            commit_hash = parts[0]
            commit_msg = parts[1] if len(parts) > 1 else ''
            break
    
    metadata = {
        'wled_commit': commit_hash,
        'wled_commit_message': commit_msg,
        'extracted_at': datetime.now().isoformat(),
        'extractor_version': '3.0'
    }
    
    save_json(en_template_dir / 'metadata.json', metadata)
    print("✓ Metadata saved")
    
    # Summary
    print("\n=== Summary ===")
    print(f"static.json: {len(static)} entries")
    print(f"js.json: {len(js)} entries")
    if effects_file.exists():
        try:
            effects_data = load_json(effects_file)
            print(f"effects.json: {len(effects_data)} entries")
        except (json.JSONDecodeError, IOError):
            print("effects.json: error reading file")
    if palettes_file.exists():
        try:
            palettes_data = load_json(palettes_file)
            print(f"palettes.json: {len(palettes_data)} entries")
        except (json.JSONDecodeError, IOError):
            print("palettes.json: error reading file")
    
    # Clean up intermediate file
    if strings_file.exists():
        strings_file.unlink()
        print(f"\n✓ Cleaned up {strings_file.name}")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
