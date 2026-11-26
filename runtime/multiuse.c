/* Use several at once (multiuse.h): the item menu's "To whom?" answered with
 * A on an everyday item the party has more than one of opens a small window
 * over the game, "How many do you want to use?", then "Use 5 Seeds of
 * Strength on Patty?" Yes / No; Yes uses them one after another, the game's
 * own way (its keys pressed for the player), the game's messages shown.
 *
 * All from main RAM: the field menu's object (the item under the hand, the
 * top window's chosen row, how deep its windows go), the menus' stack (which
 * window is on top), the message shown, the bag and what each one carries (as
 * journal_alchemy.c reads them), the party's names. Found by comparing memory
 * between runs going through Items (European version: on the others it stays
 * off, rom_is_eur()). The player stops it with B. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "nds.h"
#include "romdata.h"
#include "journal.h"
#include "multiuse.h"

/* ---------------------------------------------------------------- the windows over the game */
static int lang(void) { return g_fw_language >= 1 && g_fw_language <= 5 ? g_fw_language - 1 : 0; }

enum { MU_OFF, MU_COUNT, MU_CONFIRM, MU_RUN };
static int g_state, g_n = 1, g_max = 1, g_yes = 1;     /* g_yes: Yes chosen (the confirmation) */
static char g_item[96], g_who[64];
static int g_dirty = 1;
static uint8_t* g_pic; static int g_pw, g_ph;

/* a key's cap: a small framed box with its name, its meaning under it */
static void cap(const JournalCanvas* c, int cx, int y, const char* key, const char* under) {
    int w = journal_text_width(18, key) + 22; if (w < 40) w = 40;
    journal_box(c, cx - w / 2 - 1, y - 1, w + 2, 36, 0x8A8A8A, 1);
    journal_box(c, cx - w / 2, y, w, 34, 0x1A1E24, 1);
    journal_text(c, cx - journal_text_width(18, key) / 2, y + 6, 18, 0xF0F0F0, key);
    if (under && *under) journal_text(c, cx - journal_text_width(15, under) / 2, y + 40, 15, 0xB8BCC4, under);
}
static void centred(const JournalCanvas* c, int cx, int y, float px, uint32_t rgb, const char* s) {
    journal_text(c, cx - journal_text_width(px, s) / 2, y, px, rgb, s);
}

