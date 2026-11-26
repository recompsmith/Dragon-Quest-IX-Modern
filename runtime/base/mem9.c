/* Adapted from melonDS src/NDS.cpp (b86390e), GPL-3.0-or-later:
 * NDS::ARM9Read8 ... ARM9Write32, MapSharedWRAM, and the TCM checks of
 * src/CP15.cpp (ARMv5::DataRead*, DataWrite*). Map: GBATEK "DS Memory Map".
 *
 * Added for this project: every write into the overlay area bumps
 * g_overlay_gen (dispatch rechecks which overlays are loaded); with HD text
 * on, hdtext_write sees each CPU write to main RAM or VRAM before it lands;
 * writes through the null-pointer mirror of the ITCM are reported once per
 * return address; DQIX_WATCH logs writes to chosen addresses. */
#include "base.h"

uint8_t g_main_ram[MAIN_RAM_SIZE], g_itcm[ITCM_SIZE], g_dtcm[DTCM_SIZE], g_swram[SWRAM_SIZE];
uint8_t g_palette[0x800], g_oam[0x800];
uint32_t g_overlay_area_start, g_overlay_area_end, g_overlay_gen;
uint8_t g_wramcnt;

/* shared WRAM as each CPU sees it (MapSharedWRAM) */
uint8_t* g_swram9; uint32_t g_swram9_mask;
uint8_t* g_swram7; uint32_t g_swram7_mask;

void mem_set_wramcnt(uint8_t v) {
    g_wramcnt = v & 3;
    switch (g_wramcnt) {
    case 0: g_swram9 = &g_swram[0]; g_swram9_mask = 0x7FFF; g_swram7 = NULL; g_swram7_mask = 0; break;
    case 1: g_swram9 = &g_swram[0x4000]; g_swram9_mask = 0x3FFF; g_swram7 = &g_swram[0]; g_swram7_mask = 0x3FFF; break;
    case 2: g_swram9 = &g_swram[0]; g_swram9_mask = 0x3FFF; g_swram7 = &g_swram[0x4000]; g_swram7_mask = 0x3FFF; break;
    default: g_swram9 = NULL; g_swram9_mask = 0; g_swram7 = &g_swram[0]; g_swram7_mask = 0x7FFF; break;
    }
}

/* ---- DQIX_WATCH ---- */
static uint32_t g_watch[16];
int g_nwatch;
void mem_watch_init(void) {
    g_nwatch = 0;
    const char* w = getenv("DQIX_WATCH");
    while (w && *w && g_nwatch < 16) {
        char* end;
        unsigned long v = strtoul(w, &end, 16);
        if (end == w) break;
        g_watch[g_nwatch++] = (uint32_t)v;
        w = *end == ',' ? end + 1 : end;
    }
}
void mem_watch_check(int cpu, uint32_t addr, uint32_t val, int size) {
    for (int i = 0; i < g_nwatch; i++)
        if (g_watch[i] >= addr && g_watch[i] < addr + (uint32_t)size)
            LOG_WARN("watch: ARM%d writes %0*x at %08x (lr %08x)\n", cpu, size * 2, val, addr,
                     cpu == 7 ? g_arm7.r[14] : g_arm9.r[14]);
}

/* ---- main RAM writes (both CPUs, DMA, BIOS) ---- */
void mem_main_write(uint32_t addr, uint32_t val, int size, int hooks) {
    uint8_t* p = &g_main_ram[addr & (MAIN_RAM_SIZE - 1)];
    if ((hooks & MEMHOOK_HDTEXT) && g_hdtext_any) {
        uint8_t v[4]; memcpy(v, &val, 4);
        hdtext_write(p, v, size);
    }
    uint32_t a = 0x02000000u | (addr & (MAIN_RAM_SIZE - 1));
    if (a < g_overlay_area_end && a + (uint32_t)size > g_overlay_area_start) g_overlay_gen++;
    memcpy(p, &val, (size_t)size);
}

static void vram_write9(uint32_t addr, uint32_t val, int size, int hooks) {
    uint8_t* p = vram_ptr(addr);
    if (!p) return;
    if ((hooks & MEMHOOK_HDTEXT) && g_hdtext_any) { uint8_t v[4]; memcpy(v, &val, 4); hdtext_write(p, v, size); }
    memcpy(p, &val, (size_t)size);
}

/* ---- null pointer ---- */
static uint32_t g_null_lr[64];
static int g_nnull;
static void null_write(uint32_t addr, uint32_t val) {
    uint32_t lr = g_arm9.r[14];
    for (int i = 0; i < g_nnull; i++) if (g_null_lr[i] == lr) return;
    if (g_nnull < 64) g_null_lr[g_nnull++] = lr;
    LOG_WARN("frame %llu: write to %08x (ITCM via the null-pointer mirror) lr=%08x r0=%08x value %08x\n",
             (unsigned long long)g_frame, addr, lr, g_arm9.r[0], val);
}

static int g_unmapped_logs;
static void unmapped(const char* what, uint32_t addr) {
    if (g_unmapped_logs < 32) { g_unmapped_logs++; LOG_INFO("ARM9 %s %08x: nothing there\n", what, addr); }
}

void mem9_reset(void) {
    memset(g_main_ram, 0, sizeof g_main_ram); memset(g_itcm, 0, sizeof g_itcm);
    memset(g_dtcm, 0, sizeof g_dtcm); memset(g_swram, 0, sizeof g_swram);
    memset(g_palette, 0, sizeof g_palette); memset(g_oam, 0, sizeof g_oam);
    g_overlay_gen = 0;
    g_nnull = 0; g_unmapped_logs = 0;
    mem_set_wramcnt(3);
    mem_watch_init();
}

