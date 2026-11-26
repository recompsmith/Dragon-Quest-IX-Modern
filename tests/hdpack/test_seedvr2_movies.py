# Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later
"""Test movie_frames.write_movie: .sig layout and the MPEG-1 size/frame count."""
import os
import struct
import subprocess
import sys
import tempfile

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
import movie_frames

N = 30
rng = np.random.default_rng(1)
src = [rng.integers(0, 256, (192, 256, 3), dtype=np.uint8) for _ in range(N)]
hd = [rng.integers(0, 256, (768, 1024, 3), dtype=np.uint8) for _ in range(N)]

with tempfile.TemporaryDirectory() as out:
    n = movie_frames.write_movie(out, "t", iter(src), iter(hd))
    assert n == N, n
    sig = open(f"{out}/t.sig", "rb").read()
    assert len(sig) == 4 + 4 + N * 576, len(sig)
    assert sig[:4] == b"DQMS" and struct.unpack("<I", sig[4:8])[0] == N
    want = src[0].reshape(12, 16, 16, 16, 3).mean(axis=(1, 3)).round().astype(np.uint8).tobytes()
    assert sig[8:8 + 576] == want
    assert not os.path.exists(f"{out}/t.tmp")
    r = subprocess.run(["ffprobe", "-v", "error", "-count_frames", "-select_streams", "v:0",
                        "-show_entries", "stream=width,height,nb_read_frames", "-of", "csv=p=0",
                        f"{out}/t.mpg"], capture_output=True, text=True)
    assert r.stdout.strip().rstrip(",") == "1024,768,30", r.stdout + r.stderr
print("OK")

# write_movie rejects a wrong-size HD frame
with tempfile.TemporaryDirectory() as out:
    try:
        movie_frames.write_movie(out, "bad", iter(src[:1]), iter([np.zeros((768, 768, 3), np.uint8)]))
        raise SystemExit("wrong-size HD frame was accepted")
    except ValueError:
        pass
print("OK size")
