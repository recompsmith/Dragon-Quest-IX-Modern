@echo off
rem Dragon Quest IX for PC: turns your own DQIX ROM into the PC game (the USA version is the tested one).
rem Double-click this. The first time it installs the free MSYS2 build tools
rem (a few minutes, needs the internet); after that it only rebuilds.
rem The result goes in the "game" folder next to this file.
setlocal
cd /d "%~dp0"
title Dragon Quest IX PC - build

rem ---- RecompLauncher (recomplauncher.json "prepare"): BUILD.bat --prepare "<your ROM>"
rem builds without pausing and prints "progress <0-1> <step>" lines for its setup screen;
rem a failure ends with one "BUILD FAILED (...): <cause>" line (full log: build\build.log).
rem Without --prepare nothing changes.
set "ROM="
if /i not "%~1"=="--prepare" goto pick_rom
set DQIX_NOPAUSE=1
set DQIX_PROGRESS=1
if not "%~2"=="" set "ROM=%~f2"
if defined ROM if exist "%ROM%" goto have_rom
echo No ROM given, or it doesn't exist: "%~2"
exit /b 2

:pick_rom
rem ---- the ROM: the first .nds in the "rom" folder, or ask for it
for %%f in ("%~dp0rom\*.nds") do if not defined ROM set "ROM=%%~ff"
if defined ROM goto have_rom
rem the ROM chosen last time (updates build without asking again)
if exist "%~dp0rom\last-rom.txt" set /p ROM=<"%~dp0rom\last-rom.txt"
if defined ROM if exist "%ROM%" goto have_rom
set "ROM="
echo Choose your Dragon Quest IX ROM in the window that opens...
for /f "usebackq delims=" %%i in (`powershell -NoProfile -STA -Command "Add-Type -AssemblyName System.Windows.Forms; $d = New-Object System.Windows.Forms.OpenFileDialog; $d.Filter = 'DS ROM (*.nds)|*.nds'; $d.Title = 'Choose your Dragon Quest IX ROM'; if ($d.ShowDialog() -eq 'OK') { $d.FileName }"`) do set "ROM=%%i"
if defined ROM goto have_rom
echo No ROM chosen. Put your ROM in the "rom" folder (or choose it) and run this again.
if not defined DQIX_NOPAUSE pause
exit /b 1

:have_rom
if defined DQIX_PROGRESS echo progress 0.02 Checking your ROM
echo ROM: "%ROM%"
if not exist "%~dp0rom" mkdir "%~dp0rom"
>"%~dp0rom\last-rom.txt" echo %ROM%

rem ---- the build tools (MSYS2)
set "MSYSROOT=C:\msys64"
if exist "%MSYSROOT%\usr\bin\bash.exe" goto have_msys
if defined DQIX_PROGRESS echo progress 0.03 Installing the build tools (MSYS2, one time)
echo Installing MSYS2 (one time, a few minutes)...
winget install --id MSYS2.MSYS2 -e --accept-source-agreements --accept-package-agreements
if exist "%MSYSROOT%\usr\bin\bash.exe" goto have_msys
echo BUILD FAILED: the build tools (MSYS2) are not in C:\msys64 and could not be installed there. Install MSYS2 from https://www.msys2.org into C:\msys64, then try again.
if not defined DQIX_NOPAUSE pause
exit /b 1

:have_msys
if defined DQIX_PROGRESS echo progress 0.08 Checking the build tools
echo Checking the build tools (the first time downloads them)...
rem cc-libs pins gcc's runtime libraries: it has to move with gcc, or a newer gcc (or numpy,
rem which needs the newer libgcc) cannot be installed and the whole line fails
rem a full upgrade first: "pacman -Sy" alone (a partial upgrade) can bring a newer Python whose
rem libraries stay old, then Python stops with "DLL load failed while importing binascii".
rem The first pass may only update MSYS2 itself, so it runs twice.
rem a package manager stopped half way (its window closed, the PC turned off) leaves its lock, and every
rem install after it fails ("unable to lock database"): Python never comes. None running: the lock is stale.
tasklist /FI "IMAGENAME eq pacman.exe" 2>nul | find /i "pacman.exe" >nul || if exist "%MSYSROOT%\var\lib\pacman\db.lck" del /f /q "%MSYSROOT%\var\lib\pacman\db.lck"
rem SDL2 left alone: the upgrade would swap it for sdl2-compat (SDL2 over SDL3), the line after it back again
"%MSYSROOT%\usr\bin\bash.exe" -lc "pacman -Syuu --noconfirm --ignore mingw-w64-ucrt-x86_64-SDL2,mingw-w64-ucrt-x86_64-sdl2-compat"
"%MSYSROOT%\usr\bin\bash.exe" -lc "pacman -Syuu --noconfirm --ignore mingw-w64-ucrt-x86_64-SDL2,mingw-w64-ucrt-x86_64-sdl2-compat"
"%MSYSROOT%\usr\bin\bash.exe" -lc "pacman -S --noconfirm --needed make diffutils mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cc-libs mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-python-yaml"
if errorlevel 1 echo WARNING: the build tools could not be updated; building with the ones already installed.
rem optional (townspeople in 3D): on its own, so that without it, or offline, nothing else changes
"%MSYSROOT%\usr\bin\bash.exe" -lc "pacman -S --noconfirm --needed mingw-w64-ucrt-x86_64-cc-libs mingw-w64-ucrt-x86_64-python-numpy"
if errorlevel 1 echo WARNING: Python's numpy could not be installed: townspeople in 3D will not be made.

rem ---- build
set MSYSTEM=UCRT64
set CHERE_INVOKING=1
set "DQIX_ROM=%ROM%"
rem the tools really there (an install that failed above only warned): else say what to do, not a
rem "python: command not found" or "No module named yaml" further on
"%MSYSROOT%\usr\bin\bash.exe" -lc "command -v make >/dev/null && command -v gcc >/dev/null && python -c 'import yaml'" >nul 2>&1
if not errorlevel 1 goto have_tools
echo The build tools are not all installed (make, gcc, Python and its yaml module).
echo Open "MSYS2 UCRT64" from the Start menu, type: pacman -Syu   then Enter, answer Y,
echo again until it says "there is nothing to do", then run this again.
if defined DQIX_PROGRESS echo BUILD FAILED (build tools): make, gcc or Python with yaml missing; open MSYS2 UCRT64, run pacman -Syu until nothing to do, then try again
goto failed
:have_tools
"%MSYSROOT%\usr\bin\bash.exe" -lc "./tools/build-game.sh \"$(cygpath -u \"$DQIX_ROM\")\""
if errorlevel 1 goto failed
echo.
echo Finished. Play with game\dqix.exe (you can make a shortcut to it).
if not defined DQIX_NOPAUSE pause
exit /b 0

:failed
rem --prepare: build-game.sh's own last line already says why (RecompLauncher shows only that one)
if defined DQIX_PROGRESS exit /b 1
echo.
echo BUILD FAILED. The text above says why.
if not defined DQIX_NOPAUSE pause
exit /b 1
