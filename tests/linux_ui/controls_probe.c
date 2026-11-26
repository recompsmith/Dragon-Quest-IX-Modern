#include "controls.h"
#include <assert.h>

int main(void) {
    controls_load();
    struct Check { SDL_Scancode key; Uint16 mods; int action; } checks[] = {
        /* SDL's physical W/A positions are Z/Q on AZERTY. */
        {SDL_SCANCODE_W, 0, CA_UP},
        {SDL_SCANCODE_A, 0, CA_LEFT},
        {SDL_SCANCODE_S, 0, CA_DOWN},
        {SDL_SCANCODE_D, 0, CA_RIGHT},
        {SDL_SCANCODE_UP, 0, CA_UP},
        {SDL_SCANCODE_DOWN, 0, CA_DOWN},
        {SDL_SCANCODE_X, 0, CA_A},
        {SDL_SCANCODE_Z, 0, CA_B},
        {SDL_SCANCODE_Q, 0, CA_L},
        {SDL_SCANCODE_R, 0, CA_R},
        {SDL_SCANCODE_J, 0, CA_JOURNAL},
        {SDL_SCANCODE_F11, 0, CA_FULL},
        {SDL_SCANCODE_R, KMOD_CTRL, CA_RESET},
        {SDL_SCANCODE_W, KMOD_CTRL, -1},
        {SDL_SCANCODE_UNKNOWN, 0, -1},
    };
    for (unsigned i = 0; i < sizeof checks / sizeof checks[0]; i++)
        assert(controls_key_action(checks[i].key, checks[i].mods) == checks[i].action);

    /* Gameplay still polls the same DS buttons, independently of menus. */
    Uint8 keys[SDL_NUM_SCANCODES] = {0};
    SDL_SetModState(KMOD_NONE);
    keys[SDL_SCANCODE_W] = keys[SDL_SCANCODE_X] = 1;
    assert(controls_ds_keys(keys) == ((1u << 6) | 1u));
    return 0;
}
