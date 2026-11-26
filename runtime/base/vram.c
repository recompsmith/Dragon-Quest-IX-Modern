/* Adapted from melonDS src/GPU.cpp (b86390e), GPL-3.0-or-later:
 * GPU::MapVRAM_AB ... MapVRAM_I (which bank shows where for each VRAMCNT
 * value). Table: GBATEK "DS Memory Control - VRAM".
 *
 * melonDS keeps a bitmask of banks per 16 KiB page and ORs overlapping
 * banks; here each page points at one bank (the first, A to I) and an
 * overlap is reported once (the game is not seen to overlap banks). */
#include "base.h"

uint8_t g_vram[0xA4000];
uint8_t* g_tex_slot[4];
uint8_t* g_texpal_slot[6];

/* banks A..I: offset in g_vram and size */
static const uint32_t k_bank_off[9] = { 0x00000, 0x20000, 0x40000, 0x60000, 0x80000, 0x90000, 0x94000, 0x98000, 0xA0000 };
static const uint32_t k_bank_size[9] = { 0x20000, 0x20000, 0x20000, 0x20000, 0x10000, 0x4000, 0x4000, 0x8000, 0x4000 };
static const uint8_t k_cnt_mask[9] = { 0x9B, 0x9B, 0x9F, 0x9F, 0x87, 0x9F, 0x9F, 0x83, 0x83 };
static uint8_t g_vramcnt[9];

/* 16 KiB pages of each ARM9 view */
static uint8_t* g_abg[32];      /* 512 KiB, 0x06000000 */
static uint8_t* g_bbg[8];       /* 128 KiB, 0x06200000 */
static uint8_t* g_aobj[16];     /* 256 KiB, 0x06400000 */
static uint8_t* g_bobj[8];      /* 128 KiB, 0x06600000 */
static uint8_t* g_lcdc[41];     /* 0x06800000 - 0x068A3FFF */
static uint8_t* g_arm7v[2];     /* ARM7 view, 2 x 128 KiB at 0x06000000 */
static int g_overlap_warned;

uint8_t* vram_bank(int b) { return (b >= 0 && b < 9) ? &g_vram[k_bank_off[b]] : NULL; }
uint32_t vram_bank_size(int b) { return (b >= 0 && b < 9) ? k_bank_size[b] : 0; }
uint8_t vram_bank_cnt(int b) { return (b >= 0 && b < 9) ? g_vramcnt[b] : 0; }

static void put(uint8_t** pages, int npages, int first, uint8_t* base, uint32_t size, int bank) {
    for (uint32_t o = 0; o < size; o += 0x4000) {
        int p = first + (int)(o >> 14);
        if (p < 0 || p >= npages) continue;
        if (pages[p]) {
            if (!g_overlap_warned) { g_overlap_warned = 1; LOG_WARN("VRAM: bank %c overlaps another bank (first one kept)\n", 'A' + bank); }
            continue;
        }
        pages[p] = base + o;
    }
}
static void put_slot(uint8_t** slots, int n, int slot, uint8_t* base, int bank) {
    if (slot < 0 || slot >= n) return;
    if (slots[slot]) {
        if (!g_overlap_warned) { g_overlap_warned = 1; LOG_WARN("VRAM: bank %c overlaps another bank (first one kept)\n", 'A' + bank); }
        return;
    }
    slots[slot] = base;
}

