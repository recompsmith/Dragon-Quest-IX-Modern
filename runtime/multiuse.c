/* Use several at once (multiuse.h). On an everyday item the party has more
 * than one of (a fixed heal: Medicinal herb and the Strong, Special and
 * Superior medicines; Magic water, the Sage's and Elfin elixirs; or a seed), the item's "What do you want to do?" has
 * one more row under Use: "Use several". Chosen (with A, a touch or the
 * mouse), the game asks "To whom?" as for Use; the companion chosen, a small
 * window opens beside it, "How many?", then "Use 5 Seeds of Strength on
 * Patty?" Yes / No; Yes uses one the game's own way (its keys pressed for
 * the player). Its effect is then the game's own, once for each one chosen
 * (a seed's gain, HP or MP healed: those functions hooked), added up in the
 * one message ("Strength goes up by 100", "recovers 120 HP"), the others
 * taken from the bag or the companion. B stops it.
 *
 * The row is the game's own: its window is written as text with tags
 * (<N=0>Use</N>...) by func_ov002_0215cd98, hooked (hooks.txt), which gets
 * one more: <N=1>, the ones after it one further on (the window tells its
 * rows apart by that value, and moves between them). What the item's menu
 * then asks of the window (func_0205d7a4, hooked) is given back as the game
 * counts its rows, ours being Use's: the game does Use for it; which of the
 * two was chosen, the hook on that menu's frame (func_ov002_021631e8) sees.
 * "To whom?"'s frame
 * (func_ov002_02163664), hooked too, opens the window instead of using the
 * item when the companion is chosen after "Use several".
 *
 * All from main RAM: the field menu's object (the item under the hand, the
 * top window's chosen row, how deep its windows go), the message shown, the
 * bag and what each one carries (as journal_alchemy.c reads them), the
 * party's names. Found by comparing memory between runs going through Items
 * (European version: on the others it stays off, rom_is_eur()). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "nds.h"
#include "cpu.h"
#include "romdata.h"
#include "journal.h"
#include "text.h"
#include "hints.h"
#include "multiuse.h"

/* ---------------------------------------------------------------- the game's menus (main RAM, the European version) */
#define MENU_OBJ   0x1D83AC      /* -> the field menu's object (r0 of the hooked functions): */
#define OBJ_ITEM   0x7C0         /*   u16 the item under the hand (Items), kept while its windows are open */
#define OBJ_WM     0xEC8         /*   its windows: */
#define WM_CURSOR  0x20          /*     u8 the top window's chosen row */
#define WM_LEVEL   0xB0          /*     u8 how deep they go: 4 who, 5 the items, 6 the item's menu, 7 "To whom?" */
#define OBJ_STEP   0x1BC0        /*   u32 the screen's step (1: its window up, waiting) */
#define OBJ_WHOSE  0x1C20        /*   s8 whose items the list shows: a companion (0-3, the game's order), else the bag */
#define MSG_OBJ    0x11532C      /* -> the message shown (0: none) */
#define PAD        0x02114E30    /* the game's buttons this frame: u16 pressed, u16 ... (func_02012454) */
#define PAD_OK     0x601         /*   the ones that choose (as the menus test them) */
#define TOUCH_ON   0x02114E54    /* +0x55: the touch screen in use (func_0205bd30) */
#define PARTY      0x0F3888      /* the party's four records, 0x964 apart: the name at +0... */
#define MEMBER_GAP 0x964
#define CARRIED    0x418         /* ...what one carries at +0x418 (u16 each) */
#define CARRIED_N  8
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
static void w8(uint32_t a, uint8_t v) { g_main_ram[a & (MAIN_RAM_SIZE - 1)] = v; }
static void w16(uint32_t a, uint16_t v) { memcpy(&g_main_ram[a & (MAIN_RAM_SIZE - 2)], &v, 2); }
static uint32_t obj(void) { uint32_t p = r32(MENU_OBJ); return p >= 0x02000000u && p < 0x02400000u - 0x2000u ? p : 0; }
static uint16_t hovered(void) { uint32_t o = obj(); return o ? r16(o + OBJ_ITEM) : 0xFFFF; }
static int cursor(void) { uint32_t o = obj(); return o ? r8(o + OBJ_WM + WM_CURSOR) : -1; }
static int level(void) { uint32_t o = obj(); return o ? r8(o + OBJ_WM + WM_LEVEL) : -1; }
static int message(void) { return r32(MSG_OBJ) != 0; }

/* the items it is for: the fixed heals (not the leaf or dew of Yggdrasil, the
 * antidotes), the ones for MP (Magic water, Sage's and Elfin elixirs) and the seeds */
static int several(uint16_t id) { return (id >= 0x55F0 && id <= 0x55F3) || (id >= 0x55FF && id <= 0x5601) || (id >= 0x561D && id <= 0x5625); }

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
/* how many the list's owner holds: a companion's own (0-3), else the bag (4) */
static int g_src;
static int held(uint16_t id) { return g_src < 4 ? carried_count(g_src, id) : bag_count(id); }
/* whose list it is: the bag (4), else the companion as shown in "who"
 * (its row there, kept by multiuse_keys(): the game's own number, at
 * OBJ_WHOSE, counts them in another order) */
static int g_who_row = 4;
static int whose(uint32_t o) { int8_t s = (int8_t)r8(o + OBJ_WHOSE); return s >= 0 && s <= 3 && record(g_who_row) ? g_who_row : 4; }

static void member_name(int slot, char* out, size_t size) {
    char raw[24]; uint32_t r = record(slot);
    if (!r) { snprintf(out, size, "?"); return; }
    memcpy(raw, &g_main_ram[r], 16); raw[16] = 0;
    romdata_clean_name(raw, out, size);
}

