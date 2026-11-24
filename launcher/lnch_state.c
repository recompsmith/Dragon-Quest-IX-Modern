/* When PLAY lights up, and what the status line says: see lnch.h and the
 * spec (docs/superpowers/specs/2026-11-23-launcher-design.md). */
#include <stdlib.h>
#include <string.h>
#include "lnch.h"

static int update_pending(const LState* s) {
    return s->platform_updates && s->online != 0 && s->update_check && s->available[0]
        && update_newer(s->available, s->installed) && s->update != ST_DONE;
}
/* the HD pack still has to come before playing (offline: play without it,
 * it is only ever installed whole) */
static int pack_pending(const LState* s) {
    return s->platform_updates && s->hd_pack_wanted && !s->pack_complete && s->online != 0;
}

int lstate_can_play(const LState* s) {
    if (s->rom_needed || s->update_unbuilt) return 0; /* no ROM yet; a half-new tree: the build must finish first */
    if (s->download_failed && s->modern) return 0;    /* Romuald: a broken download blocks Modern, not Classic */
    if (s->update == ST_RUNNING || s->build == ST_RUNNING || s->pack == ST_RUNNING) return 0;
    if (s->update == ST_FAILED || s->build == ST_FAILED || s->pack == ST_FAILED) return 0;
    if (!s->game_built) return 0;
    return !update_pending(s) && !pack_pending(s);
}

const char* lstate_status_key(const LState* s) {
    if (s->update == ST_FAILED || s->build == ST_FAILED || s->pack == ST_FAILED) return "st_failed";
    if (s->download_failed && s->modern) return "st_failed";
    if (s->rom_needed && s->build != ST_RUNNING) return "st_rom";
    if (s->update == ST_RUNNING || update_pending(s)) return "st_updating";
    if (s->build == ST_RUNNING || !s->game_built || s->update_unbuilt) return "st_building";
    if (s->pack == ST_RUNNING || pack_pending(s)) return "st_pack";
    if (!s->platform_updates) return "st_no_updates_here";
    if (s->online == 0) return "st_offline";
    return "st_ready";
}

const char* lstate_branch(const char* installed) {
    return strstr(installed, "nightly") ? "main" : "stable";
}

int rom_kind_ok(const char* path) {
    size_t n = strlen(path);
    if (n < 5) return 0;
    const char* e = path + n - 4;
    char low[5];
    for (int i = 0; i < 4; i++) low[i] = (char)(e[i] >= 'A' && e[i] <= 'Z' ? e[i] + 32 : e[i]);
    low[4] = 0;
    return !strcmp(low, ".nds") || !strcmp(low, ".zip");
}

void lstate_settings(const char* t, LState* s) {
    char v[32];
    int classic = ini_get(t, "mode", v, sizeof v) && strcmp(v, "enhanced") != 0;
    int pack = !ini_get(t, "hd_pack_download", v, sizeof v) || atoi(v) != 0;
    s->modern = !classic;
    s->hd_pack_wanted = pack && !classic;
    s->update_check = !ini_get(t, "update_check", v, sizeof v) || atoi(v) != 0;
}

/* a broken download (hd-pack.c says "offline" or "download", or it stopped
 * mid-download) is the network's; no status line at all is an early local
 * fault (folder refused, lock, manifest), never retried as the network */
int pack_outcome(int exe_found, int code, int have, const PackStatus* s) {
    if (!exe_found) return PACK_MISSING;
    if (code == 0) return PACK_DONE;
    if (code == 2) return PACK_BUSY;
    if (code == 1 && have && (s->kind == 'd' || (s->kind == 'f' && (!strncmp(s->reason, "offline", 7) || !strncmp(s->reason, "download", 8)))))
        return PACK_NET;
    return PACK_FAILED;
}
