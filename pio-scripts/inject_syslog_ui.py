# pio-scripts/inject_syslog_ui.py

"""
PlatformIO build script to conditionally inject Syslog UI elements into the settings HTML file.

This script:
1. Injects Syslog UI elements when WLED_ENABLE_SYSLOG is defined in build flags
2. Restores the original HTML file after build completion
3. Tracks state between builds to force UI rebuilds when necessary
"""

import os, re, shutil
from SCons.Script import Import

Import("env")

# detect full vs. partial compile
is_full_build = env.get("PIOENV") is not None

# Track the state between builds
def get_previous_syslog_state(project_dir):
    state_file = os.path.join(project_dir, "wled00/data/.syslog_state")
    if os.path.exists(state_file):
        with open(state_file, 'r') as f:
            return f.read().strip() == "1"
    return None  # None means no previous state recorded

def set_syslog_state(project_dir, enabled):
    state_file = os.path.join(project_dir, "wled00/data/.syslog_state")
    with open(state_file, 'w') as f:
        f.write("1" if enabled else "0")

# This is the HTML we want to inject
SYSLOG_HTML = """
<h3>Syslog</h3>
<div id="Syslog">
  Enable Syslog: <input type="checkbox" name="SL_en"><br>
  Host: <input type="text" name="SL_host" maxlength="32"><br>
  Port: <input type="number" name="SL_port" min="1" max="65535" value="%SL_port%"><br>

  <!-- These UI elements are commented out but preserved for potential future use -->
  <!--
  Protocol:
  <select name="SL_proto">
    <option value="0">BSD (RFC3164)</option>
    <option value="1">RFC5424</option>
    <option value="2">Raw</option>
  </select><br>
  Facility:
  <select name="SL_fac">
    <option value="0">KERN</option>
    <option value="1">USER</option>
    <option value="3">DAEMON</option>
    <option value="5">SYSLOG</option>
    <option value="16">LOCAL0</option>
    <option value="17">LOCAL1</option>
    <option value="18">LOCAL2</option>
    <option value="19">LOCAL3</option>
    <option value="20">LOCAL4</option>
    <option value="21">LOCAL5</option>
    <option value="22">LOCAL6</option>
    <option value="23">LOCAL7</option>
  </select><br>
  Severity:
  <select name="SL_sev">
    <option value="0">EMERG</option>
    <option value="1">ALERT</option>
    <option value="2">CRIT</option>
    <option value="3">ERR</option>
    <option value="4">WARNING</option>
    <option value="5">NOTICE</option>
    <option value="6">INFO</option>
    <option value="7">DEBUG</option>
  </select><br>
  -->
</div>
"""

def inject_syslog_ui(source, target, env, retry_count=0):
    print("\033[44m==== inject_syslog_ui.py (PRE BUILD) ====\033[0m")
    if not is_full_build:
        print("\033[43mNot a full build, skipping Syslog UI operations.\033[0m")
        return

    # Check for the define in BUILD_FLAGS
    build_flags = env.get("BUILD_FLAGS", "")
    if isinstance(build_flags, list):
        build_flags = " ".join(build_flags)
    has_syslog = bool(re.search(r'-D\s*WLED_ENABLE_SYSLOG', build_flags))

    project_dir = env.subst("$PROJECT_DIR")
    html_path = os.path.join(project_dir, "wled00/data/settings_sync.htm")
    bak = html_path + ".backup"

    # Detect state change → touch to force rebuild
    prev = get_previous_syslog_state(project_dir)
    if prev is not None and prev != has_syslog:
        print(f"\033[43mSYSLOG state changed from {prev} to {has_syslog}, forcing UI rebuild.\033[0m")
        if os.path.exists(html_path):
            with open(html_path, 'a'):
                os.utime(html_path, None)

    set_syslog_state(project_dir, has_syslog)

    if not has_syslog:
        print("\033[43mWLED_ENABLE_SYSLOG not defined, skipping injection.\033[0m")
        # restore if backup exists
        if os.path.exists(bak):
            print("Restoring original file from backup...")
            shutil.copy2(bak, html_path)
            os.remove(bak)
        return

    # backup + inject only once
    if not os.path.exists(bak):
        print("Backing up and injecting Syslog UI...")
        shutil.copyfile(html_path, bak)
        try:
            with open(html_path, 'r', encoding='utf8') as f:
                original = f.read()
            modified = original

            # replace existing section if present
            if '<!-- SYSLOG-START -->' in modified and '<!-- SYSLOG-END -->' in modified:
                start = modified.index('<!-- SYSLOG-START -->')
                end   = modified.index('<!-- SYSLOG-END -->') + len('<!-- SYSLOG-END -->')
                modified = (
                    modified[:start]
                    + '<!-- SYSLOG-START -->\n' + SYSLOG_HTML + '\n<!-- SYSLOG-END -->'
                    + modified[end:]
                )
            else:
                # insert before last <hr>
                idx = modified.rfind('<hr>')
                if idx == -1:
                    print("\033[41mCould not find <hr> to insert Syslog UI!\033[0m")
                    return
                modified = (
                    modified[:idx]
                    + '<!-- SYSLOG-START -->\n' + SYSLOG_HTML + '\n<!-- SYSLOG-END -->\n'
                    + modified[idx:]
                )

            with open(html_path, 'w', encoding='utf8') as f:
                f.write(modified)
            print("\033[42mSyslog UI injected successfully!\033[0m")
            
        except (IOError, OSError) as e:
            print(f"\033[41mFile operation error during injection: {e}\033[0m")
            # injection failed → remove backup so we'll retry next time
            if os.path.exists(bak):
                os.remove(bak)
        except Exception as e:
            print(f"\033[41mUnexpected error during injection: {e}\033[0m")
            # injection failed → remove backup so we’ll retry next time
            if os.path.exists(bak):
                os.remove(bak)
    else:
        print("Backup exists; assume already injected.")
        # verify that SYSLOG markers really are in the file
        with open(html_path, 'r', encoding='utf8') as f:
            content = f.read()
        if '<!-- SYSLOG-START -->' not in content or '<!-- SYSLOG-END -->' not in content:
            print("Backup exists but SYSLOG markers missing—forcing re-injection.")
            os.remove(bak)
            # only retry up to 3 times
            if retry_count < 3:
                # Add a small delay before retrying
                import time
                time.sleep(0.5 * (retry_count + 1))  # Increasing delay with each retry
                inject_syslog_ui(source, target, env, retry_count + 1)
            else:
                print("\033[41mToo many retry attempts. Manual intervention required.\033[0m")
        else:
            print("Backup exists and markers found; already injected.")

def restore_syslog_ui(source, target, env):
    print("\033[44m==== inject_syslog_ui.py (POST BUILD) ====\033[0m")
    project_dir = env.subst("$PROJECT_DIR")
    html_path = os.path.join(project_dir, "wled00/data/settings_sync.htm")
    bak = html_path + ".backup"

    # restore only if backup file is present
    if os.path.exists(bak):
        print("Restoring original file from backup...")
        if os.path.exists(html_path):
            os.chmod(html_path, 0o644)
        shutil.copy2(bak, html_path)
        os.remove(bak)

# always register the post-action on checkprogsize so it runs every build
env.AddPostAction("checkprogsize", restore_syslog_ui)

# only inject on full build
if is_full_build:
    inject_syslog_ui(None, None, env)
