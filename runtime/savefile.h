/* Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SAVEFILE_H
#define SAVEFILE_H
#include <stddef.h>

/* Writes n bytes to path without ever leaving it empty or cut short: they go
 * to <path>.tmp, are flushed to the disk, then replace path in one step.
 * 0 on success; -1 on failure, path then unchanged (a whole <path>.tmp is
 * left behind if only the replace failed). */
int save_file_atomic(const char* path, const void* data, size_t n);

#endif
