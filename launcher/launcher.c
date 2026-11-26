/* The launcher (game/dqix-launcher): the game and its HD pack brought up to
 * date before playing, the news, Discord, the language and the options.
 * Spec: docs/superpowers/specs/2026-11-23-launcher-design.md; the work itself
 * (update, build, pack) is lnch_work.c, the drawing lnch_ui.c.
 *
 * Tests and pictures: DQIX_LAUNCHER_DEMO=download|build|ready|failed|options|options-classic|language|rom
 * shows a fixed state (no network, nothing run); DQIX_LAUNCHER_SHOT=<file.png>
 * saves the window and quits; DQIX_LAUNCHER_HEADLESS=1 does the work without
 * a window and prints how it ended (DQIX_LAUNCHER_ROM=<file> stands for the
 * ROM chosen on a first launch). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#define SDL_MAIN_HANDLED                 /* our own main(): no SDL2main to link */
#include <SDL.h>
#include "lnch.h"
#include "lnch_ui.h"
#include "lnch_work.h"
#include "lnch_keys.h"
#include "controls.h"
#include "uitext.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

#define DISCORD_URL "https://discord.gg/z9X9PH8AH4"
#define REPO_URL    "https://github.com/recompsmith/Dragon-Quest-IX-Modern"
static const char* const LANGUAGE_KEY[6] = {"", "english", "french", "german", "italian", "spanish"};
static const char* const NOTES_LANG[6] = {"", "", "fr", "de", "it", "es"};

Model g_m;                               /* shared with lnch_work.c under g_m.lock */
static SDL_Window* g_win;
static SDL_Renderer* g_ren;
static SDL_Texture* g_back; static int g_back_w, g_back_h;
extern const unsigned char LAUNCHER_BACK[]; extern const unsigned int LAUNCHER_BACK_SIZE;   /* back_jpg.c: built in */
static int g_lang = 1, g_classic, g_fullscreen, g_volume = 100;
static int g_options, g_lang_menu, g_drag_volume;
/* the key screen (in the options): 1 keyboard, 2 controller; the action whose
 * key (slot 0/1) or button (slot 2) is being asked, or -1 */
static int g_keys, g_keys_top, g_cap = -1, g_cap_slot;
static SDL_GameController* g_pad;
static int g_size;                       /* the window: 100, 125 or 150 % of 1100x674; 0 = fit the screen */
static float S = 1, OX, OY;              /* logical 1100x674 -> pixels */

/* ---- settings.ini */
static int g_settings_unreadable;
/* a red line on the INSTALL bar (what failed, when nothing else says it) */
static void say_failed(const char* text) {
    SDL_LockMutex(g_m.lock);
    snprintf(g_m.in.text, sizeof g_m.in.text, "%.199s", text);
    g_m.in.state = 2;
    SDL_UnlockMutex(g_m.lock);
}
static void settings_read(void) {
    char* t; char v[64];
    g_settings_unreadable = ini_load("settings.ini", &t) < 0;   /* shown on screen once the window is there */
    g_lang = 1;
    if (ini_get(t, "menu_language", v, sizeof v)) for (int i = 1; i <= 5; i++) if (!strcmp(v, LANGUAGE_KEY[i])) g_lang = i;
    if (g_lang == 1 && ini_get(t, "language", v, sizeof v)) for (int i = 1; i <= 5; i++) if (!strcmp(v, LANGUAGE_KEY[i])) g_lang = i;
    g_classic = ini_get(t, "mode", v, sizeof v) && strcmp(v, "enhanced") != 0;
    g_fullscreen = ini_get(t, "fullscreen", v, sizeof v) && atoi(v) != 0;
    g_volume = ini_get(t, "volume", v, sizeof v) ? atoi(v) : 100;
    if (g_volume < 0 || g_volume > 100) g_volume = 100;
    free(t);
}
/* 0 if settings.ini couldn't be written: the caller puts the old choice back, the screen says why */
static int settings_set(const char* key, const char* value) {
    if (settings_update("settings.ini", key, value)) return 1;
    say_failed(tr("err_settings"));
    return 0;
}
static int size_read(void) {
    char* t; char v[16];
    ini_load("launcher.ini", &t);
    int s = ini_get(t, "size", v, sizeof v) ? atoi(v) : 0;
    free(t);
    return s == 100 || s == 125 || s == 150 ? s : 0;
}
/* the size asked, or the biggest that leaves room around it on this screen */
static int size_fit(int asked) {
    SDL_Rect u;
    if (SDL_GetDisplayUsableBounds(g_win ? SDL_GetWindowDisplayIndex(g_win) : 0, &u) != 0) return asked ? asked : 100;
    static const int SZ[] = {150, 125, 100};
    for (int i = 0; i < 3; i++)
        if ((!asked || SZ[i] <= asked) && 1100 * SZ[i] <= u.w * 85 && 674 * SZ[i] <= u.h * 92) return SZ[i];
    return 100;
}
static void size_apply(void) {
    if (SDL_GetWindowFlags(g_win) & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) return;
    int s = size_fit(g_size);
    SDL_SetWindowSize(g_win, 1100 * s / 100, 674 * s / 100);
    int d = SDL_GetWindowDisplayIndex(g_win);              /* the screen it is on, not always the first */
    SDL_SetWindowPosition(g_win, SDL_WINDOWPOS_CENTERED_DISPLAY(d < 0 ? 0 : d), SDL_WINDOWPOS_CENTERED_DISPLAY(d < 0 ? 0 : d));
}
static void set_language(int l) {
    g_lang = l;
    tr_set_language(l);
    if (g_keys) ui_lang_load(l);                         /* the actions' names, from the game's lang/ */
    settings_set("language", LANGUAGE_KEY[l]);
    settings_set("menu_language", LANGUAGE_KEY[l]);
    work_news_language(NOTES_LANG[l]);
}

/* ---- hit areas, made again every frame */
enum { A_NONE, A_LANG, A_LANG1, A_LANG5 = A_LANG1 + 4, A_DISCORD, A_GITHUB, A_FOLDER, A_NEWS_ALL, A_VERIFY, A_OPTIONS,
       A_PLAY, A_RETRY, A_CLASSIC, A_MODERN, A_FULLSCREEN, A_VOLUME, A_KEYS_PAD, A_KEYS_KB, A_RESET, A_CLOSE_OPT, A_CHOOSE_ROM, A_SIZE,
       A_KEYS_BACK, A_KEYS_DEFAULTS, A_KEYCELL = 1000 /* + action * 3 + slot */ };
typedef struct { float x, y, w, h; int id; } Hit;
static Hit g_hits[64]; static int g_nhits;
static int g_mx = -1, g_my = -1;
static void hit(float x, float y, float w, float h, int id) {
    if (g_nhits < 64) { Hit hh = {OX + x * S, OY + y * S, w * S, h * S, id}; g_hits[g_nhits++] = hh; }
}
static int hit_at(int x, int y) {
    for (int i = g_nhits - 1; i >= 0; i--)
        if (x >= g_hits[i].x && x < g_hits[i].x + g_hits[i].w && y >= g_hits[i].y && y < g_hits[i].y + g_hits[i].h) return g_hits[i].id;
    return A_NONE;
}
static int hover(float x, float y, float w, float h) {
    float px = (g_mx - OX) / S, py = (g_my - OY) / S;
    return px >= x && px < x + w && py >= y && py < y + h;
}

