Import('env')


usermods = env.GetProjectOption("custom_usermods","").split()
# Check for dependencies
if "Temperature" in usermods:
    env.Append(CPPDEFINES=[("USERMOD_DALLASTEMPERATURE")])
elif "sht" in usermods:
    env.Append(CPPDEFINES=[("USERMOD_SHT")])
elif "PWM_fan" in usermods:  # The script can be run if this module was previously selected
    raise RuntimeError("PWM_fan usermod requires Temperature or sht to be enabled")
