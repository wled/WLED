#!/usr/bin/env python3
"""WLED Effects and Palettes Extractor

Extracts effect names and palette names from WLED source code.
Works with extract.py to provide complete template coverage.

Usage:
    python3 extract_effects_palettes.py effects --wled-path ~/WLED --output effects.json
    python3 extract_effects_palettes.py palettes --wled-path ~/WLED --output palettes.json
"""

import json
import re
import sys
import argparse
from pathlib import Path


def extract_effects(wled_path: Path) -> dict:
    """Extract effect names from FX.cpp."""
    fx_file = wled_path / 'wled00' / 'FX.cpp'
    
    if not fx_file.exists():
        print(f"Error: {fx_file} not found")
        return {}
    
    effects = {}
    
    with open(fx_file, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Pattern: static const char _data_FX_MODE_XXX[] PROGMEM = "Effect Name@metadata";
    pattern = r'static const char (_data_FX_MODE_\w+)\[\]\s*PROGMEM\s*=\s*"([^"]+)"'
    
    for match in re.finditer(pattern, content):
        var_name = match.group(1)
        full_str = match.group(2)
        
        # Split name and metadata
        if '@' in full_str:
            name, metadata = full_str.split('@', 1)
        else:
            name = full_str
            metadata = ''
        
        effects[var_name] = {
            'name': name,
            'metadata': metadata,
            'full': full_str
        }
    
    return effects


def extract_palettes(wled_path: Path) -> dict:
    """Extract palette names from FX_fcn.cpp."""
    fx_fcn_file = wled_path / 'wled00' / 'FX_fcn.cpp'
    
    if not fx_fcn_file.exists():
        print(f"Error: {fx_fcn_file} not found")
        return {}
    
    with open(fx_fcn_file, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Find the JSON_palette_names array
    pattern = r'const char JSON_palette_names\[\]\s*PROGMEM\s*=\s*R"=====\((.*?)\)=====";'
    match = re.search(pattern, content, re.DOTALL)
    
    if not match:
        print("Error: Could not find JSON_palette_names")
        return {}
    
    json_str = match.group(1)
    
    try:
        palettes_list = json.loads(json_str)
    except json.JSONDecodeError as e:
        print(f"Error parsing palette JSON: {e}")
        return {}
    
    palettes = {}
    for i, name in enumerate(palettes_list):
        palettes[f'palette_{i}'] = {
            'name': name,
            'index': i
        }
    
    return palettes


def main():
    parser = argparse.ArgumentParser(
        description='Extract WLED effects/palettes')
    parser.add_argument('type', choices=['effects', 'palettes'],
                        help='Type to extract')
    parser.add_argument('--wled-path', required=True,
                        help='Path to WLED repository')
    parser.add_argument('--output', required=True,
                        help='Output JSON file')
    args = parser.parse_args()
    
    wled_path = Path(args.wled_path)
    output_path = Path(args.output)
    
    if args.type == 'effects':
        data = extract_effects(wled_path)
    else:
        data = extract_palettes(wled_path)
    
    if not data:
        print(f"No {args.type} extracted")
        sys.exit(1)
    
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
    
    print(f"✓ Extracted {len(data)} {args.type} to {output_path}")


if __name__ == '__main__':
    main()
