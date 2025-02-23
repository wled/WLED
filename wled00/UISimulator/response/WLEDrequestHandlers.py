import re
import os
import json
from response.requestHandler import RequestHandler
from bs4 import BeautifulSoup, Comment, Declaration

#TBD paths relative or not how they are specified etc.

class WLEDFileHandler(RequestHandler):
    def __init__(self, prefixUrl, method, options):
        super().__init__(prefixUrl, method, options),
        self.extensions = {
            ".htm":  {"ct": "text/html", "enc":lambda text: bytes(text,"UTF-8"), "mode":"r"},
            ".html": {"ct": "text/html", "enc":lambda text: bytes(text,"UTF-8"), "mode":"r"},
            ".css": {"ct": "text/css", "enc":lambda text: bytes(text,"UTF-8"), "mode":"r"},
            ".json": {"ct": "application/json", "enc":lambda text: bytes(text,"UTF-8"), "mode":"r"},
            ".js":   {"ct": "text/javascript", "enc":lambda text: bytes(text,"UTF-8"), "mode":"r"},
            ".ico":  {"ct": "image/vnd.microsoft.icon", "enc": lambda d: d, "mode":"rb"},
            ".icon":  {"ct": "image/vnd.microsoft.icon", "enc": lambda d: d, "mode":"rb"}
        }
        # { "root":folder, "file":relativePath}

    def can_handle(self, method, path):
        #print("can_handle",method,url,self.prefixUrl,self.method)
        if not path.startswith(self.prefixUrl): return False
        if self.method != "*" and self.method != method: return False
        return True


    def handle_path(self, method, parsedUrl, postData):
        if method == "GET":
            return self.handle_GET_path(parsedUrl)
        if method == "POST":
            return self.handle_POST_path(parsedUrl, postData)

    def handle_path(self, method, parsedUrl, postData):
        fpath = self.resolvePath(parsedUrl.path)
        if method == "HEADERS":
            return self.handle_GET_path(parsedUrl)
        if method == "GET":
            return self.handle_GET_path(parsedUrl)
        if method == "POST":
            return self.handle_POST_path(parsedUrl, postData)
        
        return self.error405(parsedUrl.path)


    def handle_GET_path(self, parsedUrl):
        path = parsedUrl.path
        if not self.can_handle("GET", path): return None

        fpath = self.resolvePath(path)
        return self.respondFile(fpath)

    def handle_POST_path(self, parsedUrl, postData):
        pass
    
    def resolvePath(self, path):
        if path == "/":
            return self.options["root"] + "index.htm"
        if "file" in self.options:
            return self.options["file"] # directory???
        elif "root" in self.options:
            return path.replace(self.prefixUrl,self.options["root"],1)
        elif "mapper" in self.options:
            return self.options["mapper"](path)

    def getResponseSpec(self, fpath):
        extension = os.path.splitext(fpath)[1]
        if extension in self.extensions:
            return self.extensions[extension]
        else:
            return {"ct":None, "enc":lambda x: x, "mode":"r"}

    def respondFile(self, fpath):
        spec = self.getResponseSpec(fpath)
        try:
            content = open(fpath, spec["mode"]).read()
            if("postFilter" in self.options):
                postFilters = self.options["postFilter"]
                for i in range(0, len(postFilters)):
                    filter = postFilters[i]
                    print("Checking processor ",i)
                    if filter["condition"](fpath):
                        print("Processor being invoked")
                        content = filter["processor"](content,filter["args"])
            content = spec["enc"](content)
            return {"resolved":fpath, "status":200, "content-type":spec["ct"], "content":content}
        except Exception as err:
            print("Exception for path {}: {}".format(fpath, err), spec, self.options)
            return {"resolved":fpath, "status":500, "content-type":"text/plain", "content":bytes("500 Internal Server Error","UTF-8")}

def isHtml(fpath):
    if os.path.splitext(fpath)[1].lower() in (".html",".htm"):
        return True
    return False

def Patcher_I18N_Script(text, options):
    print("Patcher_I18N_Script", len(text), options)
    soup = BeautifulSoup(text, features='html.parser')
    print("soup created")

    if "replaceScript" in options:
        # replace old with new script
        found = None
        for script in soup.find_all('script'):
            if script.string is not None: continue # inline script
            if script.get('src') == options["replaceScript"]["old"]:
                found = script
        
        if found:
            found['src'] = options["replaceScript"]["new"]
            print("replaced", found)
        else:
            #add script
            print("inserting")
            head = soup.find('head')
            if head == None:
                head = soup.new_tag("head")
                soup.find("html").insert(0,head)
            script = soup.new_tag("script")
            script["src"] = options["replaceScript"]["new"]
            print("inserted", script)
            head.append(script)

    # insert the runI18N call after body load.  Allow for an existing @onload, even if it fails.
    body = soup.find("body")
    if body.has_attr("onload"):
        curr = body["onload"]
        if -1 == curr.rfind("runI18N();"):
            body["onload"] = "try {{ {} }} finally {{ {} }}".format(curr,"runI18N();")
    else:
        body["onload"] = "runI18N();"

    return str(soup)


