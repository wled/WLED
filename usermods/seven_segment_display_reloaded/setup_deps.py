from platformio.package.meta import PackageSpec
Import('env')


libs = [PackageSpec(lib).name for lib in env.GetProjectOption("lib_deps",[])]
# Check for partner usermods
if "SN_Photoresistor" in libs:
    env.Append(CPPDEFINES=[("USERMOD_SN_PHOTORESISTOR")])
if any(mod in ("BH1750_v2", "BH1750") for mod in libs):    
    env.Append(CPPDEFINES=[("USERMOD_BH1750")])
