#!/usr/bin/env python3
"""Translate the game's ARM7 code to C (no symbol map: code is found by following it)."""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import inputs  # noqa: E402


def parse_args(argv):
    p = argparse.ArgumentParser(description="Translate the ARM7 code of an extracted ROM to C.")
    p.add_argument("--extract", required=True, metavar="DIR", help="output of tools/extract_rom.py")
    p.add_argument("--out", required=True, metavar="DIR", help="folder for the generated C (created if missing)")
    p.add_argument("--hle", metavar="FILE", help="functions the runtime provides: <module> <addr>")
    p.add_argument("--seeds", metavar="FILE", help="extra entry points: <addr> arm|thumb")
    return p.parse_args(argv)


def main(argv=None):
    a = parse_args(argv)
    try:
        import recomp7
        return recomp7.run(a)
    except inputs.InputError as e:
        print(e, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