class PagePOSTHandler(WLEDFileHandler):
    def handle_POST_path(self, parsedUrl, postData):
        dictionaryFormat = False

        path = parsedUrl.path
        if not self.can_handle("POST", path): return None

        fpath = self.resolvePath(path)
        print("PagePOSTHandler", path, self.options, fpath)
  
        sData = postData
        
        if os.path.exists(fpath):
            with open(fpath, "r") as jsonfile:
                data = json.load(jsonfile)
        else:
            data = {} if dictionaryFormat else []

        additions = json.loads(sData)
        count = 0
        for i in range(0,len(additions)):
            addition = additions[i]
            if dictionaryFormat:
                key = addition["value"] if "attr" in addition else addition["content"]
                if not key in data:
                    data[key] = addition
                    count += 1
            else:
                #TODO Check if already exists in data (text and index if provided)
                print(addition)
                if not any(filter(lambda o: o["content"] == addition["content"]
                                            and (
                                                not("path" in addition and "path" in o)
                                                or o["path"] == addition["path"]
                                            ),
                                  data)):
                    data.append(addition)
                    count += 1

        message = ("total {}: posted {}, new {}".format(len(data), len(additions), count))
        print("POST: {} {}".format(fpath, message))
        with open(fpath, "w") as jsonfile:
            json.dump(data, jsonfile, indent=4)

        return {"resolved":fpath, "status":201, "content-type":"text/plain", "content":bytes("201 Created\n" + message,"UTF-8")}

# For showToast and similar, we ignore duplicates
class SinglePOSTHandler(WLEDFileHandler):
    def handle_POST_path(self, parsedUrl, postData):
        path = parsedUrl.path
        if not self.can_handle("POST", path): return None

        fpath = self.resolvePath(path)
        print("SinglePOSTHandler", path, self.options, fpath)
  
        sData = postData

        if os.path.exists(fpath):
            with open(fpath, "r") as jsonfile:
                database = json.load(jsonfile)
        else:
            database = []

        addition = json.loads(sData)
        # Ignore duplicates (text+page)
        if not any(filter(lambda o: o["content"] == addition["content"] and o["page"] == addition["page"], database)):
            database.append(addition)

        message = ("total {}: added".format(len(database)))
        print("POST: {} {}".format(fpath, message))
        with open(fpath, "w") as jsonfile:
            json.dump(database, jsonfile, indent=4)

        return {"resolved":fpath, "status":201, "content-type":"text/plain", "content":bytes("201 Created\n" + message,"UTF-8")}

class SI_POSTHandler(WLEDFileHandler):
    # ignore and just return the existing si.json file
    def handle_POST_path(self, parsedUrl, postData):
        print("SI_POSTHandler", postData)
        path = parsedUrl.path
        if not self.can_handle("POST", path): return None

        fpath = self.resolvePath(path)
        return self.respondFile(fpath)
 
class FunctionHandler(RequestHandler):
    def __init__(self, prefixUrl, method, options):
        super().__init__(prefixUrl, method, options),
        self.extensions = {
            ".htm":  {"ct": "text/html", "enc":lambda text: bytes(text,"UTF-8"), "mode":"r"},
            ".html": {"ct": "text/html", "enc":lambda text: bytes(text,"UTF-8"), "mode":"r"},
            ".css": {"ct": "text/css", "enc":lambda text: bytes(text,"UTF-8"), "mode":"r"},
            ".json": {"ct": "application/json", "enc":lambda text: bytes(text,"UTF-8"), "mode":"r"},
            ".js":   {"ct": "text/javascript", "enc":lambda text: bytes(text,"UTF-8"), "mode":"r"},
            ".ico":  {"ct": "image/vnd.microsoft.icon", "enc": lambda d: d, "mode":"rb"},
            ".icon":  {"ct": "image/vnd.microsoft.icon", "enc": lambda d: d, "mode":"rb"}
        }

    def can_handle(self, method, path):
        #print("can_handle",method,url,self.prefixUrl,self.method)
        if not path.startswith(self.prefixUrl): return False
        if self.method != "*" and self.method != method: return False
        return True
    
    def handle_path(self, method, parsedUrl, postData):
        result = self.options["function"](parsedUrl, method, self.options, postData)
        return result

def GetAvailableLangs(**args):
    result = []
    # get the langs from iso639.json
    # filter by existence in I18N/langs
    #return result 

