Import('env')
import json

PACKAGE_FILE = "package.json"

with open(PACKAGE_FILE, "r") as package:
    version = json.load(package)["version"]
    env.Append(BUILD_FLAGS=[f"-DWLED_VERSION={version}"])
