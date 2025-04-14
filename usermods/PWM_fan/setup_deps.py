Import('env')


usermods = env.GetProjectOption("custom_usermods","").split()
# Check for dependencies
if "Temperature" in usermods:
    env.Append(CPPDEFINES=[("USERMOD_DALLASTEMPERATURE")])
elif "sht" in usermods:
    env.Append(CPPDEFINES=[("USERMOD_SHT")])
else:    
    raise RuntimeError("PWM_fan usermod requires Temperature or sht to be enabled")

