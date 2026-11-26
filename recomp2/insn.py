"""Decoded instruction: one record for ARM and Thumb alike.

Thumb instructions are decoded to the ARM instruction with the same effect
(ARM ARM, ARMv5TE, chapter A7 gives each Thumb instruction's ARM equivalent),
so one translator handles both. `pcval` is the value an instruction reads
for r15 as an operand: address + 8 in ARM (+ 12 for a register-specified
shift), address + 4 in Thumb, word-aligned where the Thumb instruction says so.
"""

DP_OPS = ("and", "eor", "sub", "rsb", "add", "adc", "sbc", "rsc",
          "tst", "teq", "cmp", "cmn", "orr", "mov", "bic", "mvn")
DP_TEST = ("tst", "teq", "cmp", "cmn")
DP_LOGIC = ("and", "eor", "tst", "teq", "orr", "mov", "bic", "mvn")
SHIFTS = ("lsl", "lsr", "asr", "ror")
CONDS = ("eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc", "hi", "ls", "ge", "lt", "gt", "le", "al", "nv")
AL = 14


class Insn:
    """Fields used depend on `op`:

    dp      dp_op s rd rn op2      op2 = ('imm', value, carry|None)
                                         ('reg', rm, shift, amount)   amount 0..31 as encoded
                                         ('rrx', rm)
                                         ('regshift', rm, shift, rs)
    mem     load size rd rn ofs add pre wb   size: w b h sb sh d
                                         ofs = ('imm', n) | ('reg', rm, shift, amount) | ('rrx', rm)
    lit     rd value size             ldr from a literal pool folded to a constant (set by the translator)
    ldm/stm rn regs mode wb user      mode: ia ib da db ; regs: 16-bit mask
    mul     rd rn rs rm s acc         MUL / MLA (rd = rm * rs [+ rn])
    mull    rdlo rdhi rs rm s acc signed
    dsp     kind rd rn rs rm x y      kind: smla smul smlaw smulw smlal (rd = RdHi, rn = RdLo for smlal)
    q       kind rd rm rn             kind: qadd qsub qdadd qdsub
    clz     rd rm
    swp     byte rd rm rn
    b       target link exchange      B, BL, BLX imm (exchange: target is in the other mode)
    bx      rm link                   BX, BLX reg
    blhi    value                     lone Thumb BL/BLX first half: lr = pc + offset
    bllo    offset exchange           lone Thumb BL/BLX second half: call lr + offset
    swi     imm
    mrs     rd spsr
    msr     spsr mask op2             mask: 4-bit field mask (c x s f)
    mcr/mrc cp op1 crn crm op2 rd
    bkpt    imm
    pld
    undef                             not an instruction we translate (cpu_unhandled)
    """

    __slots__ = ("addr", "size", "thumb", "word", "cond", "op", "mnem", "pcval",
                 "dp_op", "s", "rd", "rn", "rm", "rs", "op2",
                 "load", "msize", "ofs", "add", "pre", "wb",
                 "regs", "mode", "user", "acc", "signed", "rdlo", "rdhi",
                 "kind", "x", "y", "byte", "target", "link", "exchange",
                 "imm", "spsr", "mask", "cp", "op1", "crn", "crm", "cop2", "value", "offset")

    def __init__(self, addr, size, thumb, word, op, cond=AL, mnem=None):
        for k in self.__slots__:
            object.__setattr__(self, k, None)
        self.addr, self.size, self.thumb, self.word = addr, size, thumb, word
        self.op, self.cond = op, cond
        self.mnem = mnem or op
        self.pcval = (addr + 4) if thumb else (addr + 8)

    # ---- properties used by discovery and flow --------------------------------
    @property
    def writes_pc(self):
        op = self.op
        if op == "dp":
            return self.rd == 15 and self.dp_op not in DP_TEST
        if op == "mem":
            return self.load and (self.rd == 15 or (self.msize == "d" and self.rd == 14))
        if op == "lit":
            return self.rd == 15
        if op == "ldm":
            return bool(self.regs & 0x8000)
        if op in ("b", "bx", "bllo"):
            return True
        if op in ("mrc",):
            return False
        return False

    @property
    def is_call(self):
        return (self.op in ("b", "bx") and bool(self.link)) or self.op == "bllo"

    @property
    def mode_switch(self):
        """True when a direct branch lands in the other instruction set."""
        return self.op == "b" and bool(self.exchange)

    def writes(self):
        """Registers this instruction may write (as a bit mask), r15 included."""
        op, m = self.op, 0
        if op == "dp":
            if self.dp_op not in DP_TEST:
                m |= 1 << self.rd
        elif op == "mem":
            if self.load:
                m |= 1 << self.rd
                if self.msize == "d":
                    m |= 1 << (self.rd + 1)
            if self.wb:
                m |= 1 << self.rn
        elif op == "lit":
            m |= 1 << self.rd
        elif op == "ldm":
            m |= self.regs
            if self.wb:
                m |= 1 << self.rn
        elif op == "stm":
            if self.wb:
                m |= 1 << self.rn
        elif op in ("mul", "dsp", "q", "clz", "swp", "mrs", "mrc"):
            if op == "dsp" and self.kind == "smlal":
                m |= (1 << self.rd) | (1 << self.rn)
            elif self.rd is not None:
                m |= 1 << self.rd
        elif op == "mull":
            m |= (1 << self.rdlo) | (1 << self.rdhi)
        elif op == "blhi":
            m |= 1 << 14
        elif op in ("b", "bx", "bllo"):
            if self.link or op == "bllo":
                m |= 1 << 14
            m |= 1 << 15
        return m

    def __repr__(self):
        return "<%s %08x %s>" % ("T" if self.thumb else "A", self.addr, self.mnem)
