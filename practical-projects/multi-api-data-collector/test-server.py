#!/usr/bin/env python3
import json
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = urlparse(self.path)
        query = parse_qs(parsed.query)
        page = int(query.get("page", ["1"])[0])
        offset = int(query.get("offset", ["0"])[0])

        if parsed.path == "/openalex":
            start = 1 if page == 1 else 2
            body = {"results": [
                {"id": f"https://openalex.org/W{n}", "display_name": f"OpenAlex Work {n}", "type": "article", "cited_by_count": n * 10}
                for n in range(start, start + 2)
            ]}
        elif parsed.path == "/crossref":
            start = 1 if offset == 0 else 2
            body = {"message": {"items": [
                {"DOI": f"10.1000/{n}", "title": [f"Crossref Work {n}"], "URL": f"https://doi.org/10.1000/{n}", "type": "journal-article", "reference-count": n * 5}
                for n in range(start, start + 2)
            ]}}
        elif parsed.path == "/github":
            start = 1 if page == 1 else 2
            body = {"items": [
                {"full_name": f"example/repo-{n}", "html_url": f"https://github.com/example/repo-{n}", "stargazers_count": n * 100}
                for n in range(start, start + 2)
            ]}
        else:
            self.send_error(404)
            return

        encoded = json.dumps(body).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)

    def log_message(self, _format, *_args):
        pass


server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
with open(sys.argv[1], "w", encoding="utf-8") as handle:
    handle.write(str(server.server_address[1]))
server.serve_forever()
