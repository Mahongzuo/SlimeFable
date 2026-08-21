#!/usr/bin/env bash
# Clone UE5.8 infra on the Lighthouse. Uses a GitHub proxy if github.com is blocked.
set -euo pipefail
DEST="${1:-/opt/PixelStreamingInfrastructure}"
if [[ -d "$DEST/.git" ]]; then
  echo "Already present: $DEST"
  exit 0
fi
for url in \
  "https://ghproxy.net/https://github.com/EpicGames/PixelStreamingInfrastructure.git" \
  "https://github.com/EpicGames/PixelStreamingInfrastructure.git"
do
  echo "Cloning UE5.8 from $url"
  if git -c http.version=HTTP/1.1 clone --branch UE5.8 --depth 1 "$url" "$DEST"; then
    echo "OK $DEST"
    exit 0
  fi
done
echo "Clone failed." >&2
exit 1
