#!/usr/bin/env python3
"""WLED Auto-Translation Script

Automatically translates newly added WLED strings using LLM.
Works with the diff output to translate only new/modified strings.

Usage:
    python3 auto_translate.py --diff-file diff.json --locale zh_CN
    python3 auto_translate.py --translations-path ~/WLED-translations --locale zh_CN
"""

import sys
import argparse
import tempfile
from pathlib import Path
from typing import Dict, List, Optional
from common import load_json, save_json


# Translation rules for WLED
TRANSLATION_RULES = """
## WLED翻译规则

### 必须翻译
- 按钮文字（Back→返回、Save→保存、Scan→扫描）
- 标题（WiFi Settings→WiFi 设置）
- 标签和表单文字
- 错误消息和提示
- 状态信息

### 不可改动
- HTML标签和属性
- JavaScript代码逻辑
- CSS样式
- 技术术语（GPIO、JSON、RGB、WiFi、NTP、OTA、DMX）
- 变量名、函数名
- URL和API端点

### 翻译风格
- 简洁技术中文
- 按钮用动词
- 标题用名词短语
- 保留英文技术术语
- 中文全角标点（：，。）

### WLED特有术语
- Preset → 预设
- Segment → 段
- Palette → 调色板
- Effect → 特效
- LED → LED（不翻译）
- Brightness → 亮度
- Color → 颜色
- Speed → 速度
- Intensity → 强度
"""


def load_diff(diff_file: Path) -> Dict:
    """Load diff output from diff.py."""
    return load_json(diff_file)


def load_template(template_path: Path, filename: str) -> Dict:
    """Load a template JSON file. Returns empty dict if not found."""
    filepath = template_path / filename
    if not filepath.exists():
        return {}
    return load_json(filepath)


def save_template(template_path: Path, filename: str, data: Dict):
    """Save template JSON file."""
    save_json(template_path / filename, data)


def extract_string_info(key: str, template_data: Dict) -> Optional[Dict]:
    """Extract string information from template."""
    if key not in template_data:
        return None
    
    entry = template_data[key]
    if isinstance(entry, dict):
        return {
            'key': key,
            'en': entry.get('en', ''),
            'translation': entry.get('translation', ''),
            'file': entry.get('file', ''),
            'path': entry.get('path', '')
        }
    else:
        return {
            'key': key,
            'en': str(entry),
            'translation': '',
            'file': '',
            'path': ''
        }


def create_translation_prompt(strings: List[Dict], locale: str) -> str:
    """Create prompt for LLM translation."""
    locale_name = {'zh_CN': '简体中文', 'zh_TW': '繁体中文', 'ja': '日语', 'ko': '韩语'}.get(locale, locale)
    prompt = f"""你是一个专业的WLED固件Web UI翻译器。请将以下英文字符串翻译成{locale_name}。

{TRANSLATION_RULES}

## 需要翻译的字符串

"""
    
    for i, s in enumerate(strings, 1):
        prompt += f"### {i}. Key: `{s['key']}`\n"
        prompt += f"英文: `{s['en']}`\n"
        if s['file']:
            prompt += f"文件: {s['file']}\n"
        if s['path']:
            prompt += f"路径: {s['path']}\n"
        prompt += "\n"
    
    prompt += """## 输出格式

请以JSON格式返回翻译结果，格式如下：
```json
{
  "translations": [
    {
      "key": "原始key",
      "en": "英文原文",
      "translation": "中文翻译"
    }
  ]
}
```

只返回JSON，不要其他内容。"""
    
    return prompt



