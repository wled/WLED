from platformio.package.meta import PackageSpec
Import('env')

libs = [PackageSpec(lib).name for lib in env.GetProjectOption("lib_deps",[])]
# Check for partner usermod
# Allow both "usermod_v2" and unqualified syntax
if any(mod in ("four_line_display_ALT", "usermod_v2_four_line_display_ALT") for mod in libs):
    env.Append(CPPDEFINES=[("USERMOD_FOUR_LINE_DISPLAY")])
