Import("env")
import shutil
import os

node_ex = shutil.which("node")
# Check if Node.js is installed and present in PATH if it failed, abort the build
if node_ex is None:
    print('\x1b[0;31;43m' + 'Node.js is not installed or missing from PATH html css js will not be processed check https://kno.wled.ge/advanced/compiling-wled/' + '\x1b[0m')
    exitCode = env.Execute("null")
    exit(exitCode)
else:
    # Install the necessary node packages for the pre-build asset bundling script
    print('\x1b[6;33;42m' + 'Installing node packages' + '\x1b[0m')
    env.Execute("npm ci")

    # Extract the release name from build flags
    release_name = "Custom"
    build_flags = env.get("BUILD_FLAGS", [])
    for flag in build_flags:
        if 'WLED_RELEASE_NAME=' in flag:
            # Extract the release name, remove quotes and handle different formats
            parts = flag.split('WLED_RELEASE_NAME=')
            if len(parts) > 1:
                release_name = parts[1].split()[0].strip('\"\\')
                break
    
    # Set environment variable for cdata.js to use
    os.environ['WLED_RELEASE_NAME'] = release_name
    print(f'Building web UI with release name: {release_name}')

    # Call the bundling script
    exitCode = env.Execute("npm run build")

    # If it failed, abort the build
    if (exitCode):
      print('\x1b[0;31;43m' + 'npm run build fails check https://kno.wled.ge/advanced/compiling-wled/' + '\x1b[0m')
      exit(exitCode)