static int g_trace = -1;
#define TRACE(...) do { if (g_trace < 0) g_trace = getenv("DQIX_MULTIUSE_TRACE") != NULL; \
    if (g_trace) { fprintf(stderr, "[multiuse] f%llu ", (unsigned long long)g_frame); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

/* ---------------------------------------------------------------- the state */
enum { MU_OFF, MU_COUNT, MU_CONFIRM, MU_RUN };
static int g_state, g_n = 1, g_max = 1, g_yes = 1;     /* g_yes: Yes chosen (the confirmation) */
static char g_item[96], g_who[64];
static int g_dirty = 1;
static uint16_t g_id;                                  /* the item */
static int g_target, g_left, g_had;                    /* who (the row in "To whom?"), how many still to use, the count before this one */
static int g_row;                                      /* "Use several" is this row of the item's menu (0: not there) */
static int g_pending;                                  /* chosen: the companion chosen next opens the window */
static uint64_t g_alive;                               /* the frame the frontend last asked (its window can be drawn) */
static int g_true = -1;                                /* the row really chosen in the item's menu (hook_MenuRow) */
static int g_more;                                     /* the ones used by repeat() besides the game's own (to take away) */
static int g_redo;                    /* a companion's others taken: their list shown again after (B, then A: the game's own list is a copy) */
static int g_bag_more;                                 /* ...from the bag: taken with the game's own one (hook_BagTake) */

/* ---------------------------------------------------------------- the row in the item's menu */
static int lang(void) { return g_fw_language >= 1 && g_fw_language <= 5 ? g_fw_language - 1 : 0; }

/* func_ov002_0215cd98(object, text, frame): the window's text, "<CURSOR=0>
 * <TITLE=16>What do you want to do?</TITLE><N=0>Use</N>" + a line +
 * "<N=1>Transfer</N>"... (<FRAME=..> first when a row was touched). Ours
 * goes after Use: <N=1>, the rows after it one further on; the row chosen
 * and the one touched (CURSOR, FRAME: the game's [object + 0x1BEA], as it
 * counts them) as the window counts them. */
void hook_ItemCmdText(Cpu* c, void (*orig)(Cpu*)) {
    uint32_t o = c->r[0], text = c->r[1];
    orig(c);
    g_row = 0;
    if (!rom_is_eur() || g_frame - g_alive > 4) return;     /* (only with its window drawn: OpenGL) */
    uint16_t id = r16(o + OBJ_ITEM);
    g_src = whose(o);
    int n = several(id) ? held(id) : 0;
    TRACE("item menu: item %04x, whose %d, %d held", id, g_src, n);
    if (n < 2) return;
    static const char* const label[5] = { "Use several", "Utiliser plusieurs", "Mehrere benutzen", "Usa pi<`u> volte", "Usar varios" };
    enum { MAX = 0x960 };
    char s[MAX];
    uint32_t len = 0;
    while (len < MAX - 1 && (s[len] = (char)r8(text + len))) len++;
    s[len] = 0;
    static const char* const at[2] = { "<CURSOR=", "<FRAME=" };   /* the row chosen, the row touched: as the game counts them, without ours (ours is Use's) */
    for (int i = 0; i < 2; i++) {
        char* cur = strstr(s, at[i]);
        if (!cur) continue;
        char* num = cur + strlen(at[i]);
        int k = atoi(num), t = g_true >= 0 && (g_true == 1 ? 0 : g_true > 1 ? g_true - 1 : g_true) == k ? g_true : k >= 1 ? k + 1 : k;
        char* e = num; while (*e == '-' || (*e >= '0' && *e <= '9')) e++;
        char v[12]; int vl = snprintf(v, sizeof v, "%d", t);
        if (vl <= 0 || strlen(s) + (size_t)vl >= MAX) continue;
        memmove(num + vl, e, strlen(e) + 1); memcpy(num, v, (size_t)vl);
    }
    len = (uint32_t)strlen(s);
    char* end = strstr(s, "</N>");
    if (!end) return;
    end += 4;
    char* next = strstr(end, "<N=");                       /* the line between two rows: as the game writes it */
    if (!next) return;
    for (char* p = next; (p = strstr(p, "<N=")); p += 3) {    /* the rows after it: one further on (the window's rows are told apart by their value) */
        int v = atoi(p + 3);
        char* e = strchr(p, '>');
        char tag[16]; int tl = snprintf(tag, sizeof tag, "<N=%d>", v + 1);
        if (!e || tl <= 0 || strlen(s) + (size_t)tl - (size_t)(e + 1 - p) >= MAX) return;
        memmove(p + tl, e + 1, strlen(e + 1) + 1); memcpy(p, tag, (size_t)tl);
    }
    len = (uint32_t)strlen(s);
    char row[160];
    int k = snprintf(row, sizeof row, "%.*s<N=1>%s</N>", (int)(next - end), end, label[lang()]);
    if (k <= 0 || len + (uint32_t)k >= MAX) return;
    memmove(end + k, end, strlen(end) + 1);
    memcpy(end, row, (size_t)k);
    for (uint32_t i = 0; i <= len + (uint32_t)k; i++) w8(text + i, (uint8_t)s[i]);
    g_row = 1;
    TRACE("row added: %s", s);
}

/* func_0205d7a4(windows): the top window's chosen row. The item's menu asks
 * it once a frame (func_ov002_021631e8, returning to 0x0216329c) and does
 * what that row does (0 Use, 1 Transfer...): with ours, its rows as the game
 * counts them (ours: Use's). */
#define CMD_ASKS 0x0216329Cu
void hook_MenuRow(Cpu* c, void (*orig)(Cpu*)) {
    uint32_t lr = c->r[14];
    orig(c);
    if (lr != CMD_ASKS) return;
    int t = (int32_t)c->r[0];
    g_true = t;
    if (g_row && t >= g_row) c->r[0] = (uint32_t)(t == g_row ? 0 : t - 1);
}

/* The game's own test for "chosen" on the top window: a button that chooses,
 * pressed this frame (func_02012454), or the row touched (func_0205da48: the
 * window's record, func_0205d82c; touched, its bit 1 at +0xC5). */
static uint32_t window_record(uint32_t wm) {
    if (!r32(wm + 0x98)) return 0;
    uint32_t base = r32(wm + 0x9C);
    if (!base) return 0;
    int id = r8(wm + WM_LEVEL), n = r8(wm + 0xB3), skip = r8(wm + 0xB4);
    for (int i = 0; i < n; i++) {
        if (i == skip) return 0;
        uint32_t rec = base + 0xE0u * (uint32_t)i;
        if (r8(rec + 0xC4) == id) return rec;
    }
    return 0;
}
static int pressed_ok(void) { return (r16(PAD) & PAD_OK) && !(r16(PAD + 2) & PAD_OK); }
static uint32_t touched(uint32_t wm) {                 /* the window's record if a row was touched, else 0 */
    if (!r8(TOUCH_ON + 0x55) || r8(wm + 4 + 0x4E) != 1 || !r8(wm + 4 + 0x4C)) return 0;
    uint32_t rec = window_record(wm);
    return rec && r32(rec + 0x9C) == 3 && (r8(rec + 0xC5) & 2) ? rec : 0;
}

/* func_ov002_021631e8(object): the item's menu, once a frame. Chosen on our
 * row: the game does Use ("To whom?"), and the companion chosen opens ours. */
void hook_ItemCmdFrame(Cpu* c, void (*orig)(Cpu*)) {
    uint32_t o = c->r[0], wm = o + OBJ_WM;
    if (g_row && r32(o + OBJ_STEP) == 1 && g_state == MU_OFF) {
        int row = r8(wm + WM_CURSOR), key = pressed_ok(), tch = touched(wm) != 0;
        if (key || tch) TRACE("item menu: row %d chosen (%s)", row, key ? "key" : "touch");
        g_pending = row == g_row && (key || tch);
        if (g_pending) { g_id = r16(o + OBJ_ITEM); g_src = whose(o); TRACE("Use several chosen (item %04x, whose %d)", g_id, g_src); }
    }
    orig(c);
}

static void open_count(int target);

/* func_ov002_02163664(object): "To whom?", once a frame. After "Use
 * several": the companion chosen, our window instead of the item used (the
 * game's frame still runs, without the choosing: its cursor goes where it
 * was touched). */
void hook_ItemWhoFrame(Cpu* c, void (*orig)(Cpu*)) {
    uint32_t o = c->r[0], wm = o + OBJ_WM;
    if (!g_pending || g_state != MU_OFF || r32(o + OBJ_STEP) != 1) { orig(c); return; }
    uint32_t rec = touched(wm);
    int key = pressed_ok();
    if (!key && !rec) { orig(c); return; }
    uint16_t pad = r16(PAD);
    w16(PAD, (uint16_t)(pad & ~PAD_OK));
    if (rec) w8(rec + 0xC5, (uint8_t)(r8(rec + 0xC5) & ~2));
    orig(c);
    open_count(r8(wm + WM_CURSOR));
}

/* Several used at once: the game's own effect, once for the item it uses,
 * again for each other one chosen (the same call, its arguments kept), the
 * amounts added up for the one message; none more once one does nothing (its
 * most reached: the rest kept). The others are taken by take_more(). */
static void take_more(int n);
static int take_carried(int n);
static void says_many(int n);
static void says_back(void);
static int is_seed(uint16_t id) { return id >= 0x561D && id <= 0x5625; }
static void repeat(Cpu* c, void (*orig)(Cpu*), int seed, uint32_t result) {
    uint32_t a[4] = { c->r[0], c->r[1], c->r[2], c->r[3] };
    orig(c);
    if (g_state != MU_RUN || g_left <= 1 || g_more || !several(g_id) || is_seed(g_id) != seed) return;
    int32_t sum = (int16_t)c->r[0];
    if (seed) sum = (int32_t)c->r[0];
    if (sum <= 0) return;
    uint8_t res = result ? r8(result) : 0;                 /* (the heal's outcome, for its message: kept as the first one's) */
    int more = 0;
    while (more < g_left - 1) {
        for (int i = 0; i < 4; i++) c->r[i] = a[i];
        orig(c);
        int32_t got = seed ? (int32_t)c->r[0] : (int16_t)c->r[0];
        if (got <= 0) break;
        sum += got; more++;
    }
    if (result) w8(result, res);
    c->r[0] = (uint32_t)sum;
    g_more = more;
    if (more < g_left - 1) g_left = 1 + more;
    if (more) says_many(1 + more); else says_back();      /* (the message: how many were used) */
    g_bag_more = g_src >= 4 ? more : 0;                    /* (the bag's: taken with the game's own one, hook_BagTake()) */
    if (g_src < 4 && more) { if (take_carried(more)) g_more = 0; g_redo = 1; }   /* (a companion's: now, before the game takes its own one) */
    TRACE("gain: %d more, %d in all", more, sum);
}

/* func_020a0a18(bag, item, count): count of the item taken from the bag (its
 * place emptied at none). The one used: the others too. */
void hook_BagTake(Cpu* c, void (*orig)(Cpu*)) {
    if (g_bag_more > 0 && (uint16_t)c->r[1] == g_id) { c->r[2] += (uint32_t)g_bag_more; TRACE("bag: %d more taken", g_bag_more); g_bag_more = 0; }
    orig(c);
}

/* func_02084a74(member, effect): a lasting gain (a seed's): a random amount
 * added to the member's (up to its most), given back for the message. */
void hook_StatGain(Cpu* c, void (*orig)(Cpu*)) { repeat(c, orig, 1, 0); }
/* func_ov002_021548c4 and _02154944(?, member, amount, outcome): HP and MP
 * healed by the amount (up to the most), given back (s16); *outcome 3, or 0
 * if nothing to heal. */
void hook_HealHP(Cpu* c, void (*orig)(Cpu*)) { repeat(c, orig, 0, c->r[3]); }
void hook_HealMP(Cpu* c, void (*orig)(Cpu*)) { repeat(c, orig, 0, c->r[3]); }

/* A companion's others, before the game takes its own one (the one under
 * the hand, its place g_list_row): the others' places emptied, the ones after
 * moved up, and the hand's place still one of them (the game then takes that
 * place: the list as it would have been). 0 if not as expected. */
static int g_list_row = -1;
static int take_carried(int n) {
    uint32_t r = record(g_src);
    int h = g_list_row;
    if (!r || h < 0 || h >= CARRIED_N || r16(r + CARRIED + 2u * h) != g_id) return 0;
    uint16_t v[CARRIED_N];
    for (int i = 0; i < CARRIED_N; i++) v[i] = r16(r + CARRIED + 2u * i);
    for (int k = 0; k < n; k++) {
        int j = -1;
        for (int i = CARRIED_N - 1; i >= 0 && j < 0; i--) if (i != h && v[i] == g_id) j = i;   /* (after the hand's first) */
        if (j < 0) return 0;
        for (int i = j; i < CARRIED_N - 1; i++) v[i] = v[i + 1];
        v[CARRIED_N - 1] = 0xFFFF;
        if (j < h) { uint16_t t = v[h - 1]; v[h - 1] = v[h]; v[h] = t; }   /* (the hand's one moved up a place: back under the hand) */
    }
    for (int i = 0; i < CARRIED_N; i++) w16(r + CARRIED + 2u * i, v[i]);
    return 1;
}

/* the others used besides the game's own one, a companion's: after it
 * (their places emptied, the ones after moved up, as the game keeps them) */
static void take_more(int n) {
    if (n <= 0 || g_src >= 4) return;
    uint32_t r = record(g_src);
    if (!r) return;
    uint16_t keep[CARRIED_N]; int k = 0;
    for (int i = 0; i < CARRIED_N; i++) {
        uint16_t v = r16(r + CARRIED + 2u * i);
        if (v == g_id && n > 0) { n--; continue; }
        if (v != 0xFFFF && v != 0) keep[k++] = v;
    }
    for (int i = 0; i < CARRIED_N; i++) w16(r + CARRIED + 2u * i, i < k ? keep[i] : 0xFFFF);
}

/* ---------------------------------------------------------------- the windows over the game */
static uint8_t* g_pic; static int g_pw, g_ph;
static float g_anchor[5];                              /* the game's window it stands by: x, y, w, h (drawable pixels), its scale */
static uint64_t g_anchor_at;
typedef struct { float x, y, w, h; int what; } Hit;    /* where the mouse does what (drawable pixels) */
enum { H_LESS, H_MORE, H_LESS10, H_MORE10, H_OK, H_CANCEL, H_YES, H_NO };
static Hit g_hit[8]; static int g_nhit;
static MuKey g_keyshow[4]; static int g_keys_set;      /* A, B, L, R as the player has them */

void multiuse_anchor(float x, float y, float w, float h, float scale) {
    float a[5] = { x, y, w, h, scale };
    if (memcmp(a, g_anchor, sizeof a)) { memcpy(g_anchor, a, sizeof a); g_dirty = 1; }
    g_anchor_at = g_frame;
}
void multiuse_show_keys(const MuKey k[4]) {
    if (g_keys_set && !memcmp(k, g_keyshow, sizeof g_keyshow)) return;
    memcpy(g_keyshow, k, sizeof g_keyshow); g_keys_set = 1; g_dirty = 1;
}
int multiuse_up(void) { return g_state == MU_COUNT || g_state == MU_CONFIRM; }

/* an item's name, one or several (itemname_<language>.nat: 16 bytes each,
 * +0 its name, +4 the plural, +12 its number; the strings after), as UTF-8 */
static void item_name(uint16_t id, int plural, char* out, size_t size) {
    static uint8_t* b; static uint32_t sz; static int blang = -1;
    int l = lang();
    if (blang != l) {
        static const char* const suffix[5] = { "en", "fr", "de", "it", "es" };
        free(b); b = NULL; sz = 0; blang = l;
        uint32_t pn = 0; const uint8_t* pack = romdata_file("data/prm/itemname.gp2", &pn);
        char f[32]; snprintf(f, sizeof f, "itemname_%s.nat", suffix[l]);
        if (pack) b = romdata_gpc_file(pack, pn, f, &sz);
    }
    if (b && sz >= 4) {
        uint32_t count = (b[0] | b[1] << 8) & 0xFFF, base = 4 + 16 * count;
        for (uint32_t i = 0; i < count && base <= sz; i++) {
            const uint8_t* r = b + 4 + 16 * i;
            if ((uint16_t)(r[12] | r[13] << 8) != id) continue;
            uint32_t off = (uint32_t)r[plural ? 4 : 0] | (uint32_t)r[plural ? 5 : 1] << 8 | (uint32_t)r[plural ? 6 : 2] << 16;
            if (base + off >= sz) break;
            char raw[96]; size_t k = 0;
            while (k < sizeof raw - 1 && base + off + k < sz && b[base + off + k]) { raw[k] = (char)b[base + off + k]; k++; }
            raw[k] = 0;
            romdata_clean_name(raw, out, size);
            return;
        }
    }
    const char* nm = romdata_item_name(id);
    if (nm) romdata_clean_name(nm, out, size); else snprintf(out, size, "?");
}

static void open_count(int target) {
    g_pending = 0;
    g_max = held(g_id); if (g_max > 99) g_max = 99;
    TRACE("To whom: item %04x from %d, %d held, row %d", g_id, g_src, g_max, target);
    if (g_max < 1) return;
    g_target = target; g_n = 1;
    member_name(g_target, g_who, sizeof g_who);
    g_state = MU_COUNT; g_dirty = 1;
}

/* the picture: drawable pixels; drawn in the game's window's pixels (DS pixels, its scale) */
typedef struct { float x, y, u; } Org;
static Org g_o;
static JournalCanvas canvas(void) {
    JournalCanvas c = { g_pic, g_pw, g_ph, g_o.u, g_o.x, g_o.y, 0, 0, 0, 0, lang() };
    return c;
}
static void hit(float x, float y, float w, float h, int what) {
    if (g_nhit < 8) { Hit* t = &g_hit[g_nhit++]; t->x = g_o.x + x * g_o.u; t->y = g_o.y + y * g_o.u; t->w = w * g_o.u; t->h = h * g_o.u; t->what = what; }
}
static void blend(int X, int Y, const uint8_t* src, int w, int h) {   /* premultiplied src over the picture */
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int px = X + x, py = Y + y;
            if (px < 0 || py < 0 || px >= g_pw || py >= g_ph) continue;
            const uint8_t* s = &src[((size_t)y * w + x) * 4];
            if (!s[3]) continue;
            uint8_t* d = &g_pic[((size_t)py * g_pw + px) * 4];
            float k = 1 - s[3] / 255.0f;
            for (int i = 0; i < 4; i++) d[i] = (uint8_t)fminf(255, s[i] + d[i] * k);
        }
}
static float keycap(int which, float x, float y, float h) {   /* the player's key (A, B, L, R): its width (DS pixels), 0 if none */
    if (!g_keys_set || !g_keyshow[which].label[0]) return 0;
    const MuKey* k = &g_keyshow[which];
    float aspect = k->style == HINT_KEYBOARD ? fmaxf(1, hint_key_aspect(k->label)) : 1;
    int ph = (int)lroundf(h * g_o.u), pw = (int)lroundf(h * aspect * g_o.u);
    if (ph < 4 || pw < 4 || ph > 400 || pw > 1200) return 0;
    uint8_t* img = malloc((size_t)pw * ph * 4);
    if (!img) return 0;
    hint_draw(img, pw, ph, k->style, k->face, k->label);
    blend((int)lroundf(g_o.x + x * g_o.u), (int)lroundf(g_o.y + y * g_o.u), img, pw, ph);
    free(img);
    return h * aspect;
}
static void triangle(const JournalCanvas* c, float cx, float cy, float r, int left, uint32_t rgb) {   /* an arrow: points left or right */
    (void)c;
    float X = g_o.x + cx * g_o.u, Y = g_o.y + cy * g_o.u, R = r * g_o.u;
    for (int py = (int)floorf(Y - R); py <= (int)ceilf(Y + R); py++)
        for (int px = (int)floorf(X - R); px <= (int)ceilf(X + R); px++) {
            if (px < 0 || py < 0 || px >= g_pw || py >= g_ph) continue;
            float dx = (px + 0.5f - X) * (left ? -1 : 1), dy = fabsf(py + 0.5f - Y);
            float d = fmaxf(-dx - R * 0.6f, dy + dx * 0.95f - R * 0.6f);   /* < 0 inside */
            float a = fminf(fmaxf(0.5f - d, 0), 1);
            if (a <= 0) continue;
            uint8_t* p = &g_pic[((size_t)py * g_pw + px) * 4];
            float k = 1 - a;
            p[0] = (uint8_t)(((rgb >> 16) & 255) * a + p[0] * k); p[1] = (uint8_t)(((rgb >> 8) & 255) * a + p[1] * k);
            p[2] = (uint8_t)((rgb & 255) * a + p[2] * k); p[3] = (uint8_t)(255 * a + p[3] * k);
        }
}
static void line(const JournalCanvas* c, float x, float y, float w) {   /* a window's line inside it */
    journal_box(c, (int)lroundf(x), (int)lroundf(y), (int)lroundf(w), 1, 0x8A8A8A, 0.9f);
}

