#!/usr/bin/env python3
"""
WLED i18n String Extractor
Extracts translatable strings from WLED Web UI HTML files.

Usage: python3 extract.py [--locale zh_CN]

Outputs: locales/<locale>.json (or locales/_template.json if no locale specified)

Handles three layers:
1. Static HTML text (BeautifulSoup DOM parsing)
2. JS strings in <script> blocks (regex-based extraction)
3. Element attributes (placeholder, title, alt, aria-label)
"""

import json
import os
import re
import sys
import hashlib
from pathlib import Path
from bs4 import BeautifulSoup, Comment
from bs4.element import NavigableString

# WLED data directory (relative to repo root)
DATA_DIR = Path(__file__).resolve().parent.parent.parent / "wled00" / "data"
LOCALES_DIR = Path(__file__).resolve().parent / "locales"

# Tags whose text content is translatable
TRANSLATABLE_TAGS = {
    'h1', 'h2', 'h3', 'h4', 'h5', 'h6',
    'p', 'span', 'div', 'label', 'a', 'button',
    'option', 'td', 'th', 'li', 'legend', 'dt', 'dd',
    'figcaption', 'summary', 'small', 'strong', 'em',
    'b', 'i', 'u', 'mark', 'code', 'pre',
}

# Attributes that contain translatable text
TRANSLATABLE_ATTRS = ['placeholder', 'title', 'alt', 'aria-label', 'value']

# JS patterns to extract (regex → group containing the string)
JS_PATTERNS = [
    # alert("...") / alert(`...`)
    (r'alert\s*\(\s*["\x27`]([^"\x27`]*(?:\{[^}]*\}[^"\x27`]*)*)["\x27`]\s*\)', 'alert'),
    # innerHTML = "..." / innerHTML += "..."
    (r'innerHTML\s*\+?=\s*["\x27`]([^"\x27`]+)["\x27`]', 'innerHTML'),
    # innerText = "..."
    (r'innerText\s*=\s*["\x27`]([^"\x27`]+)["\x27`]', 'innerText'),
    # .textContent = "..."
    (r'textContent\s*=\s*["\x27`]([^"\x27`]+)["\x27`]', 'textContent'),
    # title = "..." (in JS context, not HTML attribute)
    (r'(?<!\.)title\s*=\s*["\x27`]([^"\x27`]+)["\x27`]', 'title'),
    # confirm("...")
    (r'confirm\s*\(\s*["\x27`]([^"\x27`]+)["\x27`]\s*\)', 'confirm'),
    # prompt("...", "...")
    (r'prompt\s*\(\s*["\x27`]([^"\x27`]+)["\x27`]', 'prompt'),
]

# Skip patterns - these are not user-visible text
SKIP_PATTERNS = [
    r'^\s*$',           # empty
    r'^\d+$',           # pure numbers
    r'^#[0-9a-fA-F]+$', # hex colors
    r'^https?://',      # URLs
    r'^[{}\[\]();,\s]+$', # pure punctuation
    r'^function\s',     # function declarations
    r'^var\s|^let\s|^const\s', # variable declarations
    r'document\.|window\.|console\.', # DOM API calls
    r'^[a-z]+[A-Z]\w*$',  # camelCase variable names (e.g. togglePower, loadPresets)
]


def is_translatable_text(text):
    """Check if text is worth translating (not just whitespace/code)."""
    text = text.strip()
    if not text or len(text) < 2:
        return False
    for pat in SKIP_PATTERNS:
        if re.match(pat, text):
            return False
    # Skip if it's mostly HTML entities or special chars
    clean = re.sub(r'&[a-zA-Z]+;|&#\d+;|[^\w\s]', '', text)
    if len(clean) < 2:
        return False
    # Skip if it's a CSS/JS keyword or value
    if text.lower() in ('true', 'false', 'null', 'undefined', 'none', 'auto',
                         'inherit', 'initial', 'unset', 'block', 'inline',
                         'flex', 'grid', 'hidden', 'visible', 'scroll'):
        return False
    return True


def get_element_path(element):
    """Generate a unique CSS selector path for an element."""
    parts = []
    current = element
    while current and current.name and current.name != '[document]':
        tag = current.name
        # Add id if available
        eid = current.get('id')
        if eid:
            parts.insert(0, f"{tag}#{eid}")
            break  # id is unique, no need to go further
        # Count siblings of same type
        if current.parent:
            siblings = [c for c in current.parent.children
                       if hasattr(c, 'name') and c.name == tag]
            if len(siblings) > 1:
                idx = siblings.index(current) + 1
                parts.insert(0, f"{tag}:nth-of-type({idx})")
            else:
                parts.insert(0, tag)
        else:
            parts.insert(0, tag)
        current = current.parent
    return ' > '.join(parts)