const uint8_t* multiuse_picture(int w, int h, int* changed) {
    *changed = 0;
    if (!rom_is_eur()) return NULL;                       /* (the menus' addresses: found on the European version only) */
    if ((g_state != MU_COUNT && g_state != MU_CONFIRM) || w <= 0 || h <= 0) return NULL;
    if (w != g_pw || h != g_ph || !g_pic) {
        free(g_pic); g_pic = malloc((size_t)w * h * 4);
        if (!g_pic) { g_pw = g_ph = 0; return NULL; }
        g_pw = w; g_ph = h; g_dirty = 1;
    }
    if (!g_dirty) return g_pic;
    g_dirty = 0; *changed = 1;
    memset(g_pic, 0, (size_t)w * h * 4);
    int l = lang();
    JournalCanvas c = { g_pic, w, h, 0, 0, 0, 0, 0, 0, 0, l };
    c.u = fminf((float)w / JOURNAL_W, (float)h / JOURNAL_H);
    c.ox = (w - JOURNAL_W * c.u) / 2; c.oy = (h - JOURNAL_H * c.u) / 2;
    const int bx = 283, by = 190, bw = 714, bh = 330, cx = bx + bw / 2;
    journal_box(&c, bx - 1, by - 1, bw + 2, bh + 2, 0x8A8A8A, 0.9f);   /* the grey line round it */
    journal_box(&c, bx, by, bw, bh, 0x101216, 0.80f);                  /* see-through */
    char t[256];
    if (g_state == MU_COUNT) {
        static const char* const q[5] = { "How many do you want to use?", "Combien voulez-vous en utiliser ?", "Wie viele m\xC3\xB6" "chtest du benutzen?", "Quanti vuoi usarne?", "\xC2\xBF" "Cu\xC3\xA1ntos quieres usar?" };
        static const char* const of[5] = { "of %d \xC2\xB7 %s", "sur %d \xC2\xB7 %s", "von %d \xC2\xB7 %s", "su %d \xC2\xB7 %s", "de %d \xC2\xB7 %s" };
        static const char* const ok[5] = { "OK", "Valider", "OK", "Conferma", "Aceptar" };
        static const char* const no[5] = { "Cancel", "Annuler", "Abbrechen", "Annulla", "Cancelar" };
        centred(&c, cx, by + 30, 27, 0xF0F0F0, q[l]);
        snprintf(t, sizeof t, "%d", g_n);
        centred(&c, cx, by + 82, 64, 0xF2C94C, t);
        cap(&c, cx - 140, by + 102, "\xE2\x97\x80", "\xE2\x88\x92" "1");
        cap(&c, cx + 140, by + 102, "\xE2\x96\xB6", "+1");
        cap(&c, bx + 52, by + 130, "L", "\xE2\x88\x92" "10");
        cap(&c, bx + bw - 52, by + 130, "R", "+10");
        snprintf(t, sizeof t, of[l], g_max, g_item);
        centred(&c, cx, by + 190, 17, 0xB8BCC4, t);
        int aw = journal_text_width(18, ok[l]), nw = journal_text_width(18, no[l]);
        int x0 = cx - (40 + 10 + aw + 40 + 40 + 10 + nw) / 2;
        cap(&c, x0 + 20, by + 258, "A", NULL); journal_text(&c, x0 + 50, by + 264, 18, 0xF0F0F0, ok[l]);
        x0 += 40 + 10 + aw + 40;
        cap(&c, x0 + 20, by + 258, "B", NULL); journal_text(&c, x0 + 50, by + 264, 18, 0xF0F0F0, no[l]);
    } else {
        static const char* const use[5] = { "Use %s \xC3\x97%d", "Utiliser %s \xC3\x97%d", "%s \xC3\x97%d benutzen", "Usare %s \xC3\x97%d", "\xC2\xBFUsar %s \xC3\x97%d" };
        static const char* const on[5] = { "on %s?", "sur %s ?", "f\xC3\xBCr %s?", "su %s?", "en %s?" };
        static const char* const yn[5][2] = { { "Yes", "No" }, { "Oui", "Non" }, { "Ja", "Nein" }, { "S\xC3\xAC", "No" }, { "S\xC3\xAD", "No" } };
        snprintf(t, sizeof t, use[l], g_item, g_n);
        centred(&c, cx, by + 60, 27, 0xF0F0F0, t);
        snprintf(t, sizeof t, on[l], g_who);
        centred(&c, cx, by + 102, 27, 0xF0F0F0, t);
        for (int i = 0; i < 2; i++) {
            int x = cx + (i ? 30 : -230), sel = i == !g_yes;
            if (sel) { journal_box(&c, x, by + 215, 200, 56, 0x8A6A1C, 0.85f); journal_text(&c, x + 14, by + 228, 22, 0xF2C94C, "\xE2\x96\xB6"); }
            centred(&c, x + 100, by + 228, 22, sel ? 0xFFE89A : 0xF0F0F0, yn[l][i]);
        }
    }
    return g_pic;
}

/* ---------------------------------------------------------------- the game's menus (main RAM, the European version) */
#define MENU_TOP   0x10F3D0      /* the menus' stack: its top window's record (0x44 bytes each, one more per window opened) */
#define MENU_OBJ   0x1D83AC      /* -> the field menu's object: */
#define OBJ_ITEM   0x7C0         /*   u16 the item under the hand (Items), kept while its windows are open */
#define OBJ_CURSOR 0xEE8         /*   u8 the top window's chosen row */
#define OBJ_LEVEL  0xF78         /*   u8 how deep its windows go: 4 who, 5 the items, 6 the item's menu, 7 "To whom?" */
#define MSG_OBJ    0x11532C      /* -> the message shown (0: none) */
#define PARTY      0x0F3888      /* the party's four records, 0x964 apart: the name at +0... */
#define MEMBER_GAP 0x964
#define CARRIED    0x418         /* ...what one carries at +0x418 (u16 each; the leader, the fourth record at 0x0F54B4: 0x0F58CC as in journal_alchemy.c and mapmarks.c) */
#define CARRIED_N  8             /*   8 of them (mapmarks.c's HERO_SLOTS; the skill points follow at +0x428) */
#define NAMES      0x0FB258      /* the names in the order shown, 0x34 apart */
#define BAG_IDS    0x0F5DE8      /* the bag's everyday items (u16, 0xFFFF after the last)... */
#define BAG_COUNTS 0x0F5F18      /* ...and their counts (u8) */
#define BAG_N      152
#define K_A 0x001
#define K_B 0x002
#define K_RIGHT 0x010
#define K_LEFT 0x020
#define K_UP 0x040
#define K_DOWN 0x080
#define K_R 0x100
#define K_L 0x200