#define WIN_W 92
#define WIN_H 60
const uint8_t* multiuse_picture(int w, int h, int* changed) {
    *changed = 0;
    if (!rom_is_eur()) return NULL;                       /* (the menus' addresses: found on the European version only) */
    g_alive = g_frame;
    if (!multiuse_up() || w <= 0 || h <= 0) return NULL;
    if (w != g_pw || h != g_ph || !g_pic) {
        free(g_pic); g_pic = malloc((size_t)w * h * 4);
        if (!g_pic) { g_pw = g_ph = 0; return NULL; }
        g_pw = w; g_ph = h; g_dirty = 1;
    }
    if (g_frame - g_anchor_at > 2 && g_anchor[2] > 0) { memset(g_anchor, 0, sizeof g_anchor); g_dirty = 1; }   /* (the game's window gone: in the middle) */
    { static unsigned font; if (font != text_font_gen()) { font = text_font_gen(); g_dirty = 1; } }   /* (the font changed) */
    if (!g_dirty) return g_pic;
    g_dirty = 0; *changed = 1; g_nhit = 0;
    memset(g_pic, 0, (size_t)w * h * 4);
    float u = g_anchor[2] > 0 ? g_anchor[4] : h / 192.0f;
    g_o.u = u;
    if (g_anchor[2] > 0) {                                 /* beside the game's window, its top; on its left if no room */
        g_o.x = g_anchor[0] + g_anchor[2] + 5 * u; g_o.y = g_anchor[1];
        if (g_o.x + WIN_W * u > w) g_o.x = g_anchor[0] - (5 + WIN_W) * u;
    } else { g_o.x = (w - WIN_W * u) / 2; g_o.y = (h - WIN_H * u) / 2; }
    JournalCanvas c = canvas();
    int l = lang();
    journal_box(&c, 0, 0, WIN_W, WIN_H, 0x0E1014, 0.80f);        /* as the game's windows restyled: see-through, a grey line round it */
    journal_box(&c, 0, 0, WIN_W, 1, 0x8A8A8A, 0.95f); journal_box(&c, 0, WIN_H - 1, WIN_W, 1, 0x8A8A8A, 0.95f);
    journal_box(&c, 0, 0, 1, WIN_H, 0x8A8A8A, 0.95f); journal_box(&c, WIN_W - 1, 0, 1, WIN_H, 0x8A8A8A, 0.95f);
    char t[256];
    if (g_state == MU_COUNT) {
        static const char* const q[5] = { "How many?", "Combien ?", "Wie viele?", "Quanti?", "\xC2\xBF" "Cu\xC3\xA1ntos?" };
        static const char* const ok[5] = { "OK", "Valider", "OK", "Conferma", "Aceptar" };
        static const char* const no[5] = { "Cancel", "Annuler", "Abbrechen", "Annulla", "Cancelar" };
        journal_text(&c, 6, 3, 7.5f, 0xF0F0F0, q[l]);
        line(&c, 4, 14, WIN_W - 8);
        snprintf(t, sizeof t, "%d", g_n);
        int nw = journal_text_width(12, t);
        float cx = WIN_W / 2.0f - 6;
        journal_text(&c, (int)lroundf(cx - nw / 2.0f), 16, 12, 0xF2C94C, t);
        triangle(&c, cx - nw / 2.0f - 9, 24.5f, 3.2f, 1, g_n > 1 ? 0xF0F0F0 : 0x60646C);
        triangle(&c, cx + nw / 2.0f + 9, 24.5f, 3.2f, 0, g_n < g_max ? 0xF0F0F0 : 0x60646C);
        hit(cx - nw / 2.0f - 15, 17, 12, 15, H_LESS); hit(cx + nw / 2.0f + 3, 17, 12, 15, H_MORE);
        snprintf(t, sizeof t, "/ %d", g_max);
        journal_text(&c, (int)lroundf(cx + nw / 2.0f + 15), 20, 6.5f, 0xB8BCC4, t);
        float kw = keycap(2, 6, 36, 7);                         /* L -10, R +10 */
        journal_text(&c, (int)lroundf(6 + kw + 2), 36, 6.5f, 0xB8BCC4, "\xE2\x88\x92" "10");
        hit(4, 34, 34, 10, H_LESS10);
        int pw = journal_text_width(6.5f, "+10");
        float rw = g_keys_set && g_keyshow[3].label[0] ? 7 * (g_keyshow[3].style == HINT_KEYBOARD ? fmaxf(1, hint_key_aspect(g_keyshow[3].label)) : 1) : 0;
        float rx = WIN_W - 6 - pw - (rw ? rw + 2 : 0);
        keycap(3, rx, 36, 7);
        journal_text(&c, (int)lroundf(rx + (rw ? rw + 2 : 0)), 36, 6.5f, 0xB8BCC4, "+10");
        hit(rx - 2, 34, WIN_W - rx, 10, H_MORE10);
        kw = keycap(0, 6, 48, 7);                               /* A OK, B Cancel */
        journal_text(&c, (int)lroundf(6 + kw + 2), 48, 6.5f, 0xF0F0F0, ok[l]);
        hit(4, 46, 40, 11, H_OK);
        int cw = journal_text_width(6.5f, no[l]);
        float bw = g_keys_set && g_keyshow[1].label[0] ? 7 * (g_keyshow[1].style == HINT_KEYBOARD ? fmaxf(1, hint_key_aspect(g_keyshow[1].label)) : 1) : 0;
        float bx = WIN_W - 6 - cw - (bw ? bw + 2 : 0);
        keycap(1, bx, 48, 7);
        journal_text(&c, (int)lroundf(bx + (bw ? bw + 2 : 0)), 48, 6.5f, 0xF0F0F0, no[l]);
        hit(bx - 2, 46, WIN_W - bx, 11, H_CANCEL);
    } else {
        static const char* const ask[5] = { "Use %d %s on %s?", "Utiliser %d %s sur %s ?", "%d %s f\xC3\xBCr %s benutzen?", "Usare %d %s su %s?", "\xC2\xBFUsar %d %s en %s?" };
        static const char* const yn[5][2] = { { "Yes", "No" }, { "Oui", "Non" }, { "Ja", "Nein" }, { "S\xC3\xAC", "No" }, { "S\xC3\xAD", "No" } };
        item_name(g_id, g_n > 1, g_item, sizeof g_item);
        snprintf(t, sizeof t, ask[l], g_n, g_item, g_who);
        journal_wrap(&c, 6, 3, WIN_W - 12, 7, 0xF0F0F0, t, 2);
        line(&c, 4, 23, WIN_W - 8);
        for (int i = 0; i < 2; i++) {
            float y = 27 + i * 14.0f;
            int sel = i == !g_yes;
            if (sel) { journal_box(&c, 5, (int)y, WIN_W - 10, 12, 0x8A6A1C, 0.85f); triangle(&c, 11, y + 6, 3, 0, 0xF2C94C); }
            journal_text(&c, 18, (int)y + 1, 8, sel ? 0xFFE89A : 0xF0F0F0, yn[l][i]);
            hit(5, y, WIN_W - 10, 12, i ? H_NO : H_YES);
        }
    }
    if (g_trace) for (int i = 0; i < g_nhit; i++) TRACE("hit %d: %.0f %.0f %.0f %.0f", g_hit[i].what, g_hit[i].x, g_hit[i].y, g_hit[i].w, g_hit[i].h);   /* (tests: where to click) */
    return g_pic;
}