/* drawing in logical units */
#define X(v) (OX + (v) * S)
#define Y(v) (OY + (v) * S)
static float text(int f, float px, float x, float y, Col c, const char* s, float maxw, int al) {
    return ui_text(f, px * S, X(x), Y(y), c, s, maxw > 0 ? maxw * S : 0, al) / S;
}
static void rrect(float x, float y, float w, float h, float r, Col a, Col b) { ui_round_rect(X(x), Y(y), w * S, h * S, r * S, a, b); }
static void rborder(float x, float y, float w, float h, float r, float t, Col c) { ui_round_border(X(x), Y(y), w * S, h * S, r * S, t * S, c); }
static void frame(float x, float y, float w, float h) {         /* the game's window look: blue, white edge */
    rrect(x - 2, y - 2, w + 4, h + 4, 14, col(0x0B1430, 230), col(0x0B1430, 230));
    rrect(x, y, w, h, 12, col(0x14275A, 215), col(0x0B1638, 238));
    rborder(x, y, w, h, 12, 2, col(0xC9D6FF, 255));
}
static void button(float x, float y, float w, float h, const char* label, int id, Col bg, Col edge, Col fg) {
    int hv = hover(x, y, w, h);
    rrect(x, y, w, h, 10, hv ? col(0x1A2B5E, 255) : bg, hv ? col(0x1A2B5E, 255) : bg);
    rborder(x, y, w, h, 10, 1, hv ? col(0xE8C46A, 255) : edge);
    text(FONT_BOLD, 13, x + w / 2, y + h / 2 - 9, fg, label, w - 12, 1);
    hit(x, y, w, h, id);
}

static const Col GOLD = {0xE8, 0xC4, 0x6A, 255}, INK = {0xEE, 0xF2, 0xFF, 255}, MUTE = {0x9F, 0xB0, 0xD8, 255},
                 OK = {0x9F, 0xF0, 0xB5, 255}, BAD = {0xFF, 0x9A, 0x8F, 255}, SKY = {0x5F, 0xB4, 0xFF, 255};

static void draw_news(const Model* m) {
    float x = 22, y = 76, w = 418;
    float h = 46 + (m->nnews ? m->nnews : 1) * 72;
    frame(x, y, w, h);
    text(FONT_TITLE, 15, x + 18, y + 14, GOLD, tr("news"), 0, 0);
    int hv = hover(x + w - 150, y + 8, 140, 30);
    text(FONT_BOLD, 12, x + w - 18, y + 17, hv ? INK : SKY, tr("nav_all"), 0, 2);
    hit(x + w - 150, y + 8, 140, 30, A_NEWS_ALL);
    ui_fill(X(x), Y(y + 45), w * S, S, col(0xFFFFFF, 34));
    const char* months = tr("month");
    for (int i = 0; i < m->nnews; i++) {
        const NewsItem* n = &m->news[i];
        float iy = y + 46 + i * 72;
        char day[3] = "", mon[8] = "";
        if (n->date[0]) {
            int mm = atoi(n->date + 5);
            memcpy(day, n->date + 8, 2); day[2] = 0;
            const char* p = months;
            for (int k = 1; k < mm && p; k++) { p = strchr(p, ' '); if (p) p++; }
            if (p) { int l = 0; while (p[l] && p[l] != ' ' && l < 7) { mon[l] = p[l]; l++; } mon[l] = 0; }
        }
        if (day[0]) {
            text(FONT_BOLD, 21, x + 39, iy + 12, INK, day, 0, 1);
            text(FONT_BOLD, 12, x + 39, iy + 40, MUTE, mon, 0, 1);
        }
        float tx = x + 76;
        float tw = text(FONT_BOLD, 10, 0, -100, GOLD, n->tag, 0, 0) + 12;      /* measured off screen */
        rrect(tx, iy + 15, tw, 17, 4, GOLD, GOLD);
        text(FONT_BOLD, 10, tx + tw / 2, iy + 16, col(0x0B1430, 255), n->tag, 0, 1);
        text(FONT_BOLD, 14, tx + tw + 8, iy + 13, INK, n->title, w - (tx - x) - tw - 26, 0);
        if (i + 1 < m->nnews) ui_fill(X(x), Y(iy + 71), w * S, S, col(0xFFFFFF, 20));
    }
    if (m->news_offline) text(FONT_TEXT, 12, x + 18, y + h + 6, MUTE, tr("offline_news"), 0, 0);
}

static void draw_bar(float x, float y, float w, const char* label, const Bar* b, int blue) {
    float lw = 150;
    Col tc = b->state == 1 ? OK : b->state == 2 ? BAD : INK;
    float rw = b->right[0] ? text(FONT_TEXT, 12.5f, 0, -100, MUTE, b->right, 0, 0) + 12 : 0;
    text(FONT_BOLD, 13.5f, x + lw, y, tc, b->text, w - lw - rw - (b->state == 2 ? 110 : 0), 0);
    if (rw) text(FONT_TEXT, 12.5f, x + w, y + 1, MUTE, b->right, 0, 2);
    float by = y + 24;
    text(FONT_BOLD, 12, x, by - 6, GOLD, label, lw - 8, 0);
    rrect(x + lw, by, w - lw, 9, 5, col(0x1A2547, 255), col(0x1A2547, 255));
    float f = b->frac < 0 ? 0 : b->frac > 1 ? 1 : b->frac;
    if (f > 0) {
        if (b->state == 2) rrect(x + lw, by, (w - lw) * f, 9, 5, col(0xC0453F, 255), col(0xC0453F, 255));
        else if (blue) { ui_hgrad(X(x + lw + 2), Y(by), ((w - lw) * f - 4) * S, 9 * S, col(0x2B7FD0, 255), col(0xCFE9FF, 255)); }
        else ui_hgrad(X(x + lw + 2), Y(by), ((w - lw) * f - 4) * S, 9 * S, col(0xB8892F, 255), col(0xFFF3C4, 255));
    }
    rborder(x + lw, by, w - lw, 9, 5, 1, col(0x2C3B6A, 255));
    if (b->state == 2) {                                 /* RETRY, right of the failed bar's text */
        float bw = 100;
        button(x + w - bw - rw, y - 4, bw, 22, tr("retry"), A_RETRY, col(0xFF9A8F, 255), col(0xFF9A8F, 255), col(0x0B1430, 255));
    }
}

