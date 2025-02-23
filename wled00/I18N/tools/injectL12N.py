import sys
import shutil
import os
from pathlib import Path
import re
import json
import hashlib
import argparse
from HTMLPatchingParser import HTMLPatchingParser
from collections import deque

parser = argparse.ArgumentParser(
                    prog='Inject Localization script and hooks',
                    description='Patches HTML files to include localization script'
)

parser.add_argument('-ns', '--noscan', help='Use list.json instead of scanning wled00/data',
                    action='store_true')  # on/off flag
parser.add_argument('-v', '--verbose',
                    action='store_true')  # on/off flag

args = parser.parse_args()

class MyHTMLParser(HTMLPatchingParser):
    def __init__(self, options):
        super().__init__()
        self.foundL12N = False
        self.options = options

    def handle_starttag(self, tag, attrs):
        dAttrs = dict((key, {"value":value, "from":r[0], "to":r[1]}) for key, value, r in attrs)
        # Replace <script src=.../>
        if tag == "script":
            if not "src" in dAttrs: return
            src = dAttrs["src"] 
            if src["value"] == self.options["oldScript"]:
                self.addChange({"action":    "replace",
                                        "from": src["from"],
                                        "to":   src["to"],
                                        "text": "src='{}'".format(self.options["addScript"])
                                    })
                self.foundL12N = True
                return
            if dAttrs["src"]["value"] == self.options["addScript"]:
                self.foundL12N = True
                return
        
        # create/append to onload handler
        if tag == "body":
            (line,offset) = self.getpos()
            length = len(self.get_starttag_text())
            if "onload" in dAttrs:
                onload = dAttrs["onload"] 
                value = onload["value"]
                if -1 == value.find("runI18N()"):
                    newValue = "onload='try {{ {} }} finally {{ runI18N(); }}'".format(value)
                    self.addChange({"action":    "replace",
                                         "from": onload["from"],
                                         "to":   onload["to"],
                                         "text": newValue
                                        })
            else:
                self.addChange({"action":"insert",
                                "text":  " onload='runI18N();' ",
                                "at":   self.getOffset() + len(tag) + 1})

    def handle_endtag(self, tag):
        if tag == "head" and self.foundL12N == False:
            self.addChange({"action":"insert",
                                "text":"<script src='{}'></script>".format(self.options["addScript"]),
                                "at":self.getOffset()
                            })

if args.noscan:
    with open('wled00/I18N/data/list.json', 'r') as file:
        listjson = json.load(file)
        sourceFileList = filter(lambda fn: os.path.splitext(fn)[1] in ('.htm','.html'), listjson)
else:
    sourceFileList = list(Path("wled00/data/").rglob("*.htm*"))
    print(sourceFileList)

for fn in sourceFileList:
    print("Examining",fn)

    htmlPatcher = MyHTMLParser({"addScript": "/L12N.js", "oldScript":"L12N.js"})

    with open(fn, "r") as f:
        text = f.read()

    htmlPatcher.feed(text)
    mtext = htmlPatcher.applyChanges()

    # Write patched version
    with open(fn, mode="w") as f:
        f.write(mtext)

# the end