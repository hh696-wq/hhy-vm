#!/usr/bin/env python3
import http.server
import sys

PAGES = {
    "/page": b'''<!doctype html><html><body><main><article>
<section class="card"><h2 id="one">First item</h2><a href="/docs/child#fragment">Child</a></section>
<a href="/docs/../docs/child">Duplicate normalized child</a>
<a href="http://example.invalid/outside">Outside domain</a>
</article></main></body></html>''',
    "/docs/child": b'''<!doctype html><html><body><main><article>
<section class="card"><h2 id="two">Second item</h2><a href="../final?b=2&amp;a=1">Final</a></section>
<a href="/page#again">Already visited</a>
</article></main></body></html>''',
    "/final?b=2&a=1": b'''<!doctype html><html><body><main><article>
<section class="card"><h2 id="three">Third item</h2></section>
</article></main></body></html>''',
}

class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        body = PAGES.get(self.path)
        if body is None:
            self.send_response(404)
            self.end_headers()
            return
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_):
        pass

server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
with open(sys.argv[1], "w", encoding="utf-8") as handle:
    handle.write(str(server.server_port))
server.serve_forever()
