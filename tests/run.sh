#!/usr/bin/env bash
# Host test suite. Only platform-free translation units belong here: the
# sources under test must build and run on Linux with no Windows header.
#
#   tests/run.sh
#
# Windows-only behaviour is verified by the MinGW cross-build and a live game
# run, never from this script.

set -euo pipefail
cd "$(dirname "$0")/.."

CXX=${CXX:-g++}
command -v "$CXX" >/dev/null || {
    echo "[!] $CXX not found" >&2
    exit 1
}

FLAGS=(-std=c++20 -O2 -Wall -Wextra -Wshadow -Ishim/src -Ishared/include)
OUT=tests/build
mkdir -p "$OUT"

step() { printf '[*] %s\n' "$1"; }

step "anticheat_policy_test"
"$CXX" "${FLAGS[@]}" -o "$OUT/anticheat_policy_test" \
    tests/anticheat_policy_test.cpp shim/src/abi/anticheat_policy.cpp
"$OUT/anticheat_policy_test"

step "anticheat_options_test"
"$CXX" "${FLAGS[@]}" -o "$OUT/anticheat_options_test" tests/anticheat_options_test.cpp
"$OUT/anticheat_options_test"

step "eos_result_test"
"$CXX" "${FLAGS[@]}" -o "$OUT/eos_result_test" \
    tests/eos_result_test.cpp shim/src/abi/eos_result.cpp
"$OUT/eos_result_test"

step "tick_rate_test"
"$CXX" "${FLAGS[@]}" -o "$OUT/tick_rate_test" \
    tests/tick_rate_test.cpp shim/src/core/tick_rate.cpp
"$OUT/tick_rate_test"

step "fault_site_test"
"$CXX" "${FLAGS[@]}" -o "$OUT/fault_site_test" \
    tests/fault_site_test.cpp shim/src/core/fault_site.cpp
"$OUT/fault_site_test"

printf '\n[ok] host suite passed\n'
