@echo off
rem Starts the game on Windows: double-click it (or run it from any folder). The game finds its files
rem (assets\, third_party\) from the repository's root, so it is started from there.
rem Before that, it checks that the setup is up to date (tools\setup.py --check: a new version downloaded over this
rem one, or a setup that did not finish) and offers to run it again; never set up, the setup runs first.
setlocal
cd /d "%~dp0"
if not exist "build\rac1-native.exe" (
    call "%~dp0setup.cmd"
    exit /b %errorlevel%
)
set "PY=%~dp0.toolchain\msys64\clang64\bin\python.exe"
set "DUE=%TEMP%\rac1-native-setup-check.txt"
rem (without the setup's own Python, e.g. set up from an MSYS2 shell, the game just starts)
if exist "%PY%" (
    "%PY%" tools\setup.py --check > "%DUE%" 2>nul
    if errorlevel 10 if not errorlevel 11 goto ask
)
goto play

:ask
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\windows\ask_setup.ps1" "%DUE%"
set "CHOICE=%errorlevel%"
del "%DUE%" >nul 2>nul
if "%CHOICE%"=="0" (
    call "%~dp0setup.cmd"
    exit /b %errorlevel%
)
if "%CHOICE%"=="2" exit /b 0

:play
del "%DUE%" >nul 2>nul
"build\rac1-native.exe" %*
exit /b %errorlevel%
