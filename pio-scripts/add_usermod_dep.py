# Attempt to fix incorrect SCons cache lookups
# The build system doesn't seem to always pick up cases where the dependency list itself has
# changed, and can incorrectly return a cached build result with the wrong set of usermods
# linked in.
# We put the usermod list in a file which can be listed as dep for the final link,
# ensuring that it will always link correctly based on the hashes.

Import('env')
from pathlib import Path

# Write out the usermod list to a text file
lib_path = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV")
usermods = env.GetProjectOption("custom_usermods","")
usermod_file = Path(lib_path) / "usermod_list.txt"

with usermod_file.open("a+", encoding="utf-8") as um_file:
  um_file.seek(0)
  old_ums = um_file.readline()
  if old_ums != usermods:
    um_file.truncate(0)
    um_file.seek(0)
    um_file.write(usermods)

# Add a dependency on this file
env.Depends(env.subst("$PROGPATH"), str(usermod_file))
