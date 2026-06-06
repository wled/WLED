#!/usr/bin/env python3
"""
WLED i18n Build Script (v3 - fixes from coderabbitai review)
Generates translated HTML files from English source + locale JSON.

Uses raw string replacement instead of BeautifulSoup serialization
to preserve original HTML formatting exactly (critical for ESP32 flash size).

Fixes applied:
1. File-scoped HTML replacement (no cross-file bleed)
2. Script-block-aware HTML replacement (skip <script> content)
3. Per-script-block JS replacement (no cross-block matching)
4. Safe default output (never overwrites source)

Usage:
  python3 build.py --locale zh_CN --source-dir wled00/data --output-dir build/i18n/zh_CN

PlatformIO integration:
  extra_scripts = pre:tools/i18n/build.py
"""

import json
import os
import re
import sys
import tempfile
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
DATA_DIR = SCRIPT_DIR.parent.parent / "wled00" / "data"
LOCALES_DIR = SCRIPT_DIR / "locales"

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
    """Load translation JSON for the given locale, keyed by file."""
    locale_file = LOCALES_DIR / f"{locale}.json"
    if not locale_file.exists():
        print(f"Error: Locale file not found: {locale_file}", file=sys.stderr)
        sys.exit(1)

    with open(locale_file, 'r', encoding='utf-8') as f:
        by_file = json.load(f)

    # Build per-file translation dicts
    # Result: {filename: {key: {original, translation}}}
    file_translations = {}
    for fname, entries in by_file.items():
        file_translations[fname] = {}
        for key, entry in entries.items():
            trans = entry.get('translation', '').strip()
            if trans:
                file_translations[fname][key] = {
                    'original': entry.get('en', ''),
                    'translation': trans,
                }
    return file_translations


def split_script_blocks(content):
    """Split content into (non_script, script) segments for safe processing.
    Returns list of (text, is_script) tuples.
    """
    segments = []
    pattern = re.compile(r'(<script[^>]*>)(.*?)(</script>)', re.DOTALL | re.IGNORECASE)
    last_end = 0

    for match in pattern.finditer(content):
        # Non-script content before this script block
        before = content[last_end:match.start()]
        if before:
            segments.append((before, False))
        # The script block itself (including tags)
        segments.append((match.group(0), True))
        last_end = match.end()

    # Remaining non-script content after last script block
    after = content[last_end:]
    if after:
        segments.append((after, False))

    return segments


def replace_html_text(content, original, translated):
    """Replace HTML text content using exact string matching.
    Handles:
    - >original< (direct child text)
    - >  original  < (with whitespace)
    - >...</child> original< (text after sibling element)

    IMPORTANT: content must be non-script segments only.
    """
    escaped = re.escape(original)
    total = 0

    # Pattern 1: Text between > and </tag> or > and < (with optional whitespace)
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
    """Replace HTML attribute value using exact string matching."""
    pattern = re.compile(
        r'(' + re.escape(attr) + r'\s*=\s*")(' + re.escape(original) + r')(")',
        re.IGNORECASE
    )
    new_content, count = pattern.subn(r'\g<1>' + translated + r'\g<3>', content)
    return new_content, count


def replace_js_in_block(script_block, original, translated):
    """Replace a JS string literal within a single <script>...</script> block.
    Returns (new_block, count).
    """
    for quote in ['"', "'", '`']:
        escaped = re.escape(original)
        # Match quoted string within this single script block
        pattern = re.compile(
            r'([' + quote + r'])(' + escaped + r')([' + quote + r'])'
        )
        new_block, count = pattern.subn(
            r'\g<1>' + translated + r'\g<3>',
            script_block, count=1
        )
        if count > 0:
            return new_block, count

    return script_block, 0


def apply_translations(content, file_key, translations, lang_code):
    """Apply all translations to a file's content.
    Uses script-block-aware processing to avoid cross-contamination.
    """
    total = 0

    # 1. Update lang attribute
    content = re.sub(
        r'(<html\s[^>]*lang\s*=\s*")([^"]+)(")',
        r'\g<1>' + lang_code + r'\g<3>',
        content, count=1
    )

    # 2. Split into script/non-script segments
    segments = split_script_blocks(content)

    # 3. Apply translations per-segment
    file_translations = translations.get(file_key, {})
    new_segments = []

    for segment_text, is_script in segments:
        if is_script:
            # Apply JS translations to script blocks
            for key, entry in file_translations.items():
                if key.startswith('js:'):
                    segment_text, count = replace_js_in_block(
                        segment_text, entry['original'], entry['translation']
                    )
                    total += count
        else:
            # Apply HTML translations to non-script content only
            for key, entry in file_translations.items():
                if key.startswith('html:'):
                    parts = key.split(':')
                    attr_name = parts[-1]

                    if attr_name == 'text':
                        segment_text, count = replace_html_text(
                            segment_text, entry['original'], entry['translation']
                        )
                        total += count
                    elif attr_name in ('placeholder', 'title', 'alt', 'aria-label', 'value'):
                        segment_text, count = replace_html_attr(
                            segment_text, attr_name, entry['original'], entry['translation']
                        )
                        total += count

        new_segments.append(segment_text)

    return ''.join(new_segments), total


def build_locale(locale, source_dir=None, output_dir=None):
    """Build translated HTM files for a given locale."""
    file_translations = load_translations(locale)
    if not file_translations:
        print(f"Warning: No translations found for {locale}")
        return 0

    lang_code = LOCALE_LANG.get(locale, locale.split('_')[0])
    src_dir = Path(source_dir) if source_dir else DATA_DIR

    htm_files = sorted(src_dir.glob('*.htm'))
    if not htm_files:
        print(f"Error: No .htm files found in {src_dir}", file=sys.stderr)
        return 0

    # BUG FIX #4: Never default to overwriting source files
    if output_dir:
        out_dir = Path(output_dir)
    else:
        out_dir = Path(tempfile.mkdtemp(prefix=f'wled_i18n_{locale}_'))
        print(f"[i18n] No --output-dir specified, using temp: {out_dir}")

    out_dir.mkdir(parents=True, exist_ok=True)

    # Safety check: warn if output == source
    if out_dir.resolve() == src_dir.resolve():
        print(f"WARNING: Output dir equals source dir ({src_dir}).", file=sys.stderr)
        print(f"  English source files will be overwritten!", file=sys.stderr)
        print(f"  Pass --output-dir to a different location.", file=sys.stderr)

    total_applied = 0

    for filepath in htm_files:
        file_key = filepath.name
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        content, applied = apply_translations(content, file_key, file_translations, lang_code)

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
    parser.add_argument('--output-dir', default=None, help='Output directory (default: temp dir)')
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
    # Use build directory for output, not source directory
    build_dir = Path(env.subst('$BUILD_DIR')) / 'i18n' / locale
    build_locale(locale, output_dir=build_dir)


try:
    Import("env")
    env.AddPreAction("buildprog", pre_build)
except NameError:
    if __name__ == '__main__':
        main()
