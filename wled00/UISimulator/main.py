#!/usr/bin/env python3
import time
from http.server import HTTPServer
from server import WLEDServer, WLEDHandler
from response.WLEDrequestHandlers import WLEDFileHandler, Patcher_I18N_Script, isHtml, PagePOSTHandler, SinglePOSTHandler, SI_POSTHandler
from response.WLEDrequestHandlers import FunctionHandler, GetAvailableLangs
import argparse

parser = argparse.ArgumentParser(
                    prog='main',
                    description='Web Server for WLED UI I18N and Testing'
)
parser.add_argument('-m', '--mode', choices=['I18N','L12N'], help='Run in I18N text capture mode')      # option that takes a value
parser.add_argument('-host', '--host', required=False, default="127.0.0.1")
parser.add_argument('-v', '--verbose',
                    action='store_true')  # on/off flag

args = parser.parse_args()

HOST_NAME = args.host
PORT_NUMBER = 80

if __name__ == '__main__':
    httpd = WLEDServer((HOST_NAME, PORT_NUMBER), WLEDHandler)

    if args.mode == 'I18N':
        postHandlers = [{
            "condition": isHtml,
            "processor": Patcher_I18N_Script,
            "args":
                {"replaceScript": { "old": "/L12N.js",
                                    "new": "/I18N.js"}
                }
        }]
    elif args.mode == 'L12N':
        postHandlers = [{
            "condition": isHtml,
            "processor": Patcher_I18N_Script,
            "args": {}
        }]
    else:
        postHandlers = []

    httpd.on("/json/si", "POST", SI_POSTHandler, {"file": "I18N/data/cachedFromWLED/si.json"})
    httpd.on("/json/", "GET", WLEDFileHandler, {
        "mapper": lambda url: url.replace("/json/","I18N/data/cachedFromWLED/") + ".json"
    })
    httpd.on("/cfg.json", "GET", WLEDFileHandler, {"file": "I18N/data/cachedFromWLED/cfg.json"})
    httpd.on("/presets.json", "GET", WLEDFileHandler, {"file":"I18N/data/cachedFromWLED/presets.json"})
    httpd.on("/langcodes.json", "GET", WLEDFileHandler, {"file":"I18N/data/langs.json"})
    httpd.on("/langs/", "GET", WLEDFileHandler, {"root":"I18N/langs/" })

    httpd.on("/settings/s.js", "GET", WLEDFileHandler, {"file":"scripts/s.js"})
    httpd.on("/settings/style.css", "GET", WLEDFileHandler, {"file":"data/style.css"})
    httpd.on("/settings/2D", "GET", WLEDFileHandler, {"file":"data/settings_2D.htm", "postFilter": postHandlers})
    httpd.on("/settings/dmx", "GET", WLEDFileHandler, {"file":"data/settings_dmx.htm", "postFilter": postHandlers})
    httpd.on("/settings/leds", "GET", WLEDFileHandler, {"file":"data/settings_leds.htm", "postFilter": postHandlers})
    httpd.on("/settings/pin", "GET", WLEDFileHandler, {"file":"data/settings_pin.htm", "postFilter": postHandlers})
    httpd.on("/settings/sec", "GET", WLEDFileHandler, {"file":"data/settings_sec.htm", "postFilter": postHandlers})
    httpd.on("/settings/sync", "GET", WLEDFileHandler, {"file":"data/settings_sync.htm", "postFilter": postHandlers})
    httpd.on("/settings/time", "GET", WLEDFileHandler, {"file":"data/settings_time.htm", "postFilter": postHandlers})
    httpd.on("/settings/ui", "GET", WLEDFileHandler, {"file":"data/settings_ui.htm", "postFilter": postHandlers})
    httpd.on("/settings/um", "GET", WLEDFileHandler, {"file":"data/settings_um.htm", "postFilter": postHandlers})
    httpd.on("/settings/wifi", "GET", WLEDFileHandler, {"file":"data/settings_wifi.htm", "postFilter": postHandlers})
    #    "mapper": lambda url: url.replace("/settings/","data/settings_") + ".htm"})
    httpd.on("/settings/", "GET", WLEDFileHandler, {"root":"data/"})
    httpd.on("/settings", "GET", WLEDFileHandler, {"file":"data/settings.htm", "postFilter": postHandlers})
    #httpd.on("/scripts/", "GET", WLEDFileHandler, {"root":"scripts/"})
    httpd.on("/I18N.js", "GET", WLEDFileHandler, {"file":"I18N/scripts/I18N.js"})
    httpd.on("/L12N.js", "GET", WLEDFileHandler, {"file":"I18N/scripts/L12N.js"})
    httpd.on("/", "GET", WLEDFileHandler, {"root":"data/", "default":"data/index.htm", "postFilter":postHandlers})

    httpd.on("/I18N/toast.json", "POST", SinglePOSTHandler, {"file":"I18N/data/fromScripts/toast.json"})
    httpd.on("/I18N", "POST", PagePOSTHandler, {"root":"I18N/data/fromPages/"})

    print(time.asctime(), 'Server UP - %s:%s' % (HOST_NAME, PORT_NUMBER))
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    httpd.server_close()

    print(time.asctime(), 'Server DOWN - %s:%s' % (HOST_NAME, PORT_NUMBER))