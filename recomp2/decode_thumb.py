"""Thumb (16-bit) instruction decoder, ARMv5T.

Encodings and ARM equivalents from the ARM Architecture Reference Manual
(ARM DDI 0100I), chapter A6 (Thumb instruction set encoding, figure A6-1) and
A7 (each Thumb instruction). A BL/BLX pair is decoded as one 4-byte
instruction (A7.1.17-A7.1.19).
"""
from insn import Insn, AL

_ALU = ("and", "eor", "lsl", "lsr", "asr", "adc", "sbc", "ror",
        "tst", "neg", "cmp", "cmn", "orr", "mul", "bic", "mvn")


def _sext(v, bits):
    return v - (1 << bits) if v & (1 << (bits - 1)) else v


def _dp(addr, h, o, rd, rn, op2, s, mnem=None):
    i = Insn(addr, 2, True, h, "dp", AL, mnem or o)
    i.dp_op, i.rd, i.rn, i.op2, i.s = o, rd, rn, op2, s
    return i


def _mem(addr, h, load, size, rd, rn, ofs, mnem):
    i = Insn(addr, 2, True, h, "mem", AL, mnem)
    i.load, i.msize, i.rd, i.rn, i.ofs = load, size, rd, rn, ofs
    i.add, i.pre, i.wb = True, True, False
    return i


def _undef(addr, h):
    return Insn(addr, 2, True, h, "undef", AL, "undef")


