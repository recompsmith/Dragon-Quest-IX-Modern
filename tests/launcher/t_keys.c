/* Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later */
/* The launcher's key screen (launcher/lnch_keys.c over runtime/controls.c):
 * what a key or button pressed while waiting does, and the controls.ini it
 * writes is the one the game reads. Run by tests/launcher/run.sh in a mktemp -d
 * folder; needs SDL2's headers and library (names of keys), no window. */
#include "lnch_keys.h"
#include "controls.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif

static int fails, total;
#define CHECK(c) do { if (!(c)) { printf("  line %d: %s\n", __LINE__, #c); ok = 0; } } while (0)
#define RUN(f) do { int ok = 1; f(&ok); total++; if (ok) printf("ok   %s\n", #f); else { printf("FAIL %s\n", #f); fails++; } } while (0)

static void keys_esc_cancels(int* okp) {
    int ok = 1;
    controls_defaults();
    int before = g_ctl_key[CA_A][0];
    CHECK(keys_take_key(CA_A, 0, SDL_SCANCODE_ESCAPE, 0) == KEYS_CANCELLED);
    CHECK(g_ctl_key[CA_A][0] == before);
    *okp = ok;
}
static void keys_taken_key_swaps(int* okp) {
    int ok = 1;
    controls_defaults();                                       /* A = X, B = Z */
    CHECK(keys_take_key(CA_A, 0, SDL_SCANCODE_Z, 0) == KEYS_SET);
    CHECK(g_ctl_key[CA_A][0] == SDL_SCANCODE_Z && g_ctl_key[CA_B][0] == SDL_SCANCODE_X);   /* B gets A's old key */
    *okp = ok;
}
static void keys_modifiers_only_for_the_programs_actions(int* okp) {
    int ok = 1;
    controls_defaults();
    CHECK(keys_take_key(CA_SHOT, 0, SDL_SCANCODE_S, KMOD_LCTRL) == KEYS_SET);
    CHECK(g_ctl_key[CA_SHOT][0] == (SDL_SCANCODE_S | CTL_MOD_CTRL));
    CHECK(keys_take_key(CA_A, 0, SDL_SCANCODE_K, KMOD_LSHIFT) == KEYS_SET);   /* a DS button: the key alone */
    CHECK(g_ctl_key[CA_A][0] == SDL_SCANCODE_K);
    CHECK(keys_take_key(CA_A, 1, SDL_SCANCODE_LSHIFT, KMOD_LSHIFT) == KEYS_SET);   /* Shift itself, as a key */
    CHECK(g_ctl_key[CA_A][1] == SDL_SCANCODE_LSHIFT);
    *okp = ok;
}
static void keys_pad_button_and_triggers(int* okp) {
    int ok = 1;
    controls_defaults();
    CHECK(keys_take_pad(CA_FAST, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) == KEYS_SET);
    CHECK(g_ctl_pad[CA_FAST] == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER && g_ctl_pad[CA_R] == CTL_NONE);   /* R had it: gets FAST's none */
    CHECK(keys_trigger(SDL_CONTROLLER_AXIS_TRIGGERLEFT, 30000) == CTL_PAD_LT);
    CHECK(keys_trigger(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 30000) == CTL_PAD_RT);
    CHECK(keys_trigger(SDL_CONTROLLER_AXIS_TRIGGERLEFT, 2000) == CTL_NONE);   /* barely touched */
    CHECK(keys_trigger(SDL_CONTROLLER_AXIS_LEFTX, 30000) == CTL_NONE);        /* a stick: walking, fixed */
    *okp = ok;
}
static void keys_saved_file_is_the_games(int* okp) {
    int ok = 1;
    controls_defaults();
    keys_take_key(CA_JOURNAL, 0, SDL_SCANCODE_F9, 0);
    keys_take_pad(CA_PAUSE, CTL_PAD_RT);
    CHECK(keys_save());
    controls_defaults();
    controls_load();                                           /* what the game does at start */
    CHECK(g_ctl_key[CA_JOURNAL][0] == SDL_SCANCODE_F9 && g_ctl_pad[CA_PAUSE] == CTL_PAD_RT);
    *okp = ok;
}
static void keys_save_says_when_it_cannot(int* okp) {
    int ok = 1;
    remove("controls.ini");
#ifdef _WIN32
    CHECK(_mkdir("controls.ini") == 0);
#else
    CHECK(mkdir("controls.ini", 0755) == 0);
#endif
    CHECK(!keys_save());                                       /* a folder in its place: not written, said */
    rmdir("controls.ini");
    *okp = ok;
}

int main(int argc, char** argv) {
    if (argc < 2 || chdir(argv[1]) != 0) { printf("usage: t_keys <empty folder>\n"); return 2; }
    RUN(keys_esc_cancels); RUN(keys_taken_key_swaps); RUN(keys_modifiers_only_for_the_programs_actions);
    RUN(keys_pad_button_and_triggers); RUN(keys_saved_file_is_the_games); RUN(keys_save_says_when_it_cannot);
    printf("%d/%d passed\n", total - fails, total);
    return fails ? 1 : 0;
}
