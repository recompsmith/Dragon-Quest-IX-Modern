#!/bin/sh
# Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later
# Tests of hd-pack.exe (tools/hd-pack.c, tools/hdpack_core.h), download
# tests included (tests/hdpack/t_download.py).
#
#   sh tests/hdpack/run.sh
#
# This code moves and deletes files, so its tests run ONLY in a Linux cloud
# session: cross-built with x86_64-w64-mingw32-gcc and run under Wine, on a
# folder made by mktemp -d and nothing else. Never on a player's or a
# developer's Windows PC (incident of 2026-11-14). The links these tests make
# point only inside that same temp folder.
set -eu

if [ "$(uname -s)" != "Linux" ]; then
    echo "run.sh: refused, these tests run only on Linux (cloud session, Wine)" >&2
    exit 2
fi
for tool in x86_64-w64-mingw32-gcc wine winepath python3; do
    command -v "$tool" >/dev/null 2>&1 || { echo "run.sh: $tool missing (apt-get install gcc-mingw-w64-x86-64 wine)" >&2; exit 2; }
done

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d)
case "$T" in /tmp/?*) ;; *) echo "run.sh: unexpected temp folder $T" >&2; exit 2 ;; esac
cleanup() { rm -rf -- "$T"; }
trap cleanup EXIT INT TERM
export WINEDEBUG=-all
wine_path() { winepath -w "$1" 2>/dev/null; }

status=0
echo "== build"
x86_64-w64-mingw32-gcc -O2 -Wall -Wextra -municode -I "$ROOT/tools" -o "$T/hd-pack.exe" "$ROOT/tools/hd-pack.c" -lbcrypt -lwinhttp
x86_64-w64-mingw32-gcc -O2 -Wall -Wextra -I "$ROOT/runtime" -o "$T/t_running.exe" "$ROOT/tests/hdpack/t_running.c" "$ROOT/runtime/hdpack.c"
x86_64-w64-mingw32-gcc -O2 -Wall -Wextra -I "$ROOT/tools" -o "$T/t_hdpack.exe" "$ROOT/tests/hdpack/t_hdpack.c" -lbcrypt

echo "== pack made by tools/make_hd_pack.py (Task 3)"
mkdir -p "$T/src/tex" "$T/src/mov"
python3 - "$T/src" <<'EOF'
import os, sys
src = sys.argv[1]
for i in range(5):
    with open(os.path.join(src, "tex", "%016x.png" % (0xabc0 + i)), "wb") as f:
        f.write(bytes([i + 1]) * 600)
with open(os.path.join(src, "mov", "a.1.mpg"), "wb") as f:
    f.write(b"M" * 300)
with open(os.path.join(src, "mov", "a.sig"), "wb") as f:
    f.write(b"S" * 20)
EOF
# 2400 bytes per zip: the 5 textures go in 2 zips
python3 "$ROOT/tools/make_hd_pack.py" "$T/src/tex" "$T/src/mov" "$T/pack" --max-gb "$(python3 -c 'print(2400/1024**3)')"

# a pack whose movie zip does not match its SHA-256 (one byte changed)
mkdir -p "$T/pack-bad"
cp "$T/pack/"* "$T/pack-bad/"
python3 - "$T/pack-bad/hd-pack-movies.zip" <<'EOF'
import sys
p = sys.argv[1]
b = bytearray(open(p, "rb").read()); b[40] ^= 0xFF
open(p, "wb").write(bytes(b))
EOF

# zips hd-pack.exe must refuse to unpack
mkdir -p "$T/evil"
python3 - "$T/evil" <<'EOF'
import os, sys, zipfile
d = sys.argv[1]
def one(name, entry, method=zipfile.ZIP_STORED):
    with zipfile.ZipFile(os.path.join(d, name), "w", method) as z:
        z.writestr(zipfile.ZipInfo(entry, (2024, 1, 1, 0, 0, 0)), b"x" * 50, compress_type=method)
