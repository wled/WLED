Import('env')


usermods = env.GetProjectOption("custom_usermods","").split()
# Check for partner usermods
if "SN_Photoresistor" in usermods:
    env.Append(CPPDEFINES=[("USERMOD_SN_PHOTORESISTOR")])
if any(mod in ("BH1750_v2", "BH1750") for mod in usermods):    
    env.Append(CPPDEFINES=[("USERMOD_BH1750")])