static uint32_t r32(uint32_t a) { uint32_t v; memcpy(&v, &g_main_ram[a & (MAIN_RAM_SIZE - 4)], 4); return v; }
static uint16_t r16(uint32_t a) { uint16_t v; memcpy(&v, &g_main_ram[a & (MAIN_RAM_SIZE - 2)], 2); return v; }
static uint8_t r8(uint32_t a) { return g_main_ram[a & (MAIN_RAM_SIZE - 1)]; }
static uint32_t obj(void) { uint32_t p = r32(MENU_OBJ); return p >= 0x02000000u && p < 0x02400000u - 0x1000u ? p - 0x02000000u : 0; }
static uint32_t top_win(void) { return r32(MENU_TOP); }   /* (changes as windows open) */
static uint16_t hovered(void) { uint32_t o = obj(); return o ? r16(o + OBJ_ITEM) : 0xFFFF; }
static int cursor(void) { uint32_t o = obj(); return o ? r8(o + OBJ_CURSOR) : -1; }
static int everyday(uint16_t id) { return id >= 0x55F0 && id < 0x5800; }

static int bag_count(uint16_t id) {
    for (int i = 0; i < BAG_N; i++) { uint16_t k = r16(BAG_IDS + 2u * i); if (k == 0xFFFF) break; if (k == id) return r8(BAG_COUNTS + (uint32_t)i); }
    return 0;
}
static uint32_t record(int slot) {                     /* the record of the one shown slot-th (0: none) */
    if (slot < 0 || slot > 3) return 0;
    const char* n = (const char*)&g_main_ram[NAMES + 0x34u * (uint32_t)slot];
    if (!n[0]) return 0;
    for (int i = 0; i < 4; i++) { uint32_t r = PARTY + MEMBER_GAP * (uint32_t)i; if (!strncmp(n, (const char*)&g_main_ram[r], 12)) return r; }
    return 0;
}
static int carried_count(int slot, uint16_t id) {
    uint32_t r = record(slot); int n = 0;
    if (!r) return 0;
    for (int i = 0; i < CARRIED_N; i++) n += r16(r + CARRIED + 2u * i) == id;   /* (an empty slot: 0 or 0xFFFF, never an everyday item) */
    return n;
}
/* how many the source holds: a companion's own (0-3), else the bag (4) */
static int g_src;
static int held(uint16_t id) { return g_src < 4 ? carried_count(g_src, id) : bag_count(id); }

static int level(void);
static int g_last_lv = -1, g_last_row = -1, g_who_row = -1;   /* the last A: its window's level and row; the row chosen in "who" (level 4) */
static void remember(void) { g_last_lv = level(); g_last_row = cursor(); if (g_last_lv == 4) g_who_row = g_last_row; }

static uint16_t g_id;                 /* the item */
static int g_target, g_left, g_had;   /* who (the row in "To whom?"), how many still to use, the count before this one */
static int g_step, g_wait, g_tap, g_hold, g_away;   /* the macro's step, frames waited, a key held for the game (frames left) */
static uint16_t g_key;
static uint16_t g_was;                /* the player's keys last frame */
static int g_rep, g_repk;             /* held arrow: repeats */

static void member_name(int slot, char* out, size_t size) {
    char raw[24]; uint32_t r = record(slot);
    if (!r) { snprintf(out, size, "?"); return; }
    memcpy(raw, &g_main_ram[r], 16); raw[16] = 0;
    romdata_clean_name(raw, out, size);
}

