#!/bin/sh
# dqix started with no arguments hands over to the launcher (runtime/exe_main.c):
#   sh tests/launcher/t_redirect.sh <built game folder>
# Linux cloud session only (it runs the game, under xvfb-run if no display):
# never on a player's PC. Works on a copy of the game folder in mktemp -d.
set -eu
[ "$(uname -s)" = Linux ] || { echo "t_redirect.sh: Linux cloud session only" >&2; exit 2; }
GAME=${1:?usage: t_redirect.sh <built game folder>}
T=$(mktemp -d)
case "$T" in /tmp/?*) ;; *) echo "unexpected temp folder $T" >&2; exit 2 ;; esac
trap 'rm -rf -- "$T"' EXIT INT TERM
cp -r "$GAME" "$T/game"
cd "$T/game"
rm -f dqix-launcher
export DQIX_NOWINDOW=1 SDL_AUDIODRIVER=dummy
fails=0
check() { if [ "$1" = 0 ]; then echo "ok   $2"; else echo "FAIL $2"; fails=$((fails + 1)); fi; }
fake_launcher() { printf '#!/bin/sh\necho "started DQIX_LAUNCHER=$DQIX_LAUNCHER" > "%s/marker"\n' "$T" > dqix-launcher; chmod +x dqix-launcher; }

# no launcher: the game plays (still running after 5 s)
rm -f "$T/marker"; set +e; timeout 5 ./dqix >/dev/null 2>&1; r=$?; set -e
[ "$r" = 124 ] && [ ! -f "$T/marker" ]; check $? redirect_skips_when_missing

# a launcher: started, and dqix ends at once with 0
fake_launcher; rm -f "$T/marker"; set +e; timeout 5 ./dqix >/dev/null 2>&1; r=$?; set -e
sleep 1
[ "$r" = 0 ] && grep -q "started DQIX_LAUNCHER=$" "$T/marker"; check $? redirect_starts_launcher

# with arguments (play.sh, tests): no hand-over
rm -f "$T/marker"; set +e; timeout 5 ./dqix data/dqix.nds data/code 60 1 t.sav >/dev/null 2>&1; set -e
[ ! -f "$T/marker" ]; check $? redirect_not_with_args

# started by the launcher (DQIX_LAUNCHER set): no hand-over back
rm -f "$T/marker"; set +e; DQIX_LAUNCHER=1 timeout 5 ./dqix >/dev/null 2>&1; r=$?; set -e
[ "$r" = 124 ] && [ ! -f "$T/marker" ]; check $? redirect_not_with_env

# the release zip's launcher, in the project folder (next to BUILD.sh): the game folder's one takes over
mkdir -p "$T/root/game"; cp "$GAME/dqix-launcher" "$T/root/"; echo '#!/bin/sh' > "$T/root/BUILD.sh"
printf '#!/bin/sh\necho "game launcher started" > "%s/marker2"\n' "$T" > "$T/root/game/dqix-launcher"; chmod +x "$T/root/game/dqix-launcher"
rm -f "$T/marker2"; set +e; (cd /; DQIX_LAUNCHER_HEADLESS=1 timeout 10 "$T/root/dqix-launcher" >/dev/null 2>&1); set -e
sleep 1
grep -q "game launcher started" "$T/marker2" 2>/dev/null; check $? root_hands_over_to_game_launcher

[ "$fails" = 0 ] && echo "5/5 passed" || { echo "$fails failed"; exit 1; }
