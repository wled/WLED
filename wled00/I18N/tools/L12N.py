import argparse

import os
import json
from googletrans import Translator
import itertools
import re

parser = argparse.ArgumentParser(
                    prog='L12N',
                    description='Creates translated phrase file for specified language from data.json'
)

def LangFile(la):
    if(len(la) != 2):
        raise argparse.ArgumentTypeError("Expecting 2 letter code!")
    return la

parser.add_argument('-l', '--lang', help='Two letter language code', type=LangFile, nargs='+', required=True)      # option that takes a value
parser.add_argument('-dp', help="Input folder containing screen-scraped I18N data", default="wled00/I18N/data/fromPages")
parser.add_argument('-dj', help="Input folder containing cached JSON data", default="wled00/I18N/data/cachedFromWLED")
parser.add_argument('-ds', help="Input folder containing cached script data", default="wled00/I18N/data/fromScripts")
parser.add_argument('-v', '--verbose',
                    action='store_true')  # on/off flag

args = parser.parse_args()

pageData = [f for f in os.listdir(args.dp) if os.path.isfile(os.path.join(args.dp, f))]
jsonData = [f for f in os.listdir(args.dj) if os.path.isfile(os.path.join(args.dj, f))]
scriptData = [f for f in os.listdir(args.ds) if os.path.isfile(os.path.join(args.ds, f))]

if(args.verbose): print(args, pageData, jsonData, scriptData)

def handleJsonFile(folder, baseName):
    # How to handle each of the cached JSON files
    jsonHandlers = {
        'cfg.json': noopHandler,
        'effects.json': simpleListHandler,
        'fxdata.json': fxLabelsHandler,
        'nodes.json': noopHandler,
        'palettes.json': simpleListHandler,
        'palx.json': noopHandler,
        'presets.json': noopHandler,
        'si.json': noopHandler      # for now.  Confirm after testing
    }
    if baseName in jsonHandlers:
        jsonHandlers[baseName](os.path.join(args.dj,baseName), baseName)

translator = Translator()

def cookedKey(item):
    if "pattern" in item:
        return "2_{}".format(item["pattern"])
    else:
        return "1_{}".format(item["key"])   # We'll group on the pattern alone

def extractText(text, regexp):
    m = regexp.match(text)
    if m == None:
        print("Mismatch item {} '{}'".format(hash,text))
        return text
    else:
        return (m.group(1), m.group(2), m.group(3))


#---- pull pageData 

# Regular expression to strip off whitespace and icon characters from left and right of string
surroundWS = re.compile("(?s)^([^a-zA-Z0-9]*+)(.*[a-zA-Z0-9])([^a-zA-Z0-9]*)$")

flat = []
max_codepoint = 0
for fn in pageData:
    print("Processing page data",fn)
    with open(os.path.join(args.dp,fn),"r") as f:
        entries = json.load(f) # [...]
    index = 0
    for spec in entries:   # {content} OR {attr,content}
        #print("entry",spec)
        if "attr" in spec:
            type = spec["attr"]
            text = spec["content"]
        else:
            type = "content"
            text = spec["content"]

        key = text
        mc = ord(max(text))
        if(mc > max_codepoint): max_codepoint = mc
        index += 1
        sIndex = spec["path"] if "path" in spec else index
        parsedText = extractText(text,surroundWS)
        text = text if isinstance(parsedText,str) else parsedText[1]

        if not "pattern" in spec:
            entry = {"key":key, "fn":fn,
                     "text": text, "parsedText":parsedText, "type": type,
                     "index": sIndex}
        else:
            pattern = spec["pattern"]
            # Placeholder translation setup for patterns
            # parseText is the (first) alphabetic part of the pattern + a suffix of the parameters
            m = re.compile("[a-zA-Z]+").search(pattern)
            if m is None:
                parsedText = "?TBD?"
            else:
                parsedText = ["",m.group(0),""]
                rePattern = re.compile(pattern)
                m2 = rePattern.search(text)
                if m2:
                    parmCnt = len(m2.groups())
                    parms = " ".join(["$" + str(i) for i in range(1,parmCnt+1)])
                    parsedText[2] = " " + parms
            
            entry = {"key":key, "fn":fn,
                     "text": text, "parsedText":parsedText, "type": type,
                     "pattern": pattern,
                     "index": sIndex}

        flat.append(entry)

#---- pull jsonData

# JSON cached data handling
def noopHandler(path,fn):
    if args.verbose: print("noopHandler",path,fn)

