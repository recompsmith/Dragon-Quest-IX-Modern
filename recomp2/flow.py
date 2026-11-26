"""Control flow inside one function: reachable instructions, labels, blocks.

The code of a function is what is reached from its entry by following the
instructions (fall-through, direct branches inside the function, jump-table
targets). Literal pools and data are never decoded.

Jump tables (compiler switch idioms, ARM ARM A4.1.3 for the PC-relative ADD):
- ARM: ``addls pc, pc, rI, lsl #2`` reads pc = address + 8, so slot i is the
  word at address + 8 + 4*i; the word at address + 4 runs when the condition
  fails. The slot count is k + 1 for the last ``cmp rI, #k`` before it; without
  one, the run of unconditional branches that follows; without those, every
  word up to the first unconditional exit (a computed jump into unrolled code).
- Thumb: ``add rX, rX ; add rX, pc ; ldrh rX, [rX, #d] ; lsl #16 ; asr #16 ;
  add pc, rX`` reads signed 16-bit offsets stored after the ``add pc``; the
  table ends where the first target begins.
"""
from cost import cost
from decode import decode_at
from insn import AL, DP_TEST


class Body:
    def __init__(self, start, end, thumb):
        self.start, self.end, self.thumb = start, end, thumb
        self.insns = {}          # addr -> Insn
        self.labels = {start}
        self.tables = {}         # addr of the PC write -> ('arm'|'thumb', [target addrs], index reg)
        self.calls_after_mov_lr = set()  # indirect PC writes preceded by "mov lr, pc": calls
        self.problems = []

    def order(self):
        return sorted(self.insns)

    def blocks(self):
        """[(label, cycles)] in address order."""
        out = []
        for a in self.order():
            if a in self.labels or not out:
                out.append([a, 0])
            out[-1][1] += cost(self.insns[a])
        return [tuple(b) for b in out]


def _is_jump(i):
    return i is not None and i.cond == AL and ((i.op == "b" and not i.link) or (i.op == "bx" and not i.link))


def _arm_table(mod, i, start, end):
    """Slots of an ``add pc, pc, rI, lsl #2`` jump, or None if it is not one."""
    if not (i.op == "dp" and i.dp_op == "add" and i.rn == 15 and i.rd == 15 and not i.s
            and i.op2[0] == "reg" and i.op2[2] == "lsl" and i.op2[3] == 2 and i.op2[1] != 15):
        return None
    idx = i.op2[1]
    k = None
    if i.cond in (9, 10):  # LS / GE: the largest case number the index was compared with
        # in the instructions just before (up to six, back to the last write of the index)
        for n in range(1, 7):
            p = decode_at(mod, i.addr - 4 * n, False) if i.addr - 4 * n >= start else None
            if p is None:
                break
            if p.op == "dp" and p.dp_op == "cmp" and p.rn == idx and p.op2[0] == "imm":
                k = max(k or 0, p.op2[1] + 1)
            elif p.writes() & (1 << idx):
                break
    first = i.addr + 8
    if k is None:
        k = 0
        while first + 4 * k < end and _is_jump(decode_at(mod, first + 4 * k, False)):
            k += 1
        if k == 0:  # a computed jump into unrolled code: every word up to the first unconditional exit
            while first + 4 * k < end:
                j = decode_at(mod, first + 4 * k, False)
                k += 1
                if j is None or j.op == "undef" or (j.cond == AL and j.writes_pc and not j.is_call):
                    break
    return [first + 4 * n for n in range(k)]


