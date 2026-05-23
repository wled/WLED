/**
 * Minimal static file server for wled00/data/.
 * Used by Playwright webServer config — no npm dependencies required.
 */
const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = parseInt(process.env.PORT || '5001', 10);
const DATA_DIR = path.resolve(__dirname, '../../wled00/data');

const MIME = {
  '.htm':   'text/html',
  '.html':  'text/html',
  '.css':   'text/css',
  '.js':    'application/javascript',
  '.json':  'application/json',
  '.ico':   'image/x-icon',
  '.png':   'image/png',
  '.ttf':   'font/ttf',
  '.woff':  'font/woff',
  '.woff2': 'font/woff2',
  '.svg':   'image/svg+xml',
};

http.createServer((req, res) => {
  const url = req.url.split('?')[0];
  const filePath = path.join(DATA_DIR, url === '/' ? 'index.htm' : url);

  fs.readFile(filePath, (err, data) => {
    if (err) {
      res.writeHead(404, { 'Content-Type': 'text/plain' });
      res.end('404 Not Found: ' + url);
      return;
    }
    const ext = path.extname(filePath).toLowerCase();
    res.writeHead(200, {
      'Content-Type': MIME[ext] || 'application/octet-stream',
      'Cache-Control': 'no-store',
    });
    res.end(data);
  });
}).listen(PORT, '127.0.0.1', () => {
  console.log(`WLED data served at http://127.0.0.1:${PORT}`);
});
