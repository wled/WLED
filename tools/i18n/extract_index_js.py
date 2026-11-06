#!/usr/bin/env python3
"""WLED index.js String Extractor

Extracts translatable strings from index.js file.
Works alongside extract.py to provide complete coverage.

Usage:
    python3 extract_index_js.py --wled-path ~/WLED --output index_js.json
"""

import json
import re
import sys
import argparse
from pathlib import Path
from common import compute_hash


# Patterns for extracting user-visible strings from index.js
JS_PATTERNS = [
    # showToast("...")
    (r'showToast\s*\(\s*["`\']([^"`\']+)["`\']', 'showToast'),
    # errstr = "..."
    (r'errstr\s*=\s*["`\']([^"`\']+)["`\']', 'errstr'),
    # innerHTML = "..." (with HTML content)
    (r'innerHTML\s*=\s*["`\']([^"`\']+)["`\']', 'innerHTML'),
    # innerText = "..."
    (r'innerText\s*=\s*["`\']([^"`\']+)["`\']', 'innerText'),
    # textContent = "..."
    (r'textContent\s*=\s*["`\']([^"`\']+)["`\']', 'textContent'),
    # title = "..."
    (r'title\s*=\s*["`\']([^"`\']+)["`\']', 'title'),
    # confirm("...")
    (r'confirm\s*\(\s*["`\']([^"`\']+)["`\']', 'confirm'),
    # prompt("...")
    (r'prompt\s*\(\s*["`\']([^"`\']+)["`\']', 'prompt'),
    # <option>text</option> in template literals
    (r'<option[^>]*>([^<]+)</option>', 'option'),
    # Text in template literals (e.g., `text ${var} more text`)
    # Match sequences of non-backtick, non-dollar, non-brace characters
    (r'`([^`$]*?)(?:`|\$\{)', 'template_text'),
]

# Skip patterns
SKIP_PATTERNS = [
    r'^\s*$',           # empty
    r'^\d+$',           # pure numbers
    r'^#[0-9a-fA-F]+$', # hex colors
    r'^https?://',      # URLs
    r'^[{}\[\]();,\s]+$', # pure punctuation
    r'^function\s',     # function declarations
    r'^var\s|^let\s|^const\s', # variable declarations
    r'document\.|window\.|console\.', # DOM API calls
    r'^[a-z]+[A-Z]\w*$',  # camelCase (but allow known UI terms below)
]


KNOWN_UI_TERMS = frozenset({
    'iOS', 'WiFi', 'USB', 'LED', 'JSON', 'RGB', 'HTTP', 'HTTPS',
    'NTP', 'OTA', 'DMX', 'AP', 'DNS', 'API', 'URL', 'TCP', 'UDP',
    'mDNS', 'Alexa', 'Blynk', 'Hue', 'Mqtt', 'E131', 'DDP'
})


def is_translatable_text(text):
    """Check if text is worth translating."""
    text = text.strip()
    if not text or len(text) < 2:
        return False
    
    if text in KNOWN_UI_TERMS:
        return True
    
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


def extract_strings_from_js(js_content, filename):
    """Extract translatable strings from JS content."""
    strings = {}
    
    for pattern, pattern_type in JS_PATTERNS:
        matches = re.finditer(pattern, js_content)
        for match in matches:
            # For template_text pattern, we need to extract the text before ${
            if pattern_type == 'template_text':
                text = match.group(1).strip()
                if not text:
                    continue
            else:
                text = match.group(1)
            
            if not is_translatable_text(text):
                continue
            
            # Get line number
            line_num = js_content[:match.start()].count('\n') + 1
            
            # Create key: js:filename:line:hash
            content_hash = compute_hash(text)
            key = f"js:{filename}:{line_num}:{content_hash}"
            
            strings[key] = {
                'en': text,
                'type': 'js',
                'context': f"{filename}:{line_num} (js_{pattern_type})"
            }
    
    return strings


def main():
    parser = argparse.ArgumentParser(
        description='Extract strings from WLED index.js')
    parser.add_argument('--wled-path', required=True,
                        help='Path to WLED repository')
    parser.add_argument('--output', required=True,
                        help='Output JSON file')
    args = parser.parse_args()
    
    wled_path = Path(args.wled_path)
    output_path = Path(args.output)
    
    # Read index.js
    index_js_path = wled_path / 'wled00' / 'data' / 'index.js'
    if not index_js_path.exists():
        print(f"Error: {index_js_path} not found")
        sys.exit(1)
    
    with open(index_js_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Extract strings
    strings = extract_strings_from_js(content, 'index.js')
    
    if not strings:
        print("No translatable strings found")
        sys.exit(1)
    
    # Save
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(strings, f, indent=2, ensure_ascii=False)
    
    print(f"✓ Extracted {len(strings)} strings from index.js")


if __name__ == '__main__':
    main()
