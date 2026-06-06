#!/usr/bin/env python3
"""
WLED i18n Build Script (v2 - surgical replacement)
Generates translated HTML files from English source + locale JSON.

Uses raw string replacement instead of BeautifulSoup serialization
to preserve original HTML formatting exactly (critical for ESP32 flash size).

Usage:
  python3 build.py --locale zh_CN [--source-dir <dir>] [--output-dir <dir>]

PlatformIO integration:
  extra_scripts = pre:tools/i18n/build.py
"""

import json
import os
import re
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
DATA_DIR = SCRIPT_DIR.parent.parent / "wled00" / "data"
LOCALES_DIR = SCRIPT_DIR / "locales"

# Locale → HTML lang attribute
LOCALE_LANG = {
    'zh_CN': 'zh',
    'zh_TW': 'zh-TW',
    'de_DE': 'de',
    'fr_FR': 'fr',
    'ja_JP': 'ja',
    'ko_KR': 'ko',
    'es_ES': 'es',
    'pt_BR': 'pt-BR',
    'ru_RU': 'ru',
}


def load_translations(locale):
    """Load translation JSON for the given locale."""
    locale_file = LOCALES_DIR / f"{locale}.json"
    if not locale_file.exists():
        print(f"Error: Locale file not found: {locale_file}", file=sys.stderr)
        sys.exit(1)

    with open(locale_file, 'r', encoding='utf-8') as f:
        by_file = json.load(f)

    # Flatten into {key: {original, translation}}
    translations = {}
    for fname, entries in by_file.items():
        for key, entry in entries.items():
            trans = entry.get('translation', '').strip()
            if trans:
                translations[key] = {
                    'original': entry.get('en', ''),
                    'translation': trans,
                }
    return translations


def replace_html_text(content, original, translated):
    """Replace HTML text content using exact string matching.
    Handles:
    - >original< (direct child text)
    - >  original  < (with whitespace)
    - >...</child> original< (text after sibling element)
    """
    escaped = re.escape(original)
    total = 0

    # Pattern 1: Text between > and </tag> or > and < (with optional whitespace)
    # Matches: >text< and >text</tag>
    p1 = re.compile(r'(>)\s*(' + escaped + r')\s*(</?\w)')
    content, n = p1.subn(r'\g<1>' + translated + r'\g<3>', content)
    total += n

    # Pattern 2: Text after a closing tag (e.g. </i> Color palette</p>)
    p2 = re.compile(r'(</\w+>)\s*(' + escaped + r')\s*(</?\w)')
    content, n = p2.subn(r'\g<1>' + translated + r'\g<3>', content)
    total += n

    # Pattern 3: Standalone text line (with leading whitespace)
    p3 = re.compile(r'^(\s*)(' + escaped + r')(\s*)$', re.MULTILINE)
    content, n = p3.subn(r'\g<1>' + translated + r'\g<3>', content)
    total += n

    return content, total


def replace_html_attr(content, attr, original, translated):
    """Replace HTML attribute value using exact string matching.
    Finds: attr="original" and replaces with attr="translated"
    """
    # Match attribute="value" with exact value
    pattern = re.compile(
        r'(' + re.escape(attr) + r'\s*=\s*")(' + re.escape(original) + r')(")',
        re.IGNORECASE
    )
    new_content, count = pattern.subn(r'\g<1>' + translated + r'\g<3>', content)
    return new_content, count


def replace_js_string(content, original, translated):
    """Replace a JS string literal in <script> blocks using content matching.
    Finds: "original" or 'original' or `original` within script blocks.
    Only replaces the first occurrence to avoid false matches.
    """
    for quote in ['"', "'", '`']:
        escaped = re.escape(original)
        # Match the string within quotes, only inside <script> blocks
        pattern = re.compile(
            r'(<script[^>]*>.*?)([' + quote + r'])(' + escaped + r')([' + quote + r'])',
            re.DOTALL
        )
        new_content, count = pattern.subn(
            r'\g<1>\g<2>' + translated + r'\g<4>',
            content, count=1
        )
        if count > 0:
            return new_content, count

    # Handle template literals with ${...} - match partial strings
    # e.g. "Hardware channels used: RMT ${usage.rmtUsed}/${max" won't match
    # because the full string has variables. Skip these for now.

    return content, 0


