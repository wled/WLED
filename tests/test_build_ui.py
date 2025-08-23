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

# ----- Appended tests to expand coverage of the PR diff behavior -----

def test_install_failure_still_runs_build_no_exit(monkeypatch, capsys, tmp_path):
    """
    Even if 'npm install' returns non-zero, the script does not check it and should still attempt the build.
    Verify no SystemExit is raised and build proceeds.
    """
    script = _write_temp_script(tmp_path, SCRIPT_CONTENT)
    monkeypatch.setattr("shutil.which", lambda name: "/usr/bin/node")
    fake_env = FakeEnv(results=[5, 0])  # install fails, build succeeds

    module = _exec_script_with_env(str(script), fake_env, monkeypatch)

    assert fake_env.calls == ["npm install", "npm run build"]
    out = capsys.readouterr().out
    assert "Installing node packages" in out
    assert "npm run build fails" not in out
    assert hasattr(module, "exitCode") and module.exitCode == 0

def test_which_called_with_node(monkeypatch, tmp_path):
    """
    Ensure the check uses shutil.which('node') specifically.
    """
    script = _write_temp_script(tmp_path, SCRIPT_CONTENT)
    calls = []
    def fake_which(name):
        calls.append(name)
        return "/usr/bin/node"
    monkeypatch.setattr("shutil.which", fake_which)
    fake_env = FakeEnv(results=[0, 0])

    _exec_script_with_env(str(script), fake_env, monkeypatch)

    assert calls == ["node"]

def test_success_exposes_exitCode_and_node_ex(monkeypatch, tmp_path):
    """
    On success, the module should expose both 'exitCode' (0) and the resolved 'node_ex' path.
    """
    script = _write_temp_script(tmp_path, SCRIPT_CONTENT)
    fake_path = "/usr/local/bin/node"
    monkeypatch.setattr("shutil.which", lambda name: fake_path)
    fake_env = FakeEnv(results=[0, 0])

    module = _exec_script_with_env(str(script), fake_env, monkeypatch)

    assert module.node_ex == fake_path
    assert module.exitCode == 0

def test_build_failure_message_includes_docs_url_and_exit_code(monkeypatch, capsys, tmp_path):
    """
    On build failure, verify the failure message includes the documentation URL and that SystemExit uses the build exit code.
    """
    script = _write_temp_script(tmp_path, SCRIPT_CONTENT)
    monkeypatch.setattr("shutil.which", lambda name: "/usr/bin/node")
    fake_env = FakeEnv(results=[0, 2])

    try:
        _exec_script_with_env(str(script), fake_env, monkeypatch)
        raise AssertionError("Expected SystemExit when npm build fails")
    except SystemExit as e:
        assert e.code == 2

    out = capsys.readouterr().out
    assert "npm run build fails" in out
    assert "https://kno.wled.ge/advanced/compiling-wled/" in out

def test_empty_string_which_treated_as_present(monkeypatch, capsys, tmp_path):
    """
    Defensive edge: if shutil.which returns an empty string (falsy but not None), the script's 'is None' check
    should treat it as present and proceed with install/build.
    """
    script = _write_temp_script(tmp_path, SCRIPT_CONTENT)
    monkeypatch.setattr("shutil.which", lambda name: "")
    fake_env = FakeEnv(results=[0, 0])

    _exec_script_with_env(str(script), fake_env, monkeypatch)

    assert fake_env.calls == ["npm install", "npm run build"]
    out = capsys.readouterr().out
    assert "Installing node packages" in out