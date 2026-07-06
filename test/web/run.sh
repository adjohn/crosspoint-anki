#!/usr/bin/env bash
# End-to-end test for the browser side of deck upload:
# fixture .apkg -> upload.html in headless Chromium -> mock device server
# implementing the firmware /upload-deck contract -> JSONL byte assertions.
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WEBROOT="$DIR/../../web"
BUILD="$DIR/build"
PORT="${PORT:-8791}"

rm -rf "$BUILD"
mkdir -p "$BUILD/received"

echo "== building fixture =="
python3 "$DIR/make_fixture.py" "$BUILD"

echo "== starting mock device server on port $PORT =="
python3 "$DIR/mock_server.py" "$PORT" "$WEBROOT" "$BUILD/received" &
SERVER_PID=$!
trap 'kill "$SERVER_PID" 2>/dev/null || true' EXIT

# Local traffic only: keep the agent proxy out of the loop.
unset HTTP_PROXY HTTPS_PROXY http_proxy https_proxy ALL_PROXY all_proxy || true
export NO_PROXY=127.0.0.1,localhost
export no_proxy=127.0.0.1,localhost

for i in $(seq 1 50); do
    if curl -sf --noproxy '*' -o /dev/null "http://127.0.0.1:$PORT/upload.html"; then
        break
    fi
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "mock server died" >&2
        exit 1
    fi
    sleep 0.1
done

echo "== running browser test =="
export PLAYWRIGHT_BROWSERS_PATH="${PLAYWRIGHT_BROWSERS_PATH:-/opt/pw-browsers}"
export NODE_PATH="${NODE_PATH:-/opt/node22/lib/node_modules}"
node "$DIR/test_upload.js" "$PORT" "$BUILD"
