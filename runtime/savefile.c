/* Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later */
/* Safe write of the save file (see savefile.h). */
#include <stdio.h>
#include "savefile.h"
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define to_disk(f) _commit(_fileno(f))
#define replace(from, to) (MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : -1)
#else
#include <errno.h>
#include <unistd.h>
/* EINVAL: a file system that cannot sync (some shared or FUSE folders); the write itself went through */
#define to_disk(f) (fsync(fileno(f)) == 0 || errno == EINVAL ? 0 : -1)
/* shortcut: the folder itself is not fsynced after the rename, upgrade if a save is ever lost to a power cut on Linux */
#define replace(from, to) rename(from, to)
#endif

int save_file_atomic(const char* path, const void* data, size_t n) {
    char tmp[1100];   /* g_backup_path (card.c) is 1024 */
    if (snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp) return -1;
    FILE* f = fopen(tmp, "wb");
    if (!f) return -1;
    int ok = fwrite(data, 1, n, f) == n && fflush(f) == 0 && to_disk(f) == 0;
    if (fclose(f) != 0) ok = 0;
    if (!ok) { remove(tmp); return -1; }
    return replace(tmp, path) == 0 ? 0 : -1;   /* a whole .tmp is kept if only the replace failed */
}
