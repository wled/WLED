#!/usr/bin/env python3
"""WLED Translation Evolution Controller

Main controller for the self-evolving translation system.
Detects upstream changes, extracts new strings, and prepares for translation.

Usage:
    python3 wled_evolve.py check          # Check for updates only
    python3 wled_evolve.py extract        # Extract new template
    python3 wled_evolve.py diff           # Compare templates
    python3 wled_evolve.py translate      # Prepare translation prompt
    python3 wled_evolve.py apply          # Apply translations
    python3 wled_evolve.py full           # Full cycle (check + extract + diff + translate)
"""

import json
import sys
import argparse
from pathlib import Path
from datetime import datetime

# Ensure scripts/ directory is in sys.path for common module import
_scripts_dir = str(Path(__file__).resolve().parent)
if _scripts_dir not in sys.path:
    sys.path.insert(0, _scripts_dir)
from common import run_command


class WLEDEvolution:
    """Main controller for WLED translation evolution."""

    def __init__(self, wled_path: str, translations_path: str):
        self.wled_path = Path(wled_path)
        self.translations_path = Path(translations_path)
        self.scripts_dir = self.translations_path / 'scripts'

    def _run_optional_extraction(self, script: Path, args: list[str],
                                 label: str) -> str | None:
        """Run an optional extraction script. Returns error msg or None."""
        if not script.exists():
            return f"{script.name} not found, skipping {label}"
        success, output = run_command([sys.executable, str(script)] + args)
        if not success:
            return f"{label} extraction failed: {output}"
        return None

    def check_upstream(self) -> dict:
        """Check if upstream has new commits."""
        cmd = [
            sys.executable,
            str(self.scripts_dir / 'check_upstream.py'),
            '--wled-path', str(self.wled_path),
            '--translations-path', str(self.translations_path),
            '--json'
        ]

        success, output = run_command(cmd)
        if not success:
            return {'error': output}

        try:
            return json.loads(output)
        except json.JSONDecodeError as e:
            return {'error': f'Invalid JSON from check_upstream: {e}'}

    def extract_template(self) -> bool:
        """Extract new template from upstream.

        Returns False if the critical HTML/JS extraction fails.
        Returns True even if optional steps (effects, palettes, index.js, convert) fail,
        but prints warnings for each failure.
        """
        i18n_extract = self.wled_path / 'tools' / 'i18n' / 'extract.py'
        effects_palettes_script = self.scripts_dir / 'extract_effects_palettes.py'
        index_js_script = self.scripts_dir / 'extract_index_js.py'
        output_file = self.translations_path / 'en_template_new' / 'strings.json'
        effects_file = self.translations_path / 'en_template_new' / 'effects.json'
        palettes_file = self.translations_path / 'en_template_new' / 'palettes.json'
        index_js_file = self.translations_path / 'en_template_new' / 'index_js.json'
        old_format_file = self.translations_path / 'en_template_new' / 'old_format.json'

        if not i18n_extract.exists():
            print(f"Error: extract.py not found at {i18n_extract}")
            return False

        # Create output directory
        output_file.parent.mkdir(exist_ok=True)

        # Run extract.py from WLED directory (HTML + JS strings) — critical step
        cmd = [sys.executable, str(i18n_extract), '--output', str(output_file)]
        success, output = run_command(cmd, cwd=self.wled_path)
        if not success:
            print(f"Error: {output}")
            return False

        print(f"✓ HTML/JS strings extracted to {output_file}")

        # Track optional step failures
        warnings = []

        # Optional extraction steps
        extraction_steps = [
            (effects_palettes_script,
             ['effects', '--wled-path', str(self.wled_path),
              '--output', str(effects_file)],
             'Effects', effects_file),
            (effects_palettes_script,
             ['palettes', '--wled-path', str(self.wled_path),
              '--output', str(palettes_file)],
             'Palettes', palettes_file),
            (index_js_script,
             ['--wled-path', str(self.wled_path),
              '--output', str(index_js_file)],
             'index.js strings', index_js_file),
        ]
        for script, args, label, step_output in extraction_steps:
            err = self._run_optional_extraction(script, args, label)
            if err:
                print(f"Warning: {err}")
                warnings.append(err)
            else:
                print(f"✓ {label} extracted to {step_output}")

        # Convert to old format for diff compatibility (optional)
        convert_script = self.scripts_dir / 'convert_template.py'
        if convert_script.exists():
            cmd = [
                sys.executable, str(convert_script),
                '--input', str(output_file),
                '--output', str(old_format_file),
                '--to-old',
            ]
            if effects_file.exists():
                cmd.extend(['--merge-effects', str(effects_file)])
            if palettes_file.exists():
                cmd.extend(['--merge-palettes', str(palettes_file)])
            if index_js_file.exists():
                cmd.extend(['--merge-index-js', str(index_js_file)])
            success, output = run_command(cmd)
            if success:
                print(f"✓ Converted to old format: {old_format_file}")
            else:
                msg = f"Format conversion failed: {output}"
                print(f"Warning: {msg}")
                warnings.append(msg)

        if warnings:
            print(f"\n⚠ {len(warnings)} optional step(s) failed")

        return True

    def diff_templates(self, locale: str = 'zh_CN') -> dict:
        """Compare old and new templates."""
        diff_script = self.wled_path / 'tools' / 'i18n' / 'diff.py'
        old_template = self.translations_path / 'en_template'
        new_template = self.translations_path / 'en_template_new' / 'old_format.json'

        if not diff_script.exists():
            return {'error': 'diff.py not found'}

        if not new_template.exists():
            return {'error': 'New template not found. Run extract first.'}

        cmd = [
            sys.executable, str(diff_script),
            '--old', str(old_template),
            '--new', str(new_template),
            '--locale', locale
        ]

        # diff.py returns 0 if no changes, 1 if changes found — both are valid
        success, output = run_command(cmd, allow_nonzero_stdout=True)
        if not success:
            return {'error': output}

        try:
            return json.loads(output)
        except json.JSONDecodeError as e:
            return {'error': f'Invalid JSON from diff: {e}'}

    def prepare_translation(self, diff_result: dict) -> dict:
        """Prepare translation prompt for new strings."""
        added_keys = diff_result.get('added', [])

        if not added_keys:
            return {'message': 'No new strings to translate'}

        # Save diff to file first
        diff_file = self.translations_path / 'last_diff.json'
        with open(diff_file, 'w', encoding='utf-8') as f:
            json.dump(diff_result, f, indent=2, ensure_ascii=False)

        # Run auto_translate.py to generate prompt with diff file
        cmd = [
            sys.executable,
            str(self.scripts_dir / 'auto_translate.py'),
            '--translations-path', str(self.translations_path),
            '--diff-file', str(diff_file),
            '--prompt-only'
        ]

        success, output = run_command(cmd)
        if not success:
            return {'error': output}

        return {
            'added_count': len(added_keys),
            'diff_file': str(diff_file),
            'prompt': output
        }

    def apply_translations(self, translation_file: str) -> bool:
        """Apply translations from file."""
        cmd = [
            sys.executable,
            str(self.scripts_dir / 'auto_translate.py'),
            '--translations-path', str(self.translations_path),
            '--apply', translation_file
        ]

        success, output = run_command(cmd)
        if not success:
            print(f"Error: {output}")
            return False

        print(output)
        return True

    def full_cycle(self, locale: str = 'zh_CN') -> dict:
        """Run full evolution cycle."""
        result = {
            'timestamp': datetime.now().isoformat(),
            'steps': []
        }

        # Step 1: Check upstream
        print("Step 1: Checking upstream...")
        check_result = self.check_upstream()
        result['steps'].append({'name': 'check', 'result': check_result})

        if 'error' in check_result:
            result['error'] = check_result['error']
            return result

        if not check_result.get('has_updates'):
            result['message'] = 'No updates available'
            return result

        # Step 2: Extract template
        print("Step 2: Extracting template...")
        if not self.extract_template():
            result['error'] = 'Extraction failed'
            return result

        result['steps'].append({'name': 'extract', 'status': 'success'})

        # Step 3: Diff templates
        print("Step 3: Comparing templates...")
        diff_result = self.diff_templates(locale=locale)
        result['steps'].append({'name': 'diff', 'result': diff_result})

        if 'error' in diff_result:
            result['error'] = diff_result['error']
            return result

        added_count = len(diff_result.get('added', []))
        if added_count == 0:
            result['message'] = 'No new strings to translate'
            return result

        # Step 4: Prepare translation
        print("Step 4: Preparing translation...")
        translate_result = self.prepare_translation(diff_result)
        result['steps'].append({'name': 'translate', 'result': translate_result})

        if 'error' in translate_result:
            result['error'] = translate_result['error']
            return result

        result['success'] = True
        result['message'] = f"Found {added_count} new strings ready for translation"

        return result


