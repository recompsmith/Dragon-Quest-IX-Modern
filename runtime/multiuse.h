/* Copyright (C) 2026 TitanFr199. SPDX-License-Identifier: GPL-3.0-or-later */
/* Use several at once: the item menu's "Use several", "How many?" (multiuse.c). */
#pragma once
#include <stdint.h>

typedef struct { int style, face; char label[32]; } MuKey;   /* a DS button's sign as the player has it (hints.h); label "": none */

const uint8_t* multiuse_picture(int w, int h, int* changed);   /* its window over everything (premultiplied RGBA), NULL: none */
int multiuse_keys(uint16_t* keys, int field);   /* the game's keys this frame (field: on foot, not a battle): 1 if it took or changed them */
int multiuse_up(void);                          /* its window is up (the mouse and the keys are its) */
int multiuse_mouse(float x, float y);           /* a click (drawable pixels) while it's up: 1 taken */
void multiuse_anchor(float x, float y, float w, float h, float scale);   /* the game's window it stands by this frame (drawable pixels; scale: per DS pixel) */
void multiuse_show_keys(const MuKey k[4]);      /* A, B, L, R as the player has them */