one("dotdot.zip", "../evil.txt")
one("dotdot2.zip", "textures/../../evil.txt")
one("backslash.zip", "..\\evil.txt")
one("absolute.zip", "/evil.txt")
one("drive.zip", "C:/evil.txt")
one("device.zip", "textures/con.png")
one("trailingdot.zip", "textures/x.")
one("deflate.zip", "textures/0000000000000001.png", zipfile.ZIP_DEFLATED)
EOF

# links (Wine reports a Unix symlink as FILE_ATTRIBUTE_REPARSE_POINT, like a
# junction): every one points inside $T only
mkdir -p "$T/fx/sp/game" "$T/fx/sp/sib"
echo "sibling" > "$T/fx/sp/sib/f.txt"
ln -s ../sib "$T/fx/sp/game/lnk"
ln -s game "$T/fx/sp/gamelink"
mkdir -p "$T/fx/sa/game/textures/hd" "$T/fx/sa/sentinel"
echo "sentinel" > "$T/fx/sa/sentinel/s.txt"
ln -s ../sentinel "$T/fx/sa/game/textures-avant-lien"
ln -s "$T/fx/sa/sentinel" "$T/fx/sa/game/textures/hd/dedans"   # absolute (inside $T): still valid once moved
mkdir -p "$T/fx/ml/game/textures" "$T/fx/ml/sentinel"
echo "sentinel" > "$T/fx/ml/sentinel/a.mpg"
ln -s ../sentinel "$T/fx/ml/game/movies"
mkdir -p "$T/fx/tl/game/textures" "$T/fx/tl/sentinel"
echo "sentinel" > "$T/fx/tl/sentinel/old.png"
ln -s ../../sentinel "$T/fx/tl/game/textures/hd-pack"

# hd-pack.lock as a link, hd-pack-progress.txt hard-linked: both to a sentinel in $T
mkdir -p "$T/fx/lk/game" "$T/fx/lk/sentinel" "$T/fx/hl/game" "$T/fx/hl/sentinel"
echo "sentinel" > "$T/fx/lk/sentinel/lock.txt"
ln -s ../sentinel/lock.txt "$T/fx/lk/game/hd-pack.lock"
echo "sentinel" > "$T/fx/hl/sentinel/hl.txt"
ln "$T/fx/hl/sentinel/hl.txt" "$T/fx/hl/game/hd-pack-progress.txt"

echo "== t_rank: which textures/ folder wins (runtime/hdpack.c)"
gcc -O2 -Wall -Wextra -I "$ROOT/runtime" -o "$T/t_rank" "$ROOT/tests/hdpack/t_rank.c" "$ROOT/runtime/hdpack.c"
"$T/t_rank" || status=1

echo "== t_hdpack.exe"
mkdir -p "$T/w"
wine "$T/t_hdpack.exe" "$(wine_path "$T")" "$(wine_path "$T/hd-pack.exe")" || status=1
if [ -L "$T/fx/lk/game/hd-pack.lock" ] && [ "$(cat "$T/fx/lk/sentinel/lock.txt")" = sentinel ]; then echo "PASS lock link and its target kept"; else echo "FAIL lock link and its target kept"; status=1; fi
if [ "$(stat -c %h "$T/fx/hl/sentinel/hl.txt")" = 2 ] && [ "$(cat "$T/fx/hl/sentinel/hl.txt")" = sentinel ]; then echo "PASS hard-linked progress file: both names kept, content kept"; else echo "FAIL hard-linked progress file kept"; status=1; fi

