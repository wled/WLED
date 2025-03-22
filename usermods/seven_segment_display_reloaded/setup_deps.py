Import('env')


usermods = env.GetProjectOption("custom_usermods","").split()
# Check for partner usermods
if "SN_Photoresistor" in usermods:
    env.Append(CPPDEFINES=[("USERMOD_SN_PHOTORESISTOR")])
if "BH1750_v2" in usermods:
    env.Append(CPPDEFINES=[("USERMOD_BH1750")])
