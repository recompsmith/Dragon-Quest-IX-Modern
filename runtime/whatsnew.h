/* Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later */
/* What's new: after an update, the release notes of the new version, shown
 * once at start in the journal's panel (alone), in the game's language; and
 * again from Help > What's new.
 *
 * The notes are the project's release-notes/vX.md (English, the GitHub
 * release's) and release-notes/<fr|de|it|es>/vX.md, copied by the build to
 * game/notes/. A language without its file shows the English one. Only the
 * part above a line "<!-- in-game notes end here -->" is shown (without
 * one: above "**How to install**"). The version that ran last is kept in
 * settings.ini (last_version), whichever way the update came (the game's own
 * updater, RecompLauncher, a new download, git pull). (whatsnew.c) */
#pragma once

/* At start, once settings.ini was read: `last` its last_version ("" when
 * there was none), `had_settings` whether there was a settings.ini at all (a
 * new install: nothing to show). 1 when the notes are to be shown. */
int whatsnew_start(const char* last, int had_settings);
int whatsnew_pending(void);

/* Shows them now (the journal's panel): after an update the versions since
 * the last one run (at most a few, newest first), else this version's. */
void whatsnew_show(void);