static void draw_options(void) {
    float x = 22, y = 72, w = 1056, h = 382;
    frame(x, y, w, h);
    text(FONT_TITLE, 15, x + 18, y + 14, GOLD, tr("options_title"), 0, 0);
    text(FONT_TEXT, 13, x + w - 52, y + 16, MUTE, tr("options_note"), 0, 2);
    int hv = hover(x + w - 42, y + 8, 32, 30);
    text(FONT_BOLD, 18, x + w - 26, y + 9, hv ? GOLD : INK, "\xC3\x97", 0, 1);   /* × closes */
    hit(x + w - 42, y + 8, 32, 30, A_CLOSE_OPT);
    ui_fill(X(x), Y(y + 45), w * S, S, col(0xFFFFFF, 34));
    /* the two modes */
    for (int k = 0; k < 2; k++) {
        float cx = x + 18 + k * (w - 36) / 2 + (k ? 6 : 0), cw = (w - 36) / 2 - 6, cy = y + 60, ch = 96;
        int on = k == 0 ? g_classic : !g_classic;
        int hvv = hover(cx, cy, cw, ch);
        rrect(cx, cy, cw, ch, 10, on ? col(0x1A2A5E, 255) : col(0x0B1638, 255), on ? col(0x1A2A5E, 255) : col(0x0B1638, 255));
        rborder(cx, cy, cw, ch, 10, 2, on ? GOLD : hvv ? col(0x6A80C0, 255) : col(0x2F4277, 255));
        /* the screens' picture: two DS screens, or one wide */
        if (k == 0) { rborder(cx + 20, cy + 22, 44, 24, 2, 1, col(0x9FD0FF, 255)); rborder(cx + 20, cy + 50, 44, 24, 2, 1, col(0x9FD0FF, 255)); }
        else rborder(cx + 14, cy + 26, 56, 44, 2, 1, col(0x9FD0FF, 255));
        text(FONT_TITLE, 20, cx + 88, cy + 22, GOLD, tr(k == 0 ? "mode_classic" : "mode_modern"), 0, 0);
        text(FONT_TEXT, 13, cx + 88, cy + 54, MUTE, tr(k == 0 ? "mode_classic_help" : "mode_modern_help"), cw - 100, 0);
        hit(cx, cy, cw, ch, k == 0 ? A_CLASSIC : A_MODERN);
    }
    /* below: window, volume, keys */
    float ry = y + 176, colw = (w - 36 - 30) / 2;
    for (int i = 0; i < 5; i++) {
        float rx = x + 18 + (i % 2) * (colw + 30), yy = ry + (i / 2) * 46;
        ui_fill(X(rx), Y(yy + 40), colw * S, S, col(0xFFFFFF, 16));
        if (i == 0) {
            text(FONT_TEXT, 14, rx, yy + 10, INK, tr("window"), 0, 0);
            char v[64]; snprintf(v, sizeof v, "\xE2\x80\xB9  %s  \xE2\x80\xBA", tr(g_fullscreen ? "fullscreen" : "windowed"));
            text(FONT_BOLD, 14, rx + colw, yy + 10, GOLD, v, 0, 2);
            hit(rx + colw - 220, yy, 220, 40, A_FULLSCREEN);
        } else if (i == 1) {
            text(FONT_TEXT, 14, rx, yy + 10, INK, tr("volume"), 0, 0);
            float sx = rx + colw - 220, sw = 170;
            rrect(sx, yy + 17, sw, 6, 3, col(0x38446B, 255), col(0x38446B, 255));
            rrect(sx, yy + 17, sw * g_volume / 100, 6, 3, GOLD, GOLD);
            rrect(sx + sw * g_volume / 100 - 7, yy + 13, 14, 14, 7, INK, INK);
            char v[8]; snprintf(v, sizeof v, "%d", g_volume);
            text(FONT_BOLD, 13, rx + colw, yy + 11, GOLD, v, 0, 2);
            hit(sx - 8, yy + 5, sw + 16, 30, A_VOLUME);
        } else if (i == 4) {
            text(FONT_TEXT, 14, rx, yy + 10, INK, tr("launcher_size"), 0, 0);
            char v[32]; snprintf(v, sizeof v, "\xE2\x80\xB9  %d %%  \xE2\x80\xBA", size_fit(g_size));
            text(FONT_BOLD, 14, rx + colw, yy + 10, GOLD, v, 0, 2);
            hit(rx + colw - 220, yy, 220, 40, A_SIZE);
        } else {
            text(FONT_TEXT, 14, rx, yy + 10, INK, tr(i == 2 ? "keys_pad" : "keys_keyboard"), 0, 0);
            int h2 = hover(rx + colw - 220, yy, 220, 40);
            text(FONT_BOLD, 14, rx + colw, yy + 10, h2 ? INK : GOLD, tr("configure"), 0, 2);
            hit(rx + colw - 220, yy, 220, 40, i == 2 ? A_KEYS_PAD : A_KEYS_KB);
        }
    }
    ui_fill(X(x), Y(y + h - 46), w * S, S, col(0xFFFFFF, 24));
    text(FONT_TEXT, 13, x + 18, y + h - 31, MUTE, tr("mode_note"), w - 260, 0);
    button(x + w - 218, y + h - 38, 200, 30, tr("reset_defaults"), A_RESET, col(0x0E1A3D, 255), col(0x3A4F86, 255), INK);
}

