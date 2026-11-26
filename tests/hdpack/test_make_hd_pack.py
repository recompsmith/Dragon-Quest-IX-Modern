# Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later
"""Test tools/make_hd_pack.py: stored zips + manifest, split by size."""
import hashlib
import os
import subprocess
import sys
import tempfile
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, "..", "..", "tools", "make_hd_pack.py")

with tempfile.TemporaryDirectory() as tmp:
    tex = os.path.join(tmp, "tex")
    mov = os.path.join(tmp, "mov")
    out = os.path.join(tmp, "out")
    os.makedirs(tex)
    os.makedirs(mov)
    names = ["%016x.png" % (0xabc0 + i) for i in range(5)]
    for i, n in enumerate(names):
        with open(os.path.join(tex, n), "wb") as f:
            f.write(bytes([i]) * 600)
    with open(os.path.join(tex, "readme.txt"), "wb") as f:
        f.write(b"stray")
    with open(os.path.join(tex, "123.png"), "wb") as f:
        f.write(b"short name, not a texture")
    with open(os.path.join(mov, "a.mpg"), "wb") as f:
        f.write(b"M" * 300)
    with open(os.path.join(mov, "a.sig"), "wb") as f:
        f.write(b"S" * 20)
    with open(os.path.join(mov, "notes.txt"), "wb") as f:
        f.write(b"stray movie dir file")

    # ~730 bytes per entry; 2400 bytes allows 3 per zip -> 2 texture zips
    r = subprocess.run([sys.executable, TOOL, tex, mov, out,
                        "--max-gb", str(2400 / 1024 ** 3)],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr

    with open(os.path.join(out, "hd-pack.txt"), "rb") as f:
        raw = f.read()
    assert b"\r" not in raw
    lines = raw.decode().split("\n")
    assert lines[-1] == "", lines
    lines = lines[:-1]
    assert lines[0] == "hd-pack 1", lines
    zips = lines[1:]
    assert len(zips) == 3, zips

    seen = []
    for line in zips:
        kind, name, size, sha = line.split(" ")
        assert kind == "zip"
        path = os.path.join(out, name)
        data = open(path, "rb").read()
        assert len(data) == int(size), name
        assert hashlib.sha256(data).hexdigest() == sha, name
        assert len(data) <= 2400 or name == "hd-pack-movies.zip", name
        with zipfile.ZipFile(path) as z:
            for info in z.infolist():
                assert info.compress_type == zipfile.ZIP_STORED, info.filename
                seen.append(info.filename)
    import struct
    for line in zips:
        path = os.path.join(out, line.split(" ")[1])
        data = open(path, "rb").read()
        with zipfile.ZipFile(path) as z:
            for info in z.infolist():
                h = info.header_offset
                assert data[h:h + 4] == b"PK"
                assert struct.unpack("<H", data[h + 6:h + 8])[0] == 0, info.filename
                assert struct.unpack("<H", data[h + 28:h + 30])[0] == 0, info.filename
    assert sorted(os.listdir(out)) == sorted(
        ["hd-pack.txt", "hd-pack-textures-1.zip", "hd-pack-textures-2.zip",
         "hd-pack-movies.zip"]), os.listdir(out)
    assert sorted(seen) == sorted(
        ["textures/" + n for n in names] + ["movies/a.mpg", "movies/a.sig"]), seen

    # --version
    out2 = os.path.join(tmp, "out2")
    r = subprocess.run([sys.executable, TOOL, tex, mov, out2, "--version", "7"],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    assert open(os.path.join(out2, "hd-pack.txt")).read().startswith("hd-pack 7\n")

    # stale files in out_dir are removed (only hd-pack* names)
    with open(os.path.join(out2, "hd-pack-textures-9.zip"), "wb") as f:
        f.write(b"stale")
    with open(os.path.join(out2, "keep.txt"), "wb") as f:
        f.write(b"x")
    r = subprocess.run([sys.executable, TOOL, tex, mov, out2], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    assert sorted(os.listdir(out2)) == ["hd-pack-movies.zip", "hd-pack-textures-1.zip",
                                        "hd-pack.txt", "keep.txt"], os.listdir(out2)

    # empty inputs fail
    empty = os.path.join(tmp, "empty")
    os.makedirs(empty)
    for a, b in ((empty, mov), (tex, empty)):
        r = subprocess.run([sys.executable, TOOL, a, b, os.path.join(tmp, "out3")],
                           capture_output=True, text=True)
        assert r.returncode != 0 and "no " in r.stderr, (r.returncode, r.stderr)

    # entry-count cut
    sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
    import make_hd_pack
    make_hd_pack.MAX_ENTRIES = 2
    out4 = os.path.join(tmp, "out4")
    assert make_hd_pack.main([tex, mov, out4]) == 0
    assert sorted(os.listdir(out4)) == [
        "hd-pack-movies.zip", "hd-pack-textures-1.zip", "hd-pack-textures-2.zip",
        "hd-pack-textures-3.zip", "hd-pack.txt"], os.listdir(out4)
    for i in (1, 2, 3):
        with zipfile.ZipFile(os.path.join(out4, "hd-pack-textures-%d.zip" % i)) as z:
            assert len(z.infolist()) <= 2
    make_hd_pack.MAX_ENTRIES = 65535

    # --extra: the launcher's background goes in the movies zip as movies/launcher-back.jpg
    # (installed as movies/launcher-back.jpg; a textures zip would make everyone fetch the
    # textures again for one picture, PR 167); a name that isn't the one expected is refused
    back = os.path.join(tmp, "launcher-back.jpg")
    with open(back, "wb") as f:
        f.write(bytes([0xFF, 0xD8]) + b" picture")
    out5 = os.path.join(tmp, "out5")
    assert make_hd_pack.main([tex, mov, out5, "--extra", back]) == 0
    with zipfile.ZipFile(os.path.join(out5, "hd-pack-movies.zip")) as z:
        assert z.read("movies/launcher-back.jpg") == bytes([0xFF, 0xD8]) + b" picture", z.namelist()
    with zipfile.ZipFile(os.path.join(out5, "hd-pack-textures-1.zip")) as z:
        assert "textures/launcher-back.jpg" not in z.namelist()
    other = os.path.join(tmp, "other.jpg")
    with open(other, "wb") as f:
        f.write(b"x")
    r = subprocess.run([sys.executable, TOOL, tex, mov, os.path.join(tmp, "out6"), "--extra", other],
                       capture_output=True, text=True)
    assert r.returncode != 0 and "launcher-back.jpg" in r.stderr, (r.returncode, r.stderr)
print("OK")
