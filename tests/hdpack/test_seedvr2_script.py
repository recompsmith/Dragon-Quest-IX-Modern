# Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later
"""Test tools/seedvr2_movies.py with a fake SeedVR2 service (no GPU, no ROM)."""
import os
import struct
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
import movie_frames
import seedvr2_movies as sm

NFR = 10
ORIG = [np.full((192, 256, 3), i * 20, np.uint8) for i in range(NFR)]
movie_frames.frames = sm.frames = lambda path: iter(ORIG)

calls = []
pairs = {}
real_write = movie_frames.write_movie


def spy_write(out_dir, name, src_frames, hd_frames):
    s, h = list(src_frames), list(hd_frames)
    pairs[name] = (s, h)
    return real_write(out_dir, name, iter(s), iter(h))


sm.write_movie = spy_write

with tempfile.TemporaryDirectory() as tmp:
    fake = os.path.join(tmp, "fake.py")
    open(fake, "w").write('''import os, sys
from PIL import Image
import numpy as np
a = sys.argv[1:]
i, o = a[a.index("-i") + 1], a[a.index("-o") + 1]
open(os.path.join(os.path.dirname(__file__), "calls.txt"), "a").write(i + chr(10))
rc = int(os.environ.get("FAKE_RC", "0"))
if rc: sys.exit(rc)
for n in sorted(os.listdir(i)):
    if n.endswith(".png"):
        v = Image.open(os.path.join(i, n)).getpixel((0, 0))[0]
        Image.fromarray(np.full((768, 1024, 3), v, np.uint8)).save(os.path.join(o, n))
''')
    bat = os.path.join(tmp, "fake.bat")
    open(bat, "w").write(f'@"{sys.executable}" "{fake}" %*\r\n')
    sm.SERVICE = bat
    mods, work, out = f"{tmp}/mods", f"{tmp}/work", f"{tmp}/out"
    os.makedirs(mods)
    open(f"{mods}/m.mods", "w").close()

    def ncalls():
        p = f"{tmp}/calls.txt"
        return len(open(p).read().split()) if os.path.exists(p) else 0

    def sig_count():
        return struct.unpack("<I", open(f"{out}/m.sig", "rb").read()[4:8])[0]

    # full run: frame i of the source is paired with HD frame i
    sm.remake(mods, work, out, "m", 0)
    assert ncalls() == 1 and sig_count() == NFR
    s, h = pairs["m"]
    assert len(s) == len(h) == NFR
    for i in range(NFR):
        assert s[i][0, 0, 0] == h[i][0, 0, 0] == i * 20, i
    r = subprocess.run(["ffprobe", "-v", "error", "-count_frames", "-select_streams", "v:0", "-show_entries",
                        "stream=width,height,nb_read_frames", "-of", "csv=p=0", f"{out}/m.mpg"], capture_output=True, text=True)
    assert r.stdout.strip().rstrip(",") == f"1024,768,{NFR}", r.stdout

    # re-run skips
    sm.remake(mods, work, out, "m", 0)
    assert ncalls() == 1

    # changing max-frames redoes the movie
    sm.remake(mods, work, out, "m", 4)
    assert ncalls() == 2 and sig_count() == 4
    assert len(os.listdir(f"{work}/m/src")) == 4
    sm.remake(mods, work, out, "m", 0)
    assert ncalls() == 3 and sig_count() == NFR

    # guard exit codes stop with a message and leave no done marker
    for code, text in ((75, "RAM"), (76, "VRAM"), (77, "refused")):
        os.environ["FAKE_RC"] = str(code)
        try:
            sm.remake(mods, f"{tmp}/w{code}", out, "m", 0)
            raise AssertionError("no stop")
        except SystemExit as e:
            assert str(code) in str(e.code) and text in str(e.code), e.code
        assert not os.path.exists(f"{tmp}/w{code}/m/done")
print("OK")