def main():
    parser = argparse.ArgumentParser(
        description='WLED Translation Evolution Controller')
    parser.add_argument('command',
                        choices=['check', 'extract', 'diff', 'translate', 'apply', 'full'],
                        help='Command to execute')
    parser.add_argument('--wled-path', default=str(Path.home() / 'WLED'),
                        help='Path to WLED repository')
    parser.add_argument('--translations-path', default=str(Path.home() / 'WLED-translations'),
                        help='Path to WLED-translations repository')
    parser.add_argument('--apply-file',
                        help='Translation file to apply (for apply command)')
    parser.add_argument('--locale', default='zh_CN',
                        help='Target locale (default: zh_CN)')
    parser.add_argument('--json', action='store_true',
                        help='Output as JSON')
    args = parser.parse_args()

    evolution = WLEDEvolution(args.wled_path, args.translations_path)
    result = {}

    if args.command == 'check':
        result = evolution.check_upstream()
    elif args.command == 'extract':
        success = evolution.extract_template()
        result = {'success': success}
    elif args.command == 'diff':
        result = evolution.diff_templates(locale=args.locale)
    elif args.command == 'translate':
        diff_result = evolution.diff_templates(locale=args.locale)
        if 'error' in diff_result:
            result = diff_result
        else:
            result = evolution.prepare_translation(diff_result)
    elif args.command == 'apply':
        if not args.apply_file:
            print("Error: --apply-file required for apply command")
            sys.exit(1)
        success = evolution.apply_translations(args.apply_file)
        result = {'success': success}
    elif args.command == 'full':
        result = evolution.full_cycle(locale=args.locale)

    if args.json:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    else:
        if 'error' in result:
            print(f"Error: {result['error']}")
        elif 'has_updates' in result:
            if result['has_updates']:
                print(f"✓ New upstream commit: {result['upstream_commit']}")
                print(f"  Message: {result['upstream_message']}")
                if result.get('last_extracted_commit'):
                    print(f"  Last extracted: {result['last_extracted_commit']}")
                else:
                    print(f"  No previous extraction found")
            else:
                print(f"No updates. Current: {result['upstream_commit']}")
        elif 'message' in result:
            print(result['message'])
        elif 'success' in result:
            print(f"Success: {result.get('message', 'Operation completed')}")


if __name__ == '__main__':
    main()