/* ---------------------------------------------------------------- using them: the keys pressed for the player */
/* The game's "<actor> uses <the item>." (its messages' file, in main RAM),
 * while several are used: "<actor> uses 5 <items>." (the plural's tag), put
 * back after. In place: no longer than it was. */
static uint32_t g_says; static char g_said[64]; static size_t g_said_n;
static void says_back(void) {
    if (g_says) { memcpy(&g_main_ram[g_says], g_said, g_said_n); g_says = 0; }
}
static void says_many(int n) {
    static const char* const uses[] = {                    /* (en fr de it es; de: both the game has) */
        "<Cap><DEF_ART_ACTOR> uses <DEF_ART_SGL_I_NAME>.", "<Cap><DEF_ART_ACTOR> utilise <DEF_ART_SGL_I_NAME>.",
        "<Cap><DEF_ART_ACTOR> benutzt <DEF_ART_SGL_ACC_I_NAME>.", "<Cap><DEF_ART_ACTOR> usa <INDEF_ART_SGL_I_NAME>.",
        "<Cap><DEF_ART_ACTOR> utiliza <DEF_ART_SGL_I_NAME>." };
    const char* u = uses[lang()];
    if (!g_says) {
        size_t un = strlen(u) + 1;
        const uint8_t* at = NULL;
        for (uint32_t i = 0; i + un <= MAIN_RAM_SIZE && !at; i++)   /* (memmem: not on Windows) */
            if (g_main_ram[i] == '<' && !memcmp(&g_main_ram[i], u, un)) at = &g_main_ram[i];
        if (!at || un > sizeof g_said) return;
        g_says = (uint32_t)(at - g_main_ram); g_said_n = un; memcpy(g_said, at, un);
    }
    const char* tag = strrchr(g_said, '<');                 /* (the item's tag: the last one) */
    char t[64]; int k = snprintf(t, sizeof t, "%.*s%d <PLR_I_NAME>.", (int)(tag - g_said), g_said, n);
    if (k <= 0 || (size_t)k + 1 > g_said_n) return;
    memcpy(&g_main_ram[g_says], t, (size_t)k + 1);
}
static void stop(const char* why) { TRACE("stop: %s", why); g_state = MU_OFF; g_dirty = 1; says_back(); }

