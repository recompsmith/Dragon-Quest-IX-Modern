#!/usr/bin/env python3
"""Bundle the HD textures and movies into stored zips plus a manifest.

  make_hd_pack.py <textures_dir> <movies_dir> <out_dir> [--version 1] [--max-gb 1.9]
                  [--extra launcher-back.jpg]

Writes hd-pack-textures-N.zip (entries textures/<16 hex>.png), hd-pack-movies.zip
(entries movies/<file>) and hd-pack.txt:

  hd-pack <version>
  zip <file name> <size in bytes> <sha256 hex>

Entries are ZIP_STORED (the game's own minimal reader only does method 0).
Each zip stays under --max-gb, zip overhead included.
--extra puts the launcher's background picture in the movies zip as
movies/launcher-back.jpg (installed as movies/launcher-back.jpg, read by
launcher/launcher.c): the movies zip already changes with each movie fix, and
a textures zip would make everyone fetch the textures again for one picture.
It is never kept in the repository.
"""
import argparse
import hashlib
import os
import re
import sys
import zipfile

TEXTURE_RE = re.compile(r"^[0-9a-fA-F]{16}\.png$")
MOVIE_EXTS = (".mpg", ".sig")
EOCD = 22
# A zip without ZIP64 holds at most 65535 entries.
MAX_ENTRIES = 65535
# Fixed timestamp keeps the zips reproducible.
STAMP = (2024, 1, 1, 0, 0, 0)


def entry_cost(arcname, size):
    """Bytes one stored entry adds: local header + data + central directory record."""
    n = len(arcname.encode("utf-8"))
    return 30 + n + size + 46 + n


def list_files(folder, accept):
    out = []
    for name in sorted(os.listdir(folder)):
        path = os.path.join(folder, name)
        if os.path.isfile(path) and accept(name):
            out.append((name, path, os.path.getsize(path)))
    return out


def split(files, prefix, limit):
    """Group files into chunks whose zips would stay within limit bytes."""
    chunks, cur, used = [], [], EOCD
    for name, path, size in files:
        cost = entry_cost(prefix + name, size)
        if EOCD + cost > limit:
            raise SystemExit("%s alone exceeds the size limit" % name)
        if cur and (used + cost > limit or len(cur) >= MAX_ENTRIES):
            chunks.append(cur)
            cur, used = [], EOCD
        cur.append((name, path, size))
        used += cost
    if cur:
        chunks.append(cur)
    return chunks


def write_zip(zip_path, prefix, chunk):
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_STORED, allowZip64=False) as z:
        for name, path, _ in chunk:
            info = zipfile.ZipInfo(prefix + name, STAMP)
            info.compress_type = zipfile.ZIP_STORED
            info.external_attr = 0o644 << 16
            with open(path, "rb") as src, z.open(info, "w") as dst:
                while True:
                    block = src.read(1 << 20)
                    if not block:
                        break
                    dst.write(block)


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            block = f.read(1 << 20)
            if not block:
                break
            h.update(block)
    return h.hexdigest()


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("textures_dir")
    ap.add_argument("movies_dir")
    ap.add_argument("out_dir")
    ap.add_argument("--version", type=int, default=1)
    ap.add_argument("--max-gb", type=float, default=1.9)
    ap.add_argument("--extra", help="the launcher's background: a file named launcher-back.jpg")
    args = ap.parse_args(argv)
    if args.extra and os.path.basename(args.extra) != "launcher-back.jpg":
        raise SystemExit("--extra takes the launcher's background, a file named launcher-back.jpg")

    limit = int(args.max_gb * 1024 ** 3)
    textures = list_files(args.textures_dir, lambda n: TEXTURE_RE.match(n))
    movies = list_files(args.movies_dir, lambda n: n.lower().endswith(MOVIE_EXTS))
    if not textures:
        raise SystemExit("no <16 hex>.png textures found in %s" % args.textures_dir)
    if not movies:
        raise SystemExit("no .mpg/.sig movie files found in %s" % args.movies_dir)
    if args.extra:
        movies.append(("launcher-back.jpg", args.extra, os.path.getsize(args.extra)))
    os.makedirs(args.out_dir, exist_ok=True)
    for old in os.listdir(args.out_dir):
        if old == "hd-pack.txt" or (old.startswith("hd-pack-") and old.endswith(".zip")):
            os.remove(os.path.join(args.out_dir, old))

    names = []
    for i, chunk in enumerate(split(textures, "textures/", limit), 1):
        name = "hd-pack-textures-%d.zip" % i
        write_zip(os.path.join(args.out_dir, name), "textures/", chunk)
        names.append(name)
    chunks = split(movies, "movies/", limit)
    if len(chunks) > 1:
        raise SystemExit("movies do not fit in one zip under --max-gb")
    write_zip(os.path.join(args.out_dir, "hd-pack-movies.zip"), "movies/", chunks[0])
    names.append("hd-pack-movies.zip")

    lines = ["hd-pack %d" % args.version]
    for name in names:
        path = os.path.join(args.out_dir, name)
        size = os.path.getsize(path)
        if size > limit:
            raise SystemExit("%s is %d bytes, over the limit" % (name, size))
        lines.append("zip %s %d %s" % (name, size, sha256_of(path)))
    with open(os.path.join(args.out_dir, "hd-pack.txt"), "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print("%d textures, %d movie files, %d zips" % (len(textures), len(movies), len(names)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
