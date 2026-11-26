/* Tests of the launcher's logic without a window (launcher/lnch.h).
 *   t_launcher <temp folder>      (built and run by: sh tests/launcher/run.sh)
 * Writes only inside the temp folder given as argv[1]. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lnch.h"

static int fails, runs;
static const char* g_tmp;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d: %s\n", __func__, __LINE__, #c); fails++; return; } } while (0)
#define RUN(f) do { int before = fails; runs++; f(); printf("%s %s\n", fails == before ? "ok  " : "FAIL", #f); } while (0)

static void put(const char* rel, const char* text) {
    char p[1024]; snprintf(p, sizeof p, "%s/%s", g_tmp, rel);
    FILE* f = fopen(p, "wb"); if (!f) { printf("  can't write %s\n", p); exit(2); }
    fputs(text, f); fclose(f);
}

/* ---- settings.ini */
static void ini_set_keeps_comments_and_crlf(void) {
    char* t = ini_set("# a\r\nmode = classic\r\nvolume = 70\r\n", "mode", "enhanced");
    CHECK(t && !strcmp(t, "# a\r\nmode = enhanced\r\nvolume = 70\r\n"));
    free(t);
}
static void ini_set_appends_missing_key(void) {
    char* t = ini_set("volume = 70\n", "mode", "classic");
    CHECK(t && !strcmp(t, "volume = 70\nmode = classic\n"));
    free(t);
    t = ini_set("volume = 70", "mode", "classic");          /* no line end at the end */
    CHECK(t && !strcmp(t, "volume = 70\nmode = classic\n"));
    free(t);
    t = ini_set("", "mode", "classic");
    CHECK(t && !strcmp(t, "mode = classic\n"));
    free(t);
}
static void ini_set_does_not_touch_longer_key(void) {
    char* t = ini_set("modern = 1\nmode = classic\n", "mode", "enhanced");
    CHECK(t && !strcmp(t, "modern = 1\nmode = enhanced\n"));
    free(t);
}
static void ini_get_last_of_duplicates(void) {
    char v[32];                                     /* the game's settings_load: the last one wins */
    CHECK(ini_get("mode = classic\nmode = enhanced\n", "mode", v, sizeof v) == 1);
    CHECK(!strcmp(v, "enhanced"));
    char* t = ini_set("mode = classic\nvolume = 1\nmode = enhanced\n", "mode", "classic");
    CHECK(t && !strcmp(t, "mode = classic\nvolume = 1\nmode = classic\n"));
    free(t);
    CHECK(ini_get("  volume=55  \r\n", "volume", v, sizeof v) == 1 && !strcmp(v, "55"));
    CHECK(ini_get("# mode = x\n", "mode", v, sizeof v) == 0);
}
static void ini_ignores_line_without_equals(void) {
    char v[32];
    CHECK(ini_get("mode\nmode = classic\n", "mode", v, sizeof v) == 1 && !strcmp(v, "classic"));
    char* t = ini_set("mode\nvolume = 1\n", "mode", "enhanced");
    CHECK(t && !strcmp(t, "mode\nvolume = 1\nmode = enhanced\n"));
    free(t);
}
static void settings_update_new_file_has_version(void) {
    char p[1024]; snprintf(p, sizeof p, "%s/new-settings.ini", g_tmp);
    remove(p);
    CHECK(settings_update(p, "mode", "classic") == 1);      /* a new file: the game must not read it as a legacy one */
    char* t = NULL;
    CHECK(ini_load(p, &t) == 1 && !strcmp(t, "settings_version = 4\nmode = classic\n"));
    free(t);
}
static void settings_update_keeps_others(void) {
    char p[1024]; snprintf(p, sizeof p, "%s/old-settings.ini", g_tmp);
    CHECK(ini_save(p, "settings_version = 4\r\nvolume = 30\r\nmode = enhanced\r\n") == 1);
    CHECK(settings_update(p, "mode", "classic") == 1);
    char* t = NULL;
    CHECK(ini_load(p, &t) == 1 && !strcmp(t, "settings_version = 4\r\nvolume = 30\r\nmode = classic\r\n"));
    free(t);
}
static void settings_update_refuses_unreadable(void) {
    char p[1024]; snprintf(p, sizeof p, "%s/locked.ini", g_tmp);   /* there but can't be read: a folder stands for it */
#ifdef _WIN32
    char cmd[1100]; snprintf(cmd, sizeof cmd, "mkdir \"%s\" 2>nul", p); system(cmd);
#else
    char cmd[1100]; snprintf(cmd, sizeof cmd, "mkdir -p '%s'", p); if (system(cmd)) {}
#endif
    char* t = NULL;
    CHECK(ini_load(p, &t) == -1);
    free(t);
    CHECK(settings_update(p, "mode", "classic") == 0);      /* never a file with only our key */
}
static void ini_load_missing_is_empty(void) {
    char p[1024]; snprintf(p, sizeof p, "%s/nothing-here.ini", g_tmp);
    char* t = NULL;
    CHECK(ini_load(p, &t) == 0 && t && t[0] == 0);
    free(t);
}
static void ini_save_then_load(void) {
    char p[1024]; snprintf(p, sizeof p, "%s/settings.ini", g_tmp);
    CHECK(ini_save(p, "a = 1\r\n") == 1);
    char* t = NULL;
    CHECK(ini_load(p, &t) == 1 && !strcmp(t, "a = 1\r\n"));
    free(t);
}

