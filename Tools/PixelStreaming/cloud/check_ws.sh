#!/usr/bin/env bash
# Run on the Lighthouse to see if Wilbur still accepts player/streamer sockets.
set -euo pipefail
echo "=== listeners ==="
ss -lntp | grep -E '8091|18880|18888' || echo "NOT LISTENING"
echo "=== local player WS handshake ==="
curl -sS -o /dev/null -w "18880 HTTP %{http_code}\n" --max-time 3 \
  -H "Connection: Upgrade" -H "Upgrade: websocket" \
  -H "Sec-WebSocket-Version: 13" -H "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==" \
  "http://127.0.0.1:18880/" || echo "18880 curl failed"
echo "=== public path via nginx ==="
curl -sS -o /dev/null -w "wss path HTTP %{http_code}\n" --max-time 5 \
  -H "Connection: Upgrade" -H "Upgrade: websocket" \
  -H "Sec-WebSocket-Version: 13" -H "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==" \
  "https://been.chat/ps/player" || echo "public /ps/player failed"