#define KEY_ROWS 9
static void draw_keys(void) {
    float x = 22, y = 72, w = 1056, h = 382;
    frame(x, y, w, h);
    text(FONT_TITLE, 15, x + 18, y + 14, GOLD, tr(g_keys == 1 ? "keys_keyboard" : "keys_pad"), 0, 0);
    if (g_keys == 2) text(FONT_TEXT, 13, x + w - 52, y + 16, g_pad ? MUTE : BAD, g_pad ? g_ctl_pad_label : tr("keys_no_pad"), 420, 2);
    int hv = hover(x + w - 42, y + 8, 32, 30);
    text(FONT_BOLD, 18, x + w - 26, y + 9, hv ? GOLD : INK, "\xC3\x97", 0, 1);   /* × back to the options */
    hit(x + w - 42, y + 8, 32, 30, A_KEYS_BACK);
    ui_fill(X(x), Y(y + 45), w * S, S, col(0xFFFFFF, 34));
    /* the columns: the action, then its two keys or its button */
    float c1 = x + 470, cw = g_keys == 1 ? 260 : 540, gap = 20;
    text(FONT_BOLD, 12, x + 18, y + 54, MUTE, tr("keys_action"), 0, 0);
    if (g_keys == 1) { text(FONT_BOLD, 12, c1, y + 54, MUTE, tr("keys_key"), 0, 0); text(FONT_BOLD, 12, c1 + cw + gap, y + 54, MUTE, tr("keys_key2"), 0, 0); }
    else text(FONT_BOLD, 12, c1, y + 54, MUTE, tr("keys_button"), 0, 0);
    float ry = y + 76, rh = 30;
    for (int r = 0; r < KEY_ROWS && g_keys_top + r < CA_N; r++) {
        int a = g_keys_top + r;
        float yy = ry + r * rh;
        if (r) ui_fill(X(x + 14), Y(yy), (w - 28) * S, S, col(0xFFFFFF, 14));
        text(FONT_TEXT, 14, x + 18, yy + 6, INK, ui_tr(g_ctl[a].name), c1 - x - 30, 0);
        for (int s = 0; s < (g_keys == 1 ? 2 : 1); s++) {
            int slot = g_keys == 1 ? s : 2;
            float cx = c1 + s * (cw + gap);
            int asking = g_cap == a && g_cap_slot == slot, hv2 = hover(cx, yy + 3, cw, rh - 6);
            rrect(cx, yy + 3, cw, rh - 6, 6, asking ? col(0x3A2F10, 255) : hv2 ? col(0x1F3470, 255) : col(0x0E1A3D, 255),
                  asking ? col(0x3A2F10, 255) : hv2 ? col(0x1F3470, 255) : col(0x0E1A3D, 255));
            rborder(cx, yy + 3, cw, rh - 6, 6, 1, asking ? GOLD : col(0x2F4277, 255));
            char n[64];
            if (asking) snprintf(n, sizeof n, "%s", tr(g_keys == 1 ? "keys_press_key" : g_pad ? "keys_press_button" : "keys_no_pad"));
            else if (slot < 2 ? !g_ctl_key[a][slot] : g_ctl_pad[a] == CTL_NONE) snprintf(n, sizeof n, "\xE2\x80\x94");   /* none: a dash, not the "-" key */
            else if (slot < 2) controls_key_label(g_ctl_key[a][slot], n, sizeof n);
            else controls_pad_name(g_ctl_pad[a], n, sizeof n);
            int none = !asking && !strcmp(n, "\xE2\x80\x94");
            text(asking ? FONT_BOLD : FONT_TEXT, 13.5f, cx + cw / 2, yy + 7, asking ? GOLD : none ? MUTE : INK, ui_tr(n), cw - 12, 1);
            hit(cx, yy + 3, cw, rh - 6, A_KEYCELL + a * 3 + slot);
        }
    }
    /* the list goes on: a thin bar on the right shows where */
    float th = KEY_ROWS * rh, bh = th * KEY_ROWS / CA_N, by = ry + th * g_keys_top / CA_N;
    rrect(x + w - 12, ry, 4, th, 2, col(0x1A2547, 255), col(0x1A2547, 255));
    rrect(x + w - 12, by, 4, bh, 2, col(0x6A80C0, 255), col(0x6A80C0, 255));
    ui_fill(X(x), Y(y + h - 46), w * S, S, col(0xFFFFFF, 24));
    text(FONT_TEXT, 13, x + 18, y + h - 31, MUTE, tr(g_cap >= 0 ? "keys_esc_cancels" : "keys_help"), w - 260, 0);
    button(x + w - 218, y + h - 38, 200, 30, tr("reset_defaults"), A_KEYS_DEFAULTS, col(0x0E1A3D, 255), col(0x3A4F86, 255), INK);
}
static void keys_scroll(int rows) {
    g_keys_top += rows;
    if (g_keys_top > CA_N - KEY_ROWS) g_keys_top = CA_N - KEY_ROWS;
    if (g_keys_top < 0) g_keys_top = 0;
}
static void keys_open(int which) {
    g_keys = which; g_keys_top = 0; g_cap = -1;
    controls_load();                                     /* controls.ini of this game folder, or the game's defaults */
    ui_lang_load(g_lang);
    if (!g_pad) for (int i = 0; i < SDL_NumJoysticks() && !g_pad; i++) if (SDL_IsGameController(i)) g_pad = SDL_GameControllerOpen(i);
    controls_pad_detect(g_pad);
}
static void keys_done(int r) {                           /* after a key or button was pressed for g_cap */
    g_cap = -1;
    if (r == KEYS_SET && !keys_save()) { say_failed(tr("err_keys")); controls_load(); }   /* not written: back to the file's */
}

static void draw(int W, int H) {
    S = (float)W / 1100 < (float)H / 674 ? (float)W / 1100 : (float)H / 674;
    OX = (W - 1100 * S) / 2; OY = (H - 674 * S) / 2;
    g_nhits = 0;
    SDL_SetRenderDrawColor(g_ren, 5, 9, 22, 255);
    SDL_RenderClear(g_ren);
    SDL_LockMutex(g_m.lock);
    Model m = g_m;                                       /* a copy: the work thread goes on meanwhile */
    m.lock = NULL;
    SDL_UnlockMutex(g_m.lock);
    if (!g_back) g_back = ui_load_picture_mem(LAUNCHER_BACK, (int)LAUNCHER_BACK_SIZE, &g_back_w, &g_back_h);
    /* the background: the HD picture, or the night sky; never a tint over the picture */
    if (g_back) {
        ui_picture_cover(g_back, g_back_w, g_back_h, 0, 0, (float)W, (float)H);
        ui_hgrad(0, 0, W * 0.7f, (float)H, col(0x0B1430, 204), col(0x0B1430, 0));
    } else ui_stars(0, 0, (float)W, (float)H);
    ui_vgrad(0, 0, (float)W, 60 * S + OY, col(0x000000, 130), col(0x000000, 0));
    /* top */
    text(FONT_TITLE, 24, 22, 12, GOLD, "DRAGON QUEST IX", 0, 0);
    text(FONT_BOLD, 10, 24, 42, col(0xCFE3FF, 255), "M  O  D  E  R  N", 0, 0);
    float rx = 1078;
    rx -= 40; button(rx, 14, 40, 34, "", A_FOLDER, col(0x0B1430, 170), col(0x2F4277, 255), INK);
    rrect(rx + 11, 23, 8, 5, 1.5f, GOLD, GOLD);                 /* a folder: its tab, then its body */
    rrect(rx + 11, 25.5f, 18, 13, 2, GOLD, col(0xB8892F, 255));
    rx -= 90; button(rx, 14, 82, 34, "GitHub", A_GITHUB, col(0x0B1430, 170), col(0x2F4277, 255), INK);
    rx -= 102; button(rx, 14, 94, 34, "Discord", A_DISCORD, col(0x3B4BB8, 255), col(0x7E8CFF, 255), INK);
    rx -= 126; button(rx, 14, 118, 34, "", A_LANG, col(0x0B1430, 170), col(0x2F4277, 255), INK);
    text(FONT_BOLD, 13, rx + 46, 14 + 8, INK, tr("lang_name"), 80, 1);      /* the name left of the ▾ */
    ui_triangle(X(rx + 84), Y(28), X(rx + 94), Y(28), X(rx + 89), Y(34), INK);   /* ▾: not in the fonts */
    /* middle */
    if (g_options && g_keys) draw_keys(); else if (g_options) draw_options(); else draw_news(&m);
    /* bottom */
    float by = 492;
    ui_vgrad(X(0) - OX, Y(by), (float)W, (674 - by) * S + OY + 1, col(0x070D22, 242), col(0x050916, 255));
    ui_fill(0, Y(by), (float)W, S, col(0xE8C46A, 85));
    button(22, by + 40, 165, 42, tr("verify"), A_VERIFY, col(0x0E1A3D, 255), col(0x3A4F86, 255), col(0xDFE7FF, 255));
    button(22, by + 96, 165, 42, tr("options"), A_OPTIONS, col(0x0E1A3D, 255), g_options ? GOLD : col(0x3A4F86, 255), col(0xDFE7FF, 255));
    float mx = 210, mw = 1100 - 22 - 220 - 24 - mx;
    draw_bar(mx, by + 46, mw, tr("download"), &m.dl, 0);
    draw_bar(mx, by + 104, mw, tr("install"), &m.in, 1);
    int rom = m.st.rom_needed && !m.busy && m.st.build != ST_RUNNING;   /* first launch: the button asks for the ROM */
    int can = rom || (lstate_can_play(&m.st) && !m.busy);
    const char* label = tr(rom ? "choose_rom" : "play");
    float lpx = rom ? 22 : 30;                           /* "CHOISIR LA ROM" is longer than JOUER */
    float px = 1100 - 22 - 220, py = by + 54;
    if (can) {
        int hv = hover(px, py, 220, 74);
        rrect(px - 3, py - 3, 226, 80, 16, col(0xE8C46A, hv ? 110 : 70), col(0xE8C46A, hv ? 110 : 70));
        rrect(px, py, 220, 74, 14, col(0xFFE9A0, 255), col(0xB8892F, 255));
        rborder(px, py, 220, 74, 14, 2, col(0xFFF3C4, 255));
        text(FONT_TITLE, lpx, px + 110, py + 37 - lpx * 0.63f, col(0x1B1204, 255), label, 200, 1);
        hit(px, py, 220, 74, rom ? A_CHOOSE_ROM : A_PLAY);
    } else {
        rrect(px, py, 220, 74, 14, col(0x141C36, 255), col(0x141C36, 255));
        rborder(px, py, 220, 74, 14, 2, col(0x2A3558, 255));
        text(FONT_TITLE, lpx, px + 110, py + 37 - lpx * 0.63f, col(0x6B7699, 255), label, 200, 1);
    }
    char ver[96]; snprintf(ver, sizeof ver, "%s%s", m.st.installed[0] ? m.st.installed : "", m.pack_version[0] ? "  \xC2\xB7  " : "");
    if (m.pack_version[0]) strncat(ver, m.pack_version, sizeof ver - strlen(ver) - 1);
    text(FONT_TEXT, 12, 1090, 656, col(0x5C6A92, 255), ver, 0, 2);
    /* the language list, over everything */
    if (g_lang_menu) {
        float lx = 1078 - 40 - 90 - 102 - 126, ly = 52;
        rrect(lx, ly, 118, 5 * 32 + 8, 10, col(0x0B1638, 250), col(0x0B1638, 250));
        rborder(lx, ly, 118, 5 * 32 + 8, 10, 1, col(0x3A4F86, 255));
        for (int i = 1; i <= 5; i++) {
            const char* name = tr_in("lang_name", i);      /* each language's own name */
            float iy = ly + 4 + (i - 1) * 32;
            if (hover(lx, iy, 118, 32) || i == g_lang) rrect(lx + 4, iy, 110, 32, 6, col(0x1F3470, 255), col(0x1F3470, 255));
            text(FONT_BOLD, 13, lx + 14, iy + 7, i == g_lang ? GOLD : INK, name, 0, 0);
            hit(lx, iy, 118, 32, A_LANG1 + i - 1);
        }
    }
}

