Import('env')

for lb in env.GetLibBuilders():
    if lb.name == "arduinoFFT":
        lb.env.Append(CPPDEFINES=[("sqrt_internal", "sqrtf")])
