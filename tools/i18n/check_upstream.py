#!/usr/bin/env python3
"""WLED Upstream Update Checker

Checks if WLED upstream has new commits since last extraction.

Usage:
    python3 check_upstream.py --wled-path ~/WLED --translations-path ~/WLED-translations
"""

import json
import subprocess
import sys
import argparse
from pathlib import Path


def get_upstream_commit(wled_path):
    """Get latest upstream commit hash and message."""
    # Try common remote/branch combinations
    candidates = ['upstream/main', 'upstream/master', 'origin/main', 'origin/master']
    
    # Also check which remotes actually exist
    remotes_result = subprocess.run(
        ['git', 'remote'], cwd=wled_path,
        capture_output=True, text=True, timeout=10)
    if remotes_result.returncode == 0:
        remotes = remotes_result.stdout.strip().split('\n')
        if 'upstream' not in remotes and 'origin' not in remotes:
            print(f"Warning: No 'upstream' or 'origin' remote found. Available: {remotes}")
            return None, None
    
    for ref in candidates:
        result = subprocess.run(
            ['git', 'rev-parse', ref],
            cwd=wled_path, capture_output=True, text=True, timeout=10
        )
        if result.returncode == 0 and result.stdout.strip():
            full_hash = result.stdout.strip()
            # Get commit message separately
            msg_result = subprocess.run(
                ['git', 'log', '--format=%s', '-1', ref],
                cwd=wled_path, capture_output=True, text=True, timeout=10
            )
            msg = msg_result.stdout.strip() if msg_result.returncode == 0 else ''
            return full_hash, msg
    
    print(f"Error: Could not find any upstream branch. Tried: {candidates}")
    return None, None


def get_last_extracted_commit(translations_path):
    """Get the commit hash from last extraction metadata."""
    metadata_file = translations_path / 'en_template' / 'metadata.json'
    if not metadata_file.exists():
        return None
    
    try:
        with open(metadata_file, encoding='utf-8') as f:
            metadata = json.load(f)
        return metadata.get('wled_commit')
    except (json.JSONDecodeError, IOError):
        return None


def main():
    parser = argparse.ArgumentParser(
        description='Check WLED upstream for updates')
    parser.add_argument('--wled-path', required=True,
                        help='Path to WLED repository')
    parser.add_argument('--translations-path', required=True,
                        help='Path to WLED-translations repository')
    parser.add_argument('--json', action='store_true',
                        help='Output as JSON')
    args = parser.parse_args()
    
    wled_path = Path(args.wled_path)
    translations_path = Path(args.translations_path)
    
    # Get current upstream commit
    upstream_hash, upstream_msg = get_upstream_commit(wled_path)
    if not upstream_hash:
        print("Error: Could not get upstream commit")
        sys.exit(1)
    
    # Get last extracted commit
    last_hash = get_last_extracted_commit(translations_path)
    
    # Compare (handle both short and full hashes)
    if last_hash is None:
        has_updates = True
    else:
        has_updates = (
            not upstream_hash.startswith(last_hash)
            and not last_hash.startswith(upstream_hash)
        )
    
    result = {
        'has_updates': has_updates,
        'upstream_commit': upstream_hash,
        'upstream_message': upstream_msg,
        'last_extracted_commit': last_hash,
        'translations_path': str(translations_path)
    }
    
    if args.json:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    else:
        if has_updates:
            print(f"✓ New upstream commit: {upstream_hash}")
            print(f"  Message: {upstream_msg}")
            if last_hash:
                print(f"  Last extracted: {last_hash}")
            else:
                print(f"  No previous extraction found")
        else:
            print(f"No updates. Current: {upstream_hash}")


if __name__ == '__main__':
    main()
