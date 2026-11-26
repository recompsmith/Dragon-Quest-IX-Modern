/* The party's HP and MP, modern: on foot, a small panel at the bottom left,
 * each one's name with two bars (HP green, MP blue); the menu open, the same
 * panel larger: the name, the level, HP and MP as bars and numbers. A
 * companion knocked out: the name red. (TitanFr199's mockup "carte ronde v2", 31/10:
 * drawn in its pixels, 4 per DS pixel; the caller scales it.)
 *
 * From main RAM (journal_party.c): the names in the order the menus show
 * them, 0x34 apart from 0x020FB258 (the level at +0x32); each one's record
 * among the party's four, 0x964 apart from 0x020F3888, found by the name:
 * the max HP and MP at +0x30 and +0x32, the HP and MP now at -0x20 and -0x1E
 * (found by their values against the game's "To whom?" window). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nds.h"
#include "romdata.h"
#include "journal.h"
#include "text.h"
#include "partyhud.h"

#define PARTY  0x0F3888
#define MEMBER 0x964
#define NAMES  0x0FB258

typedef struct { char name[40]; int lv, hp, hpm, mp, mpm; } Member;

static uint16_t rd16(uint32_t a) { uint16_t v; memcpy(&v, &g_main_ram[a & (MAIN_RAM_SIZE - 2)], 2); return v; }

static int read_party(Member* m) {
    int n = 0;
    for (int s = 0; s < 4; s++) {
        const char* nm = (const char*)&g_main_ram[NAMES + 0x34u * (uint32_t)s];
        if (!nm[0]) break;
        uint32_t rec = 0;
        for (int i = 0; i < 4 && !rec; i++) { uint32_t r = PARTY + MEMBER * (uint32_t)i; if (!strncmp(nm, (const char*)&g_main_ram[r], 12)) rec = r; }
        if (!rec) break;
        char raw[16]; memcpy(raw, nm, 12); raw[12] = 0;
        Member* p = &m[n++];
        memset(p, 0, sizeof *p);
        romdata_clean_name(raw, p->name, sizeof p->name);
        p->lv = rd16(NAMES + 0x34u * (uint32_t)s + 0x32);
        p->hp = rd16(rec - 0x20); p->mp = rd16(rec - 0x1E);
        p->hpm = rd16(rec + 0x30); p->mpm = rd16(rec + 0x32);
        if (p->hpm > 9999 || p->mpm > 9999 || p->hp > p->hpm || p->mp > p->mpm || p->lv < 1 || p->lv > 99) return 0;   /* (not the game's party yet) */
    }
    return n;
}

static uint8_t* g_pic; static int g_pw, g_ph;
static Member g_was[4]; static int g_was_n = -1, g_was_open = -1; static float g_was_s;

static void bar(const JournalCanvas* c, int x, int y, int w, int h, int now, int max, uint32_t rgb) {
    journal_box(c, x, y, w, h, 0x5A5E66, 0.95f);                       /* its line */
    journal_box(c, x + 1, y + 1, w - 2, h - 2, 0x101216, 0.95f);
    int f = max > 0 ? (int)((long)(w - 2) * now / max) : 0;
    if (now > 0 && f < 1) f = 1;
    if (f > 0) journal_box(c, x + 1, y + 1, f, h - 2, rgb, 1.0f);
}

const uint8_t* partyhud_picture(int open, float s, int* pw_out, int* ph_out, int* wu, int* hu, int* changed) {
    Member m[4];
    int n = read_party(m);
    *changed = 0;
    if (n <= 0 || s <= 0) return NULL;
    int W = open ? 320 : 170, rows = open ? 44 : 30, H = 8 + rows * n + 2;   /* (the mockup's pixels) */
    *wu = W; *hu = H;
    static unsigned font;                                                /* (the font changed: drawn again) */
    int same_font = font == text_font_gen(); font = text_font_gen();
    if (g_pic && same_font && n == g_was_n && open == g_was_open && s == g_was_s && !memcmp(m, g_was, sizeof(Member) * (size_t)n)) { *pw_out = g_pw; *ph_out = g_ph; return g_pic; }
    float u = s / 4;                                                     /* (4 of its pixels to a DS pixel) */
    int pw = (int)(W * u + 0.5f), ph = (int)(H * u + 0.5f);
    if (pw != g_pw || ph != g_ph || !g_pic) {
        free(g_pic); g_pic = calloc((size_t)pw * ph, 4);
        if (!g_pic) { g_pw = g_ph = 0; return NULL; }
        g_pw = pw; g_ph = ph;
    } else memset(g_pic, 0, (size_t)pw * ph * 4);
    *pw_out = pw; *ph_out = ph;
    memcpy(g_was, m, sizeof(Member) * (size_t)n); g_was_n = n; g_was_open = open; g_was_s = s;
    *changed = 1;
    JournalCanvas c = { g_pic, pw, ph, u, 0, 0, 0, 0, W, H, 0 };
    journal_box(&c, 0, 0, W, H, 0x0E1014, 0.72f);                      /* the modern windows': see-through, a thin grey line */
    journal_box(&c, 0, 0, W, 2, 0x8A8A8A, 0.95f); journal_box(&c, 0, H - 2, W, 2, 0x8A8A8A, 0.95f);
    journal_box(&c, 0, 0, 2, H, 0x8A8A8A, 0.95f); journal_box(&c, W - 2, 0, 2, H, 0x8A8A8A, 0.95f);
    for (int i = 0; i < n; i++) {
        const Member* p = &m[i];
        uint32_t col = p->hp == 0 ? 0xF0443A : 0xF0F0F0;                /* (knocked out: red, as the game's lists) */
        char t[32];
        if (!open) {
            int y = 6 + rows * i;
            journal_text(&c, 10, y + 4, 15, col, p->name);
            bar(&c, 80, y + 9, 82, 6, p->hp, p->hpm, 0x41C85B);
            bar(&c, 80, y + 16, 82, 6, p->mp, p->mpm, 0x3E86E8);
            continue;
        }
        int y = 6 + rows * i;
        if (i) journal_box(&c, 8, y - 1, W - 16, 1, 0x5A5E66, 0.8f);
        journal_text(&c, 12, y + 4, 18, col, p->name);
        static const char* const lvl[5] = { "Lv. %d", "Niv. %d", "Lv. %d", "Liv. %d", "Nv. %d" };
        int l = g_fw_language >= 1 && g_fw_language <= 5 ? g_fw_language - 1 : 0;
        snprintf(t, sizeof t, lvl[l], p->lv);
        journal_text(&c, 12, y + 25, 12, 0xA8ACB4, t);
        static const char* const hp[5] = { "HP", "PV", "LP", "PV", "PV" }, *const mp[5] = { "MP", "PM", "MP", "PM", "PM" };
        journal_text(&c, 124, y + 4, 12, 0xA8ACB4, hp[l]);
        bar(&c, 150, y + 7, 100, 9, p->hp, p->hpm, 0x41C85B);
        snprintf(t, sizeof t, "%d/%d", p->hp, p->hpm);
        journal_text(&c, 258, y + 3, 13, col, t);
        journal_text(&c, 124, y + 23, 12, 0xA8ACB4, mp[l]);
        bar(&c, 150, y + 26, 100, 9, p->mp, p->mpm, 0x3E86E8);
        snprintf(t, sizeof t, "%d/%d", p->mp, p->mpm);
        journal_text(&c, 258, y + 22, 13, 0xF0F0F0, t);
    }
    return g_pic;
}
