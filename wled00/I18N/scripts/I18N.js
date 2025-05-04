console.log("I18N.js loaded")

function I18N()
{
    this.scanned = [];
}

I18N.prototype.scrape = function(subtag)
{
    targets = I18N.singleton.do_scan();
    I18N.singleton.post(subtag, targets);
}

I18N.prototype.do_scan = function()
{
    //Notes: the following will pick up the contents of <noscript as text

    self = this;

    targets = [];
    if(self.scanned.length == 0)
        targets.push({"content":document.title});

    reTranslatable = /[a-zA-Z]/; // TBD placeholder
    //reShowToast = /showToast\( *\"(?<a>[^\"]*)\"|\'([^\']+\' *)\)/
    
    //count = 0;
    function checkAttribute(e, path, attrName) {
        if(e.hasAttribute(attrName) && reTranslatable.test(e.getAttribute(attrName))) {
            if(self.scanned.includes(e.attributes[attrName])) return;

            targets.push({"attr":attrName, "content":e.getAttribute(attrName), "path":path});
            self.scanned.push(e.attributes[attrName]);
        }
    }

    function traverse(e, path) {
        switch(e.nodeType) {
            case 1: // element
                if(e.tagName == "SCRIPT") break; // not possible to get external script content
                // strings = [...e.contentText.matchAll(/(\")([^\"]*)\" | (\')([^\']*)\'/g)] 

                if(self.scanned.includes(e)) break;

                if(e.hasAttribute("id"))
                    path = e.getAttribute("id")

                // Look at atrributes, and then descend
                checkAttribute(e, path, "title");
                checkAttribute(e, path, "placeholder");
                checkAttribute(e, path, "value");

                if(e.tagName == "input") {
                    checkAttribute(e, path, "label");
                }

                for(var i=0; i < e.childNodes.length; ++i) {
                    traverse(e.childNodes[i], path + "/[" + i + "]");
                }
                break;
            case 3: // text node
                //TBD filter here for all whitespace
                if(reTranslatable.test(e.nodeValue) && !self.scanned.includes(e)) {
                    targets.push({"content": e.nodeValue, "path": path})
                    self.scanned.push(e);
                }
                break;
        }
    }
    traverse(document.body, "");

    return targets;
}

I18N.prototype.post = async function(subtag, targets)
{
    console.log("I18N",location.pathname)
    tag = location.pathname == "/" ? "index.htm" : location.pathname.replaceAll("/","_")
    if(subtag != undefined) tag += "__" + subtag

    await fetch('/I18N/' + tag + ".json", {
        method: 'POST',
        headers: {
        'Content-Type': 'application/json'
        },
        body: JSON.stringify(targets)
    });
}

async function postJStext(text)
{
    tag = location.pathname == "/" ? "index.htm" : location.pathname.replaceAll("/","_")

    await fetch('/I18N/toast.json', {
        method: 'POST',
        headers: {
        'Content-Type': 'application/json'
        },
        body: JSON.stringify({"content":text, "page":tag})
    });

}

I18N.singleton = new I18N();

// Sets up followup text capture and then runs initial scan
function runI18N(subtag)
{
    // Wrap functions that produce text needing translation
    if(window.showToast) {
        window.showToast = (function() {
            var cached_function = window.showToast;

            return function(text) {
                // pre code
                var result = cached_function.apply(this, arguments); // use .apply() to call it
                // post code
                console.log("text",text);
                if((typeof(text) == "string") && text.length > 0) {
                    postJStext(text);
                }

                return result;
            };
        })();
    }

    if(window.updateUI) {
        window.updateUI = (function() {
            var cached_function = window.updateUI;

            return function() {
                var result = cached_function.apply(this, arguments); // use .apply() to call it
                // rescrape
                I18N.singleton.scrape();

                return result;
            };
        })();
    }
    
    // makePUtil
    if(typeof window.makePUtil == "function") { // effect: retranslate on updateUI
        window.makePUtil = (function() {
            var cached_function = window.makePUtil;
        
            return function() {
                var result = cached_function.apply(this, arguments);

                // translate again
                I18N.singleton.scrape();
                return result;
            };
        })();   
    }

        // makePlUtil
        if(typeof window.makePlUtil == "function") { // effect: retranslate on updateUI
            window.makePlUtil = (function() {
                var cached_function = window.makePlUtil;
            
                return function() {
                    var result = cached_function.apply(this, arguments);
    
                    // translate again
                    I18N.singleton.scrape();
                    return result;
                };
            })();   
        }
    
    //makeSeg
    if(typeof window.makeSeg == "function") { // effect: retranslate on updateUI
        window.makeSeg = (function() {
            var cached_function = window.makeSeg;
        
            return function() {
                var result = cached_function.apply(this, arguments);

                // translate again
                I18N.singleton.scrape();
                return result;
            };
        })();   
    }
    
    //toggleInfo
    if(typeof window.toggleInfo == "function") { // effect: retranslate on updateUI
        window.toggleInfo = (function() {
            var cached_function = window.toggleInfo;
        
            return function() {
                var result = cached_function.apply(this, arguments);

                // translate again
                I18N.singleton.scrape();
                return result;
            };
        })();   
    }

    I18N.singleton.scrape();
}

console.log("I18N.js loaded")