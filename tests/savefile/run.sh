#!/bin/sh
# Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later
# Tests of save_file_atomic (runtime/savefile.c), the safe write of dqix.sav.
#
#   sh tests/savefile/run.sh
#
# These tests write and replace files, so they run ONLY in a Linux cloud
# session, on a folder made by mktemp -d: built with gcc (Linux path) and with
# x86_64-w64-mingw32-gcc under Wine (Windows path, MoveFileExA).
set -eu

if [ "$(uname -s)" != "Linux" ]; then
    echo "run.sh: refused, these tests run only on Linux (cloud session)" >&2
    exit 2
fi
for tool in gcc x86_64-w64-mingw32-gcc wine winepath; do
    command -v "$tool" >/dev/null 2>&1 || { echo "run.sh: $tool missing (apt-get install gcc gcc-mingw-w64-x86-64 wine)" >&2; exit 2; }
done

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d)
case "$T" in /tmp/?*) ;; *) echo "run.sh: unexpected temp folder $T" >&2; exit 2 ;; esac
cleanup() { rm -rf -- "$T"; }
trap cleanup EXIT INT TERM
export WINEDEBUG=-all

status=0
echo "== Linux (gcc)"
gcc -O2 -Wall -Wextra -I "$ROOT/runtime" -o "$T/t_savefile" "$ROOT/tests/savefile/t_savefile.c" "$ROOT/runtime/savefile.c"
mkdir "$T/linux"
"$T/t_savefile" "$T/linux" || status=1

echo "== Windows (mingw + Wine)"
x86_64-w64-mingw32-gcc -O2 -Wall -Wextra -I "$ROOT/runtime" -o "$T/t_savefile.exe" "$ROOT/tests/savefile/t_savefile.c" "$ROOT/runtime/savefile.c"
mkdir "$T/win"
wine "$T/t_savefile.exe" "$(winepath -w "$T/win")" || status=1
wineserver -w 2>/dev/null || true

if [ $status -eq 0 ]; then echo "== run.sh: all passed"; else echo "== run.sh: FAILED"; fi
exit $status