static int pick_rom(char* out, size_t n);

/* the window's icon: the game's own, the one the DS menu shows, read from the
 * ROM copy of the last build (as the game does, runtime/frontend_sdl.c window_icon_for):
 * nothing from the game is kept in the launcher. Before the first build: none */
static void window_icon(void) {
    FILE* f = fopen("data/dqix.nds", "rb");
    if (!f) return;
    unsigned char head[0x6C], b[0x240];
    unsigned off = 0;
    int ok = fread(head, 1, sizeof head, f) == sizeof head;
    if (ok) off = head[0x68] | head[0x69] << 8 | head[0x6A] << 16 | (unsigned)head[0x6B] << 24;
    ok = ok && off && fseek(f, (long)off, SEEK_SET) == 0 && fread(b, 1, sizeof b, f) == sizeof b;
    fclose(f);
    if (!ok) return;
    Uint32 pal[16];
    for (int i = 0; i < 16; i++) {                       /* BGR555; colour 0 is see-through */
        unsigned c = b[0x220 + i * 2] | b[0x221 + i * 2] << 8, r = c & 31, g = (c >> 5) & 31, bl = (c >> 10) & 31;
        pal[i] = (i ? 0xFF000000u : 0) | ((r << 3 | r >> 2) << 16) | ((g << 3 | g >> 2) << 8) | (bl << 3 | bl >> 2);
    }
    enum { N = 64 };                                     /* 32x32 doubled, sharp pixels */
    static Uint32 px[N * N];
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            int sx = x / 2, sy = y / 2, t = (sy / 8) * 4 + sx / 8;
            unsigned char v = b[0x20 + t * 32 + (sy & 7) * 4 + (sx & 7) / 2];
            px[y * N + x] = pal[(sx & 1) ? v >> 4 : v & 15];
        }
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom(px, N, N, 32, N * 4, SDL_PIXELFORMAT_ARGB8888);
    if (s) { SDL_SetWindowIcon(g_win, s); SDL_FreeSurface(s); }
}

/* ---- actions */
static void open_url(const char* u) { if (SDL_OpenURL(u)) fprintf(stderr, "launcher: can't open %s\n", u); }
static void open_here(const char* rel) {
    char* base = SDL_GetBasePath();
    char u[2048];
    snprintf(u, sizeof u, "file:///%s%s", base ? base : "", rel);
    for (char* c = u; *c; c++) if (*c == '\\') *c = '/';
    SDL_free(base);
    open_url(u);
}

/* the game, then the launcher closes; DQIX_LAUNCHER tells dqix not to come back here.
 * The game folder is the current one (to_own_folder): the release zip's launcher
 * sits one folder up, in the project, and works in game/ */
static int start_game(void) {
#ifdef _WIN32
    SetEnvironmentVariableA("DQIX_LAUNCHER", "1");
    wchar_t exe[MAX_PATH + 16];
    DWORD n = GetCurrentDirectoryW(MAX_PATH, exe);
    if (!n || n >= MAX_PATH) return 0;
    wchar_t* s = exe + n;
    if (s[-1] == L'\\') s--;
    *s = L'\\';
    wcscpy(s + 1, L"dqix.exe");
    STARTUPINFOW si; memset(&si, 0, sizeof si); si.cb = sizeof si;
    PROCESS_INFORMATION pi;
    wchar_t cmd[MAX_PATH + 32]; swprintf(cmd, MAX_PATH + 32, L"\"%ls\"", exe);
    *s = 0;
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, exe, &si, &pi)) return 0;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return 1;
#else
    setenv("DQIX_LAUNCHER", "1", 1);
    pid_t pid = fork();
    if (pid < 0) return 0;
    if (pid == 0) { setsid(); execl("./dqix", "./dqix", (char*)NULL); _exit(127); }
    return 1;
#endif
}

