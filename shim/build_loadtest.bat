@echo off
REM Build the export checker. Needs only cl.exe, no SDK headers beyond the CRT.
REM usage: build_loadtest.bat   then   bin\loadtest.exe bin\EOSSDK-Win64-Shipping.dll src\exports.def
REM
REM Run from a Developer Command Prompt (or call vcvars64.bat) so cl.exe is in PATH.

setlocal
cd /d "%~dp0"

where cl >nul 2>nul
if errorlevel 1 (
    echo [!] cl.exe not found - run from a Developer Command Prompt
    exit /b 1
)

set "OBJ=%LOCALAPPDATA%\robrun-shim\obj"
if not exist "%OBJ%" mkdir "%OBJ%"
if not exist bin mkdir bin

cl /nologo /O2 /MD src\loadtest.c /Fo"%OBJ%\loadtest.obj" /Fe:"%OBJ%\loadtest.exe"
if errorlevel 1 exit /b 1

copy /y "%OBJ%\loadtest.exe" bin\ >nul
echo.
echo [ok] bin\loadtest.exe
endlocal
