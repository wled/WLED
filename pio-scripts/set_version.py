Import('env')
import json
import datetime
t = datetime.datetime.now()
env.Append(BUILD_FLAGS=[f"-D BUILD={t.strftime('%y%m%d')}0"])

PACKAGE_FILE = "package.json"

with open(PACKAGE_FILE, "r") as package:
    version = json.load(package)["version"]
    env.Append(BUILD_FLAGS=[f"-DWLED_VERSION={version}"])