static void action(int a) {
    if (a != A_LANG && g_lang_menu) { g_lang_menu = 0; if (a < A_LANG1 || a > A_LANG5) return; }
    switch (a) {
    case A_LANG: g_lang_menu = !g_lang_menu; break;
    case A_DISCORD: open_url(DISCORD_URL); break;
    case A_GITHUB: open_url(REPO_URL); break;
    case A_NEWS_ALL: open_url(REPO_URL "/releases"); break;
    case A_FOLDER: open_here(""); break;
    case A_OPTIONS: g_options = !g_options; g_keys = 0; break;
    case A_CLOSE_OPT: g_options = 0; break;
    case A_KEYS_BACK: g_keys = 0; break;
    case A_KEYS_DEFAULTS: controls_defaults(); keys_done(KEYS_SET); break;
    case A_CHOOSE_ROM: {
        char path[1024];
        int got = pick_rom(path, sizeof path);
        if (got < 0) { say_failed(tr("err_picker")); break; }   /* no file window there */
        if (!got) break;                                  /* cancelled */
        if (rom_kind_ok(path)) work_set_rom(path);
        else { SDL_LockMutex(g_m.lock); snprintf(g_m.in.text, sizeof g_m.in.text, "%s", tr("rom_bad")); g_m.in.state = 2; SDL_UnlockMutex(g_m.lock); }
        break;
    }
    case A_VERIFY: case A_RETRY: work_start(); break;
    case A_PLAY:
        if (start_game()) { SDL_Event q; q.type = SDL_QUIT; SDL_PushEvent(&q); }
        else {                                            /* antivirus, a path too long...: said, the launcher stays */
            char why[32], t[200];
#ifdef _WIN32
            snprintf(why, sizeof why, "error %lu", GetLastError());
#else
            snprintf(why, sizeof why, "error %d", errno);
#endif
            snprintf(t, sizeof t, tr("err_play"), why);
            say_failed(t);
        }
        break;
    case A_CLASSIC: if (settings_set("mode", "classic")) g_classic = 1; work_settings_changed(); break;
    case A_MODERN: if (settings_set("mode", "enhanced")) g_classic = 0; work_settings_changed(); break;
    case A_FULLSCREEN: if (settings_set("fullscreen", g_fullscreen ? "0" : "1")) g_fullscreen = !g_fullscreen; break;
    case A_KEYS_PAD: keys_open(2); break;
    case A_KEYS_KB: keys_open(1); break;
    case A_RESET:
        if (settings_set("mode", "enhanced") && settings_set("fullscreen", "0") && settings_set("volume", "100")) {
            g_classic = 0; g_fullscreen = 0; g_volume = 100;
        }
        settings_read();                                  /* what the file says now, whatever got written */
        work_settings_changed();
        break;
    case A_VOLUME: g_drag_volume = 1; break;
    case A_SIZE: {
        int s = size_fit(g_size), max = size_fit(0);
        s = s >= max ? 100 : s == 100 ? 125 : 150;
        char v[8]; snprintf(v, sizeof v, "%d", s);
        if (settings_update("launcher.ini", "size", v)) { g_size = s; size_apply(); }
        else say_failed(tr("err_settings"));
        break;
    }
    default:
        if (a >= A_LANG1 && a <= A_LANG5) set_language(a - A_LANG1 + 1);
        else if (a >= A_KEYCELL && a < A_KEYCELL + CA_N * 3) { g_cap = (a - A_KEYCELL) / 3; g_cap_slot = (a - A_KEYCELL) % 3; }
    }
}
static void volume_from_mouse(int mx) {
    float x = 22 + 18 + (1056 - 36 - 30) / 2 + 30, colw = (1056 - 36 - 30) / 2;
    float sx = x + colw - 220, sw = 170;
    int v = (int)(((mx - OX) / S - sx) / sw * 100 + 0.5f);
    g_volume = v < 0 ? 0 : v > 100 ? 100 : v;
}

static void demo(const char* d) {
    Model* m = &g_m;
    m->st.platform_updates = 1; m->st.online = 1; m->st.update_check = 1; m->st.hd_pack_wanted = 1;
    m->st.game_built = 1; m->st.pack_complete = 1; strcpy(m->st.installed, "v3.2"); strcpy(m->st.available, "v3.2");
    m->st.update = m->st.build = m->st.pack = ST_DONE;
    strcpy(m->pack_version, "hd-pack-2");
    static const NewsItem N[3] = {{"v3.2", "2026-11-22", "Installing and updating work again"},
                                  {"v3.1", "2026-11-21", "Install fixes: the build tools checked first"},
                                  {"v3", "2026-11-20", "The HD pack: textures and movies downloaded"}};
    memcpy(m->news, N, sizeof N); m->nnews = 3;
    NewsItem loc[3];                                     /* the notes of the build here, in the language, when there */
    int nl = news_local("notes", NOTES_LANG[g_lang], loc, 3);
    for (int i = 0; i < nl && i < 3; i++) for (int k = 0; k < 3; k++) if (!strcmp(loc[i].tag, m->news[k].tag)) strcpy(m->news[k].title, loc[i].title);
    int fr = g_lang == 2;                                /* the units as lnch_work.c writes them */
    snprintf(m->dl.text, sizeof m->dl.text, "%s", tr("dl_done")); m->dl.frac = 1; m->dl.state = 1;
    snprintf(m->in.text, sizeof m->in.text, "%s", tr("st_ready")); m->in.frac = 1; m->in.state = 1;
    snprintf(m->in.right, sizeof m->in.right, "v3.2 \xC2\xB7 hd-pack-2");
    if (!strcmp(d, "download") || !strncmp(d, "options", 7)) {
        m->st.pack_complete = 0; m->st.pack = ST_RUNNING;
        snprintf(m->dl.text, sizeof m->dl.text, tr("dl_pack"), 2, 3); m->dl.frac = 0.44f; m->dl.state = 0;
        snprintf(m->dl.right, sizeof m->dl.right, fr ? "610 Mo / 1,4 Go \xC2\xB7 8 Mo/s \xC2\xB7 1 min 40" : "610 MB / 1.4 GB \xC2\xB7 8 MB/s \xC2\xB7 1 min 40");
        snprintf(m->in.text, sizeof m->in.text, "%s", tr("dl_waiting")); m->in.frac = 0; m->in.state = 0; m->in.right[0] = 0;
    } else if (!strcmp(d, "build")) {
        strcpy(m->st.available, "v3.3"); m->st.update = ST_DONE; m->st.build = ST_RUNNING;
        snprintf(m->dl.text, sizeof m->dl.text, tr("dl_update"), "v3.3"); snprintf(m->dl.right, sizeof m->dl.right, fr ? "12 Mo" : "12 MB");
        snprintf(m->in.text, sizeof m->in.text, tr("in_build"), tr("Compiling (the first time takes a while; later builds only redo what changed)")); m->in.frac = 0.38f; m->in.state = 0;
        snprintf(m->in.right, sizeof m->in.right, "38%%");
    } else if (!strcmp(d, "failed")) {
        m->st.pack_complete = 0; m->st.pack = ST_FAILED;
        snprintf(m->dl.text, sizeof m->dl.text, tr("err_download"), tr("err_github")); m->dl.frac = 0.44f; m->dl.state = 2; m->dl.right[0] = 0;
        snprintf(m->in.text, sizeof m->in.text, "%s", tr("dl_waiting")); m->in.frac = 0; m->in.state = 0; m->in.right[0] = 0;
    }
    if (!strncmp(d, "options", 7)) { g_options = 1; if (!strcmp(d, "options-classic")) g_classic = 1; }
    if (!strncmp(d, "keys", 4)) {                       /* keys, keys-pad, keys-wait */
        g_options = 1; keys_open(!strcmp(d, "keys-pad") ? 2 : 1);
        if (!strcmp(d, "keys-wait")) { g_cap = CA_JOURNAL; g_cap_slot = 0; g_keys_top = CA_N - KEY_ROWS; }
    }
    if (!strcmp(d, "language")) g_lang_menu = 1;
    if (!strcmp(d, "rom")) {                             /* first launch */
        m->st.game_built = 0; m->st.rom_needed = 1; m->st.build = ST_IDLE; m->st.pack = ST_IDLE; m->st.pack_complete = 0;
        snprintf(m->dl.text, sizeof m->dl.text, "%s", tr("dl_waiting")); m->dl.frac = 0; m->dl.state = 0; m->dl.right[0] = 0;
        snprintf(m->in.text, sizeof m->in.text, "%s", tr("st_rom")); m->in.frac = 0; m->in.state = 0; m->in.right[0] = 0;
    }
}

