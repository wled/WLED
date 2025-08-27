# Testing library and framework: pytest (with monkeypatch and capsys fixtures)
# These tests target the SCons-like UI build script from the PR diff.
# The script checks for Node via shutil.which and invokes env.Execute for npm commands.
import importlib.util
from pathlib import Path

def _exec_script_with_env(script_path, fake_env, monkeypatch):
    """
    Execute a Python script file as if SCons ran it, after injecting:
      - a stub Import() that binds 'env' into the script's globals
      - availability of 'env' via that Import
    Returns a namespace-like object with attributes of the executed module.
    """
    # Prepare a new module to execute the script
    spec = importlib.util.spec_from_file_location("build_ui_under_test", script_path)
    module = importlib.util.module_from_spec(spec)

    # Provide a stub Import that injects the provided fake_env
    def Import(name):
        if name != "env":
            raise AssertionError(f"Unexpected Import argument: {name!r}")
        # bind env in the module global namespace
        module.env = fake_env

    # Inject the stub Import into the module globals before execution
    module.Import = Import

    # Execute the module code
    loader = spec.loader
    assert loader is not None
    loader.exec_module(module)
    return module

class FakeEnv:
    def __init__(self, results=None):
        # results: list of return codes that Execute should yield in order
        self.calls = []
        self._results = list(results or [])

    def Execute(self, cmd):
        self.calls.append(cmd)
        if self._results:
            return self._results.pop(0)
        return 0

def _write_temp_script(tmp_path, content: str) -> Path:
    p = tmp_path / "build_ui_script.py"
    p.write_text(content, encoding="utf-8")
    return p

# This is the exact content from the PR diff, preserved for testing.
SCRIPT_CONTENT = """\
Import("env")
import shutil

node_ex = shutil.which("node")
# Check if Node.js is installed and present in PATH if it failed, abort the build
if node_ex is None:
    print('\\x1b[0;31;43m' + 'Node.js is not installed or missing from PATH html css js will not be processed check https://kno.wled.ge/advanced/compiling-wled/' + '\\x1b[0m')
    exitCode = env.Execute("null")
    exit(exitCode)
else:
    # Install the necessary node packages for the pre-build asset bundling script
    print('\\x1b[6;33;42m' + 'Installing node packages' + '\\x1b[0m')
    env.Execute("npm install")

    # Call the bundling script
    exitCode = env.Execute("npm run build")

    # If it failed, abort the build
    if (exitCode):
      print('\\x1b[0;31;43m' + 'npm run build fails check https://kno.wled.ge/advanced/compiling-wled/' + '\\x1b[0m')
      exit(exitCode)
"""

def test_node_missing_aborts_and_calls_null(monkeypatch, capsys, tmp_path):
    # Arrange: create script file
    script = _write_temp_script(tmp_path, SCRIPT_CONTENT)

    # Mock shutil.which to simulate missing node
    monkeypatch.setattr("shutil.which", lambda name: None)

    # Fake env returns 123 for Execute("null")
    fake_env = FakeEnv(results=[123])

    # Act: execute the script, expecting SystemExit with code 123
    try:
        _exec_script_with_env(str(script), fake_env, monkeypatch)
        raise AssertionError("Expected SystemExit when node is missing")
    except SystemExit as e:
        assert e.code == 123

    # Assert: validate calls and user-facing message
    assert fake_env.calls == ["null"]
    out = capsys.readouterr().out
    assert "Node.js is not installed or missing from PATH" in out
    # Color codes preserved
    assert "\x1b[0;31;43m" in out and "\x1b[0m" in out

def test_node_present_installs_and_builds_success(monkeypatch, capsys, tmp_path):
    # Arrange
    script = _write_temp_script(tmp_path, SCRIPT_CONTENT)

    # Node present
    monkeypatch.setattr("shutil.which", lambda name: "/usr/bin/node")

    # env.Execute returns 0 for install and 0 for build
    fake_env = FakeEnv(results=[0, 0])

    # Act: script should complete without exiting the process
    _exec_script_with_env(str(script), fake_env, monkeypatch)

    # Assert: env.Execute called with correct commands in order
    assert fake_env.calls == ["npm install", "npm run build"]
    out = capsys.readouterr().out
    assert "Installing node packages" in out
    # No failure message expected
    assert "npm run build fails" not in out

def test_node_present_build_failure_aborts_with_exit_code(monkeypatch, capsys, tmp_path):
    # Arrange
    script = _write_temp_script(tmp_path, SCRIPT_CONTENT)

    # Node present
    monkeypatch.setattr("shutil.which", lambda name: "/usr/bin/node")

    # env.Execute returns 0 for install, non-zero for build
    fake_env = FakeEnv(results=[0, 77])

    # Act + Assert: SystemExit with build's exit code
    try:
        _exec_script_with_env(str(script), fake_env, monkeypatch)
        raise AssertionError("Expected SystemExit when npm build fails")
    except SystemExit as e:
        assert e.code == 77

    # Assert calls and message
    assert fake_env.calls == ["npm install", "npm run build"]
    out = capsys.readouterr().out
    assert "npm run build fails" in out
    assert "\x1b[0;31;43m" in out and "\x1b[0m" in out