/* ---- state */
static LState ready(void) {
    LState s; memset(&s, 0, sizeof s);
    s.modern = 1;
    s.platform_updates = 1; s.online = 1; s.update_check = 1; s.hd_pack_wanted = 1;
    s.game_built = 1; s.pack_complete = 1;
    strcpy(s.installed, "v3.2"); strcpy(s.available, "v3.2");
    s.update = s.build = s.pack = ST_DONE;
    return s;
}
static void state_ready_can_play(void) {
    LState s = ready();
    CHECK(lstate_can_play(&s));
    CHECK(!strcmp(lstate_status_key(&s), "st_ready"));
}
static void state_offline_complete_can_play(void) {
    LState s = ready();
    s.online = 0; s.available[0] = 0; s.update = ST_SKIPPED;
    CHECK(lstate_can_play(&s));
    CHECK(!strcmp(lstate_status_key(&s), "st_offline"));
}
static void state_offline_pack_incomplete_plays_without_it(void) {
    LState s = ready();
    s.online = 0; s.pack_complete = 0; s.pack = ST_SKIPPED; s.update = ST_SKIPPED;
    CHECK(lstate_can_play(&s));
    CHECK(!strcmp(lstate_status_key(&s), "st_offline"));
}
static void state_pack_incomplete_online_cannot_play(void) {
    LState s = ready();
    s.pack_complete = 0; s.pack = ST_RUNNING;
    CHECK(!lstate_can_play(&s));
    CHECK(!strcmp(lstate_status_key(&s), "st_pack"));
}
static void state_pack_not_wanted_can_play(void) {
    LState s = ready();
    s.hd_pack_wanted = 0; s.pack_complete = 0; s.pack = ST_SKIPPED;
    CHECK(lstate_can_play(&s));
}
static void state_classic_does_not_wait_pack(void) {
    LState s = ready();                    /* classic: the caller sets hd_pack_wanted 0 */
    s.hd_pack_wanted = 0; s.pack_complete = 0; s.pack = ST_IDLE;
    CHECK(lstate_can_play(&s));
}
static void state_failed_cannot_play(void) {
    LState s = ready();
    s.build = ST_FAILED;
    CHECK(!lstate_can_play(&s));
    CHECK(!strcmp(lstate_status_key(&s), "st_failed"));
}
static void state_resume_after_kill(void) {
    LState s = ready();                    /* closed during the pack's download: started again */
    s.pack_complete = 0; s.pack = ST_IDLE;
    CHECK(!lstate_can_play(&s));
    CHECK(!strcmp(lstate_status_key(&s), "st_pack"));
}
static void state_update_pending_cannot_play(void) {
    LState s = ready();
    strcpy(s.available, "v3.3"); s.update = ST_IDLE;
    CHECK(!lstate_can_play(&s));
    CHECK(!strcmp(lstate_status_key(&s), "st_updating"));
    s.update_check = 0;                    /* the player turned updates off */
    CHECK(lstate_can_play(&s));
}
static void state_not_built_cannot_play(void) {
    LState s = ready();
    s.game_built = 0; s.build = ST_RUNNING;
    CHECK(!lstate_can_play(&s));
    CHECK(!strcmp(lstate_status_key(&s), "st_building"));
}
static void state_linux_no_updates(void) {
    LState s = ready();
    s.platform_updates = 0; s.pack_complete = 0; s.update = s.pack = ST_SKIPPED;
    CHECK(lstate_can_play(&s));
    CHECK(!strcmp(lstate_status_key(&s), "st_no_updates_here"));
}
static void state_unbuilt_update_cannot_play(void) {
    LState s = ready();                    /* update\download.zip still there: the build after it didn't finish */
    s.update_unbuilt = 1;
    CHECK(!lstate_can_play(&s));
    CHECK(!strcmp(lstate_status_key(&s), "st_building"));
    s.online = 0;                          /* offline changes nothing: the tree here is half new */
    CHECK(!lstate_can_play(&s));
}
static void state_rom_needed_cannot_play(void) {
    LState s = ready();                    /* first launch: no game, no ROM copy */
    s.game_built = 0; s.rom_needed = 1; s.build = ST_IDLE;
    CHECK(!lstate_can_play(&s));
    CHECK(!strcmp(lstate_status_key(&s), "st_rom"));
    s.online = 0;
    CHECK(!strcmp(lstate_status_key(&s), "st_rom"));
}
static void rom_kind_ok_extensions(void) {
    CHECK(rom_kind_ok("C:\\Jeux\\Dragon Quest IX (Europe).nds"));
    CHECK(rom_kind_ok("/home/deck/roms/dq9.NDS"));
    CHECK(rom_kind_ok("dq9.zip"));
    CHECK(!rom_kind_ok("dq9.7z"));
    CHECK(!rom_kind_ok("nds"));
    CHECK(!rom_kind_ok(""));
}
static void state_download_failed_modern_cannot_play(void) {
    /* recompsmith (2026-11-24): a download that broke off: Classic can play, Modern can't */
    LState s = ready();
    s.download_failed = 1; s.online = 0; s.pack_complete = 0; s.pack = ST_SKIPPED;
    CHECK(!lstate_can_play(&s));
    CHECK(!strcmp(lstate_status_key(&s), "st_failed"));
    s.modern = 0; s.hd_pack_wanted = 0;
    CHECK(lstate_can_play(&s));
}
/* the INSTALL bar covers every step of the run: full only when the last one is */
static void install_bar_covers_every_step(void) {
    float lo, hi;
    install_share(0, 1, 9, &lo, &hi);                          /* copy-over, then build and pack to come */
    CHECK(lo == 0 && hi > 0.05f && hi < 0.15f);
    float f = install_frac(lo, hi, 1);                         /* the copy-over done: not the bar's end */
    CHECK(f < 0.15f);
    install_share(f, 6, 3, &lo, &hi);                          /* the build, the pack to come */
    CHECK(lo == f && hi > 0.65f && hi < 0.75f);
    CHECK(install_frac(lo, hi, 1) < 1 && install_frac(lo, hi, 0.5f) > lo);
    install_share(hi, 3, 0, &lo, &hi);                         /* the pack: the last step */
    CHECK(hi == 1 && install_frac(lo, hi, 1) == 1);
    install_share(0, 6, 0, &lo, &hi);                          /* only a build */
    CHECK(lo == 0 && hi == 1);
    CHECK(install_frac(0.2f, 0.6f, -1) == 0.2f && install_frac(0.2f, 0.6f, 2) == 0.6f);
}
static void settings_decide_what_play_waits_for(void) {
    LState s; memset(&s, 0, sizeof s);
    lstate_settings("", &s);                                   /* no settings.ini: Modern, pack wanted, updates on */
    CHECK(s.modern && s.hd_pack_wanted && s.update_check);
    lstate_settings("mode = classic\n", &s);
    CHECK(!s.modern && !s.hd_pack_wanted && s.update_check);
    lstate_settings("mode = enhanced\r\nhd_pack_download = 0\r\nupdate_check = 0\r\n", &s);
    CHECK(s.modern && !s.hd_pack_wanted && !s.update_check);
    lstate_settings("mode = classic\nmode = enhanced\n", &s);  /* the last one wins, as in the game */
    CHECK(s.modern);
}
static void pack_outcome_rules(void) {
    PackStatus st; memset(&st, 0, sizeof st);
    CHECK(pack_outcome(0, 0, 0, &st) == PACK_MISSING);
    CHECK(pack_outcome(1, 0, 0, &st) == PACK_DONE);
    CHECK(pack_outcome(1, 2, 0, &st) == PACK_BUSY);
    CHECK(pack_outcome(1, 1, 0, &st) == PACK_FAILED);          /* no status line: an early local fault, not the network */
    parse_pack_status("failed offline: the HD pack can't be downloaded now", &st);
    CHECK(pack_outcome(1, 1, 1, &st) == PACK_NET);
    parse_pack_status("failed download: hd-pack-textures-1.zip: connection lost", &st);
    CHECK(pack_outcome(1, 1, 1, &st) == PACK_NET);
    parse_pack_status("download 5 10 1 2", &st);
    CHECK(pack_outcome(1, 1, 1, &st) == PACK_NET);
    parse_pack_status("failed install failed: undoing 3 move(s)", &st);
    CHECK(pack_outcome(1, 1, 1, &st) == PACK_FAILED);
    parse_pack_status("install 2 3", &st);
    CHECK(pack_outcome(1, 1, 1, &st) == PACK_FAILED);
}
static void update_never_goes_back(void) {
    CHECK(update_newer("v3.3", "v3.2"));
    CHECK(!update_newer("v3.2", "v3.3"));                      /* stable older than installed: no update */
    CHECK(update_newer("v3.10", "v3.9"));
    CHECK(!update_newer("v3.2", "v3.2"));
    CHECK(update_newer("v3.3-nightly2", "v3.3-nightly1"));
    CHECK(update_newer("v3.3", "v3.3-nightly9"));             /* the release after its nightlies */
    CHECK(!update_newer("garbage", "v3.2"));
}
static void branch_nightly_stays_main(void) {
    CHECK(!strcmp(lstate_branch("v3.3-nightly2"), "main"));
    CHECK(!strcmp(lstate_branch("v3.2"), "stable"));
    CHECK(!strcmp(lstate_branch(""), "stable"));
}

