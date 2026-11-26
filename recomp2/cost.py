"""Cycle cost of one instruction (fixed model, the same on every PC).

Simplified ARM9E-S timings (ARM DDI 0240A, "Instruction Cycle Timings"):
one cycle per instruction, more for branches, loads and multiplies, with no
dependence on run-time values, so every build counts the same cycles.
"""
from insn import DP_TEST


def cost(i):
    op = i.op
    if op == "dp":
        return 3 if (i.rd == 15 and i.dp_op not in DP_TEST) else 1
    if op in ("b", "bx", "bllo"):
        return 3
    if op in ("mem", "lit"):
        if i.msize == "d":
            return 3
        if i.load:
            return 4 if i.rd == 15 else 2
        return 1
    if op == "ldm":
        n = bin(i.regs).count("1")
        return n + 1 + (2 if i.regs & 0x8000 else 0)
    if op == "stm":
        return bin(i.regs).count("1") + 1
    if op in ("mul", "mull", "dsp"):
        return 3
    return 1  # clz mrs msr mcr mrc swi swp q bkpt pld blhi undef
