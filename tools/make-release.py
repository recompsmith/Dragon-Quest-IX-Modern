#!/usr/bin/env python3
"""Make the release zips, from the top of the project:

    python3 tools/make-release.py OUT_DIR

OUT_DIR/stable/DQIX-PC-<version>.zip holds the project as committed at HEAD
(no game code, no ROM), under DQIX-PC/. git archive lists the files, the
folders, the executable bits and the commit's date, so two runs on the same
commit give the same bytes. Every entry is stored deflated, and the players'
.txt files get Windows line endings (Notepad); the tables under config/ are
data read by the tools and stay as committed.

OUT_DIR/stable/DQIX-PC-<version>-extras.zip is made only when wfc/ has a game
folder with a _list.txt in it (files that are not in git): the files of each
such folder go under DQIX-PC/extras/wfc/<game>/.

<version> is DQIX_VERSION in runtime/version.h.
"""
import argparse
import io
import os
import re
import subprocess
import sys
import zipfile

TOP = "DQIX-PC/"


def git(*args):
    return subprocess.run(["git"] + list(args), check=True,
                          stdout=subprocess.PIPE).stdout


def release_version():
    header = git("show", "HEAD:runtime/version.h").decode("utf-8")
    found = re.search(r'^#define DQIX_VERSION "([^"]+)"', header, re.M)
    if not found:
        sys.exit("make-release: no DQIX_VERSION line in runtime/version.h")
    return found.group(1)


def wants_crlf(name):
    return name.endswith(".txt") and not name.startswith(TOP + "config/")


def crlf(data):
    return re.sub(rb"(?<!\r)\n", b"\r\n", data)


def game_zip(path):
    committed = git("archive", "--format=zip", "--prefix=" + TOP, "HEAD")
    with zipfile.ZipFile(io.BytesIO(committed)) as src, \
            zipfile.ZipFile(path, "w") as out:
        entries = src.infolist()
        for entry in entries:
            data = src.read(entry)
            if wants_crlf(entry.filename):
                data = crlf(data)
            out.writestr(entry, data, compress_type=zipfile.ZIP_DEFLATED)
    return len(entries)


def listed_games():
    if not os.path.isdir("wfc"):
        return []
    return [game for game in sorted(os.listdir("wfc"))
            if os.path.isfile(os.path.join("wfc", game, "_list.txt"))]


def extras_zip(path, games):
    count = 0
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as out:
        for game in games:
            folder = os.path.join("wfc", game)
            for name in sorted(os.listdir(folder)):
                out.write(os.path.join(folder, name),
                          "%sextras/wfc/%s/%s" % (TOP, game, name))
                count += 1
    return count


def main():
    parser = argparse.ArgumentParser(
        prog="make-release.py",
        description="Zip the committed project (and wfc/ extras) for a release.")
    parser.add_argument("out_dir", help="folder that gets stable/DQIX-PC-<version>.zip")
    out_dir = parser.parse_args().out_dir

    folder = os.path.join(out_dir, "stable")
    os.makedirs(folder, exist_ok=True)
    name = "DQIX-PC-" + release_version()

    path = os.path.join(folder, name + ".zip")
    entries = game_zip(path)
    print(path, os.path.getsize(path) // 1024, "KB,", entries, "entries")

    games = listed_games()
    if not games:
        print("no extras here (wfc/): skipped")
        return
    path = os.path.join(folder, name + "-extras.zip")
    files = extras_zip(path, games)
    print(path, os.path.getsize(path) // 1024, "KB,", files, "files")


if __name__ == "__main__":
    main()
