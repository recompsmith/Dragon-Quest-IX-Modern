/* Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later */
/* hdpack_dir_rank (runtime/hdpack.c): which top folder of textures/ wins.
 * 0 the player's own packs, 1 the downloaded HD pack, 2 textures-hd's own.
 * Built and run by tests/hdpack/run.sh. */
#include <stdio.h>
#include "hdpack.h"

static int pass, fail;
#define CHECK(name, cond) do { if (cond) { pass++; printf("PASS %s\n", name); } else { fail++; printf("FAIL %s\n", name); } } while (0)

int main(void) {
    CHECK("rank: a player's folder (rom) is 0", hdpack_dir_rank("rom") == 0);
    CHECK("rank: hd-pack is 1", hdpack_dir_rank("hd-pack") == 1);
    CHECK("rank: hd is 2", hdpack_dir_rank("hd") == 2);
    CHECK("rank: HD-PACK is 1 (any case)", hdpack_dir_rank("HD-PACK") == 1);
    CHECK("rank: Hd is 2 (any case)", hdpack_dir_rank("Hd") == 2);
    CHECK("rank: hd-pack2 is a player's folder", hdpack_dir_rank("hd-pack2") == 0);
    CHECK("rank: hdx is a player's folder", hdpack_dir_rank("hdx") == 0);
    CHECK("rank: empty and NULL are 0", hdpack_dir_rank("") == 0 && hdpack_dir_rank(NULL) == 0);
    printf("t_rank: %d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
