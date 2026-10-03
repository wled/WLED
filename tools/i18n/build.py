#!/usr/bin/env python3
"""
WLED i18n Build Script (v4 - out-of-tree translation support)

Generates translated HTML/JS files from English source + locale JSON.
Translations are loaded from external repos (WLED-translations) via
PlatformIO's custom_usermods mechanism, or from a local directory.

Usage:
  python3 build.py --locale zh_CN --source-dir wled00/data --output-dir build/i18n/zh_CN

PlatformIO integration (out-of-tree):
  # In platformio_override.ini:
  # [env:esp32dev_zh_CN]
  # extends = env:esp32dev
  # custom_usermods = https://github.com/foxlesbiao/WLED-translations
  # build_flags = ${env:esp32dev.build_flags} -D WLED_LOCALE=zh_CN
  # extra_scripts = pre:tools/i18n/build.py
"""

import json
import os
import re
import sys
import tempfile
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_DIR = SCRIPT_DIR.parent.parent  # WLED root
DATA_DIR = PROJECT_DIR / "wled00" / "data"

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


def find_translations_dir(locale, translations_dir=None):
    """Locate the translations directory for a locale.

    Search order:
    1. Explicit --translations-dir argument
    2. PlatformIO libdeps (out-of-tree usermod: .pio/libdeps/<env>/WLED-translations/)
    3. Local fallback (tools/i18n/locales/)
    """
    # 1. Explicit path
    if translations_dir:
        p = Path(translations_dir)
        if p.exists():
            return p
        print(f"Warning: --translations-dir not found: {p}", file=sys.stderr)

    # 2. PlatformIO libdeps (out-of-tree usermod)
    pio_libdeps = PROJECT_DIR / ".pio" / "libdeps"
    if pio_libdeps.exists():
        for env_dir in pio_libdeps.iterdir():
            if not env_dir.is_dir():
                continue
            candidate = env_dir / "WLED-translations"
            if candidate.exists() and (candidate / locale).exists():
                return candidate / locale
            for subdir in env_dir.iterdir():
                if not subdir.is_dir():
                    continue
                lib_json = subdir / "library.json"
                if lib_json.exists():
                    try:
                        with open(lib_json) as f:
                            meta = json.load(f)
                        if meta.get("name") == "WLED-translations":
                            if (subdir / locale).exists():
                                return subdir / locale
                    except (json.JSONDecodeError, KeyError):
                        pass

    # 3. Local fallback
    local = SCRIPT_DIR / "locales"
    if (local / f"{locale}.json").exists():
        return local

    return None


def load_translations(locale, translations_dir=None):
    """Load translation JSON for the given locale, keyed by file."""
    tdir = find_translations_dir(locale, translations_dir)
    if tdir is None:
        print(f"Error: No translations found for locale '{locale}'.", file=sys.stderr)
        print(f"  Searched:", file=sys.stderr)
        print(f"    --translations-dir (if provided)", file=sys.stderr)
        print(f"    .pio/libdeps/*/WLED-translations/{locale}/", file=sys.stderr)
        print(f"    tools/i18n/locales/{locale}.json", file=sys.stderr)
        sys.exit(1)

    tdir = Path(tdir)
    merged = {}

    if tdir.is_file():
        files_to_load = [tdir]
    elif tdir.is_dir():
        files_to_load = sorted(tdir.glob("*.json"))
        single = tdir / f"{locale}.json"
        if single.exists() and single not in files_to_load:
            files_to_load = [single]
    else:
        print(f"Error: Translations path is neither file nor directory: {tdir}", file=sys.stderr)
        sys.exit(1)

    for jf in files_to_load:
        if jf.name == "metadata.json":
            continue
        with open(jf, 'r', encoding='utf-8') as f:
            data = json.load(f)
        for fname, entries in data.items():
            if fname not in merged:
                merged[fname] = {}
            for key, entry in entries.items():
                trans = entry.get('translation', '').strip()
                if trans:
                    merged[fname][key] = {
                        'original': entry.get('en', ''),
                        'translation': trans,
                    }

    if not merged:
        print(f"Warning: No translations loaded for {locale} from {tdir}", file=sys.stderr)

    return merged


def split_script_blocks(content):
    """Split content into (non_script, script) segments for safe processing."""
    segments = []
    pattern = re.compile(r'(<script[^>]*>)(.*?)(</script>)', re.DOTALL | re.IGNORECASE)
    last_end = 0

    for match in pattern.finditer(content):
        before = content[last_end:match.start()]
        if before:
            segments.append((before, False))
        segments.append((match.group(0), True))
        last_end = match.end()

    after = content[last_end:]
    if after:
        segments.append((after, False))

    return segments


