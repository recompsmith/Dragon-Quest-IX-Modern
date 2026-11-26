"""ARM (32-bit) instruction decoder, ARMv5TE.

Encodings from the ARM Architecture Reference Manual (ARM DDI 0100I):
A3.1 (instruction set encoding, figure A3-1), A3.13 (miscellaneous
instructions), A4 (each instruction), A5.1-A5.5 (addressing modes 1-5),
A10 (enhanced DSP instructions).
"""
from insn import Insn, DP_OPS, SHIFTS, AL


def _bit(w, n):
    return (w >> n) & 1


def _ror(v, n):
    n &= 31
    return ((v >> n) | (v << (32 - n))) & 0xFFFFFFFF if n else v


def _sext(v, bits):
    return v - (1 << bits) if v & (1 << (bits - 1)) else v


def _shifter(w):
    """Addressing mode 1 operand (A5.1) for bits 11-0 of a register operand."""
    rm = w & 15
    st = SHIFTS[(w >> 5) & 3]
    if _bit(w, 4):
        return ("regshift", rm, st, (w >> 8) & 15)
    amt = (w >> 7) & 31
    if st == "ror" and amt == 0:
        return ("rrx", rm)
    return ("reg", rm, st, amt)


def _imm_operand(w):
    rot = ((w >> 8) & 15) * 2
    v = _ror(w & 0xFF, rot)
    return ("imm", v, (v >> 31) if rot else None)


def _undef(addr, w, cond=AL):
    return Insn(addr, 4, False, w, "undef", cond, "undef")


def decode_arm(w, addr):
    cond = w >> 28
    if cond == 15:
        return _decode_unconditional(w, addr)
    b27_25 = (w >> 25) & 7
    if b27_25 == 0:
        if _bit(w, 4) and _bit(w, 7):
            return _decode_mul_extra(w, addr, cond)
        if ((w >> 23) & 3) == 2 and not _bit(w, 20):
            return _decode_misc(w, addr, cond)
        return _dp(w, addr, cond, _shifter(w))
    if b27_25 == 1:
        if ((w >> 23) & 3) == 2 and not _bit(w, 20):
            if _bit(w, 21):  # MSR immediate (A4.1.39)
                i = Insn(addr, 4, False, w, "msr", cond, "msr")
                i.spsr, i.mask, i.op2 = _bit(w, 22), (w >> 16) & 15, _imm_operand(w)
                return i
            return _undef(addr, w, cond)
        return _dp(w, addr, cond, _imm_operand(w))
    if b27_25 in (2, 3):
        if b27_25 == 3 and _bit(w, 4):
            return _undef(addr, w, cond)
        i = Insn(addr, 4, False, w, "mem", cond)
        i.load = bool(_bit(w, 20))
        i.msize = "b" if _bit(w, 22) else "w"
        i.rn, i.rd = (w >> 16) & 15, (w >> 12) & 15
        i.add, i.pre = bool(_bit(w, 23)), bool(_bit(w, 24))
        i.wb = (not i.pre) or bool(_bit(w, 21))
        i.ofs = ("imm", w & 0xFFF) if b27_25 == 2 else _shifter(w)
        i.mnem = ("ldr" if i.load else "str") + ("b" if i.msize == "b" else "")
        return i
    if b27_25 == 4:
        i = Insn(addr, 4, False, w, "ldm" if _bit(w, 20) else "stm", cond)
        i.rn, i.regs = (w >> 16) & 15, w & 0xFFFF
        i.mode = ("da", "ia", "db", "ib")[(w >> 23) & 3]
        i.wb, i.user = bool(_bit(w, 21)), bool(_bit(w, 22))
        return i
    if b27_25 == 5:
        i = Insn(addr, 4, False, w, "b", cond, "bl" if _bit(w, 24) else "b")
        i.link = bool(_bit(w, 24))
        i.exchange = False
        i.target = (addr + 8 + (_sext(w & 0xFFFFFF, 24) << 2)) & 0xFFFFFFFF
        return i
    if b27_25 == 6:
        return _undef(addr, w, cond)  # LDC/STC: no coprocessor with memory transfers on the DS
    # b27_25 == 7
    if _bit(w, 24):
        i = Insn(addr, 4, False, w, "swi", cond, "swi")
        i.imm = w & 0xFFFFFF
        return i
    if _bit(w, 4):
        i = Insn(addr, 4, False, w, "mrc" if _bit(w, 20) else "mcr", cond)
        i.cp, i.op1 = (w >> 8) & 15, (w >> 21) & 7
        i.crn, i.crm, i.cop2, i.rd = (w >> 16) & 15, w & 15, (w >> 5) & 7, (w >> 12) & 15
        return i
    return _undef(addr, w, cond)  # CDP


def _dp(w, addr, cond, op2):
    o = DP_OPS[(w >> 21) & 15]
    i = Insn(addr, 4, False, w, "dp", cond, o)
    i.dp_op, i.s = o, bool(_bit(w, 20))
    i.rn, i.rd, i.op2 = (w >> 16) & 15, (w >> 12) & 15, op2
    if op2[0] == "regshift":
        i.pcval = addr + 12
    return i


