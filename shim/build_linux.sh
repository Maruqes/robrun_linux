#!/usr/bin/env bash
# Cross-compile the shim on Linux with MinGW-w64.
#
#   sudo dnf install -y mingw64-gcc-c++      # Fedora
#   sudo apt install g++-mingw-w64-x86-64    # Debian/Ubuntu
#   sudo pacman -S mingw-w64-gcc             # Arch
#   ./build_linux.sh
#
# Produces the same DLL as build_mingw.bat: same GCC series, same flags, same
# export set.

set -euo pipefail
cd "$(dirname "$0")"

CXX=${CXX:-x86_64-w64-mingw32-g++}
CC=${CC:-x86_64-w64-mingw32-gcc}

for tool in "$CXX" "$CC"; do
    command -v "$tool" >/dev/null || {
        echo "[!] $tool not found. Install MinGW-w64, e.g.:" >&2
        echo "      Fedora:  sudo dnf install mingw64-gcc-c++" >&2
        echo "      Debian:  sudo apt install g++-mingw-w64-x86-64" >&2
        echo "      Arch:    sudo pacman -S mingw-w64-gcc" >&2
        exit 1
    }
done

[ -f src/exports.inc ] || python3 tools/gen_stubs.py
mkdir -p bin

echo "[*] building shim"
"$CXX" -shared -O2 -Wall -Wextra \
    -Isrc -D_WIN32_WINNT=0x0601 -DWINVER=0x0601 \
    -o bin/EOSSDK-Win64-Shipping.dll \
    src/core/boot.cpp src/abi/eos.cpp src/exports.def \
    -static -static-libgcc -static-libstdc++ -s \
    -Wl,--no-insert-timestamp

echo "[*] building loadtest"
"$CC" -O2 src/loadtest.c -o bin/loadtest.exe

echo
echo "[ok] bin/EOSSDK-Win64-Shipping.dll  ($(stat -c %s bin/EOSSDK-Win64-Shipping.dll) bytes)"
echo "[ok] bin/loadtest.exe"
