/* Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later */
/* What's new after an update (whatsnew.h): the release notes in the journal's
 * panel, alone. Read only; the game waits while it is shown. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "journal.h"
#include "update.h"
#include "version.h"
#include "whatsnew.h"

#define NOTES_DIR "notes"
#define MAX_VERSIONS 5                          /* after a long time away: the newest few */
#define END_MARK "<!-- in-game notes end here -->"

static int g_pending;
static char g_since[32];                        /* after an update: the version run before ("" this one's notes only) */

int whatsnew_start(const char* last, int had_settings) {
    g_pending = 0; g_since[0] = 0;
    if (!had_settings) return 0;                                 /* a new install */
    if (last && last[0] && !update_newer(DQIX_VERSION, last)) return 0;   /* the same (or went back) */
    snprintf(g_since, sizeof g_since, "%s", last ? last : "");   /* "": older than this window, the new version's notes alone */
    g_pending = 1;
    return 1;
}
int whatsnew_pending(void) { return g_pending; }

/* ---------------------------------------------------------------- the notes' files */
static char g_ver[MAX_VERSIONS][32]; static int g_nver;          /* the versions shown, newest first */

static void pick_versions(const char* since) {
    g_nver = 0;
    DIR* d = opendir(NOTES_DIR);
    if (!d) return;
    char all[256][32]; int n = 0;
    struct dirent* e;
    while ((e = readdir(d)) && n < 256) {
        size_t l = strlen(e->d_name);
        if (l < 5 || l >= 32 || e->d_name[0] != 'v' || strcmp(e->d_name + l - 3, ".md")) continue;
        snprintf(all[n], 32, "%.*s", (int)(l - 3), e->d_name);
        if (strchr(all[n], '-')) continue;                       /* (nightly notes: not a version) */
        if (update_newer(all[n], DQIX_VERSION)) continue;        /* newer than this one (a rolled back copy) */
        if (since[0] && !update_newer(all[n], since)) continue;
        n++;
    }
    closedir(d);
    for (int i = 1; i < n; i++)                                  /* newest first */
        for (int j = i; j > 0 && update_newer(all[j], all[j - 1]); j--) { char t[32]; memcpy(t, all[j], 32); memcpy(all[j], all[j - 1], 32); memcpy(all[j - 1], t, 32); }
    int keep = since[0] ? MAX_VERSIONS : 1;                      /* (Help > What's new: this version's) */
    for (int i = 0; i < n && g_nver < keep; i++) memcpy(g_ver[g_nver++], all[i], 32);
}

static char* read_notes(const char* ver, int lang) {
    static const char* const sub[5] = { "", "fr/", "de/", "it/", "es/" };
    char path[128];
    for (int pass = 0; pass < 2; pass++) {                       /* the game's language, else English */
        snprintf(path, sizeof path, NOTES_DIR "/%s%s.md", pass ? "" : sub[lang], ver);
        FILE* f = fopen(path, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        if (n < 0 || n > (1 << 20)) { fclose(f); return NULL; }
        char* b = malloc((size_t)n + 1);
        if (!b) { fclose(f); return NULL; }
        n = (long)fread(b, 1, (size_t)n, f);
        fclose(f);
        b[n] = 0;
        return b;
    }
    return NULL;
}

/* ---------------------------------------------------------------- laid out in lines */
enum { L_VERSION, L_HEAD, L_TEXT, L_BULLET, L_GAP };
typedef struct { int kind, first, bold; char s[320]; } Line;   /* first: a bullet's first line (its dot); bold: its first bytes in gold ("**Label**") */
static Line* g_line; static int g_nline, g_cap;
static int g_lang = -1, g_top;

#define W_TEXT 17
#define W_HEAD 20
#define W_VER 26
#define INDENT 26

static void add(int kind, int first, const char* s, size_t n) {
    if (g_nline >= g_cap) {
        Line* m = realloc(g_line, sizeof(Line) * (size_t)(g_cap ? g_cap * 2 : 128));
        if (!m) return;
        g_line = m; g_cap = g_cap ? g_cap * 2 : 128;
    }
    Line* l = &g_line[g_nline++];
    l->kind = kind; l->first = first; l->bold = 0;
    if (n >= sizeof l->s) n = sizeof l->s - 1;
    memcpy(l->s, s, n); l->s[n] = 0;
}

/* the markdown's marks out: **bold**, *italic*, `code` */
static void plain(const char* s, size_t n, char* out, size_t cap) {
    size_t o = 0;
    for (size_t i = 0; i < n && o + 1 < cap; i++) {
        if (s[i] == '*' || s[i] == '`') continue;
        if (s[i] == '\\' && i + 1 < n && strchr("*_`[]#\\", s[i + 1])) { out[o++] = s[++i]; continue; }   /* (a markdown escape; rom\\3DS stays) */
        out[o++] = s[i];
    }
    while (o && out[o - 1] == ' ') o--;
    out[o] = 0;
}

/* whole words in lines of at most `w` panel units */
static void wrap(int kind, const char* s, float px, int w) {
    char buf[512];
    int first = 1;
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        const char* fit = NULL, *e = s;
        for (;;) {
            const char* x = e;
            while (*x && *x != ' ') x++;
            snprintf(buf, sizeof buf, "%.*s", (int)(x - s), s);
            if (fit && journal_text_width(px, buf) > w) break;
            fit = x;
            if (!*x) break;
            e = x + 1;
        }
        add(kind, first, s, (size_t)(fit - s));
        first = 0;
        s = fit;
    }
}