def apply_translations(content, file_key, translations, lang_code):
    """Apply all translations to a file's content."""
    total = 0

    # 1. Update lang attribute
    content = re.sub(
        r'(<html\s[^>]*lang\s*=\s*")([^"]+)(")',
        r'\g<1>' + lang_code + r'\g<3>',
        content, count=1
    )

    # 2. Apply translations
    for key, entry in translations.items():
        original = entry['original']
        translated = entry['translation']

        if key.startswith('html:'):
            # Parse key: "html:path:text" or "html:path:attr"
            parts = key.split(':')
            attr_name = parts[-1]  # Last part is attribute or "text"

            if attr_name == 'text':
                content, count = replace_html_text(content, original, translated)
                total += count
            elif attr_name in ('placeholder', 'title', 'alt', 'aria-label', 'value'):
                content, count = replace_html_attr(content, attr_name, original, translated)
                total += count

        elif key.startswith('js:'):
            # Parse key: "js:filename:line:hash"
            parts = key.split(':')
            if len(parts) >= 4 and parts[1] == file_key:
                content, count = replace_js_string(content, original, translated)
                total += count

    return content, total


def build_locale(locale, source_dir=None, output_dir=None):
    """Build translated HTM files for a given locale."""
    translations = load_translations(locale)
    if not translations:
        print(f"Warning: No translations found for {locale}")
        return 0

    lang_code = LOCALE_LANG.get(locale, locale.split('_')[0])
    src_dir = Path(source_dir) if source_dir else DATA_DIR

    htm_files = sorted(src_dir.glob('*.htm'))
    if not htm_files:
        print(f"Error: No .htm files found in {src_dir}", file=sys.stderr)
        return 0

    out_dir = Path(output_dir) if output_dir else src_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    total_applied = 0

    for filepath in htm_files:
        file_key = filepath.name
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        content, applied = apply_translations(content, file_key, translations, lang_code)

        out_path = out_dir / file_key
        with open(out_path, 'w', encoding='utf-8') as f:
            f.write(content)

        status = f"{applied} translations" if applied else "no changes"
        print(f"  {file_key}: {status}")
        total_applied += applied

    print(f"\nTotal: {total_applied} translations applied across {len(htm_files)} files")
    print(f"Output: {out_dir}")
    return total_applied


def main():
    import argparse
    parser = argparse.ArgumentParser(description='Build translated WLED Web UI files')
    parser.add_argument('--locale', required=True, help='Locale code (e.g. zh_CN)')
    parser.add_argument('--source-dir', default=None, help='Source directory (default: wled00/data/)')
    parser.add_argument('--output-dir', default=None, help='Output directory (default: same as source)')
    args = parser.parse_args()

    print(f"WLED i18n Build — {args.locale}")
    print("=" * 40)

    count = build_locale(args.locale, args.source_dir, args.output_dir)
    if count == 0:
        print("\nWarning: No translations applied!")


# PlatformIO pre-build integration
def pre_build(source, target, env):
    """PlatformIO pre-build script entry point."""
    locale = None
    for flag in env.get('BUILD_FLAGS', []):
        if '-D WLED_LOCALE=' in flag:
            locale = flag.split('=')[1].strip()
            break

    if not locale:
        print("[i18n] No WLED_LOCALE set, skipping translation")
        return

    print(f"[i18n] Building with locale: {locale}")
    build_locale(locale)


try:
    Import("env")
    env.AddPreAction("buildprog", pre_build)
except NameError:
    if __name__ == '__main__':
        main()
