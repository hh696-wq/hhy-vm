#!/usr/bin/env python3
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PROFILES = {
    "101": {"region": "eu-west", "tier": "gold"},
    "102": {"region": "us-east", "tier": "silver"},
    "104": {"region": "ap-east", "tier": "bronze"},
    "105": {"region": "us-central", "tier": "gold"},
}


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        customer_id = self.path.removeprefix("/profiles/")
        profile = PROFILES.get(customer_id)
        if self.path.startswith("/profiles/") and profile:
            body = (json.dumps(profile) + "\n").encode()
            self.send_response(200)
        else:
            body = b'{"error":"not found"}\n'
            self.send_response(404)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format, *args):
        return


if __name__ == "__main__":
    ThreadingHTTPServer(("127.0.0.1", 18992), Handler).serve_forever()
