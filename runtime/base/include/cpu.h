/* The CPU as seen by the translated game code: register file, condition
 * flags, and the services the generated C calls (memory, CPSR/SPSR, CP15,
 * BIOS calls, dispatch, cycle budget). Shared by the ARM9 and ARM7 code: the
 * ARM7 files rename mem_* / g_cyc / bios_swi / dispatch_call with #defines
 * placed before this header, so the inline helpers below bind to the ARM7
 * versions there.
 *
 * Flag semantics, shifter carry and saturation follow the ARM Architecture
 * Reference Manual (ARM DDI 0100I), sections A2.5, A5.1 and A10. */
#pragma once
#include <stdint.h>
#include <string.h>

typedef struct Cpu {
    uint32_t r[16];
    uint32_t n, z, cf, v, q;          /* condition flags, each 0 or 1 */
    uint32_t cpsr_ctl;                /* the CPSR without N Z C V Q: mode, I, F, T */
    uint32_t fault_addr;
    uint32_t bank_r13[6], bank_r14[6], spsr[6];   /* banked state, runtime only */
} Cpu;

/* ---- services ---- */
uint8_t  mem_read8(uint32_t);
uint16_t mem_read16(uint32_t);
uint32_t mem_read32(uint32_t);
void mem_write8(uint32_t, uint8_t);
void mem_write16(uint32_t, uint16_t);
void mem_write32(uint32_t, uint32_t);
void cpu_write_cpsr_ctl(Cpu*, uint32_t value, uint32_t mask);
void cpu_write_spsr(Cpu*, uint32_t value, uint32_t mask);
uint32_t cpu_read_spsr(Cpu*);
int cpu_mode_index(uint32_t mode);
void cp15_write(Cpu*, uint32_t op1, uint32_t crn, uint32_t crm, uint32_t op2, uint32_t value);
uint32_t cp15_read(Cpu*, uint32_t op1, uint32_t crn, uint32_t crm, uint32_t op2);
void bios_swi(Cpu*, uint32_t number);
void cpu_breakpoint(Cpu*, uint32_t addr, uint32_t imm);
void cpu_unhandled(Cpu*, uint32_t addr, uint32_t insn);
void cpu_exception_return(Cpu*, uint32_t addr);
void dispatch_call(Cpu*, uint32_t target, uint32_t from);
int  overlay_is_loaded(int id);

#define CPSR_GET(c) (((c)->n << 31) | ((c)->z << 30) | ((c)->cf << 29) | ((c)->v << 28) | \
                     ((c)->q << 27) | ((c)->cpsr_ctl & 0x07FFFFFFu))

/* ---- cycle budget ---- */
extern uint32_t g_cyc;
#ifndef CYC_SYNC
#define CYC_SYNC 512u
#endif
void recomp_budget_expired(Cpu*);
void recomp_loop_expired(Cpu*);
#ifdef RECOMP_TRACE
void recomp_trace_enter(uint32_t addr);
#define RECOMP_TRACE_HOOK(a) recomp_trace_enter(a)
#else
#define RECOMP_TRACE_HOOK(a) ((void)0)
#endif
int gx_tag_begin(int tag);
void gx_tag_end(int previous);

#define RECOMP_CYC(n) (g_cyc += (n))
#define RECOMP_ENTER(a) do { if (g_cyc >= CYC_SYNC) recomp_budget_expired(c); RECOMP_TRACE_HOOK(a); } while (0)
#define RECOMP_LOOP() do { if (g_cyc >= CYC_SYNC) recomp_loop_expired(c); } while (0)

/* ---- conditions (ARM ARM A3.2.1) ---- */
#define COND_EQ (c->z)
#define COND_NE (!c->z)
#define COND_CS (c->cf)
#define COND_HS COND_CS
#define COND_CC (!c->cf)
#define COND_LO COND_CC
#define COND_MI (c->n)
#define COND_PL (!c->n)
#define COND_VS (c->v)
#define COND_VC (!c->v)
#define COND_HI (c->cf && !c->z)
#define COND_LS (!c->cf || c->z)
#define COND_GE (c->n == c->v)
#define COND_LT (c->n != c->v)
#define COND_GT (!c->z && c->n == c->v)
#define COND_LE (c->z || c->n != c->v)
#define COND_AL 1

