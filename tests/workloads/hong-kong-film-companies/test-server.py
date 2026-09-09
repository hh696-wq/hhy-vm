#!/usr/bin/env python3

import json
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse


SEARCH_RESULTS = [
    {"ns": 0, "title": "邵氏兄弟", "pageid": 101, "wordcount": 2362, "timestamp": "2026-08-22T13:40:00Z"},
    {"ns": 0, "title": "橙天嘉禾", "pageid": 102, "wordcount": 1770, "timestamp": "2026-08-23T16:43:16Z"},
    {"ns": 0, "title": "天下一電影", "pageid": 103, "wordcount": 443, "timestamp": "2026-08-13T12:20:50Z"},
    {"ns": 0, "title": "香港電影", "pageid": 104, "wordcount": 3558, "timestamp": "2026-08-15T14:23:27Z"},
    {"ns": 0, "title": "香港電影金像獎", "pageid": 105, "wordcount": 2200, "timestamp": "2026-08-20T12:00:00Z"},
]

PAGES = {
    101: "邵氏兄弟是一家於1958年在香港成立的電影製作公司。\n更多歷史。",
    102: "橙天嘉禾是香港具影響力的電影製作及發行公司。\n更多歷史。",
    103: "天下一電影是位於香港的電影投資及製作公司。\n更多歷史。",
    104: "香港電影又稱港產片，是華人電影的重要組成部分。",
    105: "香港電影金像獎是電影業界的年度頒獎活動。",
}


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        query = parse_qs(urlparse(self.path).query)
        if query.get("list") == ["search"]:
            payload = {"batchcomplete": True, "query": {"search": SEARCH_RESULTS}}
        else:
            page_id = int(query["pageids"][0])
            payload = {
                "batchcomplete": True,
                "query": {
                    "pages": [
                        {
                            "pageid": page_id,
                            "title": next(row["title"] for row in SEARCH_RESULTS if row["pageid"] == page_id),
                            "extract": PAGES[page_id],
                            "touched": next(row["timestamp"] for row in SEARCH_RESULTS if row["pageid"] == page_id),
                            "fullurl": f"https://zh.wikipedia.org/wiki/fixture-{page_id}",
                        }
                    ]
                },
            }

        body = json.dumps(payload, ensure_ascii=False).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, _format, *_args):
        return


def main():
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    Path(sys.argv[1]).write_text(str(server.server_port), encoding="utf-8")
    server.serve_forever()


if __name__ == "__main__":
    main()