echo "== end to end: hd-pack.exe on a game folder with a non-ASCII name, nothing outside it changes"
E="$T/e2e"
G="$E/jeu-é"
mkdir -p "$G/textures/hd/sub" "$G/textures/perso" "$G/textures-hd-outil" "$G/textures-avant-1" "$G/movies" "$E/sentinel/deep"
cp "$T/hd-pack.exe" "$G/"
printf 'real-esrgan' > "$G/textures/hd/0000000000000001.png"
printf 'real-esrgan sub' > "$G/textures/hd/sub/0000000000000002.png"
printf 'mine' > "$G/textures/perso/0000000000000003.png"
printf 'old aside' > "$G/textures-avant-1/0000000000000004.png"
printf 'old movie' > "$G/movies/a.mpg"
printf 'other movie' > "$G/movies/b.mpg"
printf 'do not touch' > "$E/sentinel/keep.txt"
printf 'deep' > "$E/sentinel/deep/keep.bin"
ln -s ../sentinel "$G/textures-avant-lien"
ln -s ../../../sentinel "$G/textures/hd/lien"
ln -s ../../sentinel "$G/textures/perso/lien"
snapshot() { (cd "$E/sentinel" && find . -print0 | LC_ALL=C sort -z | xargs -0 ls -ld --time-style=+%s.%N | sed 's/ [0-9]*\.[0-9]* / /'; find . -type f -print0 | LC_ALL=C sort -z | xargs -0 sha256sum); }
before=$(snapshot)
around() { ls -la "$E" "$T" | grep -v -e ' e2e$' -e ' jeu-é$' || true; }
before_up=$(around)
e2e_ok=1
check() { if eval "$2"; then echo "PASS e2e: $1"; else echo "FAIL e2e: $1"; e2e_ok=0; fi; }
rc=0
HDPACK_SOURCE="$(wine_path "$T/pack")" wine "$G/hd-pack.exe" || rc=$?
check "exit 0" '[ $rc -eq 0 ]'
check "textures installed" '[ "$(ls "$G/textures/hd-pack" | grep -c "^000000000000abc[0-4]\.png$")" = 5 ]'
check "marker written" 'head -1 "$G/textures/hd-pack/hd-pack.txt" | grep -qx "hd-pack 1"'
check "texture bytes" '[ "$(od -An -tx1 -N1 "$G/textures/hd-pack/000000000000abc2.png" | tr -d " ")" = 03 ]'
check "movies installed" '[ "$(cat "$G/movies/a.1.mpg")" = "$(cat "$T/src/mov/a.1.mpg")" ] && [ -f "$G/movies/a.sig" ]'
check "old movie of the same name kept aside" '[ "$(cat "$G/movies-avant-hd-pack/a.mpg")" = "old movie" ] && [ ! -e "$G/movies/a.mpg" ]'
check "other movie untouched" '[ "$(cat "$G/movies/b.mpg")" = "other movie" ]'
check "Real-ESRGAN set aside, not deleted" '[ "$(cat "$G/hd-pack-ancien/textures-hd/0000000000000001.png")" = real-esrgan ] && [ "$(cat "$G/hd-pack-ancien/textures-hd/sub/0000000000000002.png")" = "real-esrgan sub" ] && [ ! -e "$G/textures/hd" ]'
check "textures-avant-1 set aside" '[ "$(cat "$G/hd-pack-ancien/textures-avant-1/0000000000000004.png")" = "old aside" ]'
check "link folder left where it was" '[ -L "$G/textures-avant-lien" ] && [ "$(readlink "$G/textures-avant-lien")" = ../sentinel ]'
check "player textures untouched" '[ "$(cat "$G/textures/perso/0000000000000003.png")" = mine ] && [ -L "$G/textures/perso/lien" ]'
check "work folder removed" '[ ! -e "$G/hd-pack-tmp" ]'
check "progress done" '[ "$(cat "$G/hd-pack-progress.txt")" = "done 0 100" ]'
check "lock left as an empty plain file" '[ -f "$G/hd-pack.lock" ] && [ ! -L "$G/hd-pack.lock" ] && [ ! -s "$G/hd-pack.lock" ]'
check "log written" 'grep -q "installed" "$G/hd-pack.log"'
check "sentinel byte-identical" '[ "$(snapshot)" = "$before" ]'
check "nothing changed next to the game folder" '[ "$(around)" = "$before_up" ]'
check "source pack unchanged" '(cd "$T/pack" && grep "^zip" hd-pack.txt | while read -r k n s h; do echo "$h  $n"; done | sha256sum -c --quiet)'
[ $e2e_ok -eq 1 ] || status=1

echo "== t_download.py: download with resume, local HTTP server"
python3 "$ROOT/tests/hdpack/t_download.py" "$T/hd-pack.exe" "$T/t_running.exe" || status=1

wineserver -w 2>/dev/null || true
if [ $status -eq 0 ]; then echo "== run.sh: all passed"; else echo "== run.sh: FAILED"; fi
exit $status