/* ---------------------------------------------------------------- the keys */
static int g_trace = -1;
#define TRACE(...) do { if (g_trace) { fprintf(stderr, "[multiuse] f%llu ", (unsigned long long)g_frame); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static void stop(const char* why) { TRACE("stop: %s", why); g_state = MU_OFF; g_dirty = 1; g_hold = 0; g_key = 0; g_tap = 12; }

/* one use: from the list, the item under the hand, A; "What do you want to
 * do?", Use, A; "To whom?", the companion, A; the messages, A. After it the
 * game builds its windows again: the bag's item back on "What do you want to
 * do?", a carried one's list (that one gone). Each window waited for still
 * (the field menu's level: 5 the list, 6 its menu, 7 "To whom?"). */
enum { S_NAV, S_MSGS };
static int g_stable, g_moves;
#define MSG_A_MAX 6                   /* the A pressed through one use's messages, at most (an A may only finish a line being written) */
static int g_msg_a;
static uint32_t g_lastw; static int g_lastl = -1, g_lastc = -1;
static void press(uint16_t k, int frames) { g_key = k; g_hold = frames; }
static int level(void) { uint32_t o = obj(); return o ? r8(o + OBJ_LEVEL) : -1; }
static int message(void) { return r32(MSG_OBJ) != 0; }

static void run(void) {
    uint32_t w = top_win(); int lv = level(), c = cursor(), m = message();
    if (g_trace) { static uint32_t lw; static int ll = -9, lc = -9, lm = -1; static uint16_t lh;
        if (w != lw || lv != ll || c != lc || hovered() != lh || m != lm) { TRACE("win %08x level %d row %d item %04x msg %d step %d held %d", w, lv, c, hovered(), m, g_step, held(g_id)); lw = w; ll = lv; lc = c; lh = hovered(); lm = m; } }
    g_stable = w == g_lastw && lv == g_lastl && c == g_lastc && !m && !g_hold ? g_stable + 1 : 0;
    g_lastw = w; g_lastl = lv; g_lastc = c;
    g_wait++;
    if (g_wait > 60 * 15) { stop("too long"); return; }
    if (g_hold) return;
    if (g_step == S_MSGS) {                                      /* used: its messages, A now and then, until a window is still */
        if (m) {
            g_away = 1;
            if (g_wait % 30 == 15) {                                 /* (a plain use: two or three messages; more, something else is asked: the player's to answer) */
                if (++g_msg_a > MSG_A_MAX) { stop("messages go on (a question?)"); return; }
                press(K_A, 6);
            }
            return;
        }
        if (!g_away && held(g_id) >= g_had) { if (g_wait > 120) stop("not used"); return; }
        g_away = 1;
        if (g_stable < 30) return;
        int now = held(g_id);
        if (now >= g_had) { stop("not used (no effect)"); return; }
        g_left -= g_had - now; TRACE("used, %d left (%d held)", g_left, now);
        if (g_left <= 0 || now <= 0) { stop("done"); return; }
        g_step = S_NAV; g_wait = 0; g_moves = 0;
        return;
    }
    if (g_stable < 12) return;
    if (++g_moves > 40) { stop("lost"); return; }
    switch (lv) {
    case 5:                                                      /* the list: the hand onto the item, A */
        press(hovered() != g_id ? K_DOWN : K_A, 6); return;
    case 6:                                                      /* its menu: Use (the first row), A */
        if (hovered() != g_id) { stop("other item"); return; }
        press(c != 0 ? K_UP : K_A, 6); return;
    case 7:                                                      /* "To whom?": the companion, A */
        if (c != g_target) { press(c < g_target ? K_DOWN : K_UP, 6); return; }
        g_had = held(g_id); g_away = 0; g_msg_a = 0; g_step = S_MSGS; g_wait = 0; press(K_A, 6); return;
    default: stop("not the item's windows"); return;
    }
}