def replace_html_text(content, original, translated):
    """Replace HTML text content using exact string matching."""
    escaped = re.escape(original)
    total = 0

    p1 = re.compile(r'(>)\s*(' + escaped + r')\s*(</?\w)')
    content, n = p1.subn(r'\g<1>' + translated + r'\g<3>', content)
    total += n

    p2 = re.compile(r'(</\w+>)\s*(' + escaped + r')\s*(</?\w)')
    content, n = p2.subn(r'\g<1>' + translated + r'\g<3>', content)
    total += n

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
    """Replace a JS string literal within a single <script>...</script> block."""
    for quote in ['"', "'", '`']:
        escaped = re.escape(original)
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
    """Apply all translations to a file's content."""
    total = 0

    content = re.sub(
        r'(<html\s[^>]*lang\s*=\s*")([^"]+)(")',
        r'\g<1>' + lang_code + r'\g<3>',
        content, count=1
    )

    segments = split_script_blocks(content)
    file_translations = translations.get(file_key, {})
    new_segments = []

    for segment_text, is_script in segments:
        if is_script:
            for key, entry in file_translations.items():
                if key.startswith('js:'):
                    segment_text, count = replace_js_in_block(
                        segment_text, entry['original'], entry['translation']
                    )
                    total += count
        else:
            for key, entry in file_translations.items():
                if key.startswith('html:'):
                    parts = key.split(':')
                    attr_name = parts[-1]

                    if attr_name == 'text':
                        segment_text, count = replace_html_text(
                            segment_text, entry['original'], entry['translation']
                        )
                        total += count
                    elif attr_name in ('placeholder', 'title', 'alt', 'aria-label'):
                        segment_text, count = replace_html_attr(
                            segment_text, attr_name, entry['original'], entry['translation']
                        )
                        total += count

        new_segments.append(segment_text)

    return ''.join(new_segments), total


def build_locale(locale, source_dir=None, output_dir=None, translations_dir=None):
    """Build translated HTM files for a given locale."""
    file_translations = load_translations(locale, translations_dir)
    if not file_translations:
        print(f"Warning: No translations found for {locale}")
        return 0

    lang_code = LOCALE_LANG.get(locale, locale.split('_')[0])
    src_dir = Path(source_dir) if source_dir else DATA_DIR

    htm_files = sorted(src_dir.glob('*.htm'))
    if not htm_files:
        print(f"Error: No .htm files found in {src_dir}", file=sys.stderr)
        return 0

    if output_dir:
        out_dir = Path(output_dir)
    else:
        out_dir = Path(tempfile.mkdtemp(prefix=f'wled_i18n_{locale}_'))
        print(f"[i18n] No --output-dir specified, using temp: {out_dir}")

    out_dir.mkdir(parents=True, exist_ok=True)

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


def validate_translations(locale, translations_dir=None):
    """Validate translation completeness against English source files."""
    file_translations = load_translations(locale, translations_dir)
    if not file_translations:
        print(f"No translations for {locale}")
        return False

    total_keys = 0
    total_translated = 0

    for fname, entries in file_translations.items():
        translated = sum(1 for e in entries.values() if e.get('translation', '').strip())
        total = len(entries)
        total_keys += total
        total_translated += translated
        status = "OK" if translated == total else f"{total - translated} missing"
        print(f"  {fname}: {translated}/{total} ({status})")

    pct = (total_translated / total_keys * 100) if total_keys else 0
    print(f"\nTotal: {total_translated}/{total_keys} ({pct:.1f}%)")
    return total_translated == total_keys


def main():
    import argparse
    parser = argparse.ArgumentParser(description='Build translated WLED Web UI files')
    parser.add_argument('--locale', required=True, help='Locale code (e.g. zh_CN)')
    parser.add_argument('--source-dir', default=None, help='Source directory (default: wled00/data/)')
    parser.add_argument('--output-dir', default=None, help='Output directory (default: temp dir)')
    parser.add_argument('--translations-dir', default=None,
                        help='Translations directory (default: auto-detect via PlatformIO libdeps)')
    parser.add_argument('--validate', action='store_true',
                        help='Validate translation completeness (no build)')
    args = parser.parse_args()

    if args.validate:
        print(f"Validating translations for {args.locale}")
        print("=" * 40)
        ok = validate_translations(args.locale, args.translations_dir)
        sys.exit(0 if ok else 1)

    print(f"WLED i18n Build — {args.locale}")
    print("=" * 40)

    count = build_locale(args.locale, args.source_dir, args.output_dir, args.translations_dir)
    if count == 0:
        print("\nWarning: No translations applied!")


# PlatformIO pre-build integration
def pre_build(source, target, env):
    """PlatformIO pre-build script entry point."""
    import re as _re
    locale = None
    for flag in env.get('BUILD_FLAGS', []):
        m = _re.match(r'-D\s*WLED_LOCALE=(\S+)', flag)
        if m:
            locale = m.group(1).strip()
            break

    if not locale:
        print("[i18n] No WLED_LOCALE set, skipping translation")
        return

    print(f"[i18n] Building with locale: {locale}")
    build_dir = Path(env.subst('$BUILD_DIR')) / 'i18n' / locale
    build_locale(locale, output_dir=build_dir)


try:
    Import("env")
    env.AddPreAction("buildprog", pre_build)
except NameError:
    if __name__ == '__main__':
        main()
