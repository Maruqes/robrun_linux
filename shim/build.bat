@echo off
REM Build the shim with MSVC.
REM Run from a Developer Command Prompt (or call vcvars64.bat) so cl.exe is in PATH.
REM
REM Objects and the intermediate DLL are built under %LOCALAPPDATA%\robrun-shim\obj;
REM only the finished DLL is copied into bin\.

setlocal
cd /d "%~dp0"

where cl >nul 2>nul
if errorlevel 1 (
    echo [!] cl.exe not found.
    echo     Run from a Developer Command Prompt
    exit /b 1
)

if not exist src\exports.inc (
    echo [*] generating stubs
    python tools\gen_stubs.py
    if errorlevel 1 exit /b 1
)

set "OBJ=%LOCALAPPDATA%\robrun-shim\obj"
if not exist "%OBJ%" mkdir "%OBJ%"
if not exist bin mkdir bin

echo [*] compiling
cl /nologo /LD /MD /O2 /EHsc /W3 /DNDEBUG /Isrc /D_WIN32_WINNT=0x0601 /DWINVER=0x0601 ^
   src\core\boot.cpp src\abi\eos.cpp ^
   /Fo"%OBJ%"\ /Fd"%OBJ%\shim.pdb" ^
   /link /DEF:src\exports.def /OUT:"%OBJ%\EOSSDK-Win64-Shipping.dll" ^
   /OPT:NOICF /INCREMENTAL:NO
if errorlevel 1 (
    echo [!] build failed
    exit /b 1
)

copy /y "%OBJ%\EOSSDK-Win64-Shipping.dll" bin\ >nul
if errorlevel 1 (
    echo [!] copy to bin failed
    exit /b 1
)

echo.
echo [ok] bin\EOSSDK-Win64-Shipping.dll
endlocal