def extract_html_strings(soup, filepath):
    """Extract translatable strings from HTML DOM."""
    results = {}

    # Extract text content from translatable tags
    for tag in soup.find_all(True):
        # Skip script and style tags (handled separately)
        if tag.name in ('script', 'style', 'meta', 'link', 'noscript'):
            continue

        # Direct text content (not including children)
        direct_text = ''
        for child in tag.children:
            if isinstance(child, NavigableString) and not isinstance(child, Comment):
                direct_text += str(child)

        direct_text = direct_text.strip()
        if direct_text and is_translatable_text(direct_text):
            path = get_element_path(tag)
            key = f"html:{path}:text"
            results[key] = {
                'original': direct_text,
                'type': 'html_text',
                'path': path,
                'file': filepath.name,
            }

        # Translatable attributes
        for attr in TRANSLATABLE_ATTRS:
            val = tag.get(attr)
            if val and is_translatable_text(val):
                path = get_element_path(tag)
                key = f"html:{path}:{attr}"
                results[key] = {
                    'original': val.strip(),
                    'type': f'html_{attr}',
                    'path': path,
                    'attr': attr,
                    'file': filepath.name,
                }

    return results


def extract_js_strings(soup, filepath):
    """Extract translatable strings from <script> blocks using regex."""
    results = {}

    for script in soup.find_all('script'):
        if script.string:
            lines = script.string.split('\n')
            for line_num, line in enumerate(lines, 1):
                for pattern, ptype in JS_PATTERNS:
                    for match in re.finditer(pattern, line):
                        text = match.group(1).strip()
                        if is_translatable_text(text):
                            # Create a stable key from content hash
                            content_hash = hashlib.md5(
                                f"{filepath.name}:{line_num}:{text}".encode()
                            ).hexdigest()[:8]
                            key = f"js:{filepath.name}:{line_num}:{content_hash}"
                            results[key] = {
                                'original': text,
                                'type': f'js_{ptype}',
                                'file': filepath.name,
                                'line': line_num,
                                'pattern': ptype,
                            }

    return results


def extract_file(filepath):
    """Extract all translatable strings from a single HTM file."""
    filepath = Path(filepath)
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    soup = BeautifulSoup(content, 'html.parser')

    html_strings = extract_html_strings(soup, filepath)
    js_strings = extract_js_strings(soup, filepath)

    # Merge
    all_strings = {}
    all_strings.update(html_strings)
    all_strings.update(js_strings)

    return all_strings


def extract_all():
    """Extract strings from all HTM files in the data directory."""
    htm_files = sorted(DATA_DIR.glob('*.htm'))
    if not htm_files:
        print(f"Error: No .htm files found in {DATA_DIR}", file=sys.stderr)
        sys.exit(1)

    all_strings = {}
    file_stats = {}

    for filepath in htm_files:
        strings = extract_file(filepath)
        all_strings.update(strings)
        file_stats[filepath.name] = len(strings)
        print(f"  {filepath.name}: {len(strings)} strings")

    print(f"\nTotal: {len(all_strings)} translatable strings from {len(htm_files)} files")
    return all_strings, file_stats


def build_template(strings):
    """Build a translation template (English → English)."""
    template = {}
    for key, info in strings.items():
        template[key] = {
            'en': info['original'],
            'translation': '',  # To be filled by translator
            'context': f"{info['file']}:{info.get('line', '')} ({info['type']})",
        }
    return template


def main():
    import argparse
    parser = argparse.ArgumentParser(description='Extract translatable strings from WLED Web UI')
    parser.add_argument('--locale', default=None,
                       help='Locale code (e.g. zh_CN). If omitted, generates _template.json')
    parser.add_argument('--output', default=None,
                       help='Output file path (default: locales/<locale>.json)')
    parser.add_argument('--stats', action='store_true',
                       help='Print detailed statistics')
    args = parser.parse_args()

    print("WLED i18n String Extractor")
    print("=" * 40)
    print(f"Data directory: {DATA_DIR}")
    print()

    strings, stats = extract_all()

    if args.stats:
        print("\nPer-file breakdown:")
        for fname, count in sorted(stats.items()):
            print(f"  {fname}: {count}")

    # Build template
    template = build_template(strings)

    # Determine output path
    if args.output:
        output_path = Path(args.output)
    elif args.locale:
        output_path = LOCALES_DIR / f"{args.locale}.json"
    else:
        output_path = LOCALES_DIR / "_template.json"

    output_path.parent.mkdir(parents=True, exist_ok=True)

    # Group by file for readability
    by_file = {}
    for key, entry in template.items():
        fname = entry['context'].split(':')[0]
        if fname not in by_file:
            by_file[fname] = {}
        by_file[fname][key] = entry

    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(by_file, f, ensure_ascii=False, indent=2)

    print(f"\nOutput: {output_path}")
    print(f"Template has {len(template)} entries across {len(by_file)} files")


if __name__ == '__main__':
    main()
