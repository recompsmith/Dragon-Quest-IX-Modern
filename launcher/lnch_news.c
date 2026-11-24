/* The news: GitHub's list of releases, or the release notes the build put in
 * game/notes (offline, and for the player's language). See lnch.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "lnch.h"

/* a JSON string at *p (just after its opening quote) into out; *p after its closing quote */
static void json_string(const char** p, char* out, size_t n) {
    size_t o = 0;
    const char* s = *p;
    while (*s && *s != '"') {
        char c = *s++;
        if (c == '\\' && *s) {
            c = *s++;
            if (c == 'n' || c == 'r' || c == 't') c = ' ';
            else if (c == 'u') { for (int i = 0; i < 4 && *s; i++) s++; c = '?'; }   /* rare in our titles */
        }
        if (out && o + 1 < n) out[o++] = c;
    }
    if (out && n) out[o] = 0;
    *p = *s ? s + 1 : s;
}

int news_from_github(const char* json, NewsItem* out, int max) {
    int count = 0, depth = 0, in_obj = 0;
    NewsItem cur; int draft = 0, pre = 0;
    memset(&cur, 0, sizeof cur);
    char key[32] = "";
    int want_value = 0;
    for (const char* p = json ? json : ""; *p && count < max;) {
        char c = *p;
        if (c == '"') {
            p++;
            if (depth == 2 && !want_value) { json_string(&p, key, sizeof key); continue; }
            if (depth == 2 && want_value) {
                if (!strcmp(key, "tag_name")) json_string(&p, cur.tag, sizeof cur.tag);
                else if (!strcmp(key, "name")) json_string(&p, cur.title, sizeof cur.title);
                else if (!strcmp(key, "published_at")) { char d[32]; json_string(&p, d, sizeof d); snprintf(cur.date, sizeof cur.date, "%.10s", d); }
                else json_string(&p, NULL, 0);
                want_value = 0;
                continue;
            }
            json_string(&p, NULL, 0);
            continue;
        }
        if (c == '[' || c == '{') {
            depth++;
            if (depth == 2 && c == '{') { memset(&cur, 0, sizeof cur); draft = pre = 0; want_value = 0; in_obj = 1; }
            else if (depth == 3) want_value = 0;            /* a nested value: skipped */
        } else if (c == ']' || c == '}') {
            /* a version tag only ("v3.2"): it is also part of a file name (notes/<tag>.md) */
            int tag_ok = cur.tag[0] == 'v' && !strpbrk(cur.tag, "/\\:") && !strstr(cur.tag, "..");
            if (depth == 2 && c == '}' && in_obj && tag_ok && !draft && !pre) {
                if (!cur.title[0]) snprintf(cur.title, sizeof cur.title, "%s", cur.tag);
                out[count++] = cur;
            }
            if (depth == 2) in_obj = 0;
            depth--;
        } else if (c == ':' && depth == 2) {
            want_value = 1;
        } else if (c == ',' && depth == 2) {
            want_value = 0;
        } else if (depth == 2 && want_value && (c == 't' || c == 'f' || c == 'n')) {
            if (!strcmp(key, "draft")) draft = c == 't';
            if (!strcmp(key, "prerelease")) pre = c == 't';
            while (*p && *p != ',' && *p != '}') p++;
            want_value = 0;
            continue;
        }
        p++;
    }
    return count;
}

int news_title_from_notes(const char* md, char* out, size_t n) {
    const char* p = md;
    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (!*p) return 0;
        const char* e = p;
        while (*e && *e != '\n') e++;
        const char* end = e;
        while (end > p && (end[-1] == '\r' || end[-1] == ' ')) end--;
        const char* pre = "Dragon Quest IX Modern ";
        if (!strncmp(p, pre, strlen(pre))) {            /* "Dragon Quest IX Modern v3.2: " or "v3.2 : " */
            const char* c = memchr(p, ':', (size_t)(end - p));
            if (c) { p = c + 1; while (p < end && *p == ' ') p++; }
        }
        size_t len = (size_t)(end - p);
        if (len >= n) len = n - 1;
        memcpy(out, p, len); out[len] = 0;
        /* a capital first, like a title: a-z, and the accented Latin-1 lower case (UTF-8 C3 A0-BE, not the ÷ B7) */
        unsigned char* u = (unsigned char*)out;
        if (u[0] >= 'a' && u[0] <= 'z') u[0] -= 32;
        else if (u[0] == 0xC3 && u[1] >= 0xA0 && u[1] <= 0xBE && u[1] != 0xB7) u[1] -= 0x20;
        return 1;
    }
}

static int read_text(const char* path, char* buf, size_t n) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    size_t got = fread(buf, 1, n - 1, f);
    fclose(f);
    buf[got] = 0;
    return 1;
}

int news_local(const char* notes_dir, const char* lang, NewsItem* out, int max) {
    enum { MAXN = 128 };
    static char tags[MAXN][24];
    int nt = 0;
    DIR* d = opendir(notes_dir);
    if (!d) return 0;
    struct dirent* e;
    while ((e = readdir(d)) && nt < MAXN) {
        size_t l = strlen(e->d_name);
        if (e->d_name[0] != 'v' || l < 5 || l >= 24 || strcmp(e->d_name + l - 3, ".md")) continue;
        memcpy(tags[nt], e->d_name, l - 3); tags[nt][l - 3] = 0;
        nt++;
    }
    closedir(d);
    for (int i = 1; i < nt; i++)                        /* newest first */
        for (int j = i; j > 0 && update_newer(tags[j], tags[j - 1]); j--) {
            char t[24]; strcpy(t, tags[j]); strcpy(tags[j], tags[j - 1]); strcpy(tags[j - 1], t);
        }
    int count = 0;
    static char text[16384];
    for (int i = 0; i < nt && count < max; i++) {
        char path[1200];
        int got = 0;
        if (lang && lang[0]) { snprintf(path, sizeof path, "%.900s/%.8s/%.23s.md", notes_dir, lang, tags[i]); got = read_text(path, text, sizeof text); }
        if (!got) { snprintf(path, sizeof path, "%.900s/%.23s.md", notes_dir, tags[i]); got = read_text(path, text, sizeof text); }
        NewsItem* it = &out[count];
        memset(it, 0, sizeof *it);
        snprintf(it->tag, sizeof it->tag, "%.23s", tags[i]);
        if (!got || !news_title_from_notes(text, it->title, sizeof it->title)) snprintf(it->title, sizeof it->title, "%.23s", tags[i]);
        count++;
    }
    return count;
}