void vram_remap(void) {
    memset(g_abg, 0, sizeof g_abg); memset(g_bbg, 0, sizeof g_bbg);
    memset(g_aobj, 0, sizeof g_aobj); memset(g_bobj, 0, sizeof g_bobj);
    memset(g_lcdc, 0, sizeof g_lcdc); memset(g_arm7v, 0, sizeof g_arm7v);
    memset(g_tex_slot, 0, sizeof g_tex_slot); memset(g_texpal_slot, 0, sizeof g_texpal_slot);
    for (int b = 0; b < 9; b++) {
        uint8_t cnt = g_vramcnt[b];
        if (!(cnt & 0x80)) continue;
        uint8_t* base = &g_vram[k_bank_off[b]];
        uint32_t size = k_bank_size[b];
        int mst = cnt & 7, ofs = (cnt >> 3) & 3;
        if (mst == 0) { put(g_lcdc, 41, (int)(k_bank_off[b] >> 14), base, size, b); continue; }
        switch (b) {
        case 0: case 1:                                    /* A, B */
            if (mst == 1) put(g_abg, 32, ofs * 8, base, size, b);
            else if (mst == 2) put(g_aobj, 16, (ofs & 1) * 8, base, size, b);
            else if (mst == 3) put_slot(g_tex_slot, 4, ofs, base, b);
            break;
        case 2: case 3:                                    /* C, D */
            if (mst == 1) put(g_abg, 32, ofs * 8, base, size, b);
            else if (mst == 2) put_slot(g_arm7v, 2, ofs & 1, base, b);
            else if (mst == 3) put_slot(g_tex_slot, 4, ofs, base, b);
            else if (mst == 4) { if (b == 2) put(g_bbg, 8, 0, base, size, b); else put(g_bobj, 8, 0, base, size, b); }
            break;
        case 4:                                            /* E */
            if (mst == 1) put(g_abg, 32, 0, base, size, b);
            else if (mst == 2) put(g_aobj, 16, 0, base, size, b);
            else if (mst == 3) for (int i = 0; i < 4; i++) put_slot(g_texpal_slot, 6, i, base + i * 0x4000, b);
            /* 4: BG extended palettes (gpu2d reads them through vram_bank) */
            break;
        case 5: case 6: {                                  /* F, G */
            int page = (ofs & 1) + (ofs >> 1) * 4;
            if (mst == 1) put(g_abg, 32, page, base, size, b);
            else if (mst == 2) put(g_aobj, 16, page, base, size, b);
            else if (mst == 3) put_slot(g_texpal_slot, 6, (ofs & 1) + (ofs >> 1) * 4, base, b);
            break;
        }
        case 7:                                            /* H */
            if (mst == 1) put(g_bbg, 8, 0, base, size, b);
            break;
        case 8:                                            /* I */
            if (mst == 1) put(g_bbg, 8, 2, base, size, b);
            else if (mst == 2) put(g_bobj, 8, 0, base, size, b);
            break;
        }
    }
}

/* VRAMCNT_A..G at 0x04000240-0x04000246, H and I at 0x04000248-0x04000249 */
void vram_write_cnt(int bank, uint8_t v) {
    if (bank < 0 || bank > 8) return;
    v &= k_cnt_mask[bank];
    if (g_vramcnt[bank] == v) return;
    g_vramcnt[bank] = v;
    vram_remap();
}

/* VRAMSTAT (ARM7 0x04000240): C and D given to the ARM7 */
uint8_t vram_stat(void) {
    uint8_t s = 0;
    if ((g_vramcnt[2] & 0x87) == 0x82) s |= 1;
    if ((g_vramcnt[3] & 0x87) == 0x82) s |= 2;
    return s;
}

void vram_reset(void) {
    memset(g_vram, 0, sizeof g_vram);
    memset(g_vramcnt, 0, sizeof g_vramcnt);
    g_overlap_warned = 0;
    vram_remap();
}

uint8_t* vram_ptr(uint32_t addr) {
    if ((addr >> 24) != 6) return NULL;
    uint8_t* p;
    switch ((addr >> 21) & 7) {
    case 0: p = g_abg[(addr >> 14) & 31]; break;
    case 1: p = g_bbg[(addr >> 14) & 7]; break;
    case 2: p = g_aobj[(addr >> 14) & 15]; break;
    case 3: p = g_bobj[(addr >> 14) & 7]; break;
    default: {
        uint32_t off = addr & 0xFFFFF;
        p = (off >> 14) < 41 ? g_lcdc[off >> 14] : NULL;
        break;
    }
    }
    return p ? p + (addr & 0x3FFF) : NULL;
}

uint8_t* vram7_ptr(uint32_t addr) {
    if ((addr >> 24) != 6) return NULL;
    uint8_t* p = g_arm7v[(addr >> 17) & 1];
    return p ? p + (addr & 0x1FFFF) : NULL;
}
