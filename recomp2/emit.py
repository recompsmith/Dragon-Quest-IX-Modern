"""C for one translated function.

Model: a game call is a C call, a game return is a C return. Each block
(see flow.py) starts with a label and RECOMP_CYC(sum of the block's costs).
Instruction semantics follow the ARM ARM (ARM DDI 0100I), chapter A4 for each
instruction and A5 for the addressing modes; conditions are A3.2.
"""
from insn import CONDS, DP_LOGIC, DP_TEST, AL

COND_MACRO = {n: "COND_" + CONDS[n].upper() for n in range(14)}


def hx(v):
    return "0x%08xu" % (v & 0xFFFFFFFF)


class Stats:
    KEYS = ("functions", "hle_functions", "hooked_functions", "indirect_calls", "indirect_jumps", "insns",
            "interpreted_functions", "jump_tables", "multi_overlay_calls", "secondary_entries", "thumb_offset_tables")

    def __init__(self):
        self.v = {k: 0 for k in self.KEYS}

    def add(self, k, n=1):
        self.v[k] = self.v.get(k, 0) + n


class FuncEmitter:
    """Emit one function. `ctx` gives: resolve(mod, addr) -> [Func], tags {(mod name, addr): tag},
    stats (Stats), problems (list)."""

    def __init__(self, ctx, f, cname):
        self.ctx, self.f, self.cname = ctx, f, cname
        self.b = f.body
        self.mod = f.mod
        self.out = []
        self.labels = sorted(self.b.labels)
        self.cycles = dict(self.b.blocks())
        self.needs_lr = self._needs_entry_lr()
        self.counts = {"loops": 0, "calls": 0, "multi": 0, "dispatch": 0, "switch": 0, "unhandled": 0}

    # ---- helpers -------------------------------------------------------------------
    def w(self, s):
        self.out.append(s)

    def reg(self, n, i):
        return hx(i.pcval) if n == 15 else "c->r[%d]" % n

    def in_func(self, a):
        return self.f.addr <= a < self.f.end

    def _needs_entry_lr(self):
        """True if the code may plant a return address in r14 itself (mov/ldr lr, ...).

        A BL sets r14 to a return point inside this function and an LDM/POP
        restores our caller's r14: a jump after those returns where we would.
        An explicit write of r14 means the jump returns somewhere else."""
        for i in self.b.insns.values():
            if i.op == "dp" and i.rd == 14 and i.dp_op not in DP_TEST:
                return True
            if i.op == "mem" and i.load and i.rd == 14:
                return True
        return False

    def tail_dispatch(self, t, addr):
        """Leave the function through code found at run time (`t` = C expression)."""
        self.counts["dispatch"] += 1
        if self.needs_lr:
            self.counts["dispatch"] += 1
            self.w("{ uint32_t tl = c->r[14]; dispatch_call(c, %s, %s); if (tl != entry_lr) dispatch_call(c, tl, %s); }"
                   % (t, hx(addr), hx(addr)))
        else:
            self.w("dispatch_call(c, %s, %s);" % (t, hx(addr)))
        self.w("return;")

    def call_target(self, target, thumb_target, addr):
        """A direct call of the code at `target` (resolution of the spec, 6.3)."""
        a = target & ~1
        cands = self.ctx.resolve(self.mod, a)
        t = a | (1 if thumb_target else 0)
        if len(cands) == 1:
            self.counts["calls"] += 1
            return "%s(c);" % cands[0].cname
        if not cands:
            self.counts["dispatch"] += 1
            return "dispatch_call(c, %s, %s);" % (hx(t), hx(addr))
        self.counts["multi"] += 1
        self.counts["calls"] += len(cands)
        self.counts["dispatch"] += 1
        self.ctx.stats.add("multi_overlay_calls")
        s = ""
        for k, g in enumerate(cands):
            s += "%sif (overlay_is_loaded(%d)) %s(c); " % ("else " if k else "", g.mod.ovl_id, g.cname)
        return s + "else dispatch_call(c, %s, %s);" % (hx(t), hx(addr))

    def tail_call(self, target, thumb_target, addr):
        """A jump to another function: call it, then return to our caller."""
        call = self.call_target(target, thumb_target, addr)
        if self.needs_lr:
            self.counts["dispatch"] += 1
            self.w("{ uint32_t tl = c->r[14]; %s if (tl != entry_lr) dispatch_call(c, tl, %s); }" % (call, hx(addr)))
        else:
            self.w(call)
        self.w("return;")

    def jump_to(self, target, thumb_target, addr):
        if self.in_func(target) and target in self.b.insns:
            if target <= addr:
                self.counts["loops"] += 1
                self.w("RECOMP_LOOP();")
            self.w("goto L_%08x;" % target)
        else:
            self.tail_call(target, thumb_target, addr)

    def indirect_jump(self, t, addr):
        """PC written with a value known only at run time: a switch on the function's labels."""
        self.ctx.stats.add("indirect_jumps")
        self.counts["switch"] += 1
        self.w("switch (%s & ~1u) {" % t)
        for lab in self.labels:
            self.w("case %s: goto L_%08x;" % (hx(lab), lab))
        self.w("default:")
        self.tail_dispatch(t, addr)
        self.w("}")

    def unhandled(self, i):
        self.counts["unhandled"] += 1
        self.w("cpu_unhandled(c, %s, %s); return;" % (hx(i.addr), hx(i.word)))

    # ---- whole function --------------------------------------------------------------
    def emit(self):
        f = self.f
        self.w("void %s(Cpu* c) {" % self.cname)
        self.w("RECOMP_ENTER(%s);" % hx(f.taddr))
        if self.needs_lr:
            self.w("uint32_t entry_lr = c->r[14];")
        order = self.b.order()
        for n, a in enumerate(order):
            i = self.b.insns[a]
            if a in self.cycles:
                self.w("L_%08x: RECOMP_CYC(%d);" % (a, self.cycles[a]))
            falls = self.insn(i)
            nxt = order[n + 1] if n + 1 < len(order) else None
            if falls and nxt != a + i.size:
                self.w("return;")
        body = ["    " + x for x in self.out[1:]]
        return [self.out[0]] + body + ["}"]

    def insn(self, i):
        """Emit one instruction; True if control may continue with the next one."""
        self.ctx.stats.add("insns")
        if i.cond != AL and i.cond < 14:
            self.w("if (%s) {" % COND_MACRO[i.cond])
            self.body(i)
            self.w("}")
            return True
        return self.body(i)

    def body(self, i):
        op = i.op
        fn = getattr(self, "op_" + op)
        r = fn(i)
        return True if r is None else r

    # ---- data processing (A4.1, A5.1) ----------------------------------------------------
    def shifter(self, i, need_carry):
        """(statements, value expr, carry expr or None) for the second operand."""
        o = i.op2
        if o[0] == "imm":
            return [], hx(o[1]), (str(o[2]) + "u" if (o[2] is not None and need_carry) else None)
        if o[0] == "rrx":
            rm = self.reg(o[1], i)
            st = ["uint32_t rx = %s;" % rm]
            return st, "((c->cf << 31) | (rx >> 1))", ("(rx & 1u)" if need_carry else None)
        if o[0] == "reg":
            _, rm, sh, n = o
            rm = self.reg(rm, i)
            if sh == "lsl":
                if n == 0:
                    return [], rm, None
                if need_carry:
                    return ["uint32_t co;", "uint32_t sv = r2_lsl_ic(%s, %d, &co);" % (rm, n)], "sv", "co"
                return [], "(%s << %d)" % (rm, n), None
            n = n or 32
            if sh == "lsr":
                if need_carry:
                    return ["uint32_t co;", "uint32_t sv = r2_lsr_ic(%s, %d, &co);" % (rm, n)], "sv", "co"
                return [], ("0u" if n == 32 else "(%s >> %d)" % (rm, n)), None
            if sh == "asr":
                if need_carry:
                    return ["uint32_t co;", "uint32_t sv = r2_asr_ic(%s, %d, &co);" % (rm, n)], "sv", "co"
                return [], "r2_asr_i(%s, %d)" % (rm, n), None
            # ror 1..31
            if need_carry:
                return ["uint32_t co;", "uint32_t sv = r2_ror_ic(%s, %d, &co);" % (rm, n)], "sv", "co"
            return [], "r2_ror(%s, %d)" % (rm, n), None
        # register-specified shift
        _, rm, sh, rs = o
        st = ["uint32_t co = c->cf;",
              "uint32_t sv = r2_%s_r(%s, %s, &co);" % (sh, self.reg(rm, i), self.reg(rs, i))]
        return st, "sv", ("co" if need_carry else None)

    def op_dp(self, i):
        o = i.dp_op
        logic = o in DP_LOGIC
        st, b, carry = self.shifter(i, i.s and logic)
        a = self.reg(i.rn, i)
        s = i.s
        if o == "and":
            e = "%s & %s" % (a, b)
        elif o == "eor" or o == "teq":
            e = "%s ^ %s" % (a, b)
        elif o == "tst":
            e = "%s & %s" % (a, b)
        elif o == "orr":
            e = "%s | %s" % (a, b)
        elif o == "bic":
            e = "%s & ~%s" % (a, b)
        elif o == "mov":
            e = b
        elif o == "mvn":
            e = "~%s" % b
        elif o in ("add", "cmn"):
            e = "r2_adds(c, %s, %s)" % (a, b) if (s or o == "cmn") else "%s + %s" % (a, b)
        elif o == "adc":
            e = "r2_adcs(c, %s, %s, c->cf)" % (a, b) if s else "%s + %s + c->cf" % (a, b)
        elif o in ("sub", "cmp"):
            e = "r2_subs(c, %s, %s)" % (a, b) if (s or o == "cmp") else "%s - %s" % (a, b)
        elif o == "rsb":
            e = "r2_subs(c, %s, %s)" % (b, a) if s else "%s - %s" % (b, a)
        elif o == "sbc":
            e = "r2_sbcs(c, %s, %s, c->cf)" % (a, b) if s else "%s - %s - 1u + c->cf" % (a, b)
        else:  # rsc
            e = "r2_sbcs(c, %s, %s, c->cf)" % (b, a) if s else "%s - %s - 1u + c->cf" % (b, a)
        lines = list(st)
        if logic and s:
            e = "r2_nz(c, %s)" % e
        if o in DP_TEST:
            lines.append("(void)%s;" % e)
            if carry:
                lines.append("c->cf = %s;" % carry)
            self.w("{ " + " ".join(lines) + " }" if len(lines) > 1 else lines[0])
            return True
        if i.rd != 15:
            if carry:
                lines.append("uint32_t res = %s;" % e)
                lines.append("c->cf = %s;" % carry)
                lines.append("c->r[%d] = res;" % i.rd)
            else:
                lines.append("c->r[%d] = %s;" % (i.rd, e))
            self.w("{ " + " ".join(lines) + " }" if len(lines) > 1 else lines[0])
            return True
        # PC written by a data-processing instruction
        lines.append("uint32_t t = %s;" % e)
        if carry:
            lines.append("c->cf = %s;" % carry)
        self.w("{ " + " ".join(lines))
        r = self.pc_write(i, "t")
        self.w("}")
        return r

    def pc_write(self, i, t):
        """Control flow for a computed PC value (C expression `t`)."""
        a = i.addr
        if i.op == "dp" and i.s:  # MOVS/SUBS pc, lr: return from an exception
            self.w("cpu_exception_return(c, %s);" % t)
            self.w("return;")
            return False
        if a in self.b.tables:
            return self.table(i)
        if i.op == "dp" and i.dp_op == "mov" and i.op2 == ("reg", 14, "lsl", 0):
            self.w("return;")
            return False
        if a in self.b.calls_after_mov_lr:
            self.ctx.stats.add("indirect_calls")
            self.counts["dispatch"] += 1
            self.w("dispatch_call(c, %s, %s);" % (t, hx(a)))
            return True
        self.indirect_jump(t, a)
        return False

    def table(self, i):
        kind, slots, reg = self.b.tables[i.addr]
        if kind == "thumb":
            self.ctx.stats.add("thumb_offset_tables")
            self.counts["switch"] += 1
            self.w("switch (t & ~1u) {")
            for lab in self.labels:
                self.w("case %s: goto L_%08x;" % (hx(lab), lab))
            self.w("default:")
            self.tail_dispatch("t", i.addr)
            self.w("}")
            return False
        self.ctx.stats.add("jump_tables")
        self.counts["switch"] += 1
        self.w("switch (c->r[%d]) {" % reg)
        for n, s in enumerate(slots):
            if self.in_func(s) and s in self.b.insns:
                self.w("case %d: goto L_%08x;" % (n, s))
            else:
                self.w("case %d: {" % n)
                self.tail_call(s, self.f.thumb, i.addr)
                self.w("}")
        self.w("default:")
        self.unhandled(i)
        self.w("}")
        return False

    # ---- loads and stores (A4.1.23 ..., A5.2, A5.3) -------------------------------------
    def offset(self, i):
        o = i.ofs
        if o[0] == "imm":
            return [], hx(o[1]) if o[1] else None
        if o[0] == "rrx":
            return [], "((c->cf << 31) | (%s >> 1))" % self.reg(o[1], i)
        _, rm, sh, n = o
        rm = self.reg(rm, i)
        if sh == "lsl":
            return [], (rm if n == 0 else "(%s << %d)" % (rm, n))
        n = n or 32
        if sh == "lsr":
            return [], ("0u" if n == 32 else "(%s >> %d)" % (rm, n))
        if sh == "asr":
            return [], "r2_asr_i(%s, %d)" % (rm, n)
        return [], "r2_ror(%s, %d)" % (rm, n)

    def literal(self, i):
        """Value of ``ldr rd, [pc, #imm]`` read from the image, or None."""
        if not (i.rn == 15 and i.ofs[0] == "imm" and i.pre and not i.wb and i.load and i.msize == "w"):
            return None
        a = (i.pcval + i.ofs[1]) if i.add else (i.pcval - i.ofs[1])
        if not self.mod.contains(a & ~3, 4):
            return None
        v = self.mod.word(a & ~3)
        r = (a & 3) * 8
        return ((v >> r) | (v << (32 - r))) & 0xFFFFFFFF if r else v

    def op_mem(self, i):
        lit = self.literal(i)
        if lit is not None:
            if i.rd == 15:
                self.w("{ uint32_t t = %s;" % hx(lit))
                r = self.pc_write(i, "t")
                self.w("}")
                return r
            self.w("c->r[%d] = %s;" % (i.rd, hx(lit)))
            return True
        st, off = self.offset(i)
        base = self.reg(i.rn, i)
        lines = list(st)
        lines.append("uint32_t base = %s;" % base)
        if off is None:
            moved = "base"
        else:
            moved = "base %s %s" % ("+" if i.add else "-", off)
        addr = moved if i.pre else "base"
        lines.append("uint32_t ad = %s;" % addr)
        wb = None
        if i.wb:
            wb = "c->r[%d] = %s;" % (i.rn, "ad" if i.pre else moved)
        size = i.msize
        if not i.load:
            if size == "d":
                lines.append("uint32_t v0 = %s, v1 = %s;" % (self.reg(i.rd, i), self.reg(i.rd + 1, i)))
                lines.append("mem_write32(ad & ~3u, v0); mem_write32((ad + 4u) & ~3u, v1);")
            else:
                v = hx(i.addr + 12) if i.rd == 15 else self.reg(i.rd, i)
                lines.append("uint32_t v = %s;" % v)
                if size == "w":
                    lines.append("mem_write32(ad & ~3u, v);")
                elif size == "b":
                    lines.append("mem_write8(ad, (uint8_t)v);")
                else:
                    lines.append("mem_write16(ad, (uint16_t)v);")
            if wb:
                lines.append(wb)
            self.w("{ " + " ".join(lines) + " }")
            return True
        if size == "d":
            lines.append("uint32_t v0 = mem_read32(ad & ~3u); uint32_t v1 = mem_read32((ad + 4u) & ~3u);")
            if wb:
                lines.append(wb)
            lines.append("c->r[%d] = v0; c->r[%d] = v1;" % (i.rd, i.rd + 1))
            self.w("{ " + " ".join(lines) + " }")
            return True
        rd_expr = {"w": "r2_ldr32(ad)", "b": "mem_read8(ad)", "h": "mem_read16(ad)",
                   "sb": "r2_sx8(mem_read8(ad))", "sh": "r2_sx16(mem_read16(ad))"}[size]
        lines.append("uint32_t v = %s;" % rd_expr)
        if wb:
            lines.append(wb)
        if i.rd != 15:
            lines.append("c->r[%d] = v;" % i.rd)
            self.w("{ " + " ".join(lines) + " }")
            return True
        self.w("{ " + " ".join(lines))
        if i.rn == 13 and i.ofs == ("imm", 4) and not i.pre and i.add:  # ldr pc, [sp], #4: a return
            self.w("(void)v; return;")
            self.w("}")
            return False
        r = self.pc_write(i, "v")
        self.w("}")
        return r

    def op_ldm(self, i):
        return self.block_transfer(i, True)

    def op_stm(self, i):
        return self.block_transfer(i, False)

    def block_transfer(self, i, load):
        regs = [r for r in range(16) if i.regs >> r & 1]
        n = len(regs)
        if n == 0:
            self.unhandled(i)
            return False
        start = {"ia": "base", "ib": "base + 4u", "da": "base - %du" % (4 * n - 4), "db": "base - %du" % (4 * n)}[i.mode]
        after = {"ia": "base + %du" % (4 * n), "ib": "base + %du" % (4 * n),
                 "da": "base - %du" % (4 * n), "db": "base - %du" % (4 * n)}[i.mode]
        lines = ["uint32_t base = %s;" % self.reg(i.rn, i), "uint32_t ad = (%s) & ~3u;" % start]
        if load:
            for k, r in enumerate(regs):
                a = "ad" if k == 0 else "ad + %du" % (4 * k)
                if r == 15:
                    lines.append("uint32_t pcv = mem_read32(%s);" % a)
                else:
                    lines.append("c->r[%d] = mem_read32(%s);" % (r, a))
            if i.wb and not (i.regs >> i.rn & 1):
                lines.append("c->r[%d] = %s;" % (i.rn, after))
        else:
            vals = []
            for k, r in enumerate(regs):
                vals.append("uint32_t v%d = %s;" % (k, hx(i.addr + 12) if (r == 15 and not i.thumb) else self.reg(r, i)))
            lines.extend(vals)
            for k, r in enumerate(regs):
                a = "ad" if k == 0 else "ad + %du" % (4 * k)
                lines.append("mem_write32(%s, v%d);" % (a, k))
            if i.wb:
                lines.append("c->r[%d] = %s;" % (i.rn, after))
        if not (load and i.regs & 0x8000):
            self.w("{ " + " ".join(lines) + " }")
            return True
        self.w("{ " + " ".join(lines))
        if i.user:
            self.w("cpu_exception_return(c, pcv);")
        else:
            self.w("(void)pcv;")
        self.w("return;")
        self.w("}")
        return False

    def op_swp(self, i):
        rn, rm = self.reg(i.rn, i), self.reg(i.rm, i)
        if i.byte:
            self.w("{ uint32_t ad = %s; uint32_t nv = %s; uint32_t v = mem_read8(ad); mem_write8(ad, (uint8_t)nv); c->r[%d] = v; }"
                   % (rn, rm, i.rd))
        else:
            self.w("{ uint32_t ad = %s; uint32_t nv = %s; uint32_t v = r2_ldr32(ad); mem_write32(ad & ~3u, nv); c->r[%d] = v; }"
                   % (rn, rm, i.rd))

    # ---- multiplies (A4.1.40, A4.1.35, A4.1.129, A4.1.87, A10) ------------------------------
    def op_mul(self, i):
        e = "%s * %s" % (self.reg(i.rm, i), self.reg(i.rs, i))
        if i.acc:
            e += " + %s" % self.reg(i.rn, i)
        if i.s:
            self.w("c->r[%d] = r2_nz(c, %s);" % (i.rd, e))
        else:
            self.w("c->r[%d] = %s;" % (i.rd, e))

    def op_mull(self, i):
        rm, rs = self.reg(i.rm, i), self.reg(i.rs, i)
        p = "r2_smull(%s, %s)" % (rm, rs) if i.signed else "(uint64_t)%s * (uint64_t)%s" % (rm, rs)
        lines = ["uint64_t p = %s;" % p]
        if i.acc:
            lines.append("p += ((uint64_t)c->r[%d] << 32) | c->r[%d];" % (i.rdhi, i.rdlo))
        lines.append("c->r[%d] = (uint32_t)p; c->r[%d] = (uint32_t)(p >> 32);" % (i.rdlo, i.rdhi))
        if i.s:
            lines.append("c->n = (uint32_t)(p >> 63); c->z = (p == 0);")
        self.w("{ " + " ".join(lines) + " }")

    def op_dsp(self, i):
        rm, rs = self.reg(i.rm, i), self.reg(i.rs, i)
        k = i.kind
        if k in ("smul", "smla", "smlal"):
            prod = "r2_half(%s, %d) * r2_half(%s, %d)" % (rm, i.x, rs, i.y)
        if k == "smul":
            self.w("c->r[%d] = %s;" % (i.rd, prod))
        elif k == "smla":
            self.w("c->r[%d] = r2_addq(c, %s, %s);" % (i.rd, prod, self.reg(i.rn, i)))
        elif k == "smulw":
            self.w("c->r[%d] = r2_smulw(%s, r2_half(%s, %d));" % (i.rd, rm, rs, i.y))
        elif k == "smlaw":
            self.w("c->r[%d] = r2_addq(c, r2_smulw(%s, r2_half(%s, %d)), %s);" % (i.rd, rm, rs, i.y, self.reg(i.rn, i)))
        else:  # smlal: RdHi = rd, RdLo = rn
            self.w("{ uint64_t p = (uint64_t)r2_s64(%s) + (((uint64_t)c->r[%d] << 32) | c->r[%d]);"
                   " c->r[%d] = (uint32_t)p; c->r[%d] = (uint32_t)(p >> 32); }" % (prod, i.rd, i.rn, i.rn, i.rd))

    def op_q(self, i):
        rm, rn = self.reg(i.rm, i), self.reg(i.rn, i)
        k = i.kind
        if k == "qadd":
            e = "r2_qadd(c, %s, %s)" % (rm, rn)
        elif k == "qsub":
            e = "r2_qsub(c, %s, %s)" % (rm, rn)
        elif k == "qdadd":
            e = "r2_qadd(c, %s, r2_qdouble(c, %s))" % (rm, rn)
        else:
            e = "r2_qsub(c, %s, r2_qdouble(c, %s))" % (rm, rn)
        self.w("{ uint32_t v = %s; c->r[%d] = v; }" % (e, i.rd))

    def op_clz(self, i):
        self.w("c->r[%d] = r2_clz(%s);" % (i.rd, self.reg(i.rm, i)))

    # ---- system (A4.1.38, A4.1.39, A4.1.32, A4.1.13, A4.1.107) -------------------------------
    def op_mrs(self, i):
        self.w("c->r[%d] = %s;" % (i.rd, "cpu_read_spsr(c)" if i.spsr else "CPSR_GET(c)"))

    def op_msr(self, i):
        o = i.op2
        v = hx(o[1]) if o[0] == "imm" else self.reg(o[1], i)
        m = 0
        for bit, byte in ((1, 0xFF), (2, 0xFF00), (4, 0xFF0000), (8, 0xFF000000)):
            if i.mask & bit:
                m |= byte
        if i.spsr:
            self.w("cpu_write_spsr(c, %s, %s);" % (v, hx(m)))
            return
        lines = ["uint32_t v = %s;" % v]
        if i.mask & 8:
            lines.append("c->n = v >> 31; c->z = (v >> 30) & 1u; c->cf = (v >> 29) & 1u; c->v = (v >> 28) & 1u; c->q = (v >> 27) & 1u;")
        lines.append("cpu_write_cpsr_ctl(c, v, %s);" % hx(m & 0x07FFFFFF))
        self.w("{ " + " ".join(lines) + " }")

    def op_mcr(self, i):
        if i.cp != 15:
            self.unhandled(i)
            return False
        self.w("cp15_write(c, %d, %d, %d, %d, %s);" % (i.op1, i.crn, i.crm, i.cop2, self.reg(i.rd, i)))

    def op_mrc(self, i):
        if i.cp != 15 or i.rd == 15:
            self.unhandled(i)
            return False
        self.w("c->r[%d] = cp15_read(c, %d, %d, %d, %d);" % (i.rd, i.op1, i.crn, i.crm, i.cop2))

    def op_swi(self, i):
        n = i.imm if i.thumb else (i.imm >> 16) & 0xFF
        self.w("bios_swi(c, %d);" % n)

    def op_bkpt(self, i):
        self.w("cpu_breakpoint(c, %s, %d);" % (hx(i.addr), i.imm))

    def op_pld(self, i):
        pass

    def op_undef(self, i):
        self.ctx.problems.append("%08x: untranslatable instruction %08x" % (i.addr, i.word))
        self.unhandled(i)
        return False

    # ---- branches (A4.1.5, A4.1.8, A4.1.9, A4.1.10, A7.1.17) -----------------------------
    def op_b(self, i):
        tthumb = i.thumb != bool(i.exchange)
        if not i.link:
            self.jump_to(i.target, tthumb, i.addr)
            return False
        ret = (i.addr + i.size) | (1 if i.thumb else 0)
        call = self.call_target(i.target, tthumb, i.addr)
        self.w("c->r[14] = %s;" % hx(ret))
        tag = self.ctx.tags.get((self.mod.name, i.addr))
        if tag is not None:
            self.w("{ int gx_prev = gx_tag_begin(%d); %s gx_tag_end(gx_prev); }" % (tag, call))
        else:
            self.w(call)
        return True

    def op_bx(self, i):
        if i.link:
            self.ctx.stats.add("indirect_calls")
            self.counts["dispatch"] += 1
            ret = (i.addr + i.size) | (1 if i.thumb else 0)
            self.w("{ uint32_t t = %s; c->r[14] = %s; dispatch_call(c, t, %s); }" % (self.reg(i.rm, i), hx(ret), hx(i.addr)))
            return True
        if i.rm == 14:
            self.w("return;")
            return False
        self.w("{ uint32_t t = %s;" % self.reg(i.rm, i))
        if i.addr in self.b.calls_after_mov_lr:
            self.ctx.stats.add("indirect_calls")
            self.counts["dispatch"] += 1
            self.w("dispatch_call(c, t, %s); }" % hx(i.addr))
            return True
        self.indirect_jump("t", i.addr)
        self.w("}")
        return False

    def op_blhi(self, i):
        self.w("c->r[14] = %s;" % hx(i.value))

    def op_bllo(self, i):
        self.ctx.stats.add("indirect_calls")
        self.counts["dispatch"] += 1
        t = "(c->r[14] + %du) & ~3u" % i.offset if i.exchange else "(c->r[14] + %du) | 1u" % i.offset
        self.w("{ uint32_t t = %s; c->r[14] = %s; dispatch_call(c, t, %s); }" % (t, hx((i.addr + 2) | 1), hx(i.addr)))
