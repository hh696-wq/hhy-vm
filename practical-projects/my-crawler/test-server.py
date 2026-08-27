#!/usr/bin/env python3
import http.server
import sys

PAGE = b'''<!doctype html><html><body><main><article>
<section class="card"><h2 id="one">First item</h2><a href="/one">Open one</a></section>
<section class="card"><h2 id="two">Second item</h2><a href="/two">Open two</a></section>
</article></main></body></html>'''

class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(PAGE)))
        self.end_headers()
        self.wfile.write(PAGE)

    def log_message(self, *_):
        pass

server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
with open(sys.argv[1], "w", encoding="utf-8") as handle:
    handle.write(str(server.server_port))
server.serve_forever()
