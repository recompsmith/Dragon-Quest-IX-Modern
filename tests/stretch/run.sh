#!/bin/sh
# Tests of the sound at Game speed / Battle speed (runtime/stretch.h).
#
#   sh tests/stretch/run.sh
#
# Plain gcc, no ROM; run in the Linux cloud session (nothing runs on the PC).
set -eu
here=$(cd "$(dirname "$0")" && pwd)
out=$(mktemp -d)
gcc -std=c11 -O2 -Wall -Wextra -Werror -D_DEFAULT_SOURCE -I"$here/../../runtime" "$here/t_stretch.c" -o "$out/t_stretch" -lm
"$out/t_stretch"
