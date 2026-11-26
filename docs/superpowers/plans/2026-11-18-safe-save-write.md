# Safe save write Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `backup_flush()` (`runtime/card.c`) must never leave `dqix.sav` empty or cut short: write a temporary file, check every write, then replace the save in one step.

**Architecture:** A small new module `runtime/savefile.c` holds one function that writes a buffer to a path safely (temp file, checked writes, flush to disk, atomic replace). `backup_flush()` calls it and keeps `g_backup_dirty` set when it fails. Kept apart from `card.c` so a gcc test can build it without the emulator, the same way `tests/hdpack/t_rank.c` builds `runtime/hdpack.c`.

**Tech Stack:** C (C11), Win32 `MoveFileExA` + `_commit` (MinGW), POSIX `rename` + `fsync`. Tests: gcc on Linux, x86_64-w64-mingw32-gcc + Wine for the Windows path, in the cloud session only.

**Spec:** the task from the coordinator session (ponytail audit 2026-11-18, finding 1) — no separate design doc: small change.

## Global Constraints

- Save file format unchanged; same file name; same `backup_init` read path.
- Keep `backup_copy()` (save-backups copies), its 10-second rule, and the existing `LOG_WARN` on failure.
- `g_backup_dirty` goes back to 0 only when the new save is fully on disk under its real name.
- `void backup_flush(void)` signature unchanged (9 callers: `card.c:318` backup_xfer, `autosave.c:85`, `cpu_bios.c:396`, `frontend_sdl.c:6016` and `:6231`, `hosting.c:40`, `main.c:68`, `session.c:1493`; none reads a result).
- No file touched on recompsmith's PC; tests run in the cloud session (`docs/TESTS-CLOUD.md`).
- No shared file touched (`card.c`, new `savefile.*`, `tests/` belong to Dev; `build.mk` picks `runtime/*.c` by wildcard, so it needs no change).

## Review Focus

1. Temp file cannot be created (folder read-only, disk full) → old `dqix.sav` byte-identical, function returns error, dirty stays 1, next flush retries.
2. A stale `dqix.sav.tmp` left by an earlier crash → overwritten, final save correct, no `.tmp` left.
3. `dqix.sav` does not exist yet (first save) → created.
4. Windows replace over an existing file (the `rename` of MSVCRT fails when the target exists) → must use `MoveFileExA(..., MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`; tested under Wine.
5. Cost: `backup_flush` runs at the end of every SPI command, so a game save may write the file hundreds of times; flushing to disk each time could stall the game (Task 3 measures it).

---

### Task 1: `save_file_atomic` and its test

**Files:**
- Create: `runtime/savefile.h`, `runtime/savefile.c`
- Create: `tests/savefile/t_savefile.c`, `tests/savefile/run.sh`

**Interfaces:**
- Produces: `int save_file_atomic(const char* path, const void* data, size_t n);` — returns 0 on success, -1 on any failure; on failure the file at `path` is unchanged and `<path>.tmp` removed when possible.

- [ ] **Step 1: Write the failing test** `tests/savefile/t_savefile.c` (same PASS/FAIL style as `tests/hdpack/t_rank.c`), working in a folder given as `argv[1]`:
  - `new file`: path absent → returns 0, content equals the 64 KB buffer, `<path>.tmp` absent.
  - `replace`: path holds "old" → returns 0, content is the new buffer (this is the Windows replace case under Wine).
  - `stale tmp`: `<path>.tmp` holds junk before the call → returns 0, final content correct, no `.tmp`.
  - `tmp cannot be made`: `<path>.tmp` is a directory → returns -1, path still holds "old" byte for byte.
  - `folder missing`: path inside a non-existent folder → returns -1, nothing created.
- [ ] **Step 2: Write `tests/savefile/run.sh`** — refuses to run outside Linux (copy of the guard in `tests/hdpack/run.sh`), `mktemp -d` under `/tmp`, builds with `gcc -Wall -Wextra -I runtime` and runs; then builds with `x86_64-w64-mingw32-gcc` and runs under Wine. Exit 1 on any FAIL.
- [ ] **Step 3: Run it (cloud) to see it fail** — Expected: build error, `save_file_atomic` undefined.
- [ ] **Step 4: Implement `save_file_atomic`** in `runtime/savefile.c`: `fopen(tmp,"wb")`, `fwrite` count checked, `fflush`, `_commit(_fileno(f))` on `_WIN32` / `fsync(fileno(f))` elsewhere, `fclose` checked; then `MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)` on `_WIN32`, `rename` elsewhere. Any failure: close, `remove(tmp)`, return -1.
- [ ] **Step 5: Run `sh tests/savefile/run.sh` (cloud)** — Expected: all PASS, both gcc and Wine, exit 0.
- [ ] **Step 6: Commit** `Add save_file_atomic: write to a temp file, then replace`.

### Task 2: `backup_flush` uses it

**Files:**
- Modify: `runtime/card.c:198-208` (`backup_flush`)

**Interfaces:**
- Consumes: `save_file_atomic` (Task 1).

- [ ] **Step 1:** Replace the `fopen/fwrite/fclose` in `backup_flush` by `if (save_file_atomic(g_backup_path, g_backup, g_backup_size) != 0) { LOG_WARN("save: cannot write %s\n", g_backup_path); return; }`, then `g_backup_dirty = 0`. `backup_copy()` call and `g_last_write` stay as they are (before the write).
- [ ] **Step 2: Cloud test** per `docs/TESTS-CLOUD.md`: build EU + USA, smoke run with the test save (`tests/smoke/run.py`), `tests/semantics`, plus `tests/savefile/run.sh`. Expected: PASS; after the run, `dqix.sav` loads again and no `dqix.sav.tmp` is left.
- [ ] **Step 3: Commit** `Save: never leave dqix.sav empty or cut short`.

### Task 3: measure the cost of a game save (decides if more is needed)

- [ ] **Step 1 (cloud):** in the smoke run, make the game save once (test input that saves at the church, or autosave `save_now`), with a temporary `LOG_INFO` counter in `backup_flush` (not committed): number of calls and total time spent in `save_file_atomic` for that save.
- [ ] **Step 2:** If the extra time is under ~200 ms per game save, stop here. Otherwise (decision for recompsmith, not built without his OK): keep the atomic write on every flush but do the disk flush (`_commit`/`fsync`) only once the game has stopped writing (e.g. a later flush from a frame hook) — written as a follow-up plan.

### Task 4: review and PR

- [ ] `pr-review-toolkit:code-simplifier`, `dqix-relecteur`, `/pr-review-toolkit:review-pr`, `/ponytail-review`; fix what is confirmed.
- [ ] PR `dev/safe-save-write` → `main`, English; cloud test PASS on the exact last commit (`tests/safe-save-write` report branch).