/* ARM9 bus (after the TCM checks), any size; addr aligned by the caller */
static uint32_t bus9_read(uint32_t addr, int size) {
    uint32_t v = 0;
    switch (addr >> 24) {
    case 0x02: memcpy(&v, &g_main_ram[addr & (MAIN_RAM_SIZE - 1)], (size_t)size); return v;
    case 0x03: if (g_swram9) memcpy(&v, &g_swram9[addr & g_swram9_mask], (size_t)size); return v;
    case 0x04: return io9_read(addr, size);
    case 0x05: memcpy(&v, &g_palette[addr & 0x7FF], (size_t)size); return v;
    case 0x06: { uint8_t* p = vram_ptr(addr); if (p) memcpy(&v, p, (size_t)size); return v; }
    case 0x07: memcpy(&v, &g_oam[addr & 0x7FF], (size_t)size); return v;
    case 0x08: case 0x09:                       /* GBA slot, empty: open bus */
        if (g_io[0x204] & 0x80) return 0;
        v = ((addr >> 1) & 0xFFFF) | (((addr + 2) >> 1) & 0xFFFF) << 16;
        if (size == 1) v = (addr & 1) ? (v >> 8) & 0xFF : v & 0xFF;
        else if (size == 2) v &= 0xFFFF;
        return v;
    case 0x0A:
        if (g_io[0x204] & 0x80) return 0;
        return size == 1 ? 0xFF : size == 2 ? 0xFFFF : 0xFFFFFFFFu;
    case 0xFF:                                  /* BIOS: not present (HLE) */
        return 0;
    }
    unmapped("read", addr);
    return 0;
}

static void bus9_write(uint32_t addr, uint32_t val, int size) {
    switch (addr >> 24) {
    case 0x02: mem_main_write(addr, val, size, MEMHOOK_HDTEXT); return;
    case 0x03: if (g_swram9) memcpy(&g_swram9[addr & g_swram9_mask], &val, (size_t)size); return;
    case 0x04: io9_write(addr, val, size); return;
    case 0x05: if (size > 1) memcpy(&g_palette[addr & 0x7FF], &val, (size_t)size); return;   /* 8-bit writes ignored */
    case 0x06: if (size > 1) vram_write9(addr, val, size, MEMHOOK_HDTEXT); return;
    case 0x07: if (size > 1) memcpy(&g_oam[addr & 0x7FF], &val, (size_t)size); return;
    case 0x08: case 0x09: case 0x0A: return;
    }
    unmapped("write", addr);
}

static inline int in_itcm(uint32_t addr) { return addr < g_itcm_size; }
static inline int in_dtcm(uint32_t addr) { return g_dtcm_enabled && (addr & ~(g_dtcm_size - 1)) == g_dtcm_base; }

uint32_t mem9_read(uint32_t addr, int size) {
    uint32_t v = 0;
    if (in_itcm(addr)) { memcpy(&v, &g_itcm[addr & (ITCM_SIZE - 1)], (size_t)size); return v; }
    if (in_dtcm(addr)) { memcpy(&v, &g_dtcm[addr & (DTCM_SIZE - 1)], (size_t)size); return v; }
    return bus9_read(addr, size);
}

void mem9_write(uint32_t addr, uint32_t val, int size, int hooks) {
    if (g_nwatch) mem_watch_check(9, addr, val, size);
    if (in_itcm(addr)) {
        if (addr < 0x1000) null_write(addr, val);
        memcpy(&g_itcm[addr & (ITCM_SIZE - 1)], &val, (size_t)size);
        return;
    }
    if (in_dtcm(addr)) { memcpy(&g_dtcm[addr & (DTCM_SIZE - 1)], &val, (size_t)size); return; }
    if ((addr >> 24) == 0x02) { mem_main_write(addr, val, size, hooks); return; }
    if ((addr >> 24) == 0x06) { if (size > 1) vram_write9(addr, val, size, hooks); return; }
    bus9_write(addr, val, size);
}

/* ---- what the translated code calls ---- */
uint8_t mem_read8(uint32_t a) {
    if ((a >> 24) == 0x02 && !in_dtcm(a)) return g_main_ram[a & (MAIN_RAM_SIZE - 1)];
    return (uint8_t)mem9_read(a, 1);
}
uint16_t mem_read16(uint32_t a) {
    a &= ~1u;
    if ((a >> 24) == 0x02 && !in_dtcm(a)) { uint16_t v; memcpy(&v, &g_main_ram[a & (MAIN_RAM_SIZE - 1)], 2); return v; }
    return (uint16_t)mem9_read(a, 2);
}
uint32_t mem_read32(uint32_t a) {
    a &= ~3u;
    if ((a >> 24) == 0x02 && !in_dtcm(a)) { uint32_t v; memcpy(&v, &g_main_ram[a & (MAIN_RAM_SIZE - 1)], 4); return v; }
    return mem9_read(a, 4);
}
void mem_write8(uint32_t a, uint8_t v) { mem9_write(a, v, 1, MEMHOOK_HDTEXT); }
void mem_write16(uint32_t a, uint16_t v) { mem9_write(a & ~1u, v, 2, MEMHOOK_HDTEXT); }
void mem_write32(uint32_t a, uint32_t v) { mem9_write(a & ~3u, v, 4, MEMHOOK_HDTEXT); }
