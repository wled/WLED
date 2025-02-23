from http.server import HTTPServer, BaseHTTPRequestHandler
#from routes.main import routes
import urllib
#from response.simpleFileHandler import WLEDFileHandler

class WLEDServer(HTTPServer):
  def __init__(self, server_address, RequestHandlerClass, bind_and_activate=True):
    HTTPServer.__init__(self, server_address, RequestHandlerClass)
    self.handlers = []
    #self.I18N = False

  def on(self, URL, method, handlerClass, options):
    self.handlers.append(handlerClass(URL, method, options))

class WLEDHandler(BaseHTTPRequestHandler):
  def __init__(self, request, client_address, server):
    self.handlers = server.handlers
    self.postData = None
    super().__init__(request, client_address, server)

  def do_HEAD(self):
    print("!Server.do_HEAD")
    self.respond(headers_only=True)
    
  def do_GET(self):
    print("!Server.do_GET")
    self.respond()
    
  def do_POST(self):
    print("!Server.do_POST")
    self.postData = self.rfile.read(int(self.headers['Content-Length']))
    self.respond()
    
  def handle_http(self, method):
    parsed = urllib.parse.urlparse(self.path)

    for i in range(0,len(self.handlers)):
      handler = self.handlers[i]
      #print("Trying can_handle",i,handler)
      if handler.can_handle(method, parsed.path):
        response = handler.handle_path(method, parsed, self.postData)
        if response != None: return response

    return {"status":404, "content-type":"text/plain", "content":bytes("404 Not Found","UTF-8")}
        
  def respond(self, headers_only=False):
    print("request",self.command, self.path)
    response = self.handle_http(self.command)
    print("response",self.command, self.path, response["resolved"] if "resolved" in response else "?")
    self.send_headers(response)
    if(not headers_only): self.wfile.write(response["content"])

  def send_headers(self, response):
    print(response["status"],response["content-type"])
    self.send_response(response["status"])
    self.send_header('Content-type', response["content-type"])
    self.end_headers()
