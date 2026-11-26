# Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later
"""Writes a file's bytes as a C array (the launcher's background picture, built
into the program so it shows from the first launch, before any HD pack):
    python tools/embed_file.py launcher/art/launcher-back.jpg LAUNCHER_BACK launcher/back_jpg.c
"""
import sys


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    src, name, out = sys.argv[1:]
    data = open(src, "rb").read()
    lines = [",".join(str(b) for b in data[i:i + 40]) + "," for i in range(0, len(data), 40)]
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write("/* made by tools/embed_file.py from %s: run it again when the picture changes */\n" % src)
        f.write("const unsigned char %s[] = {\n%s\n};\n" % (name, "\n".join(lines)))
        f.write("const unsigned int %s_SIZE = %d;\n" % (name, len(data)))


if __name__ == "__main__":
    main()
