#!/usr/bin/env bash
#
# Build and run the host-side tests.
#
# The sketch headers are compiled for a workstation, with Arduino, UUID and
# LoRa replaced by the stubs in test/stubs/. ArduinoJson is the real library,
# because the wire format is exactly what is under test.
#
# Usage:  test/run.sh [--verbose]

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "${here}/.." && pwd)"

# Locate the ArduinoJson checkout; the sketch requires it at compile time.
json_src=""
for candidate in \
    "${HOME}/Arduino/libraries/ArduinoJson/src" \
    "${HOME}/.arduino15/packages/*/hardware/*/*/libraries/ArduinoJson/src"; do
    if [ -d "${candidate}" ]; then
        json_src="${candidate}"
        break
    fi
done

if [ -z "${json_src}" ]; then
    echo "error: could not find ArduinoJson. Install it via the Arduino IDE or:"
    echo "       arduino-cli lib install ArduinoJson"
    exit 1
fi

build="${here}/build"
mkdir -p "${build}"

# -std=gnu++11 matches the SAMD core's platform.txt, so the tests cannot
# accidentally rely on a language feature the device does not have.
#
# Exceptions are deliberately ENABLED here, unlike on the device
# (-fno-exceptions). A test that provokes std::vector::at() out of range then
# reports a clean failure instead of aborting the whole run. On the hardware the
# same mistake is an unrecoverable abort, which is noted in BUG-02.
flags=(
    -std=gnu++11
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-return-type
    -I "${root}"
    -I "${here}"
    -I "${here}/stubs"
    -I "${json_src}"
)

sources=(
    "${here}/harness.cpp"
    "${here}/test_main.cpp"
    "${here}/test_util.cpp"
    "${here}/test_cli.cpp"
    "${here}/test_payload.cpp"
    "${here}/test_radio.cpp"
    "${here}/test_session.cpp"
    "${root}/radio.cpp"
    "${root}/session.cpp"
)

binary="${build}/beacon-tests"

echo "==> compiling (${json_src})"
g++ "${flags[@]}" -o "${binary}" "${sources[@]}"

if [ "${1:-}" = "--verbose" ]; then
    echo "==> running"
else
    # Keep the per-test output, drop it if everything passes.
    echo "==> running"
    output="$("${binary}" 2>&1)" || {
        echo "${output}"
        exit 1
    }
    echo "${output}"
    exit 0
fi

exec "${binary}"
