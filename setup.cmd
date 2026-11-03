@echo off
rem rac1-native's guided setup on Windows: checks your system, builds the game, checks your disc image
rem and generates the game's data. The work is done by tools\setup.py; the tools it needs (Python, clang, CMake,
rem Ninja) come from a private MSYS2 that tools\windows\toolchain.ps1 downloads into .toolchain\ on the first run.
rem From an MSYS2 CLANG64 / UCRT64 shell, that shell's own tools are used instead (see docs\setup.md).
rem Usage: setup.cmd [--iso <path-to-your-iso>] [--force [steps]] [-j N]  (setup.cmd --help)
rem Without --iso it picks the disc images in assets\iso\ by itself (docs\setup.md, "Which disc images").
rem Double-clicking it works too (with no image in assets\iso\ it asks for one), and so does dropping the disc image on it.
setlocal
set "DIR=%~dp0"
set "WAIT="
set "DROPPED="
rem (double-clicked, or a disc image dropped on it: the window stays open at the end)
rem (the arguments are never copied into a variable: a path with & or ) in it would end the command there)
if "%~1"=="" set "WAIT=1"
if not "%~1"=="" if exist "%~1" (
    set "DROPPED=1"
    set "WAIT=1"
)

if /i "%MSYSTEM%"=="CLANG64" goto own_tools
if /i "%MSYSTEM%"=="UCRT64" goto own_tools

powershell -NoProfile -ExecutionPolicy Bypass -File "%DIR%tools\windows\toolchain.ps1"
if errorlevel 1 (
    set "RESULT=1"
    goto done
)
set "PATH=%DIR%.toolchain\msys64\clang64\bin;%PATH%"
if defined DROPPED (
    "%DIR%.toolchain\msys64\clang64\bin\python.exe" "%DIR%tools\setup.py" --iso "%~1"
) else (
    "%DIR%.toolchain\msys64\clang64\bin\python.exe" "%DIR%tools\setup.py" %*
)
set "RESULT=%errorlevel%"
goto done

:own_tools
set "RESULT=1"
where python >nul 2>nul
if errorlevel 1 (
    echo The setup needs Python 3.8 or newer. In this shell:
    echo     pacman -S --needed mingw-w64-clang-x86_64-python
    goto done
)
if defined DROPPED (
    python "%DIR%tools\setup.py" --iso "%~1"
) else (
    python "%DIR%tools\setup.py" %*
)
set "RESULT=%errorlevel%"

:done
if not "%RESULT%"=="0" set "WAIT=1"
if defined WAIT pause
exit /b %RESULT%
