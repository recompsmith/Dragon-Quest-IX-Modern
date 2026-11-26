/* Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later */
/* Probe for tests/hdpack/t_download.py: copied next to hd-pack.exe in a game
 * folder, exits 1 while hdpack_running() (runtime/hdpack.c) sees hd-pack.exe
 * at work for that folder, 0 otherwise. */
#include "hdpack.h"
int main(void) { return hdpack_running(); }
