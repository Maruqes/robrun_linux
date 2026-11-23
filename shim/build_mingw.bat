@echo off
REM Build the shim with MinGW-w64 instead of MSVC.
REM Portable option: w64devkit (no installer, no admin):
REM   https://github.com/skeeto/w64devkit/releases
REM Run w64devkit.exe (it sets PATH) or add w64devkit\bin to PATH, then run this.
REM
REM The shim is plain C ABI, so the toolchain does not affect what the game sees;
REM only the CRT it links differs (msvcrt here, UCRT under MSVC).

setlocal
cd /d "%~dp0"

where g++ >nul 2>nul
if errorlevel 1 (
    echo [!] g++ not found. Start w64devkit.exe or add its bin folder to PATH.
    exit /b 1
)

if not exist src\exports.inc (
    python tools\gen_stubs.py
    if errorlevel 1 exit /b 1
)

if not exist bin mkdir bin

echo [*] building shim
g++ -shared -O2 -Wall -Wextra ^
    -Isrc -I..\shared\include -D_WIN32_WINNT=0x0601 -DWINVER=0x0601 ^
    -o bin\EOSSDK-Win64-Shipping.dll ^
    src\core\boot.cpp src\core\diag.cpp src\abi\eos.cpp ^
    src\abi\anticheat.cpp src\abi\anticheat_policy.cpp src\exports.def ^
    -static -static-libgcc -static-libstdc++ ^
    -Wl,--no-insert-timestamp
if errorlevel 1 (
    echo [!] shim build failed
    exit /b 1
)

echo [*] building loadtest
gcc -O2 src\loadtest.c -o bin\loadtest.exe
if errorlevel 1 (
    echo [!] loadtest build failed
    exit /b 1
)

echo.
echo [ok] bin\EOSSDK-Win64-Shipping.dll
echo [ok] bin\loadtest.exe
echo      next: bin\loadtest.exe bin\EOSSDK-Win64-Shipping.dll src\exports.def
endlocal
