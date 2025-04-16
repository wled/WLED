Import('env')


usermods = env.GetProjectOption("custom_usermods","").split()
# Check for partner usermod
# Allow both "usermod_v2" and unqualified syntax
if any(mod in ("four_line_display_ALT", "usermod_v2_four_line_display_ALT") for mod in usermods):
    env.Append(CPPDEFINES=[("USERMOD_FOUR_LINE_DISPLAY")])