/* ---- news */
static void news_github_three_latest(void) {
    const char* json =
        "[{\"tag_name\":\"v3.3\",\"name\":\"draft\",\"draft\":true,\"prerelease\":false,\"published_at\":null},"
        "{\"tag_name\":\"v3.2\",\"name\":\"v3.2 : install fixed \\\"again\\\"\",\"draft\":false,\"prerelease\":false,\"published_at\":\"2026-11-22T10:00:00Z\",\"body\":\"x\"},"
        "{\"tag_name\":\"v3.2-rc1\",\"name\":\"rc\",\"draft\":false,\"prerelease\":true,\"published_at\":\"2026-11-22T09:00:00Z\"},"
        "{\"tag_name\":\"v3.1\",\"name\":\"v3.1\",\"draft\":false,\"prerelease\":false,\"published_at\":\"2026-11-21T10:00:00Z\"},"
        "{\"tag_name\":\"v3\",\"name\":\"v3 : HD pack\",\"draft\":false,\"prerelease\":false,\"published_at\":\"2026-11-20T10:00:00Z\"}]";
    NewsItem n[3];
    CHECK(news_from_github(json, n, 3) == 3);
    CHECK(!strcmp(n[0].tag, "v3.2") && !strcmp(n[0].date, "2026-11-22"));
    CHECK(!strcmp(n[0].title, "v3.2 : install fixed \"again\""));
    CHECK(!strcmp(n[1].tag, "v3.1") && !strcmp(n[2].tag, "v3"));
    CHECK(news_from_github("{\"message\":\"API rate limit exceeded\"}", n, 3) == 0);
    CHECK(news_from_github("", n, 3) == 0);
    CHECK(news_from_github("[[}", n, 3) == 0);                 /* no object: nothing, nothing read uninitialized */
    CHECK(news_from_github("[{\"tag_name\":\"../../x\",\"name\":\"x\",\"draft\":false,\"prerelease\":false}]", n, 3) == 0);   /* a tag that is no version: left out */
}
static void news_title_strips_prefix(void) {
    char t[160];
    CHECK(news_title_from_notes("\n\nDragon Quest IX Modern v3.2: installing works again.\n\nmore", t, sizeof t) == 1);
    CHECK(!strcmp(t, "Installing works again."));
    CHECK(news_title_from_notes("Dragon Quest IX Modern v3.2 : l'installation remarche.\r\n", t, sizeof t) == 1);
    CHECK(!strcmp(t, "L'installation remarche."));
    CHECK(news_title_from_notes("Just a line\n", t, sizeof t) == 1 && !strcmp(t, "Just a line"));
    CHECK(news_title_from_notes("   \n", t, sizeof t) == 0);
    /* a title starts with a capital, like the others (the notes' sentence goes on after the version) */
    CHECK(news_title_from_notes("Dragon Quest IX Modern v3.2 : l'installation remarche.\n", t, sizeof t) == 1 && !strcmp(t, "L'installation remarche."));
    CHECK(news_title_from_notes("Dragon Quest IX Modern v3: die Installation\n", t, sizeof t) == 1 && !strcmp(t, "Die Installation"));
    CHECK(news_title_from_notes("Dragon Quest IX Modern v3: \xC3\xA9t\xC3\xA9\n", t, sizeof t) == 1 && !strcmp(t, "\xC3\x89t\xC3\xA9"));
}
static void news_local_sorted_newest_first(void) {
    char d[1024]; snprintf(d, sizeof d, "%s/notes", g_tmp);
#ifdef _WIN32
    char cmd[1100]; snprintf(cmd, sizeof cmd, "mkdir \"%s\\fr\" 2>nul", d); system(cmd);
#else
    char cmd[1100]; snprintf(cmd, sizeof cmd, "mkdir -p '%s/fr'", d); if (system(cmd)) {}
#endif
    put("notes/v3.md", "Dragon Quest IX Modern v3: the HD pack.\n");
    put("notes/v3.1.md", "Dragon Quest IX Modern v3.1: fixes.\n");
    put("notes/v3.2.md", "Dragon Quest IX Modern v3.2: install again.\n");
    put("notes/fr/v3.2.md", "Dragon Quest IX Modern v3.2 : l'installation remarche.\n");
    put("notes/nightly.md", "not a version\n");
    NewsItem n[3];
    CHECK(news_local(d, "", n, 3) == 3);
    CHECK(!strcmp(n[0].tag, "v3.2") && !strcmp(n[1].tag, "v3.1") && !strcmp(n[2].tag, "v3"));
    CHECK(!strcmp(n[0].title, "Install again.") && n[0].date[0] == 0);
    CHECK(news_local(d, "fr", n, 3) == 3);             /* the language's notes, English where it has none */
    CHECK(!strcmp(n[0].title, "L'installation remarche.") && !strcmp(n[1].title, "Fixes."));
}

