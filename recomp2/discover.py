"""ARM9 code discovery: the function table and the code of each function.

Table entries come from the symbol map (functions), the overlays' static
constructor tables (GBATEK "DS Cartridge" overlay table: ctor start/end),
interpret.txt, and secondary entries: a direct branch target that lands
inside another function (not at its start) becomes an entry of its own.
"""
import sys

import flow
from insn import AL


class Func:
    """One entry of the function table."""

    def __init__(self, mod, addr, thumb, size, name, kind="code"):
        self.mod = mod
        self.addr = addr          # without the Thumb bit
        self.thumb = thumb
        self.size = size
        self.name = name
        self.kind = kind          # code secondary hle hook interp empty
        self.hle_name = None
        self.hook_name = None
        self.body = None
        self.parent = None        # top-level function of a secondary entry

    @property
    def end(self):
        return self.addr + self.size

    @property
    def taddr(self):
        return self.addr | (1 if self.thumb else 0)

    @property
    def cname(self):
        return "fn_%s_%08x" % (self.mod.name, self.addr)

    @property
    def translated(self):
        return self.kind in ("code", "secondary", "hook")

    def __repr__(self):
        return "Func(%s %s %s)" % (self.cname, self.kind, self.name)


def warn(msg):
    print("warning: " + msg, file=sys.stderr)


def _find_mod(mods, name):
    for m in mods:
        if m.name == name:
            return m
    return None


def apply_patches(mods, patches, log=print):
    for p in patches:
        m = _find_mod(mods, p.module)
        if m is None or not m.contains(p.addr, 4):
            warn("%s: no code at %s %08x, patch ignored" % (p.where(), p.module, p.addr))
            continue
        old = m.word(p.addr)
        m.set_word(p.addr, p.word)
        log("patch %s %08x: %08x -> %08x" % (p.module, p.addr, old, p.word))


class Program:
    def __init__(self, mods):
        self.mods = mods
        self.funcs = []
        self.by_mod = {m.name: {} for m in mods}   # module name -> {addr: Func}
        self.problems = []
        self.stats = {}

    def add(self, f):
        self.by_mod[f.mod.name][f.addr] = f
        self.funcs.append(f)

    def entry(self, mod, addr):
        return self.by_mod[mod.name].get(addr)

    def ordered(self):
        out = []
        for m in self.mods:
            d = self.by_mod[m.name]
            out.extend(d[a] for a in sorted(d))
        return out


def _symbol_at(mod, addr):
    for s in mod.symbols:
        if s.addr == addr:
            return s
    return None


def build_table(mods, hle=(), interpret=(), hooks=()):
    prog = Program(mods)
    for m in mods:
        for s in m.symbols:
            if s.kind != "function":
                continue
            if s.unknown:
                f = Func(m, s.addr, s.mode == "thumb", 0, s.name, "empty")
            else:
                f = Func(m, s.addr, s.mode == "thumb", s.size, s.name)
            prog.add(f)
        # static constructors of an overlay: ARM functions listed by address
        if m.ovl_id >= 0 and m.ctor_start and m.ctor_end > m.ctor_start:
            addrs = sorted({m.word(a) for a in range(m.ctor_start, m.ctor_end, 4) if m.contains(a, 4)} - {0})
            for a in addrs:
                if prog.entry(m, a & ~1) or not m.contains(a & ~1, 2):
                    continue
                s = _symbol_at(m, a & ~1)
                nxt = [x.addr for x in m.symbols if x.addr > (a & ~1)]
                size = (min(nxt) if nxt else m.end) - (a & ~1)
                prog.add(Func(m, a & ~1, bool(a & 1), size, s.name if s else "ctor_%08x" % a))
    for e in interpret:
        m = _find_mod(mods, e.module)
        if m is None:
            warn("%s: unknown module %s" % (e.where(), e.module))
            continue
        f = prog.entry(m, e.addr)
        if f is None:
            s = _symbol_at(m, e.addr)
            if s is None and not m.contains(e.addr):
                warn("%s: no code at %s %08x, ignored" % (e.where(), e.module, e.addr))
                continue
            f = Func(m, e.addr, False, 0, s.name if s else "func_%08x" % e.addr, "interp")
            prog.add(f)
        else:
            f.kind, f.size = "interp", 0
    for e in hle:
        m = _find_mod(mods, e.module)
        f = prog.entry(m, e.addr) if m else None
        if f is None:
            warn("%s: no function at %s %08x, ignored" % (e.where(), e.module, e.addr))
            continue
        f.kind, f.hle_name = "hle", e.name
    for e in hooks:
        m = _find_mod(mods, e.module)
        f = prog.entry(m, e.addr) if m else None
        if f is None or not f.translated:
            warn("%s: no translated function at %s %08x, ignored" % (e.where(), e.module, e.addr))
            continue
        f.kind, f.hook_name = "hook", e.name
    return prog


# ---- resolution of direct branch targets (calls, tail jumps) ------------------------

def static_mods(prog):
    return [m for m in prog.mods if m.ovl_id < 0]


def resolve(prog, mod, target):
    """Entries a direct branch from `mod` to `target` may reach: [] = dispatch."""
    a = target & ~1
    f = prog.entry(mod, a)
    if f is not None:
        return [f]
    for m in static_mods(prog):
        f = prog.entry(m, a)
        if f is not None:
            return [f]
    return [prog.entry(m, a) for m in prog.mods if m.ovl_id >= 0 and prog.entry(m, a) is not None]


def _containing(prog, mod, addr):
    """Top-level function of `mod` whose code range contains `addr` (not at its start)."""
    best = None
    for f in prog.by_mod[mod.name].values():
        if f.kind == "secondary" or f.size == 0:
            continue
        if f.addr < addr < f.end and (best is None or f.addr > best.addr):
            best = f
    return best


def _target_mods(prog, mod, a):
    if mod.contains(a):
        return [mod]
    for m in static_mods(prog):
        if m.contains(a):
            return [m]
    return [m for m in prog.mods if m.ovl_id >= 0 and m.contains(a)]


def _branch_targets(f):
    """Direct branch targets leaving the function: (address, is_call)."""
    out = []
    for i in f.body.insns.values():
        if i.op == "b":
            if i.link or not (f.addr <= i.target < f.end):
                out.append(i.target)
    for _kind, slots, _reg in f.body.tables.values():
        out.extend(t for t in slots if not (f.addr <= t < f.end))
    return out


def _label_syms(f):
    """Label symbols of the map inside the function: they start blocks too."""
    return [s.addr for s in f.mod.symbols if s.kind == "label" and f.addr < s.addr < f.end]


def discover(prog):
    """Follow the code of every translated entry; add secondary entries until stable."""
    pending = [f for f in prog.funcs if f.translated]
    while pending:
        for f in pending:
            f.body = flow.analyze(f.mod, f.addr, f.end, f.thumb, _label_syms(f))
        new = []
        for f in pending:
            for t in _branch_targets(f):
                a = t & ~1
                if resolve(prog, f.mod, a):
                    continue
                for m in _target_mods(prog, f.mod, a):
                    p = _containing(prog, m, a)
                    if p is None or p.kind not in ("code", "hook"):
                        continue
                    if prog.entry(m, a):
                        continue
                    s = Func(m, a, p.thumb, p.end - a, "%s+0x%x" % (p.name, a - p.addr), "secondary")
                    s.parent = p
                    prog.add(s)
                    new.append(s)
        pending = new
    for f in prog.funcs:
        if f.translated:
            for p in f.body.problems:
                prog.problems.append("%s: %s" % (f.cname, p))
    return prog
