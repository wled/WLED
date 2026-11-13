#!/usr/bin/env python3
"""WLED Template Format Converter

Converts between different template formats:
- New format (extract.py): Nested by file, CSS selector keys
- Old format (v2 toolchain): Flat, file:hash keys

Usage:
    python3 convert_template.py --input new_template.json --output old_template.json --to-old
    python3 convert_template.py --input old_template/ --output new_template.json --to-new
"""

import sys
import argparse
from pathlib import Path
from typing import Any, Callable, Dict
from common import load_json, save_json, compute_hash


def extract_path_from_css_key(css_key: str) -> str:
    """Extract meaningful path from CSS selector key.
    
    Handles formats like:
    - "html:html > body > h1:text" -> "html > body > h1"
    - "html:settings_2D.htm:06ce2a25" -> "settings_2D.htm"
    - "js:edit.htm:77:ef93d8c8" -> "edit.htm:77"
    - "html:button#power:title" -> "button#power"
    - "html:div#fxlist > div > label > div > span:text" -> "div#fxlist > div > label > div > span"
    """
    # Handle old format keys (js:file:line:hash or html:file:hash)
    if css_key.startswith('js:') or css_key.startswith('html:'):
        parts = css_key.split(':')
        if len(parts) >= 3:
            # js:edit.htm:77:ef93d8c8 -> edit.htm:77
            # html:settings_2D.htm:06ce2a25 -> settings_2D.htm
            return ':'.join(parts[1:-1]) if len(parts) > 3 else parts[1]
        return css_key
    
    # Handle CSS selector format
    # Remove attribute suffixes (:text, :title, :placeholder, :aria-label, :nth-of-type)
    path = css_key
    for suffix in [':text', ':title', ':placeholder', ':aria-label']:
        if path.endswith(suffix):
            path = path[:-len(suffix)]
            break
    
    # Handle :nth-of-type(N) - remove the whole suffix
    if ':nth-of-type(' in path:
        path = path[:path.rfind(':nth-of-type(')]
    
    return path


def new_to_old(new_template: Dict) -> Dict:
    """Convert new format (nested by file) to old format (flat with file:hash keys)."""
    old_template = {}
    
    for filename, file_entries in new_template.items():
        if not isinstance(file_entries, dict):
            continue
        
        for css_key, entry in file_entries.items():
            if not isinstance(entry, dict):
                continue
            
            en_text = entry.get('en', '')
            if not en_text:
                continue
            
            # Check if this is already in old format (e.g., js:edit.htm:77:ef93d8c8)
            if css_key.startswith('js:') or css_key.startswith('html:'):
                # Already in old format, use as-is
                old_key = css_key
            else:
                # Create old format key: file:hash
                # Include css_key in hash to avoid collision when same en_text appears in multiple selectors
                content_hash = compute_hash(f"{css_key}:{en_text}")
                old_key = f"{filename}:{content_hash}"
            
            # Extract path from CSS selector
            path = extract_path_from_css_key(css_key)
            
            # Determine type
            entry_type = entry.get('type', 'html_text')
            if css_key.startswith('js:'):
                entry_type = 'js'
            
            old_template[old_key] = {
                'en': en_text,
                'file': filename,
                'path': path,
                'type': entry_type
            }
            
            # Copy translation if exists
            if entry.get('translation'):
                old_template[old_key]['translation'] = entry['translation']
    
    return old_template


def old_to_new(old_template: Dict) -> Dict:
    """Convert old format (flat) to new format (nested by file)."""
    new_template = {}
    
    for key, entry in old_template.items():
        if not isinstance(entry, dict):
            continue
        
        filename = entry.get('file', 'unknown')
        en_text = entry.get('en', '')
        path = entry.get('path', '')
        entry_type = entry.get('type', 'html_text')
        
        if not en_text:
            continue
        
        # Create CSS selector from path
        if entry_type == 'js':
            # JS entries keep their original key format
            css_key = key
        elif path:
            # Reconstruct CSS selector
            css_key = f"html:{path}:text"
        else:
            css_key = f"html:unknown:{key}"
        
        if filename not in new_template:
            new_template[filename] = {}
        
        new_template[filename][css_key] = {
            'en': en_text,
            'translation': entry.get('translation', ''),
            'context': f"{filename}: ({entry_type})"
        }
    
    return new_template


