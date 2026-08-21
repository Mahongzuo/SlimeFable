#!/usr/bin/env python3
"""Tiny status API for /play. Run on the Tencent box behind Nginx."""

from __future__ import annotations

import json
import os
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

TOKEN = os.environ.get("SLIME_PLAY_TOKEN", "change-me")
HOST = os.environ.get("SLIME_STATUS_HOST", "127.0.0.1")
PORT = int(os.environ.get("SLIME_STATUS_PORT", "8091"))
STALE_SECONDS = 25.0
CLAIM_SECONDS = 90.0

_lock = threading.Lock()
_state = {
    "updated": 0.0,
    "seats": {
        "0": {"id": 0, "streamerId": "slime-0", "online": False, "streaming": False, "hasPlayer": False, "playerUntil": 0.0},
        "1": {"id": 1, "streamerId": "slime-1", "online": False, "streaming": False, "hasPlayer": False, "playerUntil": 0.0},
    },
}


def _public_seats() -> list[dict]:
    now = time.time()
    stale = now - _state["updated"] > STALE_SECONDS
    out = []
    for key in ("0", "1"):
        seat = dict(_state["seats"][key])
        if stale:
            seat["online"] = False
            seat["streaming"] = False
        if seat.get("playerUntil", 0) < now:
            seat["hasPlayer"] = False
        seat.pop("playerUntil", None)
        out.append(seat)
    return out


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt: str, *args) -> None:
        return

    def _json(self, code: int, payload: dict) -> None:
        raw = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def _token_ok(self, supplied: str | None) -> bool:
        return bool(supplied) and supplied == TOKEN

    def do_OPTIONS(self) -> None:
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.send_header("Access-Control-Allow-Methods", "GET,POST,OPTIONS")
        self.end_headers()

    def do_GET(self) -> None:
        parsed = urlparse(self.path)
        if parsed.path.rstrip("/") != "/play/api/status":
            self._json(404, {"error": "not found"})
            return
        key = parse_qs(parsed.query).get("k", [""])[0]
        if not self._token_ok(key):
            self._json(403, {"error": "bad token"})
            return
        with _lock:
            self._json(200, {"seats": _public_seats()})

    def do_POST(self) -> None:
        parsed = urlparse(self.path)
        length = int(self.headers.get("Content-Length") or 0)
        try:
            body = json.loads(self.rfile.read(length).decode("utf-8") or "{}")
        except json.JSONDecodeError:
            self._json(400, {"error": "bad json"})
            return
        if not self._token_ok(str(body.get("token") or body.get("k") or "")):
            self._json(403, {"error": "bad token"})
            return

        path = parsed.path.rstrip("/")
        if path == "/play/api/status":
            seats = body.get("seats") or []
            with _lock:
                _state["updated"] = time.time()
                for item in seats:
                    seat_id = str(item.get("id", ""))
                    if seat_id not in _state["seats"]:
                        continue
                    dest = _state["seats"][seat_id]
                    dest["streamerId"] = str(item.get("streamerId") or dest["streamerId"])
                    dest["online"] = bool(item.get("online"))
                    dest["streaming"] = bool(item.get("streaming"))
            self._json(200, {"ok": True})
            return

        if path == "/play/api/claim":
            seat_id = str(body.get("seat", ""))
            role = str(body.get("role") or "watch")
            with _lock:
                if seat_id not in _state["seats"]:
                    self._json(400, {"error": "bad seat"})
                    return
                seat = _state["seats"][seat_id]
                now = time.time()
                if seat.get("playerUntil", 0) < now:
                    seat["hasPlayer"] = False
                if role == "play":
                    if seat["hasPlayer"]:
                        self._json(409, {"error": "seat taken", "seats": _public_seats()})
                        return
                    seat["hasPlayer"] = True
                    seat["playerUntil"] = now + CLAIM_SECONDS
                self._json(200, {"ok": True, "seats": _public_seats()})
            return

        self._json(404, {"error": "not found"})


def main() -> None:
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"status listening on {HOST}:{PORT}")
    server.serve_forever()


if __name__ == "__main__":
    main()
