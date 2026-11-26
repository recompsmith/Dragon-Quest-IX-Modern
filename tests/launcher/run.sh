#!/bin/sh
# Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later
# Tests of the launcher's logic without a window (launcher/lnch_*.c):
#   sh tests/launcher/run.sh
# They only read and write inside a folder made by mktemp -d, run nothing of
# the game and no build: Linux (cloud session) or MSYS2 UCRT64.
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d)
cleanup() { rm -rf -- "$T"; }
trap cleanup EXIT INT TERM
LIBS=""
case "$(uname -s)" in MINGW*|MSYS*) LIBS="-lwinhttp" ;; esac
SRC=$(ls "$ROOT"/launcher/lnch_*.c 2>/dev/null | grep -v -e lnch_ui.c -e lnch_work.c -e lnch_keys.c || true)
# shellcheck disable=SC2086
gcc -std=c99 -D_DEFAULT_SOURCE -Wall -Wextra -O1 -I "$ROOT/launcher" -I "$ROOT/runtime" \
    -o "$T/t_launcher" "$ROOT/tests/launcher/t_launcher.c" $SRC "$ROOT/runtime/update.c" $LIBS
mkdir -p "$T/work"
"$T/t_launcher" "$T/work"
# the key screen's logic, over the game's runtime/controls.c: SDL2 needed (key names).
# Linux only: on Windows, MSYS2's SDL2.dll is sdl2-compat and opens an error window
# without SDL3 (the launcher's own SDL2 is tested by t_work_wine.sh)
case "$(uname -s)" in
Linux)
    if command -v sdl2-config >/dev/null; then
        # shellcheck disable=SC2046
        gcc -std=c99 -D_DEFAULT_SOURCE -Wall -Wextra -O1 -I "$ROOT/launcher" -I "$ROOT/runtime" $(sdl2-config --cflags) \
            -o "$T/t_keys" "$ROOT/tests/launcher/t_keys.c" "$ROOT/launcher/lnch_keys.c" "$ROOT/runtime/controls.c" $(sdl2-config --libs)
        mkdir -p "$T/keys"
        "$T/t_keys" "$T/keys"
    else echo "skip t_keys (no sdl2-config: libsdl2-dev)"; fi ;;
*) echo "skip t_keys (Linux only)" ;;
esac
