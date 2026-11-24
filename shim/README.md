# shim

`EOSSDK-Win64-Shipping.dll` - the DLL that replaces the game's EOS SDK on disk.
It publishes the full EOS export surface the game imports.

## Build (Linux, MinGW-w64)

```sh
sudo dnf install -y mingw64-gcc-c++      # Fedora
sudo apt install g++-mingw-w64-x86-64    # Debian/Ubuntu
sudo pacman -S mingw-w64-gcc             # Arch

./build_linux.sh
```

Or `./build.sh` from the repository root. Output is
`bin/EOSSDK-Win64-Shipping.dll` plus `bin/loadtest.exe`.

## Build (Windows)

MSVC, from a Developer Command Prompt with `cl.exe` in PATH:

```bat
build.bat
```

MinGW-w64 (`w64devkit` works without an installer):

```bat
build_mingw.bat
```

## Verify

Host suite for the platform-free translation units, from the repository root:

```sh
tests/run.sh
```

Export surface, on Windows:

```bat
bin\loadtest.exe bin\EOSSDK-Win64-Shipping.dll src\exports.def
:: expect: checked=681 missing=0
```

## Regenerate stubs

Only when `data/eos-proxy.def` changes:

```sh
python3 tools/gen_stubs.py
```

The generator reads `data/eos-proxy.def` (681 names from the genuine SDK) and
writes a local `src/exports.def`.

## Log and switches

The DLL appends to `eos-shim.log` next to itself. `EOSSHIM_LOG=0` disables it.
`EOSSHIM_AC` selects the anti-cheat client policy: `null` (default, the session
is refused), `off`, `echo`, or `probe:<hex>` for capture runs. Both are read
from the environment at first use.