/* one use: from the list, the item under the hand, A; "What do you want to
 * do?", Use, A; "To whom?", the companion, A; the messages, A. After it the
 * game builds its windows again: the bag's item back on "What do you want to
 * do?", a carried one's list (that one gone). Each window waited for still
 * (the field menu's level: 5 the list, 6 its menu, 7 "To whom?"). */
enum { S_NAV, S_MSGS, S_REDO };
static int g_step, g_wait, g_tap, g_hold, g_away;   /* the macro's step, frames waited, a key held for the game (frames left) */
static uint16_t g_key;
static uint16_t g_was;                /* the player's keys last frame */
static int g_rep, g_repk;             /* held arrow: repeats */
static int g_stable, g_moves;
#define MSG_A_MAX 6                   /* the A pressed through one use's messages, at most (an A may only finish a line being written) */
static int g_msg_a;
static uint32_t g_lastw; static int g_lastl = -1, g_lastc = -1;
static void press(uint16_t k, int frames) { g_key = k; g_hold = frames; }
static void end_run(const char* why) { stop(why); g_hold = 0; g_key = 0; g_tap = 12; }

static void run(void) {
    uint32_t o = obj(), w = o ? r32(o + OBJ_WM + 0x98) ^ r8(o + OBJ_WM + WM_LEVEL) : 0;   /* (changes as windows open) */
    int lv = level(), c = cursor(), m = message();
    if (g_trace) { static uint32_t lw; static int ll = -9, lc = -9, lm = -1; static uint16_t lh;
        if (w != lw || lv != ll || c != lc || hovered() != lh || m != lm) { TRACE("win %08x level %d row %d item %04x msg %d step %d held %d", w, lv, c, hovered(), m, g_step, held(g_id)); lw = w; ll = lv; lc = c; lh = hovered(); lm = m; } }
    g_stable = w == g_lastw && lv == g_lastl && c == g_lastc && !m && !g_hold ? g_stable + 1 : 0;
    g_lastw = w; g_lastl = lv; g_lastc = c;
    g_wait++;
    if (g_wait > 60 * 15) { end_run("too long"); return; }
    if (g_hold) return;
    if (g_step == S_MSGS) {                                      /* used: its messages, A now and then, until a window is still */
        if (m) {
            g_away = 1;
            if (g_wait % 30 == 15) {                                 /* (a plain use: two or three messages; more, something else is asked: the player's to answer) */
                if (++g_msg_a > MSG_A_MAX) { end_run("messages go on (a question?)"); return; }
                press(K_A, 6);
            }
            return;
        }
        if (!g_away && held(g_id) >= g_had) { if (g_wait > 120) end_run("not used"); return; }
        g_away = 1;
        if (g_stable < 30) return;
        int now = held(g_id);
        if (now >= g_had) { end_run("not used (no effect)"); return; }
        if (g_more) { take_more(g_more); now = held(g_id); g_more = 0; g_bag_more = 0; }   /* (the others used with it: a companion's taken now) */
        g_left -= g_had - now; TRACE("used, %d left (%d held)", g_left, now);
        if (g_left <= 0 || now <= 0) { if (g_redo) { g_step = S_REDO; g_wait = 0; return; } end_run("done"); return; }
        g_step = S_NAV; g_wait = 0; g_moves = 0;
        return;
    }
    if (g_step == S_REDO) {                                      /* the companion's list shown again: back to "who" (B), their name again (A) */
        if (g_stable < 12) return;
        if (lv == 5) { if (g_redo) press(K_B, 6); else end_run("done (their list again)"); return; }
        if (lv == 4 && g_redo) { g_redo = 0; press(K_A, 6); return; }
        end_run("done"); return;
    }
    if (g_stable < 12) return;
    if (++g_moves > 40) { end_run("lost"); return; }
    switch (lv) {
    case 5:                                                      /* the list: the hand onto the item, A */
        press(hovered() != g_id ? K_DOWN : K_A, 6); return;
    case 6:                                                      /* its menu: Use (the first row), A */
        if (hovered() != g_id) { end_run("other item"); return; }
        press(c != 0 ? K_UP : K_A, 6); return;
    case 7:                                                      /* "To whom?": the companion, A */
        if (c != g_target) { press(c < g_target ? K_DOWN : K_UP, 6); return; }
        g_had = held(g_id); g_away = 0; g_msg_a = 0; g_step = S_MSGS; g_wait = 0; press(K_A, 6); return;
    default: end_run("not the item's windows"); return;
    }
}