/* ---- tools and their progress */
static void progress_build_line(void) {
    float f = -1; char t[96];
    CHECK(parse_build_line("progress 0.3 Compiling (x)\r\n", &f, t, sizeof t) == 1);
    CHECK(f > 0.29f && f < 0.31f && !strcmp(t, "Compiling (x)"));
    CHECK(parse_build_line("   gcc -O2 ...", &f, t, sizeof t) == 0);
    CHECK(parse_build_line("progress x", &f, t, sizeof t) == 0);
}
static void progress_build_failed_line(void) {
    float f; char t[96];
    CHECK(parse_build_line("BUILD FAILED (build tools): make missing\n", &f, t, sizeof t) == 2);
    CHECK(!strcmp(t, "BUILD FAILED (build tools): make missing"));
}
static void pack_status_download(void) {
    PackStatus s;
    CHECK(parse_pack_status("download 1048576 4194304 2 3\n", &s) == 1);
    CHECK(s.kind == 'd' && s.done == 1048576 && s.total == 4194304 && s.i == 2 && s.n == 3);
    CHECK(parse_pack_status("install 1 4", &s) == 1 && s.kind == 'i' && s.done == 1 && s.total == 4);
    CHECK(parse_pack_status("done", &s) == 1 && s.kind == 'D');
    CHECK(parse_pack_status("uptodate\r\n", &s) == 1 && s.kind == 'u');
}
static void pack_status_failed_reason(void) {
    PackStatus s;
    CHECK(parse_pack_status("failed hd-pack-textures-1.zip: wrong SHA-256\n", &s) == 1);
    CHECK(s.kind == 'f' && !strcmp(s.reason, "hd-pack-textures-1.zip: wrong SHA-256"));
}
static void pack_status_garbage_is_zero(void) {
    PackStatus s;
    CHECK(parse_pack_status("", &s) == 0);
    CHECK(parse_pack_status("working 50 100", &s) == 0);
    CHECK(parse_pack_status("download a b", &s) == 0);
    CHECK(parse_pack_status("download 5 2 1 1", &s) == 0);          /* done > total: not believed */
}
static void proc_runs_echo(void) {
#ifdef _WIN32
    const char* argv[] = {"cmd.exe", "/c", "echo a& echo %LNCH_T%& exit 3", NULL};
#else
    const char* argv[] = {"/bin/sh", "-c", "echo a; echo $LNCH_T; exit 3", NULL};
#endif
    const char* env[] = {"LNCH_T=b", NULL};
    LProc* p = proc_start(argv, env, g_tmp);
    CHECK(p);
    char lines[4][64]; int n = 0, r; char l[64];
    while ((r = proc_read_line(p, l, sizeof l)) != -1)
        if (r == 1 && n < 4) { size_t k = strlen(l); while (k && (l[k-1] == ' ' || l[k-1] == '\r')) l[--k] = 0; strcpy(lines[n++], l); }
    int code = proc_exit_code(p);
    proc_free(p);
    CHECK(n == 2 && !strcmp(lines[0], "a") && !strcmp(lines[1], "b"));
    CHECK(code == 3);
}

