Import('env')
import glob
import os

if not os.path.isdir('node_modules'):
  env.Execute("npm ci")

if not os.path.exists('wled00/html_ui.h'):
  env.Execute("npm run build")

latest_source = max(glob.glob('wled00/data/*.htm'), key=os.path.getmtime)
latest_export = max(glob.glob('wled00/html_*.h'), key=os.path.getmtime)

if os.path.getmtime(latest_source) > os.path.getmtime(latest_export):
  env.Execute("npm run build")
