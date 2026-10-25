# FastLED exports some weak cxx symbols as a way of managing integration with C-only
# projects that cause it to be preferentially linked instead of unused code being 
# discarded like other libraries.  This causes not only bloat of the final binaries
# but can incorrectly invoke some of their driver framework on some platforms.
#
# Solve this problem by moving the cxx library up in the linker command line, so
# that it will be chosen over FastLED's.
Import("env")

if "-lcxx" in env["LIBS"]:
  env["LIBS"].remove("-lcxx")
  env["LIBS"].insert(0, "-lcxx")