def apply_translations(translations_path: Path, locale: str, translations) -> bool:
    """Apply translations to locale files."""
    locale_dir = translations_path / locale
    
    # Validate input type
    if isinstance(translations, dict):
        # If top-level is a dict with 'translations' key, extract the list
        if 'translations' in translations:
            translations = translations['translations']
        else:
            print("Error: Translation file is a dict but missing 'translations' key")
            return False
    elif not isinstance(translations, list):
        print(f"Error: Translation file must be a list or dict, got {type(translations).__name__}")
        return False

    # Load existing translations
    static_trans = load_template(locale_dir, 'static.json')
    js_trans = load_template(locale_dir, 'js.json')
    effects_trans = load_template(locale_dir, 'effects.json')
    palettes_trans = load_template(locale_dir, 'palettes.json')
    
    # Build lookup index for faster searching
    # Maps en text -> list of (file_type, key) to handle duplicate en texts
    en_lookup: dict[str, list[tuple[str, str]]] = {}
    file_map = {
        'static': static_trans,
        'js': js_trans,
        'effects': effects_trans,
        'palettes': palettes_trans,
    }
    for file_type, file_data in file_map.items():
        for k, v in file_data.items():
            if isinstance(v, dict) and v.get('en'):
                en_text = v['en']
                if en_text not in en_lookup:
                    en_lookup[en_text] = []
                en_lookup[en_text].append((file_type, k))
    
    updated_count = 0
    
    for t in translations:
        if not isinstance(t, dict) or 'key' not in t:
            continue
        key = t['key']
        translation = t.get('translation', '')
        
        if not translation:
            continue
        
        # Determine which file this belongs to
        applied = False
        for ft, fd in file_map.items():
            if key in fd:
                fd[key]['translation'] = translation
                updated_count += 1
                applied = True
                break
        if not applied:
            # Try to find by en text using lookup index
            en_text = t.get('en', '')
            if en_text and en_text in en_lookup:
                # Apply translation to ALL matching keys (same en text may appear in multiple places)
                for file_type, lookup_key in en_lookup[en_text]:
                    target = file_map[file_type]
                    if lookup_key in target:
                        target[lookup_key]['translation'] = translation
                        updated_count += 1
    
    # Save updated translations
    if updated_count > 0:
        save_template(locale_dir, 'static.json', static_trans)
        save_template(locale_dir, 'js.json', js_trans)
        save_template(locale_dir, 'effects.json', effects_trans)
        save_template(locale_dir, 'palettes.json', palettes_trans)
        print(f"✓ Applied {updated_count} translations")
    else:
        print("No translations to apply")
    return True


def main():
    parser = argparse.ArgumentParser(
        description='Auto-translate WLED strings')
    parser.add_argument('--diff-file',
                        help='Path to diff.json from diff.py (required unless --apply)')
    parser.add_argument('--translations-path', required=True,
                        help='Path to WLED-translations repository')
    parser.add_argument('--locale', default='zh_CN',
                        help='Target locale (default: zh_CN)')
    parser.add_argument('--apply', 
                        help='Path to translation results JSON to apply')
    parser.add_argument('--prompt-only', action='store_true',
                        help='Only generate translation prompt, do not translate')
    args = parser.parse_args()
    
    translations_path = Path(args.translations_path)
    
    # If applying existing translations
    if args.apply:
        apply_path = Path(args.apply)
        if not apply_path.exists():
            print(f"Error: Translation file not found: {apply_path}")
            sys.exit(1)
        translations = load_json(apply_path)
        if not apply_translations(translations_path, args.locale, translations):
            sys.exit(1)
        return
    
    # Load diff if provided
    added_keys = []
    if args.diff_file:
        diff = load_diff(Path(args.diff_file))
        added_keys = diff.get('added', [])
        
        if not added_keys:
            print("No new strings to translate")
            return
        
        print(f"Found {len(added_keys)} new strings to translate")
    else:
        print("Error: --diff-file is required (unless using --apply)")
        sys.exit(1)
    
    # Load template to get string details
    template_path = translations_path / 'en_template_new'
    if not template_path.exists():
        template_path = translations_path / 'en_template'
    
    # Collect strings to translate
    strings_to_translate = []
    
    # Load all template files
    for filename in ['static.json', 'js.json', 'effects.json', 'palettes.json']:
        template_data = load_template(template_path, filename)
        
        for key in added_keys:
            info = extract_string_info(key, template_data)
            if info and info['en']:
                strings_to_translate.append(info)
    
    if not strings_to_translate:
        print("No strings found in templates")
        return
    
    # Generate translation prompt
    prompt = create_translation_prompt(strings_to_translate, args.locale)
    
    if args.prompt_only:
        print(prompt)
        return
    
    # Save prompt and instructions
    prompt_file = Path(tempfile.gettempdir()) / 'wled_translation_prompt.txt'
    with open(prompt_file, 'w', encoding='utf-8') as f:
        f.write(prompt)
    
    print(f"✓ Translation prompt saved to: {prompt_file}")
    print(f"✓ {len(strings_to_translate)} strings ready for translation")
    print("\nNext steps:")
    print("1. Copy the prompt and send to LLM")
    print("2. Save LLM response to: /tmp/wled_translation_result.json")
    print("3. Run: python3 auto_translate.py --translations-path ~/WLED-translations --apply /tmp/wled_translation_result.json")


if __name__ == '__main__':
    main()