def _merge_extra(old_template: dict, merge_path: Path,
                  build_entry: Callable[[dict], dict], label: str) -> None:
    """Merge extra entries (effects/palettes/index_js) into old_template."""
    if not merge_path.exists():
        return
    data = load_json(merge_path)
    for key, entry in data.items():
        if isinstance(entry, dict):
            old_template[key] = build_entry(entry)
    print(f"✓ Merged {len(data)} {label}")


def load_old_template_dir(template_dir: Path) -> Dict:
    """Load old format template from directory (static.json, js.json, etc.)."""
    merged = {}
    
    for json_file in sorted(template_dir.glob('*.json')):
        if json_file.name == 'metadata.json':
            continue
        
        data = load_json(json_file)
        if isinstance(data, dict):
            merged.update(data)
    
    return merged


def main():
    parser = argparse.ArgumentParser(
        description='Convert WLED template formats')
    parser.add_argument('--input', required=True,
                        help='Input file or directory')
    parser.add_argument('--output', required=True,
                        help='Output file')
    direction = parser.add_mutually_exclusive_group(required=True)
    direction.add_argument('--to-old', action='store_true',
                           help='Convert new format to old format')
    direction.add_argument('--to-new', action='store_true',
                           help='Convert old format to new format')
    parser.add_argument('--merge-effects',
                        help='Merge effects.json into output')
    parser.add_argument('--merge-palettes',
                        help='Merge palettes.json into output')
    parser.add_argument('--merge-index-js',
                        help='Merge index_js.json into output')
    args = parser.parse_args()
    
    input_path = Path(args.input)
    output_path = Path(args.output)
    
    if not input_path.exists():
        print(f"Error: Input path does not exist: {input_path}")
        return 1
    
    if args.to_old:
        # Load new format (single file or directory)
        if input_path.is_dir():
            # Load all JSON files in directory
            new_template = {}
            for json_file in input_path.glob('*.json'):
                if json_file.name in ['effects.json', 'palettes.json', 'index_js.json', 'metadata.json']:
                    continue
                data = load_json(json_file)
                if isinstance(data, dict):
                    new_template.update(data)
        else:
            new_template = load_json(input_path)
        
        old_template = new_to_old(new_template)
        
        # Merge extra data if provided
        if args.merge_effects:
            _merge_extra(old_template, Path(args.merge_effects),
                         lambda e: {'en': e.get('name', ''), 'type': 'effect',
                                    'file': 'effects.json',
                                    'metadata': e.get('metadata', ''),
                                    'full': e.get('full', '')},
                         'effects')
        if args.merge_palettes:
            _merge_extra(old_template, Path(args.merge_palettes),
                         lambda e: {'en': e.get('name', ''), 'type': 'palette',
                                    'file': 'palettes.json',
                                    'index': e.get('index', 0)},
                         'palettes')
        if args.merge_index_js:
            _merge_extra(old_template, Path(args.merge_index_js),
                         lambda e: {'en': e.get('en', ''), 'type': 'js',
                                    'file': 'index.js',
                                    'context': e.get('context', '')},
                         'index.js strings')
        
        save_json(output_path, old_template)
        print(f"✓ Converted to old format: {len(old_template)} entries")
    
    elif args.to_new:
        # Load old format
        if input_path.is_dir():
            old_template = load_old_template_dir(input_path)
        else:
            old_template = load_json(input_path)
        
        new_template = old_to_new(old_template)
        save_json(output_path, new_template)
        print(f"✓ Converted to new format: {sum(len(v) for v in new_template.values())} entries in {len(new_template)} files")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