/* ---- helpers used by translated code ---- */
static inline uint32_t ror32(uint32_t v, uint32_t s) {
    s &= 31;
    return s ? (v >> s) | (v << (32 - s)) : v;
}
/* shifts by register: amount = bottom byte of the register (A5.1.6-A5.1.13) */
static inline uint32_t lsl_r(uint32_t v, uint32_t s) { s &= 0xFF; return s >= 32 ? 0 : v << s; }
static inline uint32_t lsr_r(uint32_t v, uint32_t s) { s &= 0xFF; return s >= 32 ? 0 : v >> s; }
static inline uint32_t asr_r(uint32_t v, uint32_t s) {
    s &= 0xFF;
    if (s >= 32) return (v & 0x80000000u) ? 0xFFFFFFFFu : 0;
    return s ? (uint32_t)((int32_t)v >> s) : v;
}
static inline uint32_t ror_r(uint32_t v, uint32_t s) { return ror32(v, s & 0xFF); }
/* same, with the shifter carry out (cin when the amount is 0) */
static inline uint32_t lsl_rc(uint32_t v, uint32_t s, uint32_t cin, uint32_t* co) {
    s &= 0xFF;
    if (s == 0) { *co = cin; return v; }
    if (s < 32) { *co = (v >> (32 - s)) & 1; return v << s; }
    *co = s == 32 ? (v & 1) : 0;
    return 0;
}
static inline uint32_t lsr_rc(uint32_t v, uint32_t s, uint32_t cin, uint32_t* co) {
    s &= 0xFF;
    if (s == 0) { *co = cin; return v; }
    if (s < 32) { *co = (v >> (s - 1)) & 1; return v >> s; }
    *co = s == 32 ? (v >> 31) : 0;
    return 0;
}
static inline uint32_t asr_rc(uint32_t v, uint32_t s, uint32_t cin, uint32_t* co) {
    s &= 0xFF;
    if (s == 0) { *co = cin; return v; }
    if (s < 32) { *co = (v >> (s - 1)) & 1; return (uint32_t)((int32_t)v >> s); }
    *co = v >> 31;
    return (v & 0x80000000u) ? 0xFFFFFFFFu : 0;
}
static inline uint32_t ror_rc(uint32_t v, uint32_t s, uint32_t cin, uint32_t* co) {
    s &= 0xFF;
    if (s == 0) { *co = cin; return v; }
    s &= 31;
    if (s == 0) { *co = v >> 31; return v; }
    *co = (v >> (s - 1)) & 1;
    return (v >> s) | (v << (32 - s));
}
/* N and Z from a result; returns it */
static inline uint32_t nz(Cpu* c, uint32_t r) { c->n = r >> 31; c->z = r == 0; return r; }
static inline uint32_t add_s(Cpu* c, uint32_t a, uint32_t b) {
    uint32_t r = a + b;
    c->cf = r < a;
    c->v = ((~(a ^ b) & (a ^ r)) >> 31) & 1;
    return nz(c, r);
}
static inline uint32_t sub_s(Cpu* c, uint32_t a, uint32_t b) {
    uint32_t r = a - b;
    c->cf = a >= b;
    c->v = (((a ^ b) & (a ^ r)) >> 31) & 1;
    return nz(c, r);
}
static inline uint32_t adc_s(Cpu* c, uint32_t a, uint32_t b, uint32_t cin) {
    uint64_t w = (uint64_t)a + b + (cin & 1);
    uint32_t r = (uint32_t)w;
    c->cf = (uint32_t)(w >> 32);
    c->v = ((~(a ^ b) & (a ^ r)) >> 31) & 1;
    return nz(c, r);
}
static inline uint32_t sbc_s(Cpu* c, uint32_t a, uint32_t b, uint32_t cin) {
    uint32_t borrow = (cin & 1) ^ 1;
    uint32_t r = a - b - borrow;
    c->cf = (uint64_t)a >= (uint64_t)b + borrow;
    c->v = (((a ^ b) & (a ^ r)) >> 31) & 1;
    return nz(c, r);
}
/* word load: the aligned word rotated by 8 x (addr & 3) (A4.1.23) */
static inline uint32_t ldr_word(uint32_t addr) {
    return ror32(mem_read32(addr & ~3u), (addr & 3) * 8);
}
/* QADD / QSUB: signed saturation, Q set when it saturates (A10) */
static inline uint32_t sat_add(Cpu* c, uint32_t a, uint32_t b) {
    uint32_t r = a + b;
    if ((~(a ^ b) & (a ^ r)) >> 31) { c->q = 1; return (a >> 31) ? 0x80000000u : 0x7FFFFFFFu; }
    return r;
}
static inline uint32_t sat_sub(Cpu* c, uint32_t a, uint32_t b) {
    uint32_t r = a - b;
    if (((a ^ b) & (a ^ r)) >> 31) { c->q = 1; return (a >> 31) ? 0x80000000u : 0x7FFFFFFFu; }
    return r;
}
/* addition that sets Q on signed overflow (SMLAxy, SMLAWy), no saturation */
static inline uint32_t add_q(Cpu* c, uint32_t a, uint32_t b) {
    uint32_t r = a + b;
    if ((~(a ^ b) & (a ^ r)) >> 31) c->q = 1;
    return r;
}
static inline uint32_t clz32(uint32_t v) {
    uint32_t n = 0;
    if (!v) return 32;
    while (!(v & 0x80000000u)) { v <<= 1; n++; }
    return n;
}
/* signed halfword: the top one if top, else the bottom one */
static inline int32_t half(uint32_t v, int top) {
    return (int32_t)(int16_t)(uint16_t)(top ? v >> 16 : v);
}