def simpleListHandler(path,fn):
    if args.verbose: print("simpleListHandler",path,fn)

    with open(path, "r") as f:
        entries = json.load(f)

    index = 1
    for entry in entries:
        flat.append({"key":text, "fn":fn, "text": text, "parsedText":text, "type": type, "index": index})
        index += 1

def fxLabelsHandler(path,fn):
    # the localization script will search for ,label, and replace with translation
    if args.verbose: print("fxLabelsHandler",path,fn)

    reLabel = re.compile(r",([a-zA-Z0-9, ]+)[;]")
    
    with open(path, "r") as f:
        entryList = json.load(f)    # its a list

    index = 1
    for entry in entryList:
        m = reLabel.search(entry)
        #print("{}: {} => {}".format(index,entry,m.group(1) if not m is None else "None"))
        if not m is None:
            labels = m.group(1)
            for label in labels.split(","):
                if len(label) > 0:
                    #print(label)
                    flat.append({"key":label, "fn":fn, "text": label, "parsedText":label, "type": type, "index": index})
        index +=1

for fn in jsonData:
    print("Processing cached data",fn)
    handleJsonFile(args.dj, fn)

#---- pull scriptData
for fn in scriptData:
    print("Processing script data",fn)
    with open(os.path.join(args.ds,fn),"r") as f:
        entries = json.load(f) # [...]
        #{
        #"content": "Unable to load translation file",
        #"page": "settings_ui.htm"
        #}
    index = 0
    for entry in entries:
        text = entry["content"]
        entry = {"key":text, "fn":fn,
                 "text": text, "parsedText":text, "type": "content",
                "index": index}
        index += 1
        flat.append(entry)

#---- Consolidate all the data
# Step 1: Sort by common key
flat1 = sorted(flat,key=cookedKey)
if args.verbose:
    with open("wled00/I18N/L12N/flat.json", "w", encoding="utf-8") as f:
        json.dump(flat1, f, indent=4, ensure_ascii=False)
#exit(0) #TEMP

# Step 2: group and translate
def translate(target_lang):
    print("TRANSLATING INTO {}".format(target_lang))
    exacts = {}; patterns = {}
    cooked = {"exact":exacts, "pattern":patterns}
    cnt = 0

    fn = "wled00/I18N/L12N/{0}.json".format(target_lang)
    if os.path.exists(fn):
        with open("wled00/I18N/L12N/{0}.json".format(target_lang), "r", encoding="utf-8") as f:
            database = json.load(f)
            exacts = database["exact"]
            patterns = database["pattern"]
    else:
        database = {"exact":exacts, "pattern":patterns}
        exacts = {}; patterns = {}

    for sortKey, instanceG in itertools.groupby(flat1, key=cookedKey): # 
        instanceL = list(instanceG)
        # The following must all be reflected in the sortKey (even if implicitly) so that we can take them from any instance
        key = instanceL[0]["key"]
        parsedText = instanceL[0]["parsedText"]
        pattern = instanceL[0]["pattern"] if "pattern" in instanceL[0] else None

        if target_lang == None:
            translation = "<TBD>"
        else:
            if isinstance(parsedText,str):
                translation = translator.translate(parsedText, sr="en", dest=target_lang).text
            else:
                #print(parsedText)
                translation = parsedText[0] + translator.translate(parsedText[1], sr="en", dest=target_lang).text + parsedText[2]

        #{key:{"text":text, "translation":{translation:{fn:[sourceLine ...], ...}]}
        fnEntries = {}
        for fn, fng in itertools.groupby(instanceL, key=lambda x: x["fn"]):
            fnL = list(fng)
            #fnEntries[fn] = list(map(lambda x: int(x["index"]), fnL))
            fnEntries[fn] = list(map(lambda x: x["index"], fnL))
            #print(text, fn, list(map(lambda x: int(x["index"]), fng)))
        
        #WRONG!!  What about the multiple labels generated from fxdata?
        #         Separate entries?
        entry = {"text": key, "translations": [{"translation":translation, "for":fnEntries}]}
        if pattern:
            entry["pattern"] = pattern
            if key not in patterns:     #TODO Consider merge
                patterns[key] = entry
        else:
            if key not in exacts:       #TODO Consider merge
                exacts[key] = entry
        cnt = cnt + 1
        if(cnt % 50) == 0: print(cnt)

    with open("wled00/I18N/L12N/{0}.json".format(target_lang), "w", encoding="utf-8") as f:
        json.dump(database, f, indent=4, ensure_ascii=False)

print("Max unicode codepoint u{0:04x}".format(max_codepoint))

for target_lang in args.lang:
    translate(target_lang)