static const char* step_name(StepState s) {
    static const char* const N[] = {"idle", "running", "done", "failed", "skipped"};
    return s >= ST_IDLE && s <= ST_SKIPPED ? N[s] : "?";
}
/* tests (DQIX_LAUNCHER_HEADLESS=1): the work without a window, its outcome on
 * stdout; exit 0 if PLAY would light up, 1 if not, 2 if it couldn't start there
 * or the game folder's launcher took over. DQIX_LAUNCHER_PLAY=1: then PLAY is pressed */
static int to_own_folder(void);
static int headless(void) {
    if (to_own_folder() != 1) return 2;
    settings_read();
    tr_set_language(g_lang);
    g_m.lock = SDL_CreateMutex();
    work_init(NOTES_LANG[g_lang]);
    const char* rom = getenv("DQIX_LAUNCHER_ROM");      /* tests: the ROM "chosen" on the first launch */
    if (rom && *rom) work_set_rom(rom); else work_start();
    for (;;) {
        SDL_LockMutex(g_m.lock); int busy = g_m.busy; SDL_UnlockMutex(g_m.lock);
        if (!busy) break;
        SDL_Delay(50);
    }
    work_stop();
    Model m = g_m;
    int can = lstate_can_play(&m.st);
    printf("update=%s build=%s pack=%s online=%d can_play=%d\n", step_name(m.st.update), step_name(m.st.build), step_name(m.st.pack), m.st.online, can);
    printf("download: %s | %s\ninstall: %s | %s\n", m.dl.text, m.dl.right, m.in.text, m.in.right);
    for (int i = 0; i < m.nnews; i++) printf("news: %s %s %s\n", m.news[i].tag, m.news[i].date, m.news[i].title);
    if (can && getenv("DQIX_LAUNCHER_PLAY")) {
        int ok = start_game();
#ifdef _WIN32
        printf("play=%s %lu\n", ok ? "started" : "failed", ok ? 0ul : GetLastError());
#else
        printf("play=%s %d\n", ok ? "started" : "failed", ok ? 0 : errno);
#endif
    }
    return can ? 0 : 1;
}

#ifdef _WIN32
#define LAUNCHER_EXE "dqix-launcher.exe"
#else
#define LAUNCHER_EXE "dqix-launcher"
#endif
static int file_here(const char* p) { FILE* f = fopen(p, "rb"); if (f) fclose(f); return f != NULL; }

/* the game folder: every path after this is relative to it. Started from the
 * project folder (the release zip's ready-made launcher, next to BUILD.bat):
 * the game folder's own launcher (rebuilt with every update) takes over when
 * there is one (2: we quit); else game/ is made and worked in (first launch) */
static int to_own_folder(void) {
#ifdef _WIN32
    wchar_t dir[MAX_PATH];                            /* wide: folders with any letters (C:\\Users\\Hélène...) */
    DWORD n = GetModuleFileNameW(NULL, dir, MAX_PATH);
    if (!n || n >= MAX_PATH) return 0;
    wchar_t* s = wcsrchr(dir, L'\\');
    if (!s) return 0;
    *s = 0;
    if (!SetCurrentDirectoryW(dir)) return 0;
#else
    char* b = SDL_GetBasePath();
    int ok = b && chdir(b) == 0;
    SDL_free(b);
    if (!ok) return 0;
#endif
    if (!file_here("BUILD.bat") && !file_here("BUILD.sh")) return 1;   /* the game folder already */
#ifdef _WIN32
    CreateDirectoryA("game", NULL);
    if (!SetCurrentDirectoryA("game")) return 0;
    if (file_here(LAUNCHER_EXE)) {
        STARTUPINFOA si; memset(&si, 0, sizeof si); si.cb = sizeof si;
        PROCESS_INFORMATION pi;
        char cmd[] = "\"" LAUNCHER_EXE "\"";
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return 2; }
    }
#else
    mkdir("game", 0755);
    if (chdir("game") != 0) return 0;
    if (file_here(LAUNCHER_EXE)) {
        pid_t pid = fork();
        if (pid == 0) { setsid(); execl("./" LAUNCHER_EXE, "./" LAUNCHER_EXE, (char*)NULL); _exit(127); }
        if (pid > 0) return 2;                          /* no fork: this one works here instead */
    }
#endif
    return 1;
}

/* the ROM, chosen in the system's own file window; 0 if cancelled, -1 if no window could open */
static int pick_rom(char* out, size_t n) {
#ifdef _WIN32
    wchar_t file[1024] = L"", title[128];
    MultiByteToWideChar(CP_UTF8, 0, tr("rom_title"), -1, title, 128);
    OPENFILENAMEW o; memset(&o, 0, sizeof o);
    o.lStructSize = sizeof o;
    o.hwndOwner = NULL;
    o.lpstrFilter = L"Nintendo DS ROM (*.nds, *.zip)\0*.nds;*.zip\0";
    o.lpstrFile = file; o.nMaxFile = 1024;
    o.lpstrTitle = title;
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&o)) return CommDlgExtendedError() ? -1 : 0;
    return WideCharToMultiByte(CP_UTF8, 0, file, -1, out, (int)n, NULL, NULL) > 0;
#else
    /* the desktop's own file window: zenity (GNOME, SteamOS), else kdialog (KDE) */
    const char* z[] = {"zenity", "--file-selection", "--title", tr("rom_title"), "--file-filter", "Nintendo DS ROM | *.nds *.NDS *.zip *.ZIP", NULL};
    const char* k[] = {"kdialog", "--getopenfilename", ".", "*.nds *.NDS *.zip *.ZIP", NULL};
    const char* const* tries[2] = {z, k};
    for (int t = 0; t < 2; t++) {
        LProc* p = proc_start(tries[t], NULL, NULL);
        if (!p) continue;
        char line[1024] = ""; int r, got = 0;
        while ((r = proc_read_line(p, line, sizeof line)) != -1) { if (r == 1 && line[0] == '/') { snprintf(out, n, "%s", line); got = 1; } else if (r == 0) SDL_Delay(30); }
        int code = proc_exit_code(p);
        proc_free(p);
        if (code == 127) continue;                       /* not installed: the other one */
        if (code != 0 && code != 1) return -1;          /* 1: cancelled; anything else: the window failed */
        return got && code == 0;
    }
    return -1;                                         /* neither zenity nor kdialog */
#endif
}

