/* Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later */
/* The launcher (game/dqix-launcher).
 * This header is the part without SDL: settings.ini, the state that decides
 * when PLAY lights up, the news, the tools it runs and the texts. */
#pragma once
#include <stddef.h>

/* ---- settings.ini: only the keys the launcher sets change, the rest of the
 * text (comments, line ends, order) stays byte for byte */
int   ini_get(const char* text, const char* key, char* out, size_t n);   /* 1 found (the last one, as the game's settings_load reads it) */
char* ini_set(const char* text, const char* key, const char* value);    /* new text (malloc): replaced in place, or added at the end */
int   ini_load(const char* path, char** text);                           /* 1 read; 0 missing; -1 there but unreadable (*text = "" for both) */
int   ini_save(const char* path, const char* text);                      /* path.tmp then renamed; 1 ok */
/* one key of settings.ini: never writes when the file is there but couldn't be
 * read (a file with only that key would lose the player's settings); a new
 * file starts with settings_version (the game's SETTINGS_VERSION, 5); 1 ok */
int   settings_update(const char* path, const char* key, const char* value);

/* ---- what the screen shows and whether PLAY is lit */
typedef enum { ST_IDLE, ST_RUNNING, ST_DONE, ST_FAILED, ST_SKIPPED } StepState;
typedef struct {
    int platform_updates;     /* 1 on Windows (updater + hd-pack.exe), 0 elsewhere */
    int online;               /* -1 unknown, 0 offline, 1 online */
    int update_check;         /* settings.ini update_check (default 1) */
    int hd_pack_wanted;       /* hd_pack_download != 0 and mode = enhanced */
    int game_built;           /* game/dqix[.exe] and game/data/dqix.nds are there */
    int pack_complete;        /* textures/hd-pack/hd-pack.txt is there (the pack is all or nothing) */
    int modern;               /* mode = enhanced (the default) */
    int download_failed;      /* an update or HD pack download broke off (red bar, RETRY) */
    int rom_needed;           /* no data/dqix.nds: the player must choose the ROM (first launch) */
    int update_unbuilt;       /* ../update/download.zip is there: an update was put in place but its build didn't finish */
    char installed[32], available[32];   /* the local version.h's, the remote one's ("" unknown) */
    StepState update, build, pack;
} LState;
int         lstate_can_play(const LState* s);
const char* lstate_status_key(const LState* s);
/* the INSTALL bar over the whole run: a step of weight w, after `done` of the bar,
 * with `rest` (weights) still to come, gets [lo, hi]; its own 0..1 maps into it */
void  install_share(float done, int w, int rest, float* lo, float* hi);
float install_frac(float lo, float hi, float f);    /* a text key (lnch_text.c) for the status line */
const char* lstate_branch(const char* installed);  /* "main" for a nightly, else "stable" */
int         rom_kind_ok(const char* path);         /* a .nds, or a .zip holding it (any letter case) */
/* settings.ini's choices that decide what PLAY waits for: mode, hd_pack_download, update_check */
void        lstate_settings(const char* ini_text, LState* s);

/* ---- news */
typedef struct { char tag[24]; char date[11]; char title[160]; } NewsItem;  /* date "YYYY-MM-DD" or "" */
int news_from_github(const char* json, NewsItem* out, int max);   /* GitHub's releases list: tag_name, published_at, name; drafts and prereleases left out */
int news_title_from_notes(const char* md, char* out, size_t n);   /* first non-empty line, "Dragon Quest IX Modern vX: " taken off */
int news_local(const char* notes_dir, const char* lang, NewsItem* out, int max);  /* <dir>[/<lang>]/v*.md, newest first, date "" */

/* ---- the tools it runs (lnch_proc.c): no window, their output read through a pipe */
typedef struct LProc LProc;
LProc* proc_start(const char* const* argv, const char* const* env_add, const char* cwd);  /* env_add: "NAME=value" ..., NULL */
int    proc_read_line(LProc* p, char* line, size_t n);   /* 1 a line, 0 nothing yet, -1 finished (all read) */
int    proc_exit_code(LProc* p);                         /* once proc_read_line said -1 */
void   proc_free(LProc* p);
/* BUILD.bat / build-game.sh with DQIX_PROGRESS: 1 "progress <f> <title>", 2 "BUILD FAILED (...)" (title = the line), 0 other */
int parse_build_line(const char* line, float* frac, char* title, size_t n);
/* hd-pack-status.txt (tools/hd-pack.c) */
typedef struct { int kind; long long done, total; int i, n; char reason[161]; } PackStatus;  /* kind 'd' download, 'i' install, 'u' up to date, 'D' done, 'f' failed */
int parse_pack_status(const char* line, PackStatus* out);   /* 1 understood */
/* how hd-pack.exe ended (exe_found, its exit code, its status line if any) */
enum { PACK_DONE, PACK_NET, PACK_FAILED, PACK_BUSY, PACK_MISSING };
int         pack_outcome(int exe_found, int code, int have_status, const PackStatus* s);

/* ---- texts (lnch_text.c): lang 1-5 in LANGUAGE_KEY's order (english french
 * german italian spanish); a key it doesn't know comes back as itself */
const char* tr(const char* key);
void        tr_set_language(int lang);
const char* tr_in(const char* key, int lang);  /* in that language, whatever the current one */
const char* tr_key(int i);                   /* every key in turn, NULL after the last (tests) */
int         tr_has(int i, int lang);       /* key i has its own text in lang (not the English fallback) */

/* from runtime/update.c */
int update_newer(const char* a, const char* b);