int multiuse_keys(uint16_t* keys, int field) {
    if (!rom_is_eur()) return 0;                          /* (MENU_OBJ, MENU_TOP, MSG_OBJ: found on the European version only; elsewhere off) */
    if (g_trace < 0) g_trace = getenv("DQIX_MULTIUSE_TRACE") != NULL;
    uint16_t now = *keys, pressed = now & ~g_was;
    g_was = now;
    if (g_state == MU_RUN) {
        *keys = 0;
        if (pressed & K_B) { stop("B"); return 1; }             /* the player stops it */
        run();
        if (g_state == MU_RUN && g_hold > 0) { g_hold--; *keys = g_key; if (!g_hold) g_tap = g_tap > 4 ? g_tap : 4; }
        return 1;
    }
    if (g_state == MU_COUNT || g_state == MU_CONFIRM) {
        int n0 = g_n, y0 = g_yes, s0 = g_state;
        uint16_t arrows = now & (K_LEFT | K_RIGHT);
        if (arrows && arrows == g_repk) { if (++g_rep > 18 && g_rep % 4 == 0) pressed |= arrows; } else { g_repk = arrows; g_rep = 0; }
        if (g_state == MU_COUNT) {
            if (pressed & K_LEFT) g_n--;
            if (pressed & K_RIGHT) g_n++;
            if (pressed & K_L) g_n -= 10;
            if (pressed & K_R) g_n += 10;
            if (pressed & K_UP) g_n = g_max;
            if (pressed & K_DOWN) g_n = 1;
            if (g_n < 1) g_n = 1;
            if (g_n > g_max) g_n = g_max;
            if (pressed & K_B) { stop("cancel"); }
            else if (pressed & K_A) {
                if (g_n == 1) { g_left = 1; g_state = MU_RUN; g_step = S_NAV; g_wait = 0; g_stable = 12; g_moves = 0; g_lastw = top_win(); g_lastl = level(); g_lastc = cursor(); TRACE("one"); }
                else { g_state = MU_CONFIRM; g_yes = 1; }
            }
        } else {
            if (pressed & (K_LEFT | K_RIGHT | K_UP | K_DOWN)) g_yes = !g_yes;
            if (pressed & K_B) g_state = MU_COUNT;
            else if (pressed & K_A) {
                if (!g_yes) g_state = MU_COUNT;
                else { g_left = g_n; g_state = MU_RUN; g_step = S_NAV; g_wait = 0; g_stable = 12; g_moves = 0; g_lastw = top_win(); g_lastl = level(); g_lastc = cursor(); TRACE("run %d", g_n); }
            }
        }
        if (g_n != n0 || g_yes != y0 || g_state != s0) g_dirty = 1;
        *keys = 0;
        return 1;
    }
    if (g_tap > 0) { g_tap--; *keys = 0; if (now) g_tap = g_tap < 2 ? 2 : g_tap; return 1; }   /* (after: nothing until the keys are let go) */
    if (!field) { g_last_lv = -1; return 0; }                  /* (battles: their own windows) */
    if (!(pressed & K_A)) return 0;
    /* A: on "To whom?" after Use on an everyday item of which there is more than one? */
    uint16_t id = hovered();
    if (level() == 7 && !message() && g_last_lv == 6 && g_last_row == 0 && everyday(id) && cursor() >= 0) {
        g_id = id; g_src = record(g_who_row) && carried_count(g_who_row, g_id) ? g_who_row : 4;
        g_max = held(g_id); if (g_max > 99) g_max = 99;
        TRACE("To whom: item %04x from %d, %d held, row %d", g_id, g_src, g_max, cursor());
        if (g_max >= 2) {
            g_target = cursor(); g_n = 1;
            const char* nm = romdata_item_name(g_id);
            if (nm) romdata_clean_name(nm, g_item, sizeof g_item); else snprintf(g_item, sizeof g_item, "?");
            if (g_item[0] >= 'a' && g_item[0] <= 'z') g_item[0] -= 32;   /* (the game's names start small: "graine de Force") */
            member_name(g_target, g_who, sizeof g_who);
            g_state = MU_COUNT; g_dirty = 1; g_rep = 0; g_repk = 0;
            *keys = 0;
            return 1;
        }
    }
    remember();
    return 0;
}
