/* Adapted from melonDS src/ARM.cpp (b86390e), GPL-3.0-or-later:
 * processor modes with banked r13/r14 and SPSR (ARM::UpdateMode).
 * Mode encodings and CPSR layout: ARM ARM (DDI 0100I) A2.2-A2.6.
 *
 * Only the banked registers of the modes that are not running are kept in
 * bank_r13 / bank_r14; r[13] / r[14] always hold the current mode's. FIQ's
 * r8-r12 are not banked (the game never enters FIQ mode). */
#include "base.h"

Cpu g_arm9, g_arm7;
Core g_c9, g_c7;

int cpu_mode_index(uint32_t mode) {
    switch (mode & 0x1F) {
    case 0x11: return 1;   /* FIQ */
    case 0x12: return 2;   /* IRQ */
    case 0x13: return 3;   /* SVC */
    case 0x17: return 4;   /* ABT */
    case 0x1B: return 5;   /* UND */
    default:   return 0;   /* USR, SYS */
    }
}

void cpu_set_mode(Cpu* c, uint32_t mode) {
    uint32_t old = c->cpsr_ctl & 0x1F;
    mode &= 0x1F;
    int oi = cpu_mode_index(old), ni = cpu_mode_index(mode);
    if (oi != ni) {
        c->bank_r13[oi] = c->r[13]; c->bank_r14[oi] = c->r[14];
        c->r[13] = c->bank_r13[ni]; c->r[14] = c->bank_r14[ni];
    }
    c->cpsr_ctl = (c->cpsr_ctl & ~0x1Fu) | mode;
}

/* The whole CPSR (flags too), as on an exception return. */
void cpu_set_cpsr(Cpu* c, uint32_t v) {
    c->n = v >> 31; c->z = (v >> 30) & 1; c->cf = (v >> 29) & 1; c->v = (v >> 28) & 1; c->q = (v >> 27) & 1;
    cpu_set_mode(c, v & 0x1F);
    c->cpsr_ctl = v & 0x07FFFFFFu;
}

void cpu_write_cpsr_ctl(Cpu* c, uint32_t value, uint32_t mask) {
    mask &= 0x07FFFFFFu;
    if ((c->cpsr_ctl & 0x1F) == 0x10) mask &= 0xFF000000u;   /* user mode: control byte is read-only */
    mask &= ~0x20u;                                           /* T is not changed by MSR */
    uint32_t old = c->cpsr_ctl;
    uint32_t nv = (old & ~mask) | (value & mask);
    if ((nv & 0x1F) != (old & 0x1F)) cpu_set_mode(c, nv & 0x1F);
    c->cpsr_ctl = nv;
    if ((old & 0x80) && !(nv & 0x80)) irq_service(c);        /* IRQs unmasked: one may be waiting */
}

uint32_t cpu_read_spsr(Cpu* c) {
    int i = cpu_mode_index(c->cpsr_ctl);
    return i ? c->spsr[i] : 0;
}

void cpu_write_spsr(Cpu* c, uint32_t value, uint32_t mask) {
    int i = cpu_mode_index(c->cpsr_ctl);
    if (!i) return;
    c->spsr[i] = (c->spsr[i] & ~mask) | (value & mask);
}

void cpu_exception_return(Cpu* c, uint32_t addr) {
    LOG_INFO("%s: exception return at %08x\n", c == &g_arm7 ? "ARM7" : "ARM9", addr);
    int i = cpu_mode_index(c->cpsr_ctl);
    if (i) cpu_set_cpsr(c, c->spsr[i]);
}

void cpu_breakpoint(Cpu* c, uint32_t addr, uint32_t imm) {
    LOG_WARN("%s: BKPT %x at %08x\n", c == &g_arm7 ? "ARM7" : "ARM9", imm, addr);
}
