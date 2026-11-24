# robrun_linux

EOS shim for running **ROB RUN** on Linux through Steam Play (Proton).

## Disclaimer

This is published for **educational and research purposes only**.

- I will not explain how this works or provide support for using it.
- I do not recommend using it. Use it at your own risk.
- If your game or Steam account gets restricted or banned, that is your problem. I do not care.
- You are responsible for your own actions. I take no responsibility and accept no liability for anything that happens because of this code, including bans, account or data loss, or legal trouble.
- Not affiliated with or endorsed by the game's developer or publisher, Steam, Valve, or Epic Games.
- No warranty of any kind. Provided "as is"; see [LICENSE](LICENSE).

## Build

Requires MinGW-w64 (see the error message in the script for your distro's package).

```sh
./build.sh
```

Output: `shim/bin/EOSSDK-Win64-Shipping.dll`.

## Tests

```sh
./tests/run.sh
```

Host suite for the platform-free shim units.

## Installation

See [INSTALL.md](INSTALL.md).
