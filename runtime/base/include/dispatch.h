/* The table of translated functions produced by the recompiler, and the
 * record type of its entries. */
#pragma once
#include "cpu.h"

typedef struct RecompFunc {
    int ovl_id;            /* overlay id, -1 for code that is always there */
    uint32_t addr;         /* entry address, bit 0 = Thumb */
    uint32_t size;         /* code size in bytes; 0 = run by the interpreter */
    void (*fn)(Cpu* c);
    const char* name;
} RecompFunc;

extern const RecompFunc g_recomp_funcs[];
extern const unsigned g_recomp_func_count;
