#!/usr/bin/env bash
# Build and run the native (host) unit tests. Exits nonzero on any failure.
# Usage: test/native/run.sh
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
SRC="$REPO/firmware/src"
ARDUINOJSON="$REPO/firmware/.pio/libdeps/default/ArduinoJson/src"
BUILD="$HERE/build"
CXX="${CXX:-g++}"
CXXFLAGS="-std=c++2a -Wall -Wextra -O1"

mkdir -p "$BUILD"
failures=0

run_test() {
    local name="$1"; shift
    echo "== $name =="
    if ! "$CXX" $CXXFLAGS "$@" -o "$BUILD/$name"; then
        echo "BUILD FAILED: $name"
        failures=$((failures + 1))
        return
    fi
    if ! "$BUILD/$name"; then
        failures=$((failures + 1))
    fi
}

run_test test_sm2 \
    -I "$HERE/shim" -I "$SRC" \
    "$HERE/test_sm2.cpp" "$SRC/scheduling/SM2.cpp"

# TimeUtils' DS3231/Wire path is behind #ifdef ARDUINO; the pure
# BCD/civil-date helpers and the system-time path compile host-side.
run_test test_timeutils \
    -I "$HERE/shim" -I "$SRC" \
    "$HERE/test_timeutils.cpp" "$SRC/utils/TimeUtils.cpp"

if [ -d "$ARDUINOJSON" ]; then
    run_test test_card_json \
        -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 \
        -I "$HERE/shim" -I "$SRC" -I "$ARDUINOJSON" \
        "$HERE/test_card_json.cpp"
else
    echo "SKIP test_card_json: ArduinoJson not found at $ARDUINOJSON"
    echo "     (run 'pio run' in firmware/ once to fetch library deps)"
    failures=$((failures + 1))
fi

echo
if [ "$failures" -eq 0 ]; then
    echo "All native tests passed."
else
    echo "$failures test target(s) failed."
fi
exit "$failures"
