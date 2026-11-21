#!/usr/bin/env bash
# Build the shim DLL with MinGW-w64. Run from anywhere: ./build.sh
set -euo pipefail
cd "$(dirname "$0")"

./shim/build_linux.sh

echo
echo "[ok] $(pwd)/shim/bin/EOSSDK-Win64-Shipping.dll"
