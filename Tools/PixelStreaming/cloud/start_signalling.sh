#!/usr/bin/env bash
# Run on the Tencent Lighthouse (been.chat). Wilbur + status only; Nginx stays in 宝塔.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
INFRA="${SLIME_INFRA_DIR:-/opt/PixelStreamingInfrastructure}"
WILBUR="$INFRA/SignallingWebServer"
TOKEN="${SLIME_PLAY_TOKEN:-change-me}"

export SLIME_PLAY_TOKEN="$TOKEN"
export SLIME_STATUS_HOST="${SLIME_STATUS_HOST:-127.0.0.1}"
export SLIME_STATUS_PORT="${SLIME_STATUS_PORT:-8091}"

if [[ ! -f "$WILBUR/dist/index.js" ]]; then
  echo "Build Wilbur first: cd $INFRA && npm install && npm run build:all:cjs"
  echo "Or: $WILBUR/platform_scripts/bash/start.sh -- --streamer_port 18888 --player_port 18880"
  exit 1
fi

python3 "$ROOT/status_server.py" &
STATUS_PID=$!
trap 'kill $STATUS_PID 2>/dev/null || true' EXIT

cd "$WILBUR"
exec node dist/index.js \
  --config_file "$ROOT/wilbur.config.json" \
  --peer_options_file "$ROOT/peer_options.json" \
  --streamer_port 18888 \
  --player_port 18880 \
  --sfu_port 18889 \
  --http_root www \
  --homepage player.html
