"""Input readers: address lists, symbol maps and the extracted ROM modules.

Address lists (hle.txt, interpret.txt, patches.txt, hooks.txt, tags.txt,
hle7.txt, ARM7 seeds): one entry per line, fields separated by blanks, '#'
starts a comment, blank lines are skipped, addresses are hex without 0x.

Symbol maps use the dsd format of the decompilation project
(https://github.com/DQIX/dqix-decomp, https://github.com/AetiasHax/ds-decomp):
    <name> kind:<kind> [flags] addr:0x<hex> [flags]
"""
import os
import re

import yaml


class InputError(SystemExit):
    """A malformed or missing input: one line 'file:line: reason', exit code 1."""

    def __init__(self, msg):
        super().__init__(1)
        self.msg = msg

    def __str__(self):
        return self.msg


# Field specs: module, addr (hex), word (hex, 32 bits), name, tag (decimal),
# mode (arm|thumb). A trailing '?' makes the field optional.
HLE = ("module", "addr", "name?")
HLE7 = ("module", "addr")
INTERPRET = ("module", "addr")
PATCHES = ("module", "addr", "word")
HOOKS = ("module", "addr", "name")
TAGS = ("module", "addr", "tag")
SEEDS = ("addr", "mode")


class Entry:
    def __init__(self, path, line):
        self.path = path
        self.line = line
        self.module = self.addr = self.word = self.name = self.tag = self.mode = None

    def where(self):
        return "%s:%d" % (self.path, self.line)

    def __repr__(self):
        return "Entry(%s %s %s)" % (self.module, self.addr, self.name)


_HEX = re.compile(r"^[0-9A-Fa-f]{1,8}$")


def _parse_field(kind, tok, where):
    if kind in ("addr", "word"):
        if not _HEX.match(tok):
            raise InputError("%s: bad hex %s '%s'" % (where, "address" if kind == "addr" else "word", tok))
        return int(tok, 16)
    if kind == "tag":
        if not re.match(r"^-?[0-9]+$", tok):
            raise InputError("%s: bad decimal tag '%s'" % (where, tok))
        return int(tok)
    if kind == "mode":
        if tok not in ("arm", "thumb"):
            raise InputError("%s: mode must be arm or thumb, not '%s'" % (where, tok))
        return tok
    if kind == "module":
        if not re.match(r"^(main|itcm|dtcm|ov[0-9]{3}|a7[cwm])$", tok):
            raise InputError("%s: unknown module '%s'" % (where, tok))
        return tok
    if not re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", tok):
        raise InputError("%s: bad name '%s'" % (where, tok))
    return tok


def read_list(path, fields):
    """Read an address list; raise InputError on a bad line or a missing file."""
    try:
        with open(path, encoding="utf-8") as f:
            lines = f.read().splitlines()
    except OSError as e:
        raise InputError("%s: cannot read (%s)" % (path, e.strerror))
    need = sum(1 for k in fields if not k.endswith("?"))
    out = []
    for n, raw in enumerate(lines, 1):
        toks = raw.split("#", 1)[0].split()
        if not toks:
            continue
        where = "%s:%d" % (path, n)
        if len(toks) < need or len(toks) > len(fields):
            raise InputError("%s: expected %s" % (where, " ".join("<%s>" % k for k in fields)))
        e = Entry(path, n)
        for kind, tok in zip(fields, toks):
            kind = kind.rstrip("?")
            setattr(e, kind, _parse_field(kind, tok, where))
        out.append(e)
    return out


class Sym:
    __slots__ = ("name", "kind", "mode", "size", "unknown", "addr", "local")

    def __init__(self, name, kind, mode, size, unknown, addr, local):
        self.name, self.kind, self.mode, self.size = name, kind, mode, size
        self.unknown, self.addr, self.local = unknown, addr, local

    def __repr__(self):
        return "Sym(%s %s %s %#x)" % (self.name, self.kind, self.mode, self.addr)


_KIND = re.compile(r"^kind:(function)\((arm|thumb),size=0x([0-9a-fA-F]+)(,unknown)?\)$"
                   r"|^kind:(data)\((?:any|word|byte\[(?:0x[0-9a-fA-F]+|[0-9]+)\])\)$"
                   r"|^kind:(bss)$"
                   r"|^kind:(label)\((arm|thumb)\)$")