/* one launcher per game folder (Windows): two would download and build over each other */
static int single_instance(void) {
#ifdef _WIN32
    wchar_t dir[MAX_PATH], name[128];
    if (!GetCurrentDirectoryW(MAX_PATH, dir)) return 1;
    unsigned long long h = 0xCBF29CE484222325ull;
    for (const wchar_t* c = dir; *c; c++) h = (h ^ (unsigned)towlower(*c)) * 0x100000001B3ull;
    _snwprintf(name, 128, L"Local\\dqix-launcher-%016llx", h);
    HANDLE m = CreateMutexW(NULL, TRUE, name);
    if (m && GetLastError() == ERROR_ALREADY_EXISTS) {
        if (getenv("DQIX_LAUNCHER_RESTARTED") && WaitForSingleObject(m, 10000) != WAIT_TIMEOUT) return 1;   /* the one that restarted us closes */
        HWND w = FindWindowW(NULL, L"Dragon Quest IX Modern");
        if (w) { ShowWindow(w, SW_RESTORE); SetForegroundWindow(w); }
        return 0;
    }
#endif
    return 1;
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    SDL_SetMainReady();
    if (getenv("DQIX_LAUNCHER_HEADLESS")) return headless();
    int where = to_own_folder();
    if (where == 2) return 0;                            /* the game folder's launcher took over */
    if (!where) { fprintf(stderr, "launcher: can't go to its own folder\n"); return start_game() ? 0 : 1; }
    if (!getenv("DQIX_LAUNCHER_SHOT") && !single_instance()) return 0;
    /* any failure to show the window: the game starts as before (dqix handed over to us) */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) { fprintf(stderr, "launcher: SDL: %s\n", SDL_GetError()); return start_game() ? 0 : 1; }
    settings_read();
    tr_set_language(g_lang);
    g_m.lock = SDL_CreateMutex();
    const char* d = getenv("DQIX_LAUNCHER_DEMO");
    const char* shot = getenv("DQIX_LAUNCHER_SHOT");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    g_win = SDL_CreateWindow("Dragon Quest IX Modern", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1100, 674,
                             SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | (shot ? SDL_WINDOW_HIDDEN : 0));
    if (!g_win) { fprintf(stderr, "launcher: window: %s\n", SDL_GetError()); return start_game() ? 0 : 1; }
    SDL_SetWindowMinimumSize(g_win, 960, 588);
    window_icon();
    g_size = size_read();
    if (!shot) size_apply();
    g_ren = SDL_CreateRenderer(g_win, -1, shot ? SDL_RENDERER_SOFTWARE : SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_ren) g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_SOFTWARE);
    if (!g_ren || !ui_init(g_ren)) { fprintf(stderr, "launcher: can't draw (fonts/Marcellus.ttf and fonts/Nunito.ttf needed)\n"); return start_game() ? 0 : 1; }
    if (d) demo(d);
    else { work_init(NOTES_LANG[g_lang]); work_start(); if (g_settings_unreadable) say_failed(tr("err_settings_read")); }
    int W = 1100, H = 674, frames = 0, run = 1;
    while (run) {
        Uint32 t0 = SDL_GetTicks();
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) run = 0;
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) ui_forget_fonts();
            else if (e.type == SDL_MOUSEMOTION) { g_mx = e.motion.x; g_my = e.motion.y; if (g_drag_volume) volume_from_mouse(g_mx); }
            else if (e.type == SDL_MOUSEWHEEL && g_options && g_keys) keys_scroll(-e.wheel.y * 2);
            else if (g_cap >= 0 && e.type == SDL_KEYDOWN) {                     /* the key screen asked for a key */
                if (!e.key.repeat) {
                    if (g_cap_slot < 2) keys_done(keys_take_key(g_cap, g_cap_slot, e.key.keysym.scancode, e.key.keysym.mod));
                    else if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) keys_done(KEYS_CANCELLED);   /* a button asked: only Esc */
                }
            } else if (g_cap >= 0 && e.type == SDL_CONTROLLERBUTTONDOWN) {
                if (g_cap_slot == 2) keys_done(keys_take_pad(g_cap, e.cbutton.button));
            } else if (g_cap >= 0 && e.type == SDL_CONTROLLERAXISMOTION) {
                int t = keys_trigger(e.caxis.axis, e.caxis.value);
                if (g_cap_slot == 2 && t != CTL_NONE) keys_done(keys_take_pad(g_cap, t));
            } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                int a = hit_at(e.button.x, e.button.y);
                if (g_cap >= 0 && (a < A_KEYCELL || a >= A_KEYCELL + CA_N * 3)) g_cap = -1;   /* a click elsewhere: not changed */
                if (a == A_NONE) g_lang_menu = 0; else action(a);
                if (a == A_VOLUME) volume_from_mouse(e.button.x);
            } else if (e.type == SDL_MOUSEBUTTONUP && g_drag_volume) {
                char v[8]; snprintf(v, sizeof v, "%d", g_volume);
                if (!settings_set("volume", v)) settings_read();   /* not written: back to the file's */
                g_drag_volume = 0;
            } else if (e.type == SDL_CONTROLLERDEVICEADDED) {
                SDL_GameController* p = SDL_GameControllerOpen(e.cdevice.which);
                if (!g_pad && p) { g_pad = p; controls_pad_detect(p); }    /* the key screen shows its buttons' names */
            } else if (e.type == SDL_CONTROLLERDEVICEREMOVED && g_pad && SDL_GameControllerFromInstanceID(e.cdevice.which) == g_pad) {
                SDL_GameControllerClose(g_pad); g_pad = NULL;
                for (int i = 0; i < SDL_NumJoysticks() && !g_pad; i++) if (SDL_IsGameController(i)) g_pad = SDL_GameControllerOpen(i);
                controls_pad_detect(g_pad);
            }
            else if ((e.type == SDL_KEYDOWN && !e.key.repeat) || e.type == SDL_CONTROLLERBUTTONDOWN) {   /* a key held after it was given: not PLAY */
                int k = e.type == SDL_KEYDOWN ? e.key.keysym.sym : -1, b = e.type == SDL_CONTROLLERBUTTONDOWN ? e.cbutton.button : -1;
                if (g_options && g_keys && (k == SDLK_DOWN || k == SDLK_UP || k == SDLK_PAGEDOWN || k == SDLK_PAGEUP))
                    keys_scroll(k == SDLK_DOWN ? 1 : k == SDLK_UP ? -1 : k == SDLK_PAGEDOWN ? KEY_ROWS : -KEY_ROWS);
                else if (k == SDLK_ESCAPE || b == SDL_CONTROLLER_BUTTON_B) { if (g_keys) g_keys = 0; else g_options = 0; g_lang_menu = 0; }
                else if (k == SDLK_RETURN || b == SDL_CONTROLLER_BUTTON_A || b == SDL_CONTROLLER_BUTTON_START) {
                    SDL_LockMutex(g_m.lock); int can = lstate_can_play(&g_m.st) && !g_m.busy; SDL_UnlockMutex(g_m.lock);
                    if (can) action(A_PLAY);
                } else if (b == SDL_CONTROLLER_BUTTON_Y) g_options = !g_options;
            }
        }
        if (!d) work_poll();
        SDL_GetRendererOutputSize(g_ren, &W, &H);
        draw(W, H);
        if (shot && ++frames == 2) {
            if (!ui_save_png(shot, W, H)) { fprintf(stderr, "launcher: can't save %s\n", shot); return 1; }
            break;
        }
        SDL_RenderPresent(g_ren);
        if (shot) continue;
        Uint32 el = SDL_GetTicks() - t0;                /* vsync already waited for the screen: nothing more. A driver */
        if (el < 15) SDL_Delay(15 - el);                 /* that turns it off: 60 images a second, not a spinning core */
    }
    work_stop();
    SDL_Quit();
    return 0;
}
