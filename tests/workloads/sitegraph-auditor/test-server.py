#!/usr/bin/env python3
import http.server
import sys

def page(title, description, canonical, heading, links):
    metadata = f'<meta name="description" content="{description}">' if description is not None else ""
    canonical_tag = f'<link rel="canonical" href="{canonical}">' if canonical is not None else ""
    anchors = "".join(f'<a href="{href}">{label}</a>' for href, label in links)
    return f"""<!doctype html><html><head><title>{title}</title>{metadata}{canonical_tag}</head>
<body><main><h1>{heading}</h1>{anchors}</main></body></html>""".encode()

PAGES = {
    "/docs/index": page("Docs Home", "Documentation entry", "/docs/index", "Docs", [
        ("./guide#start", "Guide"),
        ("/docs/./guide", "Normalized duplicate"),
        ("https://example.com/reference", "External reference"),
    ]),
    "/docs/guide": page("Guide", "Start here", "/docs/guide", "Guide", [
        ("api/../api", "API"),
        ("../docs/index#back", "Back"),
    ]),
    "/docs/api": page("API", "API reference", "/docs/api", "API", [
        ("api/deep/topic?view=full", "Deep topic"),
        ("api/deep/topic?view=full#duplicate", "Deep duplicate"),
    ]),
    "/docs/api/deep/topic?view=full": page("Deep Topic", "Advanced material", "/docs/api/deep/topic?view=full", "Deep", []),
    "/docs/risky": page("Risky", None, None, "Risky", [
        ("/docs/missing", "Broken page"),
        ("/admin/private", "Outside allowed path"),
    ]),
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