/* ---- texts */
static void text_every_key_every_language(void) {
    for (int l = 1; l <= 5; l++) {
        tr_set_language(l);
        for (int i = 0; tr_key(i); i++) {
            if (!tr_has(i, l)) printf("  language %d: %s missing\n", l, tr_key(i));
            CHECK(tr_has(i, l));
        }
    }
    tr_set_language(2);
    CHECK(!strcmp(tr("play"), "JOUER") && !strcmp(tr("st_ready"), "Tout est à jour, prêt à jouer !"));
    tr_set_language(3);
    CHECK(!strcmp(tr("play"), "SPIELEN"));
    tr_set_language(1);
    CHECK(!strcmp(tr("play"), "PLAY"));
}
static void text_build_steps_translated(void) {
    /* the build's step titles (tools/build-game.sh step, BUILD.bat progress) come in English */
    static const char* const STEPS[] = {"Checking the ROM", "Extracting the game's code", "Recompiling it to C (about a minute)",
        "Compiling (the first time takes a while; later builds only redo what changed)", "Putting the game folder together",
        "Checking your ROM", "Installing the build tools (MSYS2, one time)", "Checking the build tools",
        "Checking the build tools (the first time may download some)", "Done", NULL};
    for (int l = 2; l <= 5; l++) {
        tr_set_language(l);
        for (int i = 0; STEPS[i]; i++) {
            if (!strcmp(tr(STEPS[i]), STEPS[i])) printf("  language %d: step \"%s\" not translated\n", l, STEPS[i]);
            CHECK(strcmp(tr(STEPS[i]), STEPS[i]) != 0);
        }
    }
    tr_set_language(1);
    CHECK(!strcmp(tr("Checking the ROM"), "Checking the ROM"));
}
static void text_only_drawable_characters(void) {
    /* Nunito and Marcellus have no arrows or triangles: they would come out blank */
    for (int l = 1; l <= 5; l++) {
        tr_set_language(l);
        for (int i = 0; tr_key(i); i++) {
            const char* t = tr(tr_key(i));
            int bad = strstr(t, "\xE2\x86\x92") || strstr(t, "\xE2\x96\xBE");
            if (bad) printf("  language %d: %s has an arrow the fonts lack\n", l, tr_key(i));
            CHECK(!bad);
        }
    }
}
static void text_unknown_key_returns_key(void) {
    tr_set_language(1);
    CHECK(!strcmp(tr("no_such_key"), "no_such_key"));
    tr_set_language(9);                                 /* out of range: English */
    CHECK(!strcmp(tr("play"), "PLAY"));
}

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: t_launcher <temp folder>\n"); return 2; }
    g_tmp = argv[1];
    RUN(ini_set_keeps_comments_and_crlf); RUN(ini_set_appends_missing_key); RUN(ini_set_does_not_touch_longer_key);
    RUN(ini_get_last_of_duplicates); RUN(settings_update_new_file_has_version); RUN(settings_update_keeps_others); RUN(settings_update_refuses_unreadable); RUN(ini_ignores_line_without_equals); RUN(ini_load_missing_is_empty); RUN(ini_save_then_load);
    RUN(state_ready_can_play); RUN(state_offline_complete_can_play); RUN(state_offline_pack_incomplete_plays_without_it);
    RUN(state_pack_incomplete_online_cannot_play); RUN(state_pack_not_wanted_can_play); RUN(state_classic_does_not_wait_pack);
    RUN(state_failed_cannot_play); RUN(state_resume_after_kill); RUN(state_update_pending_cannot_play);
    RUN(state_not_built_cannot_play); RUN(state_linux_no_updates); RUN(branch_nightly_stays_main); RUN(state_unbuilt_update_cannot_play); RUN(state_rom_needed_cannot_play); RUN(state_download_failed_modern_cannot_play); RUN(settings_decide_what_play_waits_for); RUN(install_bar_covers_every_step); RUN(pack_outcome_rules); RUN(update_never_goes_back); RUN(rom_kind_ok_extensions);
    RUN(news_github_three_latest); RUN(news_title_strips_prefix); RUN(news_local_sorted_newest_first);
    RUN(progress_build_line); RUN(progress_build_failed_line); RUN(pack_status_download);
    RUN(pack_status_failed_reason); RUN(pack_status_garbage_is_zero); RUN(proc_runs_echo);
    RUN(text_every_key_every_language); RUN(text_unknown_key_returns_key); RUN(text_build_steps_translated); RUN(text_only_drawable_characters);
    printf("%d/%d passed\n", runs - fails, runs);
    return fails ? 1 : 0;
}
