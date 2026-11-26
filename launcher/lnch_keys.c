/* Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later */
#include "lnch_keys.h"
#include "controls.h"

/* as the game's Game > Controls window (runtime/frontend_sdl.c capture_key): a
 * DS button takes the key alone; the program's own actions keep Ctrl/Alt/Shift */
int keys_take_key(int action, int slot, SDL_Scancode sc, Uint16 mod) {
    if (sc == SDL_SCANCODE_ESCAPE) return KEYS_CANCELLED;
    int is_mod = sc == SDL_SCANCODE_LCTRL || sc == SDL_SCANCODE_RCTRL || sc == SDL_SCANCODE_LALT || sc == SDL_SCANCODE_RALT ||
                 sc == SDL_SCANCODE_LSHIFT || sc == SDL_SCANCODE_RSHIFT;
    int key = (int)sc;
    if (!is_mod && action >= CA_N_DS)
        key |= (mod & KMOD_CTRL ? CTL_MOD_CTRL : 0) | (mod & KMOD_ALT ? CTL_MOD_ALT : 0) | (mod & KMOD_SHIFT ? CTL_MOD_SHIFT : 0);
    controls_set_key(action, slot, key);
    return KEYS_SET;
}

int keys_take_pad(int action, int pad) {
    controls_set_pad(action, pad);
    return KEYS_SET;
}

int keys_trigger(int axis, int value) {
    if (value <= 12000) return CTL_NONE;                     /* the same threshold as controls_pad_down */
    return axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ? CTL_PAD_LT : axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT ? CTL_PAD_RT : CTL_NONE;
}

int keys_save(void) { return controls_save(); }
