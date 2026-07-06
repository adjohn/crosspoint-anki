#!/usr/bin/env python3
"""Mock CrossPoint device web server.

Serves the repo's web/ directory statically and implements POST /upload-deck
faithfully to firmware/src/network/WebServer.cpp handleUploadDeck/sendJson:
  - multipart parts are consumed IN ORDER, like ESPAsyncWebServer: only text
    fields that precede the file part exist when the firmware responds, so
    deckId/name/cardCount placed after the file part are invisible
  - deckId POST param required, 1-64 chars, [A-Za-z0-9_-] only
  - 400 {"error":"Empty or missing file"} on empty/absent file part (the
    upload callback is never invoked, so this wins even over a missing deckId)
  - 413 over 10MB
  - card count = newline count (+1 if last byte isn't '\n'), overridden by a
    positive cardCount param
  - 200 {"success":true,"bytes":N,"cards":C,"deckId":"..."}
Received file bytes and params are saved into the upload dir for assertions.

Usage: mock_server.py PORT WEBROOT UPLOAD_DIR
"""
import json
import os
import sys
from email import policy
from email.parser import BytesParser
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

MAX_UPLOAD_SIZE = 10 * 1024 * 1024


class Handler(SimpleHTTPRequestHandler):
    upload_dir = None

    def send_json(self, code, body):
        payload = body.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def do_POST(self):
        if self.path.split("?")[0] != "/upload-deck":
            self.send_json(404, '{"error":"Not found"}')
            return

        length = int(self.headers.get("Content-Length") or 0)
        body = self.rfile.read(length)
        head = ("Content-Type: %s\r\nMIME-Version: 1.0\r\n\r\n"
                % self.headers.get("Content-Type", "")).encode("utf-8")
        msg = BytesParser(policy=policy.default).parsebytes(head + body)

        # Walk parts in body order. The firmware responds during the file
        # part, so only fields parsed BEFORE it are ever visible; anything
        # after the file part must be ignored to catch ordering regressions.
        fields = {}
        data = None
        if msg.is_multipart():
            for part in msg.iter_parts():
                name = part.get_param("name", header="content-disposition")
                if part.get_filename() is not None:
                    data = part.get_payload(decode=True) or b""
                    break
                if name is not None:
                    fields[name] = part.get_content().strip("\r\n")

        if data is None or len(data) == 0:
            self.send_json(400, '{"error":"Empty or missing file"}')
            return

        deck_id = fields.get("deckId")
        if deck_id is None:
            self.send_json(400, '{"error":"Missing deckId parameter"}')
            return
        if len(deck_id) == 0 or len(deck_id) > 64:
            self.send_json(400, '{"error":"Invalid deckId"}')
            return
        if not all(c.isalnum() or c in "-_" for c in deck_id):
            self.send_json(400, '{"error":"Invalid deckId characters"}')
            return

        if len(data) > MAX_UPLOAD_SIZE:
            self.send_json(413, '{"error":"File too large (max 10MB)"}')
            return

        line_count = data.count(b"\n")
        if data and not data.endswith(b"\n"):
            line_count += 1

        cards = line_count
        card_count_param = fields.get("cardCount")
        if card_count_param is not None:
            try:
                n = int(card_count_param)
                if n > 0:
                    cards = n
            except ValueError:
                pass

        params = {"deckId": deck_id}
        name = fields.get("name")
        if name is not None:
            params["name"] = name
        if card_count_param is not None:
            params["cardCount"] = card_count_param

        os.makedirs(self.upload_dir, exist_ok=True)
        with open(os.path.join(self.upload_dir, deck_id + ".jsonl"), "wb") as f:
            f.write(data)
        with open(os.path.join(self.upload_dir, "params.json"), "w", encoding="utf-8") as f:
            json.dump(params, f, ensure_ascii=False)

        self.send_json(200, '{"success":true,"bytes":%d,"cards":%d,"deckId":"%s"}'
                       % (len(data), cards, deck_id))

    def log_message(self, fmt, *args):
        sys.stderr.write("[mock] %s\n" % (fmt % args))


def main():
    port = int(sys.argv[1])
    webroot = sys.argv[2]
    Handler.upload_dir = sys.argv[3]
    handler = partial(Handler, directory=webroot)
    server = ThreadingHTTPServer(("127.0.0.1", port), handler)
    print("mock server on http://127.0.0.1:%d serving %s" % (port, webroot), flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
