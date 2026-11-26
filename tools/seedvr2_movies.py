# Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later
"""Remake DQIX's movies in HD with SeedVR2 (the local ComfyUI service) into the files the game plays
(runtime/hdmovie.c): <name>.mpg 1024x768 MPEG-1 + <name>.sig, cut into parts under 25 MB.

usage: seedvr2_movies.py <mods_dir> <work_dir> <out_dir> [--max-frames N] [names...]

mods_dir  folder with the *.mods (tools/rom_files.py output, data/movie)
work_dir  <name>/src/%05d.png (decoded frames), <name>/hd (SeedVR2 output), <name>/mpg
out_dir   final <name>.sig, <name>.mpg or <name>.N.mpg

Resumable: a finished movie is skipped (work_dir/<name>/done) and SeedVR2 skips the passes it
already made. SeedVR2 runs through the GPU guard and may queue; if it stops (exit 75/76/77 = over
the memory limits or refused, or any other failure) the script stops with a message: no retry.
"""
import argparse
import os
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from movie_frames import frames, write_movie

SERVICE = r"J:\Outil\IA\ComfyUI_windows_portable\claude-service\upscale-video.bat"
GUARD_CODES = {75: "over the RAM limit", 76: "over the VRAM limit", 77: "refused by the guard"}


def decode_frames(mods, base, max_frames):
    """Write the movie's frames as <base>/src/%05d.png and return the count. Reused only if they
    were decoded for the same max_frames; otherwise src and hd (with the service's cache) are wiped."""
    from PIL import Image
    src_dir, ok = f"{base}/src", f"{base}/src.ok"
    want = str(max_frames)
    if os.path.exists(ok):
        got, n = open(ok).read().split()
        if got == want:
            return int(n)
        print(f"max-frames changed ({got} -> {want}): redoing {os.path.basename(base)}", flush=True)
    for d in (src_dir, f"{base}/hd", f"{base}/mpg"):
        shutil.rmtree(d, ignore_errors=True)
    if os.path.exists(ok):
        os.remove(ok)
    os.makedirs(src_dir)
    n = 0
    for i, f in enumerate(frames(mods)):
        if max_frames and i >= max_frames:
            break
        Image.fromarray(f).save(f"{src_dir}/{i:05d}.png")
        n = i + 1
    with open(ok, "w") as fh:
        fh.write(f"{want} {n}")
    return n


def seedvr2(src_dir, hd_dir):
    """Run the service; stop with a clear message on failure."""
    os.makedirs(hd_dir, exist_ok=True)
    rc = subprocess.run([SERVICE, "-i", src_dir, "-o", hd_dir, "--height", "768"]).returncode
    if rc != 0:
        why = GUARD_CODES.get(rc, "failed")
        sys.exit(f"SeedVR2 stopped (exit {rc}: {why}). Not retrying; fix the cause and re-run to resume.")


def hd_frames(hd_dir, n):
    from PIL import Image
    import numpy as np
    for i in range(n):
        p = f"{hd_dir}/{i:05d}.png"
        if not os.path.exists(p):
            sys.exit(f"SeedVR2 output is missing {p}")
        yield np.asarray(Image.open(p).convert("RGB"))


def remake(mods_dir, work_dir, out_dir, name, max_frames=0):
    base = f"{work_dir}/{name}"
    marker = f"{base}/done"
    if os.path.exists(marker) and open(marker).read() == str(max_frames):
        print(name, "already done", flush=True)
        return
    if os.path.exists(marker):
        os.remove(marker)
    t0 = time.time()
    n = decode_frames(f"{mods_dir}/{name}.mods", base, max_frames)
    print(name, n, "frames decoded", flush=True)
    seedvr2(f"{base}/src", f"{base}/hd")
    shutil.rmtree(f"{base}/mpg", ignore_errors=True)
    wrote = write_movie(f"{base}/mpg", name, frames_limit(mods_dir, name, n), hd_frames(f"{base}/hd", n))
    if wrote != n:
        sys.exit(f"{name}: wrote {wrote} frames, expected {n}")
    subprocess.run([sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)), "split_movies.py"),
                    f"{base}/mpg", out_dir, "25"], check=True)
    with open(marker, "w") as fh:
        fh.write(str(max_frames))
    print(name, "done", n, "frames", f"{time.time() - t0:.0f}s", flush=True)


def frames_limit(mods_dir, name, n):
    for i, f in enumerate(frames(f"{mods_dir}/{name}.mods")):
        if i >= n:
            return
        yield f


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mods_dir")
    ap.add_argument("work_dir")
    ap.add_argument("out_dir")
    ap.add_argument("--max-frames", type=int, default=0, help="only the first N frames (test runs)")
    ap.add_argument("names", nargs="*", help="movie names (default: all .mods)")
    a = ap.parse_args()
    avail = sorted(n[:-5] for n in os.listdir(a.mods_dir) if n.endswith(".mods"))
    names = a.names or avail
    bad = [n for n in names if n not in avail]
    if bad:
        sys.exit(f"no such movie in {a.mods_dir}: {bad}")
    for name in names:
        remake(a.mods_dir, a.work_dir, a.out_dir, name, a.max_frames)


if __name__ == "__main__":
    main()
