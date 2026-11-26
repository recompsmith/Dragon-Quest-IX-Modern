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