static void start_run(void) {
    g_left = g_n; g_state = MU_RUN; g_step = S_NAV; g_wait = 0; g_stable = 12; g_moves = 0; g_more = 0; g_bag_more = 0; g_redo = 0;
    if (g_n > 1) says_many(g_n);
    uint32_t o = obj();
    g_lastw = o ? r32(o + OBJ_WM + 0x98) ^ r8(o + OBJ_WM + WM_LEVEL) : 0; g_lastl = level(); g_lastc = cursor();
    g_dirty = 1;
    TRACE("run %d", g_n);
}
static void cancel_count(void) { stop("cancel"); g_pending = 1; g_tap = 6; }   /* (back on "To whom?", still for several) */

static void act(int what) {                            /* a key or a click on its window */
    int n0 = g_n, y0 = g_yes, s0 = g_state;
    if (g_state == MU_COUNT) {
        switch (what) {
        case H_LESS: g_n--; break;
        case H_MORE: g_n++; break;
        case H_LESS10: g_n -= 10; break;
        case H_MORE10: g_n += 10; break;
        case H_OK: if (g_n == 1) start_run(); else { g_state = MU_CONFIRM; g_yes = 1; } break;
        case H_CANCEL: cancel_count(); return;
        }
        if (g_n < 1) g_n = 1;
        if (g_n > g_max) g_n = g_max;
    } else if (g_state == MU_CONFIRM) {
        switch (what) {
        case H_YES: g_yes = 1; start_run(); break;
        case H_NO: case H_CANCEL: g_state = MU_COUNT; break;
        case H_OK: if (g_yes) start_run(); else g_state = MU_COUNT; break;
        case H_LESS: case H_MORE: g_yes = !g_yes; break;
        }
    }
    if (g_n != n0 || g_yes != y0 || g_state != s0) g_dirty = 1;
}

