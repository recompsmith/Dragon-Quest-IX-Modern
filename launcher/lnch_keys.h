/* Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later */
/* The launcher's key screen: the game's own controls (runtime/controls.c), so
 * the controls.ini it writes is the one the game reads, before the game ever ran. */
#pragma once
#include "SDL.h"

enum { KEYS_CANCELLED, KEYS_SET };
/* a key pressed while an action's key (slot 0 or 1) is asked: Esc cancels; a
 * key already used elsewhere swaps (as in the game's Controls window) */
int keys_take_key(int action, int slot, SDL_Scancode sc, Uint16 mod);
int keys_take_pad(int action, int pad);                  /* a controller button (or CTL_PAD_LT/RT) */
int keys_trigger(int axis, int value);                   /* a trigger pulled: CTL_PAD_LT/RT, else CTL_NONE */
int keys_save(void);                                     /* controls.ini written (0: not, the screen says so) */
