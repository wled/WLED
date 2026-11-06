#!/usr/bin/env python3
"""Common utilities for WLED translation scripts.

Shared functions to avoid code duplication across scripts.
"""

import os
import json
import subprocess
import hashlib
from pathlib import Path
from typing import Any, Union


def load_json(path: Path) -> Any:
    """Load JSON file with utf-8 encoding."""
    with open(path, encoding='utf-8') as f:
        return json.load(f)


def save_json(path: Path, data: Any) -> None:
    """Save JSON file with utf-8 encoding."""
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2, ensure_ascii=False)


def run_command(cmd: list[str], cwd: Union[str, os.PathLike, None] = None,
                allow_nonzero_stdout: bool = False,
                timeout: float = 300) -> tuple[bool, str]:
    """Run a command and return (success, stdout).

    Args:
        cmd: Command and arguments as list.
        cwd: Working directory for the command.
        allow_nonzero_stdout: If True, treat non-zero exit code with stdout as success
                              (used for diff.py which returns 1 when changes found).
        timeout: Max seconds to wait (default 300). None for no limit.

    Returns:
        (True, stdout) on success, (False, stderr_or_stdout) on failure.
    """
    if not cmd:
        return False, 'Empty command'
    try:
        result = subprocess.run(cmd, capture_output=True, text=True,
                                cwd=cwd, timeout=timeout)
    except FileNotFoundError:
        return False, f'Command not found: {cmd[0]}'
    except OSError as e:
        return False, str(e)
    except subprocess.TimeoutExpired:
        return False, f'Command timed out after {timeout}s: {cmd[0]}'

    if result.returncode != 0:
        if allow_nonzero_stdout and result.stdout:
            return True, result.stdout
        return False, result.stderr or result.stdout
    return True, result.stdout


def compute_hash(text: str) -> str:
    """Compute short MD5 hash for text content (first 8 hex chars)."""
    return hashlib.md5(text.encode('utf-8'), usedforsecurity=False).hexdigest()[:8]