int multiuse_mouse(float x, float y) {
    if (!multiuse_up()) return 0;
    for (int i = 0; i < g_nhit; i++) {
        const Hit* t = &g_hit[i];
        if (x >= t->x && x < t->x + t->w && y >= t->y && y < t->y + t->h) { act(t->what); return 1; }
    }
    return 1;                                                    /* (elsewhere: nothing, the game doesn't get it either) */
}

int multiuse_keys(uint16_t* keys, int field) {
    if (!rom_is_eur()) return 0;                          /* (the menus' addresses: found on the European version only; elsewhere off) */
    g_alive = g_frame;
    {   uint32_t o = obj(); static int ll = -9, lc = -9;              /* (tests: the menus' windows as they change) */
        if (g_trace && o && (level() != ll || cursor() != lc)) { ll = level(); lc = cursor();
            TRACE("level %d row %d step %d sel %d/%d whose %d item %04x screen %d", ll, lc, (int)r32(o + OBJ_STEP), (int16_t)r16(o + 0x1BEA), (int16_t)r16(o + 0x1BEC), (int8_t)r8(o + OBJ_WHOSE), hovered(), (int)r32(o + 0x1BB8)); } }
    if (g_state == MU_OFF && level() == 4) g_who_row = cursor();   /* (whose list: the row in "who", however chosen) */
    if (g_state == MU_OFF && level() == 5) g_list_row = cursor();  /* (the item's place in a companion's list) */
    uint16_t now = *keys, pressed = now & ~g_was;
    g_was = now;
    if (g_state == MU_RUN) {
        *keys = 0;
        if (pressed & K_B) { end_run("B"); return 1; }          /* the player stops it */
        run();
        if (g_state == MU_RUN && g_hold > 0) { g_hold--; *keys = g_key; if (!g_hold) g_tap = g_tap > 4 ? g_tap : 4; }
        return 1;
    }
    if (multiuse_up()) {
        uint16_t arrows = now & (K_LEFT | K_RIGHT | K_UP | K_DOWN);
        if (arrows && arrows == g_repk) { if (++g_rep > 18 && g_rep % 4 == 0) pressed |= arrows; } else { g_repk = arrows; g_rep = 0; }
        if (g_state == MU_COUNT) {
            if (pressed & K_LEFT) act(H_LESS);
            if (pressed & K_RIGHT) act(H_MORE);
            if (pressed & K_L) act(H_LESS10);
            if (pressed & K_R) act(H_MORE10);
            if (pressed & K_UP) { g_n = g_max; g_dirty = 1; }
            if (pressed & K_DOWN) { g_n = 1; g_dirty = 1; }
        } else if (pressed & (K_LEFT | K_RIGHT | K_UP | K_DOWN)) act(H_MORE);
        if (pressed & K_B) act(H_CANCEL);
        else if (pressed & K_A) act(H_OK);
        *keys = 0;
        return 1;
    }
    if (g_tap > 0) { g_tap--; *keys = 0; if (now) g_tap = g_tap < 2 ? 2 : g_tap; return 1; }   /* (after: nothing until the keys are let go) */
    if (!field) g_pending = 0;                             /* (battles: their own windows) */
    return 0;
}
