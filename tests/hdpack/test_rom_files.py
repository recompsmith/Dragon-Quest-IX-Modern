# Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later
"""Test tools/rom_files.py against a real ROM (set DQIX_ROM, else skipped)."""
import os
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))

ROM = os.environ.get("DQIX_ROM")
if not ROM:
    print("SKIP: DQIX_ROM not set")
    sys.exit(0)

import rom_files

STEMS = {"TD_nouhin_DS_form", "to_be_conteinued_A", "to_be_conteinued_B",
         "xenlon_DR_henshu_DS", "xenlon_OP_A", "xenlon_OP_B"}
MAGIC = b"MODS"

files = rom_files.list_files(ROM)
movies = {p: v for p, v in files.items()
          if p.startswith("data/movie/") and p.endswith(".mods")}
got = {os.path.splitext(os.path.basename(p))[0] for p in movies}
assert len(movies) == 6 and got == STEMS, sorted(movies)

with tempfile.TemporaryDirectory() as out:
    written = rom_files.extract(ROM, out)
    assert len(written) == 6, written
    for p, (off, size) in movies.items():
        dest = os.path.join(out, *p.split("/"))
        assert os.path.getsize(dest) == size, (p, size)
        with open(dest, "rb") as f:
            assert f.read(4) == MAGIC, p
print("OK")
