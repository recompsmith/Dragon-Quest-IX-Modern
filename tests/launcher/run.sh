#!/bin/sh
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
SRC=$(ls "$ROOT"/launcher/lnch_*.c 2>/dev/null | grep -v -e lnch_ui.c -e lnch_work.c || true)
# shellcheck disable=SC2086
gcc -std=c99 -D_DEFAULT_SOURCE -Wall -Wextra -O1 -I "$ROOT/launcher" -I "$ROOT/runtime" \
    -o "$T/t_launcher" "$ROOT/tests/launcher/t_launcher.c" $SRC "$ROOT/runtime/update.c" $LIBS
mkdir -p "$T/work"
"$T/t_launcher" "$T/work"