def read_symbols(path):
    """Read a dsd symbols.txt into a list of Sym, in file order."""
    try:
        with open(path, encoding="utf-8") as f:
            lines = f.read().splitlines()
    except OSError as e:
        raise InputError("%s: cannot read (%s)" % (path, e.strerror))
    out = []
    for n, raw in enumerate(lines, 1):
        toks = raw.split()
        if not toks:
            continue
        where = "%s:%d" % (path, n)
        name, kind, addr, local = toks[0], None, None, False
        mode, size, unknown = None, 0, False
        for t in toks[1:]:
            if t.startswith("kind:"):
                m = _KIND.match(t)
                if not m:
                    raise InputError("%s: unknown symbol kind '%s'" % (where, t))
                if m.group(1):
                    kind, mode, size, unknown = "function", m.group(2), int(m.group(3), 16), bool(m.group(4))
                elif m.group(5):
                    kind = "data"
                elif m.group(6):
                    kind = "bss"
                else:
                    kind, mode = "label", m.group(8)
            elif t.startswith("addr:"):
                v = t[5:]
                if not re.match(r"^0x[0-9a-fA-F]{1,8}$", v):
                    raise InputError("%s: bad address '%s'" % (where, v))
                addr = int(v, 16)
            elif t == "local":
                local = True
            elif t == "ambiguous":
                pass
            else:
                raise InputError("%s: unknown field '%s'" % (where, t))
        if kind is None or addr is None:
            raise InputError("%s: expected <name> kind:<kind> addr:0x<hex>" % where)
        out.append(Sym(name, kind, mode, size, unknown, addr, local))
    return out


class Module:
    """One piece of code at its run address: main, itcm, dtcm or an overlay."""

    def __init__(self, name, base, data, ovl_id=-1, ctor_start=0, ctor_end=0, symbols=()):
        self.name = name
        self.base = base
        self.data = bytearray(data)
        self.ovl_id = ovl_id
        self.ctor_start = ctor_start
        self.ctor_end = ctor_end
        self.symbols = sorted(symbols, key=lambda s: s.addr)

    @property
    def end(self):
        return self.base + len(self.data)

    def contains(self, addr, n=1):
        return self.base <= addr and addr + n <= self.end

    def word(self, addr):
        o = addr - self.base
        return int.from_bytes(self.data[o:o + 4], "little")

    def half(self, addr):
        o = addr - self.base
        return int.from_bytes(self.data[o:o + 2], "little")

    def set_word(self, addr, v):
        o = addr - self.base
        self.data[o:o + 4] = v.to_bytes(4, "little")

    def __repr__(self):
        return "Module(%s %#x+%#x)" % (self.name, self.base, len(self.data))


def _yaml(path):
    try:
        with open(path, encoding="utf-8") as f:
            return yaml.safe_load(f) or {}
    except OSError as e:
        raise InputError("%s: cannot read (%s)" % (path, e.strerror))
    except yaml.YAMLError as e:
        raise InputError("%s: bad yaml (%s)" % (path, str(e).splitlines()[0]))


def _bin(path):
    try:
        with open(path, "rb") as f:
            return f.read()
    except OSError as e:
        raise InputError("%s: cannot read (%s)" % (path, e.strerror))


def _syms(path):
    return read_symbols(path) if os.path.exists(path) else []


def load_modules(extract_dir, config_dir):
    """ARM9 modules in table order: main, itcm, dtcm, then overlays by id."""
    a9 = os.path.join(extract_dir, "arm9")
    mods = [Module("main", _yaml(os.path.join(a9, "arm9.yaml"))["base_address"],
                   _bin(os.path.join(a9, "arm9.bin")), symbols=_syms(os.path.join(config_dir, "symbols.txt")))]
    for name in ("itcm", "dtcm"):
        y = os.path.join(a9, name + ".yaml")
        if not os.path.exists(y):
            continue
        info = _yaml(y)
        data = _bin(os.path.join(a9, name + ".bin"))[:info.get("code_size", 1 << 30)]
        mods.append(Module(name, info["base_address"], data,
                           symbols=_syms(os.path.join(config_dir, name, "symbols.txt"))))
    ovdir = os.path.join(extract_dir, "arm9_overlays")
    oy = os.path.join(ovdir, "overlays.yaml")
    if os.path.exists(oy):
        ovs = sorted(_yaml(oy).get("overlays") or [], key=lambda o: o["id"])
        for o in ovs:
            name = "ov%03d" % o["id"]
            data = _bin(os.path.join(ovdir, o.get("file_name", name + ".bin")))[:o["code_size"]]
            mods.append(Module(name, o["base_address"], data, o["id"], o.get("ctor_start", 0), o.get("ctor_end", 0),
                               _syms(os.path.join(config_dir, "overlays", name, "symbols.txt"))))
    return mods
