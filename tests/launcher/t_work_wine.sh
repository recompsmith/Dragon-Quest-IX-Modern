#!/bin/sh
# The launcher's work (launcher/lnch_work.c) on Windows, with
# DQIX_LAUNCHER_HEADLESS=1 (no window) and DQIX_UPDATE_SOURCE (no network):
#   sh tests/launcher/t_work_wine.sh <SDL2 mingw folder: include/SDL2, lib, bin/SDL2.dll>
# Under Wine in a Linux cloud session, or on a GitHub Actions Windows runner
# (MSYS2, DQIX_CI_WINDOWS=1: there tar and robocopy are the real ones). Never on
# a player's PC. Folders made by mktemp -d; a fake BUILD.bat stands for the build.
set -eu
SDL=${1:?usage: t_work_wine.sh <SDL2 mingw folder>}
case "$(uname -s)" in
Linux) WINE=wine; GCC=x86_64-w64-mingw32-gcc; winpath() { winepath -w "$1" 2>/dev/null; } ;;
MINGW*|MSYS*) [ -n "${DQIX_CI_WINDOWS:-}" ] || { echo "t_work_wine.sh: Windows only on a CI runner (DQIX_CI_WINDOWS=1)" >&2; exit 2; }
              WINE=; GCC=gcc; winpath() { cygpath -w "$1"; } ;;