def decode_thumb(h, nxt, addr):
    """Decode the halfword `h` at `addr`; `nxt` is the following halfword (or None)."""
    top = h >> 13
    if top == 0:
        op = (h >> 11) & 3
        rd, rm = h & 7, (h >> 3) & 7
        if op < 3:  # shift by immediate (A7.1.38 LSL(1), LSR(1), ASR(1))
            st = ("lsl", "lsr", "asr")[op]
            amt = (h >> 6) & 31
            return _dp(addr, h, "mov", rd, 0, ("reg", rm, st, amt), True, st)
        # add/sub register or 3-bit immediate
        rn = (h >> 3) & 7
        sub = (h >> 9) & 1
        if (h >> 10) & 1:
            imm = (h >> 6) & 7
            op2 = ("imm", imm, None)
        else:
            op2 = ("reg", (h >> 6) & 7, "lsl", 0)
        return _dp(addr, h, "sub" if sub else "add", rd, rn, op2, True)
    if top == 1:  # MOV/CMP/ADD/SUB 8-bit immediate
        op = (h >> 11) & 3
        rd = (h >> 8) & 7
        o = ("mov", "cmp", "add", "sub")[op]
        return _dp(addr, h, o, rd, rd, ("imm", h & 0xFF, None), True)
    if (h >> 10) == 0x10:  # data-processing register (A7.1.2 ...)
        o = _ALU[(h >> 6) & 15]
        rd, rm = h & 7, (h >> 3) & 7
        if o in ("lsl", "lsr", "asr", "ror"):
            return _dp(addr, h, "mov", rd, 0, ("regshift", rd, o, rm), True, o)
        if o == "neg":
            return _dp(addr, h, "rsb", rd, rm, ("imm", 0, None), True, "neg")
        if o == "mul":
            i = Insn(addr, 2, True, h, "mul", AL, "mul")
            i.rd, i.rm, i.rs, i.rn, i.s, i.acc = rd, rm, rd, 0, True, False
            return i
        return _dp(addr, h, o, rd, rd, ("reg", rm, "lsl", 0), True)
    if (h >> 10) == 0x11:  # special data processing / branch exchange
        op = (h >> 8) & 3
        rm = ((h >> 3) & 15)
        rd = (h & 7) | ((h >> 4) & 8)
        if op == 0:
            return _dp(addr, h, "add", rd, rd, ("reg", rm, "lsl", 0), False)
        if op == 1:
            return _dp(addr, h, "cmp", 0, rd, ("reg", rm, "lsl", 0), True)
        if op == 2:
            return _dp(addr, h, "mov", rd, 0, ("reg", rm, "lsl", 0), False)
        if h & 7:
            return _undef(addr, h)
        link = bool((h >> 7) & 1)
        i = Insn(addr, 2, True, h, "bx", AL, "blx" if link else "bx")
        i.rm, i.link = rm, link
        return i
    if (h >> 11) == 9:  # LDR literal (A7.1.30)
        i = _mem(addr, h, True, "w", (h >> 8) & 7, 15, ("imm", (h & 0xFF) * 4), "ldr")
        i.pcval = (addr + 4) & ~3
        return i
    if (h >> 12) == 5:  # load/store register offset
        op = (h >> 9) & 7
        rd, rn, rm = h & 7, (h >> 3) & 7, (h >> 6) & 7
        load, size, mnem = ((False, "w", "str"), (False, "h", "strh"), (False, "b", "strb"), (True, "sb", "ldrsb"),
                            (True, "w", "ldr"), (True, "h", "ldrh"), (True, "b", "ldrb"), (True, "sh", "ldrsh"))[op]
        return _mem(addr, h, load, size, rd, rn, ("reg", rm, "lsl", 0), mnem)
    if top == 3:  # load/store word/byte immediate offset
        b = (h >> 12) & 1
        load = bool((h >> 11) & 1)
        imm = (h >> 6) & 31
        return _mem(addr, h, load, "b" if b else "w", h & 7, (h >> 3) & 7, ("imm", imm if b else imm * 4),
                    ("ldr" if load else "str") + ("b" if b else ""))
    if (h >> 12) == 8:  # halfword immediate offset
        load = bool((h >> 11) & 1)
        return _mem(addr, h, load, "h", h & 7, (h >> 3) & 7, ("imm", ((h >> 6) & 31) * 2), "ldrh" if load else "strh")
    if (h >> 12) == 9:  # SP-relative load/store
        load = bool((h >> 11) & 1)
        return _mem(addr, h, load, "w", (h >> 8) & 7, 13, ("imm", (h & 0xFF) * 4), "ldr" if load else "str")
    if (h >> 12) == 10:  # ADD rd, pc|sp, #imm (A7.1.5, A7.1.6)
        rd = (h >> 8) & 7
        sp = (h >> 11) & 1
        i = _dp(addr, h, "add", rd, 13 if sp else 15, ("imm", (h & 0xFF) * 4, None), False)
        if not sp:
            i.pcval = (addr + 4) & ~3
        return i
    if (h >> 12) == 11:  # miscellaneous
        if (h >> 8) == 0xB0:  # ADD/SUB sp, #imm (A7.1.7, A7.1.66)
            return _dp(addr, h, "sub" if (h >> 7) & 1 else "add", 13, 13, ("imm", (h & 0x7F) * 4, None), False)
        if (h >> 9) & 3 == 2:  # PUSH/POP
            pop = bool((h >> 11) & 1)
            r = (h >> 8) & 1
            i = Insn(addr, 2, True, h, "ldm" if pop else "stm", AL, "pop" if pop else "push")
            i.rn, i.regs = 13, (h & 0xFF) | ((1 << (15 if pop else 14)) if r else 0)
            i.mode, i.wb, i.user = ("ia" if pop else "db"), True, False
            return i
        if (h >> 8) == 0xBE:
            i = Insn(addr, 2, True, h, "bkpt", AL, "bkpt")
            i.imm = h & 0xFF
            return i
        return _undef(addr, h)
    if (h >> 12) == 12:  # LDMIA/STMIA
        load = bool((h >> 11) & 1)
        rn = (h >> 8) & 7
        i = Insn(addr, 2, True, h, "ldm" if load else "stm", AL, "ldmia" if load else "stmia")
        i.rn, i.regs, i.mode, i.user = rn, h & 0xFF, "ia", False
        i.wb = not (load and (i.regs >> rn) & 1)
        return i
    if (h >> 12) == 13:  # conditional branch / SWI
        cond = (h >> 8) & 15
        if cond == 15:
            i = Insn(addr, 2, True, h, "swi", AL, "swi")
            i.imm = h & 0xFF
            return i
        if cond == 14:
            return _undef(addr, h)
        i = Insn(addr, 2, True, h, "b", cond, "b")
        i.link, i.exchange = False, False
        i.target = (addr + 4 + (_sext(h & 0xFF, 8) << 1)) & 0xFFFFFFFF
        return i
    if (h >> 11) == 0x1C:  # unconditional branch
        i = Insn(addr, 2, True, h, "b", AL, "b")
        i.link, i.exchange = False, False
        i.target = (addr + 4 + (_sext(h & 0x7FF, 11) << 1)) & 0xFFFFFFFF
        return i
    if (h >> 11) == 0x1E:  # BL/BLX prefix
        hi = _sext(h & 0x7FF, 11) << 12
        if nxt is not None and (nxt >> 11) in (0x1F, 0x1D):
            exchange = (nxt >> 11) == 0x1D
            if exchange and (nxt & 1):
                return _blhi(addr, h, hi)
            i = Insn(addr, 4, True, (h << 16) | nxt, "b", AL, "blx" if exchange else "bl")
            i.link, i.exchange = True, exchange
            t = (addr + 4 + hi + ((nxt & 0x7FF) << 1)) & 0xFFFFFFFF
            i.target = (t & ~3) if exchange else t
            return i
        return _blhi(addr, h, hi)
    if (h >> 11) in (0x1F, 0x1D):  # lone BL/BLX suffix
        if (h >> 11) == 0x1D and (h & 1):
            return _undef(addr, h)
        i = Insn(addr, 2, True, h, "bllo", AL, "bl")
        i.offset, i.exchange = (h & 0x7FF) << 1, (h >> 11) == 0x1D
        i.link = True
        return i
    return _undef(addr, h)


def _blhi(addr, h, hi):
    i = Insn(addr, 2, True, h, "blhi", AL, "bl")
    i.value = (addr + 4 + hi) & 0xFFFFFFFF
    return i