static void layout(int lang, int width) {
    static const char* const ver_word[5] = { "Version", "Version", "Version", "Versione", "Versi\xC3\xB3n" };
    g_nline = 0;
    for (int v = 0; v < g_nver; v++) {
        char head[64];
        snprintf(head, sizeof head, "%s %s", ver_word[lang], g_ver[v] + 1);
        if (v) add(L_GAP, 0, "", 0);
        add(L_VERSION, 0, head, strlen(head));
        char* t = read_notes(g_ver[v], lang);
        if (!t) continue;
        char* end = strstr(t, END_MARK);
        if (!end) end = strstr(t, "\n**How to install**");
        if (end) *end = 0;
        int gap = 0;
        for (char* p = t; *p; ) {
            char* nl = strchr(p, '\n');
            size_t n = nl ? (size_t)(nl - p) : strlen(p);
            while (n && (p[n - 1] == '\r' || p[n - 1] == ' ')) n--;
            const char* s = p; while (n && *s == ' ') { s++; n--; }
            p = nl ? nl + 1 : p + strlen(p);
            if (!n) { gap = 1; continue; }
            if (!strncmp(s, "<!--", 4)) continue;
            char out[1024];
            if (gap && g_nline && g_line[g_nline - 1].kind != L_VERSION) add(L_GAP, 0, "", 0);
            gap = 0;
            int heading = 0;                                     /* "**Fixed since v47**", "**Fixed since v47** (thanks TitanFr199)"; not "**Label**: text" */
            if (n > 4 && !strncmp(s, "**", 2)) {
                const char* close = strstr(s + 2, "**");
                size_t rest = close && close < s + n ? (size_t)(s + n - (close + 2)) : 0;
                heading = close && close < s + n && rest < 40 && !memchr(close + 2, ':', rest) && !memchr(close + 2, '*', rest);
            }
            if (*s == '#') { while (n && (*s == '#' || *s == ' ')) { s++; n--; } heading = 1; }
            if (heading) { plain(s, n, out, sizeof out); wrap(L_HEAD, out, W_HEAD, width); continue; }
            if (n > 2 && (*s == '-' || *s == '*') && s[1] == ' ') {
                plain(s + 2, n - 2, out, sizeof out);
                int at = g_nline;
                wrap(L_BULLET, out, W_TEXT, width - INDENT);
                const char* close = n > 6 && !strncmp(s + 2, "**", 2) ? strstr(s + 4, "**") : NULL;   /* "- **Label** ...": the label in gold */
                if (close && close < s + n && at < g_nline) {
                    char label[320]; plain(s + 4, (size_t)(close - (s + 4)), label, sizeof label);
                    size_t bl = strlen(label);
                    if (bl && !strncmp(g_line[at].s, label, bl)) g_line[at].bold = (int)bl;
                }
                continue;
            }
            if (n > 22 && !strncmp(s, "Dragon Quest IX Modern ", 23)) {   /* "Dragon Quest IX Modern v2.1: ..." under "Version 2.1": the rest */
                const char* c = memchr(s, ':', n);
                if (c && c - s < 40) { n -= (size_t)(c + 1 - s); s = c + 1; }
                plain(s, n, out, sizeof out);
                char* f = out; while (*f == ' ') f++;
                if (*f >= 'a' && *f <= 'z') *f -= 'a' - 'A';           /* ("...v2.1 : la nouvelle..." -> "La nouvelle...") */
                wrap(L_TEXT, f, W_TEXT, width);
                continue;
            }
            plain(s, n, out, sizeof out);
            wrap(L_TEXT, out, W_TEXT, width);
        }
        free(t);
    }
}

static int height(int kind) { return kind == L_VERSION ? 40 : kind == L_HEAD ? 32 : kind == L_GAP ? 12 : 25; }

/* the first line that can be at the top with the last one still in view */
static int last_top(int h) {
    int y = 0, i = g_nline;
    while (i > 0 && y + height(g_line[i - 1].kind) <= h) y += height(g_line[--i].kind);
    return i;
}

/* ---------------------------------------------------------------- the page */
static int g_area_h = 500;

static const char* page_title(int l) {
    static const char* const t[5] = { "What's new", "Nouveaut\xC3\xA9s", "Neuigkeiten", "Novit\xC3\xA0", "Novedades" };
    return t[l];
}