def test_node_present_install_only_one_call_if_build_result_missing(monkeypatch, capsys, tmp_path):
    """
    Defensive test: If env.Execute is implemented to ignore extra calls or returns fewer results,
    ensure at least 'npm install' is attempted and no immediate exit occurs before 'npm run build'.
    """
    script = _write_temp_script(tmp_path, SCRIPT_CONTENT)
    monkeypatch.setattr("shutil.which", lambda name: "/usr/bin/node")
    # Provide only one result; second call returns default 0 from FakeEnv
    fake_env = FakeEnv(results=[0])

    _exec_script_with_env(str(script), fake_env, monkeypatch)

    assert fake_env.calls == ["npm install", "npm run build"]
    out = capsys.readouterr().out
    assert "Installing node packages" in out
# ---------------------------------------------------------------------------
# Additional tests appended by CodeRabbit Inc — Testing library: pytest
# Focused on edge cases from the PR diff behavior.
# ---------------------------------------------------------------------------

def test_node_present_install_failure_ignored_when_build_succeeds(monkeypatch, capsys, tmp_path):
    script = _write_temp_script(tmp_path, SCRIPT_CONTENT)
    monkeypatch.setattr("shutil.which", lambda name: "/usr/bin/node")
    # Install fails (non-zero) but build succeeds; current script ignores install's result
    fake_env = FakeEnv(results=[5, 0])

    _exec_script_with_env(str(script), fake_env, monkeypatch)

    assert fake_env.calls == ["npm install", "npm run build"]
    out = capsys.readouterr().out
    assert "Installing node packages" in out
    assert "npm run build fails" not in out

def test_node_present_install_failure_then_build_failure_exits_with_build_code(monkeypatch, capsys, tmp_path):
    script = _write_temp_script(tmp_path, SCRIPT_CONTENT)
    monkeypatch.setattr("shutil.which", lambda name: "/usr/bin/node")
    fake_env = FakeEnv(results=[5, 9])

    try:
        _exec_script_with_env(str(script), fake_env, monkeypatch)
        raise AssertionError("Expected SystemExit when build fails after install failure")
    except SystemExit as e:
        assert e.code == 9

    assert fake_env.calls == ["npm install", "npm run build"]
    out = capsys.readouterr().out
    assert "npm run build fails" in out

def test_shutil_which_called_with_node(monkeypatch, tmp_path, capsys):
    script = _write_temp_script(tmp_path, SCRIPT_CONTENT)
    seen = []
    def which(name):
        seen.append(name)
        return "/usr/bin/node"
    monkeypatch.setattr("shutil.which", which)
    fake_env = FakeEnv(results=[0, 0])

    _exec_script_with_env(str(script), fake_env, monkeypatch)

    assert seen == ["node"]
    assert fake_env.calls == ["npm install", "npm run build"]

def test_node_empty_string_treated_as_present(monkeypatch, tmp_path, capsys):
    script = _write_temp_script(tmp_path, SCRIPT_CONTENT)
    # Some environments could hypothetically return an empty string; ensure truthy check isn't used for presence
    monkeypatch.setattr("shutil.which", lambda name: "")
    fake_env = FakeEnv(results=[0, 0])

    _exec_script_with_env(str(script), fake_env, monkeypatch)

    assert fake_env.calls == ["npm install", "npm run build"]
    out = capsys.readouterr().out
    assert "Installing node packages" in out

def test_env_execute_raises_runtime_error_propagates(monkeypatch, tmp_path, capsys):
    script = _write_temp_script(tmp_path, SCRIPT_CONTENT)
    monkeypatch.setattr("shutil.which", lambda name: "/usr/bin/node")
    fake_env = FakeEnv()
    def raise_exec(cmd):
        fake_env.calls.append(cmd)
        raise RuntimeError("boom")
    fake_env.Execute = raise_exec

    try:
        _exec_script_with_env(str(script), fake_env, monkeypatch)
        raise AssertionError("Expected RuntimeError from env.Execute")
    except RuntimeError as e:
        assert "boom" in str(e)

    # Only install attempted; build not reached
    assert fake_env.calls == ["npm install"]
    out = capsys.readouterr().out
    assert "Installing node packages" in out

def test_build_exit_code_as_string_propagated(monkeypatch, tmp_path, capsys):
    script = _write_temp_script(tmp_path, SCRIPT_CONTENT)
    monkeypatch.setattr("shutil.which", lambda name: "/usr/bin/node")
    # Build returns a string "1" which is truthy; ensure it's propagated by SystemExit
    fake_env = FakeEnv(results=[0, "1"])

    try:
        _exec_script_with_env(str(script), fake_env, monkeypatch)
        raise AssertionError("Expected SystemExit with string exit code")
    except SystemExit as e:
        assert e.code == "1"

    out = capsys.readouterr().out
    assert "npm run build fails" in out

def test_node_missing_exit_zero_when_env_null_returns_zero(monkeypatch, tmp_path, capsys):
    script = _write_temp_script(tmp_path, SCRIPT_CONTENT)
    monkeypatch.setattr("shutil.which", lambda name: None)
    fake_env = FakeEnv(results=[0])

    try:
        _exec_script_with_env(str(script), fake_env, monkeypatch)
        raise AssertionError("Expected SystemExit with code 0")
    except SystemExit as e:
        assert e.code == 0

    assert fake_env.calls == ["null"]
    out = capsys.readouterr().out
    assert "Node.js is not installed or missing from PATH" in out