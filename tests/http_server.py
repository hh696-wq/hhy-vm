import http.server
import json
import os
import pathlib
import sys
import urllib.parse


class Handler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        parsed = urllib.parse.urlsplit(self.path)
        if parsed.path == "/binary":
            payload = b"\x00HHY\xff\n"
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
            return
        if parsed.path == "/inspect":
            payload = json.dumps({
                "query": urllib.parse.parse_qs(parsed.query),
                "header": self.headers.get("X-HHY-Test"),
            }).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
            return
        super().do_GET()

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        payload = json.dumps({
            "body": self.rfile.read(length).decode("utf-8"),
            "header": self.headers.get("X-HHY-Test"),
        }).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

port_path = pathlib.Path(sys.argv[1]).resolve()
os.chdir(pathlib.Path(__file__).parent / "fixtures")
server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
port_path.write_text(str(server.server_port), encoding="ascii")
server.serve_forever()
