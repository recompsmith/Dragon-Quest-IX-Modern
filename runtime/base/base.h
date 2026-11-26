/* Declarations shared by the files of runtime/base only. */
#pragma once
#include "include/nds.h"

/* log.c */
extern int g_log_cpu;              /* 9 or 7: the CPU that is running, for log prefixes */
#define LOG_FRAME_CAP 200          /* log lines shown per frame */
void log_flush_frame(void);        /* print the "N more log lines" notice now */

/* cycle accounting (sched.c) */
extern uint32_t g_cyc7;
extern uint64_t g_interp_steps;

/* cpu_state.c */
void cpu_set_cpsr(Cpu* c, uint32_t v);      /* whole CPSR, flags included */

/* cp15.c */
extern uint32_t g_dtcm_size, g_itcm_size;
void cp15_reset(void);

/* HD text (hdtext.c): declared here so base files need no other header */
extern int g_hdtext_any;
void hdtext_write(const uint8_t* p, const uint8_t* v, int size);
void hdtext_copy(uint32_t dst, uint32_t src, uint32_t bytes);

/* mem9.c, mem7.c, vram.c */
#define MEMHOOK_HDTEXT 1                    /* call hdtext_write before the write (CPU writes) */
extern uint8_t g_wramcnt;
extern uint8_t* g_swram9; extern uint32_t g_swram9_mask;
extern uint8_t* g_swram7; extern uint32_t g_swram7_mask;
extern int g_nwatch;              /* DQIX_WATCH addresses */
void mem_set_wramcnt(uint8_t v);
void mem_main_write(uint32_t addr, uint32_t val, int size, int hooks);
void mem_watch_init(void);
void mem_watch_check(int cpu, uint32_t addr, uint32_t val, int size);
uint32_t mem9_read(uint32_t addr, int size);
void mem9_write(uint32_t addr, uint32_t val, int size, int hooks);
uint32_t mem7_read(uint32_t addr, int size);
void mem7_write(uint32_t addr, uint32_t val, int size, int hooks);
void mem9_reset(void);
void mem7_reset(void);
void vram_write_cnt(int bank, uint8_t v);
uint8_t vram_stat(void);
void vram_reset(void);
