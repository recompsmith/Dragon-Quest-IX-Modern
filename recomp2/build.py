#!/usr/bin/env python3
"""Translate the game's ARM9 code (main binary, ITCM, overlays) to C.

One C function per game function, run on the Cpu structure of runtime/cpu.h,
called through the g_recomp_funcs table (runtime/dispatch.h).
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import inputs  # noqa: E402


def parse_args(argv):
    p = argparse.ArgumentParser(description="Translate the ARM9 code of an extracted ROM to C.")
    p.add_argument("--extract", required=True, metavar="DIR", help="output of tools/extract_rom.py")
    p.add_argument("--config", required=True, metavar="DIR", help="arm9 symbol map folder (symbols.txt, itcm/, dtcm/, overlays/ovNNN/)")
    p.add_argument("--out", required=True, metavar="DIR", help="folder for the generated C (created if missing)")
    p.add_argument("--per-file", type=int, default=400, metavar="N", help="table entries per .c file (default: 400)")
    p.add_argument("--hle", metavar="FILE", help="functions the runtime provides: <module> <addr> [name]")
    p.add_argument("--interpret", metavar="FILE", help="functions left to the interpreter: <module> <addr>")
    p.add_argument("--patch", metavar="FILE", help="instruction words replaced before translation: <module> <addr> <word>")
    p.add_argument("--hooks", metavar="FILE", help="functions wrapped by the runtime: <module> <addr> <name>")
    p.add_argument("--tags", metavar="FILE", help="calls whose 3D drawing is tagged: <module> <addr> <tag>")
    a = p.parse_args(argv)
    if a.per_file < 1:
        p.error("--per-file must be at least 1")
    return a


def main(argv=None):
    a = parse_args(argv)
    try:
        import recomp9
        return recomp9.run(a)
    except inputs.InputError as e:
        print(e, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