def _thumb_table(mod, i, start, end, known=()):
    """Targets of a Thumb offset-table jump (``add pc, rX``), or None.

    The table ends at the first halfword that is known code or the start of a target."""
    if not (i.op == "dp" and i.dp_op == "add" and i.rd == 15 and i.rn == 15 and not i.s and i.op2[0] == "reg"):
        return None
    x = i.op2[1]
    if i.addr - 10 < start:
        return None
    seq = [decode_at(mod, i.addr - 2 * n, True) for n in range(1, 6)]
    if any(s is None for s in seq):
        return None
    asr, lsl, ldrh, addpc, add2 = seq
    if not (asr.op == "dp" and asr.op2 == ("reg", x, "asr", 16) and asr.rd == x
            and lsl.op == "dp" and lsl.op2 == ("reg", x, "lsl", 16) and lsl.rd == x
            and ldrh.op == "mem" and ldrh.load and ldrh.msize == "h" and ldrh.rd == x and ldrh.rn == x and ldrh.ofs[0] == "imm"
            and addpc.op == "dp" and addpc.dp_op == "add" and addpc.rd == x and addpc.op2 == ("reg", 15, "lsl", 0)
            and add2.op == "dp" and add2.dp_op == "add" and add2.rd == x and add2.op2[0] == "reg"):
        return None
    table = addpc.addr + 4 + ldrh.ofs[1]
    base = i.addr + 4
    targets = []
    lowest = end
    a = table
    while a < lowest and a + 2 <= end and a not in known:
        off = mod.half(a)
        if off & 0x8000:
            off -= 0x10000
        t = base + off
        targets.append(t)
        if t > a:
            lowest = min(lowest, t)
        a += 2
    return targets


def analyze(mod, start, end, thumb, extra_labels=()):
    """Follow the code of [start, end) from `start` in the given state.

    `extra_labels` (label symbols of the map inside the function) start blocks
    and are followed too."""
    b = Body(start, end, thumb)
    work = [start] + [t for t in extra_labels if start <= t < end]
    b.labels.update(work)
    deferred = []     # Thumb offset tables: read once the rest of the code is known
    while True:
        _follow(b, mod, work, deferred)
        if not deferred:
            break
        known = set(b.insns) | b.labels
        for i in deferred:
            slots = _thumb_table(mod, i, start, end, known)
            if slots is None:
                if i.cond != AL:
                    work.append(i.addr + i.size)
                continue
            b.tables[i.addr] = ("thumb", slots, i.op2[1])
            for t in slots:
                if start <= t < end:
                    b.labels.add(t)
                    work.append(t)
        deferred = []
    b.labels &= set(b.insns)
    return b


def _follow(b, mod, work, deferred):
    start, end, thumb = b.start, b.end, b.thumb
    while work:
        a = work.pop()
        if a in b.insns or not (start <= a < end):
            continue
        i = decode_at(mod, a, thumb)
        if i is None or a + i.size > end:
            b.problems.append("%08x: instruction runs past the end of the function" % a)
            continue
        b.insns[a] = i
        nxt = a + i.size
        cond = i.cond != AL
        op = i.op
        if op == "undef":
            b.problems.append("%08x: untranslatable instruction %08x" % (a, i.word))
            continue
        if op == "b" and not i.link:
            if start <= i.target < end:
                b.labels.add(i.target)
                work.append(i.target)
            if cond:
                work.append(nxt)
            continue
        if not i.writes_pc or i.is_call:
            work.append(nxt)
            continue
        # an indirect write of PC: return, table, computed jump
        if op == "dp" and thumb and _thumb_table(mod, i, start, end) is not None:
            deferred.append(i)
            continue
        slots = _arm_table(mod, i, start, end) if (op == "dp" and not thumb) else None
        if slots is not None:
            b.tables[a] = ("arm", slots, i.op2[1])
            for t in slots:
                if start <= t < end:
                    b.labels.add(t)
                    work.append(t)
        elif _after_mov_lr(b, mod, i, thumb):
            b.calls_after_mov_lr.add(a)
            work.append(nxt)
            continue
        if cond:
            work.append(nxt)


def _after_mov_lr(b, mod, i, thumb):
    """``mov lr, pc`` right before an indirect jump makes it a call (ARM state)."""
    if thumb or i.op not in ("bx", "dp", "mem"):
        return False
    if i.op == "dp" and i.dp_op in DP_TEST:
        return False
    p = decode_at(mod, i.addr - 4, False)
    return (p is not None and p.op == "dp" and p.dp_op == "mov" and p.rd == 14 and p.op2 == ("reg", 15, "lsl", 0)
            and p.cond == i.cond)
