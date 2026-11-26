/* Adapted from melonDS src/CP15.cpp (b86390e), GPL-3.0-or-later:
 * ARMv5::CP15Write / CP15Read, UpdateDTCMSetting, UpdateITCMSetting.
 * Register map: GBATEK "ARM CP15 System Control Coprocessor", ARM946E-S TRM.
 * Caches and the protection unit are not simulated (the operations do
 * nothing); "wait for interrupt" halts the ARM9. */
#include "base.h"

uint32_t g_dtcm_base = 0xFFFFFFFFu;
uint32_t g_dtcm_size;
int g_itcm_enabled, g_dtcm_enabled;
uint32_t g_itcm_size;

static uint32_t g_cp15_control = 0x00000078;
static uint32_t g_dtcm_setting, g_itcm_setting;
static uint32_t g_pu_data_cacheable, g_pu_code_cacheable, g_pu_data_cachewrite;
static uint32_t g_pu_data_rw, g_pu_code_rw;
static uint32_t g_pu_region[8];
static uint32_t g_trace_process_id;

static void update_dtcm(void) {
    if (g_cp15_control & (1u << 16)) {
        uint32_t size = 0x200u << ((g_dtcm_setting >> 1) & 0x1F);
        if (size < 0x1000) size = 0x1000;
        uint32_t mask = 0xFFFFF000u & ~(size - 1);
        g_dtcm_base = g_dtcm_setting & mask;
        g_dtcm_size = size;
        g_dtcm_enabled = 1;
    } else {
        g_dtcm_base = 0xFFFFFFFFu; g_dtcm_size = 0; g_dtcm_enabled = 0;
    }
}

static void update_itcm(void) {
    g_itcm_enabled = (g_cp15_control >> 18) & 1;
    g_itcm_size = g_itcm_enabled ? 0x200u << ((g_itcm_setting >> 1) & 0x1F) : 0;
}

void cp15_reset(void) {
    g_cp15_control = 0x00000078;
    g_dtcm_setting = g_itcm_setting = 0;
    g_pu_data_cacheable = g_pu_code_cacheable = g_pu_data_cachewrite = 0;
    g_pu_data_rw = g_pu_code_rw = 0;
    memset(g_pu_region, 0, sizeof g_pu_region);
    g_trace_process_id = 0;
    update_dtcm(); update_itcm();
}

/* legacy (2-bit) access permissions <-> extended (4-bit) */
static uint32_t perm_expand(uint32_t v) {
    uint32_t r = 0;
    for (int i = 0; i < 8; i++) r |= ((v >> (2 * i)) & 3) << (4 * i);
    return r;
}
static uint32_t perm_compact(uint32_t v) {
    uint32_t r = 0;
    for (int i = 0; i < 8; i++) r |= ((v >> (4 * i)) & 3) << (2 * i);
    return r;
}

void cp15_write(Cpu* c, uint32_t op1, uint32_t crn, uint32_t crm, uint32_t op2, uint32_t val) {
    (void)op1;
    uint32_t id = ((crn & 15) << 8) | ((crm & 15) << 4) | (op2 & 7);
    if (c == &g_arm7) { LOG_INFO("ARM7: CP15 write %03x ignored\n", id); return; }
    switch (id) {
    case 0x100:
        val &= 0x000FF085u;
        g_cp15_control = (g_cp15_control & ~0x000FF085u) | val;
        update_dtcm(); update_itcm();
        return;
    case 0x200: g_pu_data_cacheable = val; return;
    case 0x201: g_pu_code_cacheable = val; return;
    case 0x300: g_pu_data_cachewrite = val; return;
    case 0x500: g_pu_data_rw = perm_expand(val); return;
    case 0x501: g_pu_code_rw = perm_expand(val); return;
    case 0x502: g_pu_data_rw = val; return;
    case 0x503: g_pu_code_rw = val; return;
    case 0x704: case 0x782:        /* wait for interrupt */
        bios_halt(&g_c9);
        return;
    case 0x910:
        g_dtcm_setting = val & 0xFFFFF03Eu;
        update_dtcm();
        return;
    case 0x911:
        g_itcm_setting = val & 0x0000003Eu;
        update_itcm();
        return;
    case 0xD01: g_trace_process_id = val; return;
    }
    if ((id & 0xF00) == 0x600 && (id & 0xE) == 0) { g_pu_region[(id >> 4) & 7] = val; return; }
    if ((id & 0xF00) == 0x700 || (id & 0xF00) == 0xF00) return;   /* cache maintenance, debug */
    LOG_INFO("unknown CP15 write %03x %08x\n", id, val);
}

uint32_t cp15_read(Cpu* c, uint32_t op1, uint32_t crn, uint32_t crm, uint32_t op2) {
    (void)op1;
    uint32_t id = ((crn & 15) << 8) | ((crm & 15) << 4) | (op2 & 7);
    if (c == &g_arm7) { LOG_INFO("ARM7: CP15 read %03x\n", id); return 0; }
    switch (id) {
    case 0x000: case 0x003: case 0x004: case 0x005: case 0x006: case 0x007:
        return 0x41059461u;
    case 0x001: return 0x0F0D2112u;
    case 0x002: return (6u << 6) | (5u << 18);
    case 0x100: return g_cp15_control;
    case 0x200: return g_pu_data_cacheable;
    case 0x201: return g_pu_code_cacheable;
    case 0x300: return g_pu_data_cachewrite;
    case 0x500: return perm_compact(g_pu_data_rw);
    case 0x501: return perm_compact(g_pu_code_rw);
    case 0x502: return g_pu_data_rw;
    case 0x503: return g_pu_code_rw;
    case 0x910: return g_dtcm_setting;
    case 0x911: return g_itcm_setting;
    case 0xD01: return g_trace_process_id;
    }
    if ((id & 0xF00) == 0x600 && (id & 0xE) == 0) return g_pu_region[(id >> 4) & 7];
    if ((id & 0xF00) == 0xF00) return 0;
    LOG_INFO("unknown CP15 read %03x\n", id);
    return 0;
}
