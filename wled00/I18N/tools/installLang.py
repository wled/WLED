# Optimizes content of I18N/L12N/<lang>.json for use by browser
import argparse
import json
import itertools
import os

parser = argparse.ArgumentParser(
                    prog='L12N',
                    description='Optimizes and installs L12N/<lang>.json'
)

def LangFile(la):
    if(len(la) != 2):
        raise argparse.ArgumentTypeError("Expecting 2 letter code!")
    if not os.path.exists("wled00/I18N/L12N/{}.json".format(la)):
        raise argparse.ArgumentTypeError("L12N/{}.json doesn't exist".format(la))
    return la

parser.add_argument('-l', '--lang', help='Two letter language code', type=LangFile, nargs='+', required=True)      # option that takes a value
parser.add_argument('-v', '--verbose',
                    action='store_true')  # on/off flag

args = parser.parse_args()

for target_lang in args.lang:
    with open("wled00/I18N/L12N/{}.json".format(target_lang), "r") as f:
        L12N = json.load(f)

    exacts = {}
    patterns = {}
    for key in L12N["exact"]:
        entry = L12N["exact"][key]
        translationEntries = entry["translations"]
        if len(translationEntries) == 1:
            exacts[key] = translationEntries[0]["translation"]
        else:
            # flatten it out to {"t":translation,"fn":fn, "line":l}
            # group by fn.
            # get the translations in order for that fn
            flat = []
            for t in translationEntries:    # {'translation': 'tran1', 'for': {'f1': [3, 1], 'f2': [1, 4]}}
                #print(t)
                for fn in t["for"]: # 'f1'
                    #print(fn)
                    fnE = t["for"][fn] # [3, 1]
                    for line in t["for"][fn]:
                        flat.append({"t": t["translation"], "fn":fn, "line":line})
            
            overrides = {}
            flat = sorted(flat, key=lambda fe: fe["fn"])
            for fn, fnG in itertools.groupby(flat, key=lambda fe: fe["fn"]):
                # Now we need to sort the translations by line
                sortedTrans = sorted(list(fnG), key=lambda fe: fe["line"])
                sortedTranslations = list(map(lambda fe: fe["t"], sortedTrans))
                if len(set(sortedTranslations)) == 1:
                    overrides[fn] = sortedTranslations[0]
                else:
                    overrides[fn] = sortedTranslations
           
            exacts[key] = overrides
    for key in L12N["pattern"]:
        #patterns[key] = ...
        pass#TODO

    optimized = {"exact":exacts, "pattern":patterns}

    with open("wled00/I18N/langs/{0}.json".format(target_lang), "w", encoding="utf-8") as f:
        json.dump(optimized, f, indent=4, ensure_ascii=False)

# --- build the langs.json table
with open("wled00/I18N/data/iso639.json", "r") as f:
    iso639 = json.load(f)

langs = []
for fn in os.listdir("wled00/I18N/langs"):
    langCode = os.path.splitext(fn)[0]
    if(langCode in iso639):
        langs.append({"lc":langCode, "t":iso639[langCode]})
langs = sorted(langs, key=lambda e: e["t"])

with open("wled00/I18N/data/langs.json".format(target_lang), "w", encoding="utf-8") as f:
    json.dump(langs, f)

