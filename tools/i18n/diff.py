#!/usr/bin/env python3
"""
WLED i18n Translation Diff Tool
Compares two template versions and reports changes.

Usage:
    python3 diff.py --old v0.15.0.json --new v0.16.0.json
    python3 diff.py --old en_template_old/ --new en_template/ --locale zh_CN

Output:
    JSON with added/removed/modified strings and stats.
"""

import json
import sys
from pathlib import Path


def load_template(path):
    """Load a single JSON template file."""
    with open(path, encoding='utf-8') as f:
        return json.load(f)


def load_templates_from_dir(dir_path):
    """Load all JSON files from a directory and merge into single dict."""
    result = {}
    dir_path = Path(dir_path)
    for json_file in sorted(dir_path.glob('*.json')):
        if json_file.name == 'metadata.json':
            continue
        data = load_template(json_file)
        result.update(data)
    return result


def diff_templates(old, new):
    """Compare two template dicts and return differences."""
    old_keys = set(old.keys())
    new_keys = set(new.keys())

    added = new_keys - old_keys
    removed = old_keys - new_keys
    common = old_keys & new_keys

    modified = []
    for k in sorted(common):
        old_en = old[k].get('en', '')
        new_en = new[k].get('en', '')
        if old_en != new_en:
            modified.append({
                'key': k,
                'old': old_en,
                'new': new_en,
            })

    return {
        'added': sorted(added),
        'removed': sorted(removed),
        'modified': modified,
        'stats': {
            'old_count': len(old_keys),
            'new_count': len(new_keys),
            'added': len(added),
            'removed': len(removed),
            'modified': len(modified),
        }
    }


def main():
    import argparse
    parser = argparse.ArgumentParser(
        description='Diff WLED i18n templates between versions'
    )
    parser.add_argument(
        '--old', required=True,
        help='Old template JSON file or directory'
    )
    parser.add_argument(
        '--new', required=True,
        help='New template JSON file or directory'
    )
    parser.add_argument(
        '--locale',
        help='Locale to compare (e.g., zh_CN). Only used with directory mode.'
    )
    args = parser.parse_args()

    old_path = Path(args.old)
    new_path = Path(args.new)

    # Load old templates
    if old_path.is_dir():
        if args.locale:
            locale_dir = old_path / args.locale
            if locale_dir.exists():
                old = load_templates_from_dir(locale_dir)
            else:
                old = load_templates_from_dir(old_path)
        else:
            old = load_templates_from_dir(old_path)
    else:
        old = load_template(old_path)

    # Load new templates
    if new_path.is_dir():
        if args.locale:
            locale_dir = new_path / args.locale
            if locale_dir.exists():
                new = load_templates_from_dir(locale_dir)
            else:
                new = load_templates_from_dir(new_path)
        else:
            new = load_templates_from_dir(new_path)
    else:
        new = load_template(new_path)

    # Compute diff
    result = diff_templates(old, new)

    # Output
    print(json.dumps(result, indent=2, ensure_ascii=False))

    # Exit code: 0 if no changes, 1 if changes found
    stats = result['stats']
    if stats['added'] or stats['removed'] or stats['modified']:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