*) echo "t_work_wine.sh: Linux (Wine) or a Windows CI runner" >&2; exit 2 ;;
esac
for t in $GCC $WINE; do command -v $t >/dev/null || { echo "$t missing" >&2; exit 2; }; done
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d)
case "$T" in /tmp/?*) ;; *) echo "unexpected temp folder $T" >&2; exit 2 ;; esac
trap 'rm -rf -- "$T"' EXIT INT TERM
export WINEDEBUG=-all
unset TMPDIR   # Wine 9.0 aborts at start (free(): invalid pointer) with it set in the cloud container
$GCC -std=c99 -O1 -I "$ROOT/launcher" -I "$ROOT/runtime" -I "$SDL/include/SDL2" -I "$SDL/include" \
    -o "$T/dqix-launcher.exe" "$ROOT"/launcher/*.c "$ROOT/runtime/update.c" -L "$SDL/lib" -lSDL2 -lwinhttp -lcomdlg32 -lm
fails=0
set +e   # every case runs and is counted, even under a shell whose set -e stops at a failed && list
check() { if [ "$1" = 0 ]; then echo "ok   $2"; else echo "FAIL $2"; sed 's/^/     /' "$T/out.txt"; fails=$((fails + 1)); fi; }

# a project: runtime/version.h, BUILD.bat (fake), game/ with the launcher
project() {   # project <name> <local version> <BUILD.bat exit code>
    P="$T/$1"; mkdir -p "$P/runtime" "$P/game/data" "$P/game/notes"
    printf '#pragma once\n#define DQIX_VERSION "%s"\n' "$2" > "$P/runtime/version.h"
    printf '@echo off\r\necho %%2> build-rom-arg.txt\r\nif not exist "%%~2" (echo BUILD FAILED ^(rom^): no ROM at %%2& exit /b 2)\r\necho progress 0.1 Checking the ROM\r\necho progress 0.5 Compiling\r\nif "%s"=="0" (echo x> game\\dqix.exe& (if not exist game\\data\\dqix.nds copy /y "%%~2" game\\data\\dqix.nds >nul)& echo progress 1 Done& exit /b 0)\r\necho BUILD FAILED (compiling): gcc missing\r\nexit /b 1\r\n' "$3" > "$P/BUILD.bat"
    cp "$T/dqix-launcher.exe" "$SDL/bin/SDL2.dll" "$P/game/"
    echo rom > "$P/game/data/dqix.nds"
    printf 'hd_pack_download = 0\r\n' > "$P/game/settings.ini"
    printf 'Dragon Quest IX Modern v3.2: install fixed.\n' > "$P/game/notes/v3.2.md"
}
source_at() {  # the "GitHub" the launcher sees: stable at <version>
    S="$T/src-$1"; mkdir -p "$S/stable/runtime" "$S/main/runtime"
    printf '#define X\n#define DQIX_VERSION "%s"\n' "$1" > "$S/stable/runtime/version.h"
    printf '#define X\n#define DQIX_VERSION "%s-nightly1"\n' "$1" > "$S/main/runtime/version.h"
    printf '[{"tag_name":"v3.2","name":"v3.2","draft":false,"prerelease":false,"published_at":"2026-11-22T10:00:00Z"}]' > "$S/releases.json"
}
launch() { (cd "$1/game" && DQIX_LAUNCHER_HEADLESS=1 DQIX_UPDATE_SOURCE=$(winpath "$2") $WINE ./dqix-launcher.exe > "$T/out.txt" 2>&1); }
launch_rom() { (cd "$1/game" && DQIX_LAUNCHER_HEADLESS=1 DQIX_LAUNCHER_ROM="$3" DQIX_UPDATE_SOURCE=$(winpath "$2") $WINE ./dqix-launcher.exe > "$T/out.txt" 2>&1); }

# not built yet, nothing newer: the build runs, PLAY lights up, the news comes from "GitHub"
source_at v3.2; project p1 v3.2 0
set +e; launch "$T/p1" "$T/src-v3.2"; r=$?
[ "$r" = 0 ] && grep -q "update=skipped build=done pack=skipped online=1 can_play=1" "$T/out.txt" && grep -q "news: v3.2 2026-11-22 Install fixed." "$T/out.txt"
check $? work_builds_when_not_built

# the build fails: its BUILD FAILED line is shown, PLAY stays off
project p2 v3.2 1
set +e; launch "$T/p2" "$T/src-v3.2"; r=$?
[ "$r" = 1 ] && grep -q "build=failed" "$T/out.txt" && grep -q "BUILD FAILED (compiling): gcc missing" "$T/out.txt"
check $? work_build_failure_shown

# offline (no source): a built game can be played
project p3 v3.2 0; echo x > "$T/p3/game/dqix.exe"
set +e; launch "$T/p3" "$T/nowhere"; r=$?
[ "$r" = 0 ] && grep -q "online=0 can_play=1" "$T/out.txt"
check $? work_offline_complete_can_play

# a newer stable: downloaded and put in place (update\launcher-apply.bat: robocopy and tar,
# which Wine may not have: then the test says so instead of failing)
source_at v3.3
mkdir -p "$T/zip/DQIX-PC/runtime" "$T/zip/DQIX-PC/game"; cp "$T/p1/BUILD.bat" "$T/zip/DQIX-PC/"; printf '#define DQIX_VERSION "v3.3"\n' > "$T/zip/DQIX-PC/runtime/version.h"
echo "settings from the update: must never land" > "$T/zip/DQIX-PC/game/settings.ini"   # a commit that ships game/ files
(cd "$T/zip" && zip -qr "$T/src-v3.3/stable.zip" DQIX-PC)
project p4 v3.2 0; echo x > "$T/p4/game/dqix.exe"
echo "the player's save" > "$T/p4/game/dqix.sav"; cp "$T/p4/game/settings.ini" "$T/settings-before.ini"
set +e; launch "$T/p4" "$T/src-v3.3"; r=$?
if [ -n "$WINE" ] && grep -qi "tar\|robocopy" "$T/out.txt" && grep -q "update=failed" "$T/out.txt"; then
    echo "skip work_update_then_build (Wine has no tar/robocopy: check on Windows)"
else
    [ "$r" = 0 ] && grep -q "update=done build=done" "$T/out.txt" && [ -f "$T/p4/update/previous/BUILD.bat" ] && [ -f "$T/p4/update/rollback.bat" ] && grep -q v3.3 "$T/p4/runtime/version.h"
    check $? work_update_then_build
    # the player's save and settings: untouched by the update (robocopy /XD game)
    [ "$(cat "$T/p4/game/settings.ini")" = "$(cat "$T/settings-before.ini")" ] && grep -q "the player's save" "$T/p4/game/dqix.sav"
    check $? work_update_keeps_saves_and_settings
fi

# an update download cut short (launcher closed, power cut) left a damaged zip: it goes, and the
# next run plays (a broken download blocks Modern for that run, like a network failure)
project p6 v3.2 0; echo x > "$T/p6/game/dqix.exe"; mkdir -p "$T/p6/update"; printf 'PK\003\004cut' > "$T/p6/update/download.zip"
set +e; launch "$T/p6" "$T/src-v3.2"; r1=$?; launch "$T/p6" "$T/src-v3.2"; r2=$?
if [ -n "$WINE" ] && grep -qi "robocopy\|tar" "$T/out.txt" && [ -f "$T/p6/update/download.zip" ]; then
    echo "skip work_damaged_update_zip_recovers (Wine has no tar/robocopy: check on Windows)"
else
    [ ! -f "$T/p6/update/download.zip" ] && [ "$r2" = 0 ] && grep -q "can_play=1" "$T/out.txt"
    check $? work_damaged_update_zip_recovers
fi

# a run cut after the copy-over, before its build: the next one puts the update in place again
# ("resume") without making a new backup: update/previous stays the version to go back to
project p7 v3.2 0; echo x > "$T/p7/game/dqix.exe"; mkdir -p "$T/p7/update/previous"
cp "$T/src-v3.3/stable.zip" "$T/p7/update/download.zip"
echo "The copy of the version before this update is whole." > "$T/p7/update/previous/backup-whole.txt"
echo "the version before" > "$T/p7/update/previous/marker.txt"
launch "$T/p7" "$T/src-v3.3"; r=$?
if [ -n "$WINE" ] && grep -qi "robocopy\|tar" "$T/out.txt" && grep -q "update=failed" "$T/out.txt"; then
    echo "skip work_resume_keeps_the_rollback_copy (Wine has no tar/robocopy: check on Windows)"
else
    [ "$r" = 0 ] && grep -q "update=done build=done" "$T/out.txt" && grep -q "the version before" "$T/p7/update/previous/marker.txt" \
        && [ ! -f "$T/p7/update/download.zip" ] && grep -q v3.3 "$T/p7/runtime/version.h"
    check $? work_resume_keeps_the_rollback_copy
fi

# first launch: no ROM copy yet: it waits for one (PLAY off), then builds from the chosen file
project p5 v3.2 0; rm -f "$T/p5/game/data/dqix.nds"
set +e; launch "$T/p5" "$T/src-v3.2"; r=$?
[ "$r" = 1 ] && grep -q "can_play=0" "$T/out.txt" && grep -qi "ROM" "$T/out.txt"
check $? work_first_launch_waits_for_rom
mkdir -p "$T/my roms & co"; echo rom > "$T/my roms & co/Dragon Quest IX (E).nds"
set +e; launch_rom "$T/p5" "$T/src-v3.2" "$(winpath "$T/my roms & co/Dragon Quest IX (E).nds")"; r=$?
[ "$r" = 0 ] && grep -q "build=done" "$T/out.txt" && [ -f "$T/p5/rom/dq9-rom.nds" ] && [ -f "$T/p5/game/data/dqix.nds" ] && grep -q 'rom\\dq9-rom.nds' "$T/p5/build-rom-arg.txt"
check $? work_first_build_from_rom

[ "$fails" = 0 ] && echo "all passed" || { echo "$fails failed"; exit 1; }
