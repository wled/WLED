import os
import subprocess
from pathlib import Path

try:
  Import("env")  # type: ignore  # pylint: disable=undefined-variable
except NameError:
  env = None  # pylint: disable=invalid-name


def append_define(target_env, define):
  if not target_env:
    return
  try:
    target_env.Append(CPPDEFINES=[define])
  except Exception as exc:  # pylint: disable=broad-except
    print("DepartStrip: failed to append define: {}".format(exc))


def get_project_dir():
  if env:
    return env.get("PROJECT_DIR", ".")
  current_dir = Path(__file__).resolve().parent
  for candidate in [current_dir] + list(current_dir.parents):
    if (candidate / "platformio.ini").is_file() or (candidate / ".git").is_dir():
      return str(candidate)
  return os.getcwd()


def get_git_describe(project_dir):
  try:
    cmd = ["git", "describe", "--tags", "--long", "--dirty", "--always"]
    output = subprocess.check_output(cmd, cwd=project_dir, stderr=subprocess.STDOUT)
    version = output.decode("utf-8", errors="ignore").strip()
    if version:
      return version
  except Exception as exc:  # pylint: disable=broad-except
    print("DepartStrip: git describe failed: {}".format(exc))
  return "unknown"


project_dir = get_project_dir()
git_version = get_git_describe(project_dir)
escaped = git_version.replace('"', '\\"')
define = ("DEPARTSTRIP_GIT_DESCRIBE", '\\"{}\\"'.format(escaped))

if env and hasattr(env, "Append"):
  append_define(env, define)
  modules = env.get("WLED_MODULES") or []
  for dep in modules:
    append_define(getattr(dep, "env", None), define)
elif __name__ == "__main__":
  print(git_version)
