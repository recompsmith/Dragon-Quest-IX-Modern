/* Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later */
/* The HD pack (FatalPixels textures, SeedVR2 movies): hd-pack.exe, next to
 * the game, downloads and installs it in the background (tools/hd-pack.c);
 * the game only starts it, shows its bar (hd-pack-progress.txt) and uses the
 * pack once it is there. */
#pragma once

/* the order textures/<top folder>/ is looked in, the first texture found
 * winning: 0 the player's own packs, 1 hd-pack (the downloaded pack), 2 hd
 * (made by textures-hd); any letter case */
int hdpack_dir_rank(const char* top_folder);

/* start hd-pack.exe (Windows, if it is next to the game and enabled):
 * detached, no window, idle priority; it does nothing if the pack is there */
void hdpack_start(int enabled);

/* hd-pack.exe is at work for this game folder now (its named mutex is
 * there; Windows only, 0 elsewhere): a "working" bar without it is old */
int hdpack_running(void);

#ifdef _WIN32
#include <wchar.h>
#include <stdio.h>
/* the mutex hd-pack.exe holds while it works, named after the game folder
 * (tools/hd-pack.c makes it, the game only looks for it) */
static inline void hdpack_mutex_name(const wchar_t* folder, wchar_t* out, size_t n) {
    unsigned long long h = 0xCBF29CE484222325ull;              /* FNV-1a of the folder, any case */
    for (const wchar_t* c = folder; *c; c++) {
        wchar_t w = (*c >= L'A' && *c <= L'Z') ? (wchar_t)(*c + 32) : *c;
        h = (h ^ (unsigned)w) * 0x100000001B3ull;
    }
    _snwprintf(out, n, L"Local\\dqix-hd-pack-%016llx", h);
    out[n - 1] = 0;
}
#endif