static void page_draw(const JournalCanvas* c) {
    int width = c->w - 30;
    if (g_lang != c->lang) { g_lang = c->lang; layout(c->lang, width); }
    g_area_h = c->h;
    if (!g_nline) {
        static const char* const none[5] = { "There are no notes for this version.", "Pas de notes pour cette version.",
            "F\xC3\xBCr diese Version gibt es keine Hinweise.", "Nessuna nota per questa versione.", "No hay notas para esta versi\xC3\xB3n." };
        journal_text(c, c->x, c->y, W_TEXT, 0x9AA0A8, none[c->lang]);
        return;
    }
    int lt = last_top(c->h);
    if (g_top > lt) g_top = lt;
    if (g_top < 0) g_top = 0;
    int y = c->y;
    for (int i = g_top; i < g_nline; i++) {
        const Line* l = &g_line[i];
        int h = height(l->kind);
        if (y + h > c->y + c->h) break;
        switch (l->kind) {
        case L_VERSION:
            journal_text(c, c->x, y, W_VER, 0xF2C94C, l->s);
            journal_box(c, c->x, y + 36, width, 1, 0xF2C94C, 0.5f);
            break;
        case L_HEAD: journal_text(c, c->x, y + 6, W_HEAD, 0xE8D7A0, l->s); break;
        case L_TEXT: journal_text(c, c->x, y, W_TEXT, 0xE6E8EC, l->s); break;
        case L_BULLET:
            if (l->first) journal_box(c, c->x + 8, y + 9, 6, 6, 0xF2C94C, 1);
            if (l->bold) {
                char label[320]; snprintf(label, sizeof label, "%.*s", l->bold, l->s);
                int lw = journal_text(c, c->x + INDENT, y, W_TEXT, 0xF2D27A, label);
                journal_text(c, c->x + INDENT + lw, y, W_TEXT, 0xE6E8EC, l->s + l->bold);
            } else journal_text(c, c->x + INDENT, y, W_TEXT, 0xE6E8EC, l->s);
            break;
        }
        y += h;
    }
    if (lt > 0) {                                                /* where in the notes */
        int total = 0, before = 0;
        for (int i = 0; i < g_nline; i++) { total += height(g_line[i].kind); if (i < g_top) before += height(g_line[i].kind); }
        float part = (float)c->h / total, at = (float)before / total;
        journal_box(c, c->x + c->w - 6, c->y, 4, c->h, 0x3A3F48, 1);
        journal_box(c, c->x + c->w - 6, c->y + (int)(at * c->h), 4, (int)(part * c->h) < 8 ? 8 : (int)(part * c->h), 0x9AA0A8, 1);
    }
}

static int page_key(int k) {
    int lt = last_top(g_area_h), page = g_area_h / 25 - 2, t = g_top;
    switch (k) {
    case JK_UP: t -= 2; break;
    case JK_DOWN: t += 2; break;
    case JK_PAGE_UP: case JK_LEFT: t -= page; break;
    case JK_PAGE_DOWN: case JK_RIGHT: case JK_OK: t += page; break;
    default: return 0;
    }
    if (t > lt) t = lt;
    if (t < 0) t = 0;
    if (t == g_top) return 0;
    g_top = t;
    return 1;
}

static void page_opened(void) { g_lang = -1; g_top = 0; }

static const char* page_help(int l) {
    static const char* const t[5] = {
        "\xE2\x86\x91 \xE2\x86\x93: scroll     \xE2\x86\x90 \xE2\x86\x92: page     Esc (B): close     Help menu: What's new, to see them again",
        "\xE2\x86\x91 \xE2\x86\x93 : faire d\xC3\xA9" "filer     \xE2\x86\x90 \xE2\x86\x92 : page     \xC3\x89" "chap (B) : fermer     Menu Aide : Nouveaut\xC3\xA9s, pour les revoir",
        "\xE2\x86\x91 \xE2\x86\x93: bl\xC3\xA4ttern     \xE2\x86\x90 \xE2\x86\x92: Seite     Esc (B): schlie\xC3\x9F" "en     Hilfe-Men\xC3\xBC: Neuigkeiten, um sie erneut zu sehen",
        "\xE2\x86\x91 \xE2\x86\x93: scorri     \xE2\x86\x90 \xE2\x86\x92: pagina     Esc (B): chiudi     Menu Aiuto: Novit\xC3\xA0, per rivederle",
        "\xE2\x86\x91 \xE2\x86\x93: desplazar     \xE2\x86\x90 \xE2\x86\x92: p\xC3\xA1gina     Esc (B): cerrar     Men\xC3\xBA Ayuda: Novedades, para verlas de nuevo",
    };
    return t[l];
}

static const JournalPage PAGE = { page_title, page_draw, page_key, page_opened, NULL, page_help };

void whatsnew_show(void) {
    pick_versions(g_pending ? g_since : "");
    fprintf(stderr, "what's new: %d version(s) shown%s%s\n", g_nver, g_pending && g_since[0] ? " since " : "", g_pending ? g_since : "");
    g_pending = 0;
    journal_show(&PAGE);
}
