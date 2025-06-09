from platformio.package.meta import PackageSpec
Import('env')


libs = [PackageSpec(lib).name for lib in env.GetProjectOption("lib_deps",[])]
# Check for dependencies
if "Temperature" in libs:
    env.Append(CPPDEFINES=[("USERMOD_DALLASTEMPERATURE")])
elif "sht" in libs:
    env.Append(CPPDEFINES=[("USERMOD_SHT")])
elif "PWM_fan" in libs:  # The script can be run if this module was previously selected
    raise RuntimeError("PWM_fan usermod requires Temperature or sht to be enabled")
