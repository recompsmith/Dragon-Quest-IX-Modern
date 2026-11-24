/* The launcher's work before playing (lnch_work.c): a thread that checks
 * for an update, installs it, builds, and gets the HD pack, writing what the
 * screen shows into g_m (under g_m.lock). */
#pragma once
#include <SDL.h>
#include "lnch.h"

typedef struct { char text[200]; char right[80]; float frac; int state; } Bar;   /* state 0 going, 1 done, 2 failed */
typedef struct {
    SDL_mutex* lock;
    LState st;
    Bar dl, in;                   /* the DOWNLOAD and INSTALL bars */
    NewsItem news[3]; int nnews, news_offline;
    int busy;                     /* the thread is at work: PLAY stays off */
    int background_ready;         /* movies/launcher-back.jpg can be loaded */
    char pack_version[32];        /* "hd-pack-2", "" none */
} Model;
extern Model g_m;

void work_init(const char* notes_lang);   /* the local state and news, at once (no network) */
void work_start(void);                    /* the thread: check, update, build, pack (start, VERIFY, RETRY) */
void work_poll(void);                     /* from the main loop */
void work_stop(void);
void work_news_language(const char* notes_lang);
void work_settings_changed(void);         /* mode or HD pack choice changed: what PLAY waits for changes */
void work_set_rom(const char* path_utf8); /* the ROM chosen on the first launch: copied to rom/, then built */