def _decode_misc(w, addr, cond):
    """A3.13: MRS, MSR reg, BX, BLX reg, CLZ, BKPT, Q*, signed multiplies."""
    op = (w >> 21) & 3
    lo = (w >> 4) & 15
    if lo == 0:
        if op in (0, 2):  # MRS (A4.1.38)
            i = Insn(addr, 4, False, w, "mrs", cond, "mrs")
            i.rd, i.spsr = (w >> 12) & 15, _bit(w, 22)
            return i
        i = Insn(addr, 4, False, w, "msr", cond, "msr")
        i.spsr, i.mask, i.op2 = _bit(w, 22), (w >> 16) & 15, ("reg", w & 15, "lsl", 0)
        return i
    if lo == 1 and op == 1:
        i = Insn(addr, 4, False, w, "bx", cond, "bx")
        i.rm, i.link = w & 15, False
        return i
    if lo == 1 and op == 3:
        i = Insn(addr, 4, False, w, "clz", cond, "clz")
        i.rd, i.rm = (w >> 12) & 15, w & 15
        return i
    if lo == 3 and op == 1:
        i = Insn(addr, 4, False, w, "bx", cond, "blx")
        i.rm, i.link = w & 15, True
        return i
    if lo == 5:
        i = Insn(addr, 4, False, w, "q", cond)
        i.kind = ("qadd", "qsub", "qdadd", "qdsub")[op]
        i.mnem = i.kind
        i.rd, i.rm, i.rn = (w >> 12) & 15, w & 15, (w >> 16) & 15
        return i
    if lo == 7 and op == 1:
        i = Insn(addr, 4, False, w, "bkpt", cond, "bkpt")
        i.imm = ((w >> 4) & 0xFFF0) | (w & 15)
        return i
    if (lo & 9) == 8:  # signed multiplies, A10
        x, y = _bit(w, 5), _bit(w, 6)
        i = Insn(addr, 4, False, w, "dsp", cond)
        i.rd, i.rn, i.rs, i.rm = (w >> 16) & 15, (w >> 12) & 15, (w >> 8) & 15, w & 15
        i.x, i.y = x, y
        if op == 0:
            i.kind = "smla"
        elif op == 1:
            i.kind = "smulw" if x else "smlaw"
        elif op == 2:
            i.kind = "smlal"
        else:
            i.kind = "smul"
        i.mnem = i.kind
        return i
    return _undef(addr, w, cond)


def _decode_mul_extra(w, addr, cond):
    sh = (w >> 5) & 3
    if sh == 0:
        if (w >> 24) & 15 == 0:
            rd, rn, rs, rm = (w >> 16) & 15, (w >> 12) & 15, (w >> 8) & 15, w & 15
            if not _bit(w, 23):
                if _bit(w, 22):
                    return _undef(addr, w, cond)
                i = Insn(addr, 4, False, w, "mul", cond, "mla" if _bit(w, 21) else "mul")
                i.rd, i.rn, i.rs, i.rm = rd, rn, rs, rm
                i.s, i.acc = bool(_bit(w, 20)), bool(_bit(w, 21))
                return i
            i = Insn(addr, 4, False, w, "mull", cond)
            i.rdhi, i.rdlo, i.rs, i.rm = rd, rn, rs, rm
            i.signed, i.acc, i.s = bool(_bit(w, 22)), bool(_bit(w, 21)), bool(_bit(w, 20))
            i.mnem = ("s" if i.signed else "u") + ("mlal" if i.acc else "mull")
            return i
        if (w >> 23) & 31 == 2 and (w >> 20) & 3 == 0 and (w >> 8) & 15 == 0:
            i = Insn(addr, 4, False, w, "swp", cond, "swpb" if _bit(w, 22) else "swp")
            i.byte = bool(_bit(w, 22))
            i.rn, i.rd, i.rm = (w >> 16) & 15, (w >> 12) & 15, w & 15
            return i
        return _undef(addr, w, cond)
    # extra load/store (A5.3)
    load = bool(_bit(w, 20))
    if sh == 1:
        size = "h"
    elif sh == 2:
        size = "sb" if load else "d"
    else:
        size = "sh" if load else "d"
    i = Insn(addr, 4, False, w, "mem", cond)
    if size == "d":
        i.load = sh == 2  # LDRD has L=0 and bits 6-5 = 10, STRD 11
    else:
        i.load = load
    i.msize = size
    i.rn, i.rd = (w >> 16) & 15, (w >> 12) & 15
    i.add, i.pre = bool(_bit(w, 23)), bool(_bit(w, 24))
    i.wb = (not i.pre) or bool(_bit(w, 21))
    if not i.pre and _bit(w, 21):
        return _undef(addr, w, cond)
    if size == "d" and (i.rd & 1):
        return _undef(addr, w, cond)
    i.ofs = ("imm", ((w >> 4) & 0xF0) | (w & 15)) if _bit(w, 22) else ("reg", w & 15, "lsl", 0)
    i.mnem = ("ldr" if i.load else "str") + {"h": "h", "sb": "sb", "sh": "sh", "d": "d"}[size]
    return i


def _decode_unconditional(w, addr):
    if (w >> 25) & 7 == 5:  # BLX immediate (A4.1.9)
        i = Insn(addr, 4, False, w, "b", AL, "blx")
        i.link, i.exchange = True, True
        i.target = (addr + 8 + (_sext(w & 0xFFFFFF, 24) << 2) + (_bit(w, 24) << 1)) & 0xFFFFFFFF
        return i
    if (w & 0x0D70F000) == 0x0550F000:  # PLD (A10.2.5)
        return Insn(addr, 4, False, w, "pld", AL, "pld")
    return _undef(addr, w, 15)
