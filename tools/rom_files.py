# Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later
"""List and extract files from a Nintendo DS ROM (NDS) using its FNT/FAT.

usage: rom_files.py <rom.nds> <out_dir> [glob, default data/movie/*.mods]
"""
import fnmatch
import os
import struct
import sys


def list_files(rom_path):
    """Return {path inside the ROM: (offset, size)} from the NDS header's FNT/FAT."""
    with open(rom_path, "rb") as f:
        f.seek(0x40)
        fnt_off, fnt_size, fat_off, fat_size = struct.unpack("<4I", f.read(16))
        f.seek(fnt_off)
        fnt = f.read(fnt_size)
        f.seek(fat_off)
        fat = f.read(fat_size)

    def fat_entry(file_id):
        return struct.unpack_from("<2I", fat, file_id * 8)

    out = {}

    def walk(dir_id, prefix):
        sub_off, first_file = struct.unpack_from("<IH", fnt, (dir_id & 0xFFF) * 8)
        pos, file_id = sub_off, first_file
        while True:
            head = fnt[pos]
            pos += 1
            if head == 0:
                return
            length = head & 0x7F
            name = fnt[pos:pos + length].decode("latin-1")
            pos += length
            if head & 0x80:
                (sub_id,) = struct.unpack_from("<H", fnt, pos)
                pos += 2
                walk(sub_id, prefix + name + "/")
            else:
                start, end = fat_entry(file_id)
                out[prefix + name] = (start, end - start)
                file_id += 1

    walk(0xF000, "")
    return out


def extract(rom_path, out_dir, pattern="data/movie/*.mods"):
    """Write the files matching `pattern` under out_dir; return the written paths."""
    written = []
    with open(rom_path, "rb") as f:
        for path, (off, size) in sorted(list_files(rom_path).items()):
            if not fnmatch.fnmatch(path, pattern):
                continue
            dest = os.path.join(out_dir, *path.split("/"))
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            f.seek(off)
            with open(dest, "wb") as g:
                g.write(f.read(size))
            written.append(dest)
    return written


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    for p in extract(sys.argv[1], sys.argv[2], *sys.argv[3:4]):
        print(p, os.path.getsize(p))
