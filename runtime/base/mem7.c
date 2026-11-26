/* Adapted from melonDS src/NDS.cpp (b86390e), GPL-3.0-or-later:
 * NDS::ARM7Read8 ... ARM7Write32. Map: GBATEK "DS Memory Map".
 * No ARM7 BIOS is loaded (BIOS calls are HLE): its area reads as 0. */
#include "base.h"

uint8_t g_wram7[WRAM7_SIZE];

static int g_unmapped_logs;
static void unmapped(const char* what, uint32_t addr) {
    if (g_unmapped_logs < 32) { g_unmapped_logs++; LOG_INFO("ARM7 %s %08x: nothing there\n", what, addr); }
}

void mem7_reset(void) { memset(g_wram7, 0, sizeof g_wram7); g_unmapped_logs = 0; }

uint32_t mem7_read(uint32_t addr, int size) {
    uint32_t v = 0;
    if (addr < 0x4000) return 0;                                /* BIOS */
    switch (addr >> 23) {
    case 0x04: case 0x05:                                       /* 0x02000000-0x02FFFFFF */
        memcpy(&v, &g_main_ram[addr & (MAIN_RAM_SIZE - 1)], (size_t)size); return v;
    case 0x06:                                                  /* 0x03000000: shared WRAM, or ARM7 WRAM */
        if (g_swram7) memcpy(&v, &g_swram7[addr & g_swram7_mask], (size_t)size);
        else memcpy(&v, &g_wram7[addr & (WRAM7_SIZE - 1)], (size_t)size);
        return v;
    case 0x07:                                                  /* 0x03800000 */
        memcpy(&v, &g_wram7[addr & (WRAM7_SIZE - 1)], (size_t)size); return v;
    case 0x08: case 0x09:                                       /* 0x04000000 I/O, 0x04800000 wireless */
        return io7_read(addr, size);
    case 0x0C: case 0x0D: { uint8_t* p = vram7_ptr(addr); if (p) memcpy(&v, p, (size_t)size); return v; }
    case 0x10: case 0x11: case 0x12: case 0x13:                 /* GBA slot */
        if (!(g_io[0x204] & 0x80)) return 0;
        v = ((addr >> 1) & 0xFFFF) | (((addr + 2) >> 1) & 0xFFFF) << 16;
        if (size == 1) v = (addr & 1) ? (v >> 8) & 0xFF : v & 0xFF;
        else if (size == 2) v &= 0xFFFF;
        return v;
    case 0x14: case 0x15:
        if (!(g_io[0x204] & 0x80)) return 0;
        return size == 1 ? 0xFF : size == 2 ? 0xFFFF : 0xFFFFFFFFu;
    }
    unmapped("read", addr);
    return 0;
}

void mem7_write(uint32_t addr, uint32_t val, int size, int hooks) {
    if (g_nwatch) mem_watch_check(7, addr, val, size);
    switch (addr >> 23) {
    case 0x04: case 0x05: mem_main_write(addr, val, size, hooks); return;
    case 0x06:
        if (g_swram7) memcpy(&g_swram7[addr & g_swram7_mask], &val, (size_t)size);
        else memcpy(&g_wram7[addr & (WRAM7_SIZE - 1)], &val, (size_t)size);
        return;
    case 0x07: memcpy(&g_wram7[addr & (WRAM7_SIZE - 1)], &val, (size_t)size); return;
    case 0x08: case 0x09: io7_write(addr, val, size); return;
    case 0x0C: case 0x0D: {
        uint8_t* p = vram7_ptr(addr);
        if (!p) return;
        if ((hooks & MEMHOOK_HDTEXT) && g_hdtext_any) { uint8_t v[4]; memcpy(v, &val, 4); hdtext_write(p, v, size); }
        memcpy(p, &val, (size_t)size);
        return;
    }
    case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: return;
    }
    if (addr >= 0x01000000) unmapped("write", addr);
}

uint8_t mem7_read8(uint32_t a) { return (uint8_t)mem7_read(a, 1); }
uint16_t mem7_read16(uint32_t a) { return (uint16_t)mem7_read(a & ~1u, 2); }
uint32_t mem7_read32(uint32_t a) {
    a &= ~3u;
    if ((a >> 24) == 0x02) { uint32_t v; memcpy(&v, &g_main_ram[a & (MAIN_RAM_SIZE - 1)], 4); return v; }
    return mem7_read(a, 4);
}
void mem7_write8(uint32_t a, uint8_t v) { mem7_write(a, v, 1, MEMHOOK_HDTEXT); }
void mem7_write16(uint32_t a, uint16_t v) { mem7_write(a & ~1u, v, 2, MEMHOOK_HDTEXT); }
void mem7_write32(uint32_t a, uint32_t v) { mem7_write(a & ~3u, v, 4, MEMHOOK_HDTEXT); }
