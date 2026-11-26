"""ARM9 driver: read the inputs, find the code, translate it, write the files."""
import sys

import discover
import emit
import inputs
import ops_h
import output


def read_inputs(a):
    lists = {}
    for key, path, fields in (("hle", a.hle, inputs.HLE), ("interpret", a.interpret, inputs.INTERPRET),
                              ("patch", a.patch, inputs.PATCHES), ("hooks", a.hooks, inputs.HOOKS),
                              ("tags", a.tags, inputs.TAGS)):
        lists[key] = inputs.read_list(path, fields) if path else []
    mods = inputs.load_modules(a.extract, a.config)
    return mods, lists


class Ctx:
    def __init__(self, prog, tags):
        self.prog = prog
        self.tags = tags
        self.stats = emit.Stats()
        self.problems = []

    def resolve(self, mod, addr):
        return discover.resolve(self.prog, mod, addr)


def entry_code(ctx, f):
    """C text of one table entry, and its counters (or None)."""
    if f.kind == "hle":
        if f.hle_name:
            return ["void %s(Cpu* c) { hle_%s(c); }" % (f.cname, f.hle_name)], None
        return [], None
    if f.kind == "interp":
        return ["void %s(Cpu* c) { interp_stub(c, %s); }" % (f.cname, emit.hx(f.taddr))], None
    if f.kind == "empty":
        return ["void %s(Cpu* c) { RECOMP_ENTER(%s); }" % (f.cname, emit.hx(f.taddr))], None
    if f.kind == "hook":
        fe = emit.FuncEmitter(ctx, f, f.cname + "_orig")
        lines = fe.emit()
        lines.append("void %s(Cpu* c) { hook_%s(c, %s_orig); }" % (f.cname, f.hook_name, f.cname))
        return lines, fe.counts
    fe = emit.FuncEmitter(ctx, f, f.cname)
    return fe.emit(), fe.counts


def file_text(ctx, entries, funcs_h, prelude=()):
    externs = set()
    body = []
    for f in entries:
        if f.kind == "hle" and f.hle_name:
            externs.add("void hle_%s(Cpu* c);" % f.hle_name)
        elif f.kind == "hook":
            externs.add("void hook_%s(Cpu* c, void (*orig)(Cpu* c));" % f.hook_name)
        lines, _ = entry_code(ctx, f)
        body.extend(lines)
    head = list(prelude) + ['#include "%s"' % funcs_h, '#include "recomp_ops.h"',
                            "void interp_stub(Cpu* c, uint32_t addr);"]
    head.extend(sorted(externs))
    return "\n".join(head + [""] + body) + "\n"


def run(a, log=print):
    mods, lists = read_inputs(a)
    discover.apply_patches(mods, lists["patch"], log=log)
    prog = discover.build_table(mods, lists["hle"], lists["interpret"], lists["hooks"])
    discover.discover(prog)
    tags = {}
    for t in lists["tags"]:
        tags[(t.module, t.addr)] = t.tag
    ctx = Ctx(prog, tags)
    table = prog.ordered()
    files = {"recomp_ops.h": ops_h.TEXT}
    decl = ["#pragma once", '#include "cpu.h"']
    decl += ["void %s(Cpu* c);" % f.cname for f in table]
    files["funcs.h"] = "\n".join(decl) + "\n"
    rows = ['#include "funcs.h"', '#include "dispatch.h"', "const RecompFunc g_recomp_funcs[] = {"]
    for f in table:
        rows.append("    { %d, %s, %du, %s, %s }," % (f.mod.ovl_id, emit.hx(f.taddr), f.size, f.cname, output.c_string(f.name)))
    rows.append("};")
    rows.append("const unsigned g_recomp_func_count = sizeof g_recomp_funcs / sizeof g_recomp_funcs[0];")
    files["dispatch_table.c"] = "\n".join(rows) + "\n"
    for mod_name, entries in output.chunks(table, a.per_file):
        n = sum(1 for k in files if k.startswith(mod_name + "_"))
        files["%s_%02d.c" % (mod_name, n)] = file_text(ctx, entries, "funcs.h")
    st = ctx.stats.v
    st["functions"] = sum(1 for f in table if f.kind in ("code", "secondary", "hook", "empty"))
    st["hle_functions"] = sum(1 for f in table if f.kind == "hle")
    st["hooked_functions"] = sum(1 for f in table if f.kind == "hook")
    st["interpreted_functions"] = sum(1 for f in table if f.kind == "interp")
    st["secondary_entries"] = sum(1 for f in table if f.kind == "secondary")
    problems = prog.problems + ctx.problems
    files["report.txt"] = output.report(st, problems)
    for k in sorted(st):
        log("%-24s %d" % (k, st[k]))
    output.write_all(a.out, files)
    nc = sum(1 for k in files if k.endswith(".c"))
    log("%d C files written to %s" % (nc, a.out))
    sys.stdout.flush()
    return 0
