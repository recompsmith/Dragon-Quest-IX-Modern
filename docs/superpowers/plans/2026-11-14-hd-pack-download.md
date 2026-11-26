# HD Pack Download Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every player gets recompsmith's HD pack (FatalPixels textures + 6 SeedVR2 movies), downloaded by the game in the background, with no stutter.

**Architecture:** On recompsmith's PC, dev tools extract the movies from the ROM, remake them with SeedVR2, and bundle textures + movies into stored zips plus a manifest, attached to a GitHub release `hd-pack-1`. At the player's, the game starts a separate low-priority program `hd-pack.exe` (like `textures-hd.exe` today): it downloads, checks, installs into `textures/hd-pack/` and `movies/`, sets the Real-ESRGAN textures aside in `hd-pack-ancien/` (it never deletes a folder), and writes `hd-pack-progress.txt`, which the game's existing bottom-left bar reads. The game process itself only spawns, reads a small text file twice a second, and reloads at the end, so it never stutters.

**Tech Stack:** C (MSYS2 UCRT64 gcc, WinHTTP, BCrypt SHA-256), Python 3 (numpy, Pillow, ffmpeg) for dev tools, the local SeedVR2 service (`claude-service\upscale-video.bat`).

**Spec:** `docs/superpowers/specs/2026-11-14-hd-pack-download-design.md`

## Global Constraints

- Never commit textures, movies, ROM data or packs (CLAUDE.md): they only go to the GitHub release.
- The game must never stutter: no download, hashing, unzip or file move inside `dqix.exe`.
- Manifest URL: `https://github.com/recompsmith/Dragon-Quest-IX-Modern/releases/download/hd-pack-1/hd-pack.txt`.
- Release `hd-pack-1` is published once, after all 6 movies are done; publishing it needs recompsmith's explicit go-ahead.
- Every zip attached to the release is under 2 GB (use 1.9 GB as the cut).
- Pack folders: textures go to `game/textures/hd-pack/`, movies to `game/movies/` (existing movies copied to `game/movies-avant-hd-pack/` first).
- Texture priority: personal folders under `textures/` > `textures/hd-pack/` > `textures/hd/`.
- Real-ESRGAN set-aside (was "cleanup"; changed after the 2026-11-14 incident): **`hd-pack.exe` never deletes a folder.** It moves (one same-volume `MoveFileExW` rename each, no recursion) `textures/hd/`, `textures-methode5/` and `textures-avant-*/` into `game/hd-pack-ancien/`, only when `textures/hd-style.txt` does not contain `service-` (missing file counts as Real-ESRGAN only if `textures-hd-outil/` exists); never before the pack is installed and checked. The player empties `hd-pack-ancien/` if they want; the log and README say so.
- Deletion safety (all C code of this plan, hard rules):
  - No recursive delete anywhere: no `SHFileOperation`, `IFileOperation`, `rmdir /s`, `RemoveDirectory` on a non-empty folder, no tree walk that deletes.
  - The only `DeleteFileW` calls are on files the program itself wrote, named exactly (the zips from the manifest, the progress file, the lock). The only `RemoveDirectoryW` calls are on folders it created, emptied first, from the deepest up (`RemoveDirectoryW` fails on a non-empty folder: that failure is logged and left alone).
  - Before any move or delete, the path is resolved with `GetFullPathNameW`, must lie strictly under the game folder, contain no `..`, and neither it nor any parent up to the game folder may carry `FILE_ATTRIBUTE_REPARSE_POINT` (junction, symlink, mount point). Any doubt: skip, log, go on.
  - Wide-char (`W`) Windows APIs only, so a non-ASCII game path can't be misread.
- Tests of any code that moves, overwrites or deletes files never run on recompsmith's PC, not even in a temp folder: they run in a Claude Code cloud session (Linux, `x86_64-w64-mingw32-gcc` + Wine). No test ever creates a junction or symlink pointing outside its own temp folder, and no test disables a safety check ("mutation test") outside the cloud.
- Setting `hd_pack_download = 1` in `settings.ini` (0 = never), menu item in Graphics.
- Credits in README and release notes: FatalPixels (twittman, CC-BY-NC-SA-4.0), UltraSharp (Kim2091), SeedVR2 (ByteDance); non-commercial use only.
- GPU jobs on recompsmith's PC go through `claude-service\guard.bat` (16 GB RAM, 13 GB VRAM, one GPU job at a time). Never launch the game on his PC: he checks in game himself.
- Shared files touched (`runtime/gl_render.c`, `runtime/frontend_sdl.c`, `runtime/menu_win32.c`, `tools/build-game.sh`): tell the coordinator the branch `graph/hd-pack-download` touches them.
- Commit messages in English, ending with the `Co-Authored-By` line.

## Review Focus

1. Download cut off mid-zip (game closed, Wi-Fi drop): the next launch resumes with `Range`, never restarts from 0, never installs a partial zip. (Task 5 test `resume_after_cut`.)
2. Disk full or a write failing during extraction: nothing is moved into `textures/hd-pack/`, old files stay, Real-ESRGAN textures are not set aside. (Task 4 test `install_fails_cleanly`.)
3. Game started twice, or started while `hd-pack.exe` already runs: a second `hd-pack.exe` exits at once (lock file). (Task 4 test `second_instance_exits`.)
4. Offline at first launch: `hd-pack.exe` exits quietly, no progress file left in `working`, the bar never shows. (Task 5 test `offline_leaves_no_bar`.)
5. Pack already installed, same manifest version: no download at all on later launches. (Task 4 test `installed_is_noop`.)

---

### Task 1: Extract the movies from the ROM

**Files:**
- Create: `tools/rom_files.py`
- Test: `tests/hdpack/test_rom_files.py`

**Interfaces:**
- Produces: `rom_files.py <rom.nds> <out_dir> [glob, default data/movie/*.mods]` and `def list_files(rom_path: str) -> dict[str, tuple[int, int]]` (path inside the ROM -> (offset, size)), from the NDS header's FNT (0x40) / FAT (0x48).

- [ ] **Step 1: Write the test**: with `DQIX_ROM` set (skip if unset), `list_files` contains exactly 6 `data/movie/*.mods` whose stems are `TD_nouhin_DS_form`, `to_be_conteinued_A`, `to_be_conteinued_B`, `xenlon_DR_henshu_DS`, `xenlon_OP_A`, `xenlon_OP_B`; extracting one writes a file of the listed size starting with the Mobiclip magic `MO`.
- [ ] **Step 2: Run** `DQIX_ROM=J:/DQIX-Enhanced/rom/dqix_usa.nds python tests/hdpack/test_rom_files.py`: fails (no module).
- [ ] **Step 3: Implement** `tools/rom_files.py` (FNT directory walk, FAT offsets).
- [ ] **Step 4: Run** the test: passes.
- [ ] **Step 5: Commit** `tools: extract files from the ROM (the movies for the HD pack)`.

### Task 2: Remake the movies with SeedVR2

**Files:**
- Modify: `tools/movie_frames.py` (add the shared writer), `tools/upscale_movies.py` (use it)
- Create: `tools/seedvr2_movies.py`
- Test: `tests/hdpack/test_seedvr2_movies.py`

**Interfaces:**
- Consumes: Task 1 output (`.mods` files), `movie_frames.frames(path)`.
- Produces: `def write_movie(out_dir: str, name: str, src_frames: Iterable[np.ndarray], hd_frames: Iterable[np.ndarray]) -> int` in `movie_frames.py` (MPEG-1 1024×768, `-q:v 3 -g 24 -bf 0`, 24 fps, `.sig` = "DQMS" + u32 count + 16×12 RGB block means of the source frames; returns frame count; same bytes as `upscale_movies.py` writes today); `seedvr2_movies.py <mods_dir> <work_dir> <out_dir> [--max-frames N] [names...]`: decodes frames to `<work>/<name>/src/%05d.png`, runs `upscale-video.bat -i src -o hd --height 768` (resumes by itself), then `write_movie`, then `split_movies.py` into `<out_dir>` (25 MB parts).

- [ ] **Step 1: Write the test**: `write_movie` on 30 synthetic 256×192 frames + 30 1024×768 frames gives a `.sig` of 4 + 4 + 30×576 bytes with count 30, and `ffprobe` reports 1024×768 and 30 frames for the `.mpg`.
- [ ] **Step 2: Run** `python tests/hdpack/test_seedvr2_movies.py`: fails.
- [ ] **Step 3: Implement** `write_movie`, switch `upscale_movies.py` to it, write `seedvr2_movies.py`.
- [ ] **Step 4: Run** the test: passes. Then a real short run: `python tools/seedvr2_movies.py <mods> <work> <out> --max-frames 42 xenlon_OP_A` (2 SeedVR2 passes through the guard): `.sig` count 42, `.mpg` 1024×768.
- [ ] **Step 5: Commit** `tools: remake the HD movies with SeedVR2`.
- [ ] **Step 6: Full run in the background** (hours): all 6 movies into `J:\DQIX-HD-staging\movies`. Report the guard's peak lines. recompsmith watches them in game himself (copy into his `game\movies` only with his go-ahead, old ones backed up to `J:\DQIX-Backups`).

### Task 3: Build the pack (zips + manifest)

**Files:**
- Create: `tools/make_hd_pack.py`
- Test: `tests/hdpack/test_make_hd_pack.py`

**Interfaces:**
- Produces: `make_hd_pack.py <textures_dir> <movies_dir> <out_dir> [--version 1] [--max-gb 1.9]` writing stored (no compression) zips `hd-pack-textures-N.zip` (entries `textures/<16 hex>.png`) and `hd-pack-movies.zip` (entries `movies/<file>`), and `hd-pack.txt`:
  ```
  hd-pack 1
  zip <file name> <size in bytes> <sha256 hex>
  ...
  ```
  Only `<16 hex>.png` files are taken from `textures_dir`; `.mpg`/`.sig` from `movies_dir`.

- [ ] **Step 1: Write the test**: on a temp dir with 5 PNGs, 1 stray file, 2 movie files and `--max-gb` tiny enough to force 2 texture zips: manifest first line `hd-pack 1`, 3 `zip` lines whose sizes and SHA-256 match the files, the stray file is in no zip, every zip entry has `compress_type == ZIP_STORED`.
- [ ] **Step 2: Run** `python tests/hdpack/test_make_hd_pack.py`: fails.
- [ ] **Step 3: Implement** `tools/make_hd_pack.py`.
- [ ] **Step 4: Run** the test: passes.
- [ ] **Step 5: Commit** `tools: make the HD pack's zips and manifest`.

### Task 4: hd-pack.exe: check, install, set aside (local source)

> Redone after the 2026-11-14 incident (a first version's test followed a junction to `C:\` and deleted files across the PC). Its old code is kept, unused, in `.superpowers/sdd/2026-11-14-hd-pack-download/task-4-old-code/`. Follow "Deletion safety" in Global Constraints to the letter. **Built and tested in a cloud session only.**

**Files:**
- Create: `tools/hd-pack.c`, `tools/hdpack_core.h` (the pure parts, included by `hd-pack.c` and the test)
- Test: `tests/hdpack/t_hdpack.c`, `tests/hdpack/run.sh`

**Interfaces:**
- Consumes: Task 3's manifest and zip format.
- Produces (in `hdpack_core.h`):
  - `typedef struct { char name[128]; long long size; char sha[65]; } HpZip;`
  - `int hp_parse_manifest(const char* text, int* version, HpZip* zips, int max)` (count, or -1 if not `hd-pack <n>`)
  - `int hp_sha256_file(const char* path, char out[65])` (BCrypt)
  - `int hp_unzip_stored(const char* zip, const char* dest)` (local headers, method 0 only; creates folders; 0 ok)
  - `int hp_safe_path(const wchar_t* game, const wchar_t* path)` (1 only if `path` is strictly under `game`, has no `..`, and no reparse point on it or any parent up to `game`)
  - `int hp_realesrgan_pack(const char* game)` (1 if the set-aside rule of Global Constraints applies)
  - `int hp_set_aside(const char* game)` (renames each Real-ESRGAN folder into `game/hd-pack-ancien/`, adding `-2`, `-3`... if the name exists; never deletes; 0 ok)
  - `int hp_install(const char* game, const char* stage)` (move `stage/textures/*` to `game/textures/hd-pack/`, copy `game/movies/*` to `game/movies-avant-hd-pack/` then move `stage/movies/*` in, write `game/textures/hd-pack/hd-pack.txt` last; 0 ok, nothing half-moved on error)
- `hd-pack.exe` flow: lock `game/hd-pack.lock` (like `textures-hd.c`, exit if held); IDLE priority; read manifest (env `HDPACK_SOURCE=<dir>` = take manifest and zips from a local folder, for tests; else Task 5); exit if `textures/hd-pack/hd-pack.txt` has the same version; each zip into `game/hd-pack-tmp/` (outside `textures/`), check size + SHA-256, unzip to `hd-pack-tmp/stage/`; `hp_install`; then `hp_set_aside` if `hp_realesrgan_pack`; then delete the verified zips by their manifest names and remove the now-empty `hd-pack-tmp/stage/...` folders bottom-up (non-recursive); progress file `game/hd-pack-progress.txt`: `working <percent> 100` then `done 0 100`; log `game/hd-pack.log`.

- [ ] **Step 1: Write the tests** in `t_hdpack.c`: manifest parse (good, bad header, CRLF); SHA-256 of `"abc"` = `ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`; unzip of a zip made by Task 3; `install_fails_cleanly` (a stage file that can't be moved: `textures/hd-pack/` unchanged, `textures/hd/` kept); `installed_is_noop` (second run with same version touches nothing, no progress file); `second_instance_exits`; set-aside rule cases (`ia-seule 1` → 1; `ia-seule 1 service-auto 1` → 0; no file + `textures-hd-outil/` → 1; no file, no tool → 0); `set_aside_moves_not_deletes` (files of `textures/hd/` are found intact in `hd-pack-ancien/`); `hp_safe_path` refuses `..`, a path outside the game folder, and a game folder holding a junction to a sibling temp folder (the junction is created inside the test's own temp folder, pointing only to another folder in that same temp folder; in the cloud under Wine, `mklink /J` or `DeviceIoControl(FSCTL_SET_REPARSE_POINT)`; if Wine can't make junctions, that case is skipped and said so); `nothing_outside_game_changes` (a sentinel folder next to the game folder is byte-identical after a full run). `run.sh` cross-builds with `x86_64-w64-mingw32-gcc -O2 -I tools -o t_hdpack.exe tests/hdpack/t_hdpack.c -lbcrypt` and runs it with `wine`, refusing to run on Windows (`uname` must say Linux), plus an end-to-end run of `hd-pack.exe` with `HDPACK_SOURCE` on a temp game folder.
- [ ] **Step 2: Run** `sh tests/hdpack/run.sh`: fails.
- [ ] **Step 3: Implement** `hdpack_core.h` and `tools/hd-pack.c`.
- [ ] **Step 4: Run** `sh tests/hdpack/run.sh`: all pass.
- [ ] **Step 5: Commit** `hd-pack: install the HD pack from its zips (checked, all or nothing)`.

### Task 5: hd-pack.exe: download with resume

**Files:**
- Modify: `tools/hd-pack.c`
- Test: `tests/hdpack/t_download.py` (local HTTPS-less test server: `HDPACK_URL` override accepts `http://127.0.0.1:<port>/`), `tests/hdpack/run.sh`

**Interfaces:**
- Produces: `static int hp_fetch(const wchar_t* host, int port, int secure, const wchar_t* path, const char* dest, long long have, long long size)` (WinHTTP, `Range: bytes=<have>-` when `have > 0`, appends to `<dest>.part`, renames when complete, updates the progress file at most twice a second); manifest from the Global Constraints URL, overridable with `HDPACK_URL` (base URL of the manifest and zips).

- [ ] **Step 1: Write the tests**: Python serves a temp pack with `http.server` (Range support); `resume_after_cut`: server closes after 1 MB on the first request, second `hd-pack.exe` run sends a `Range` header and the installed files match; `offline_leaves_no_bar`: `HDPACK_URL=http://127.0.0.1:9/` → exit 0, no `hd-pack-progress.txt` or one not saying `working`.
- [ ] **Step 2: Run**: fails.
- [ ] **Step 3: Implement** `hp_fetch` and the URL handling.
- [ ] **Step 4: Run** `sh tests/hdpack/run.sh` and `python tests/hdpack/t_download.py`: pass.
- [ ] **Step 5: Commit** `hd-pack: download the pack in the background, resuming where it stopped`.

### Task 6: The game: start it, show it, use it

**Files:**
- Create: `runtime/hdpack.c`, `runtime/hdpack.h`
- Modify: `runtime/gl_render.c` (`pack_scan` order, around line 1146), `runtime/frontend_sdl.c` (settings ~3311/3401, start ~3619, `hdbar_update` ~5474, menu ~5742/6380), `runtime/menu.h` (`MI_HDPACK`), `runtime/menu_win32.c` (~135), `runtime/hdmovie.c` (look again after install), `build.mk` if runtime sources are listed there
- Test: `tests/hdpack/t_rank.c` (in `run.sh`)

**Interfaces:**
- Produces: `int hdpack_dir_rank(const char* top_folder)` (0 personal folders, 1 `hd-pack`, 2 `hd`); `void hdpack_start(int enabled)` (spawns `hd-pack.exe` detached if enabled and present, Windows only); `void hdmovie_rescan(void)` in `hdmovie.c`.
- `pack_scan` scans top-level folders of `textures/` in rank order so the first found wins as the rules say. `hdbar_update` also reads `hd-pack-progress.txt` (text `T("HD pack: %d%%")`, then `T("HD pack ready")`; on `done`: remove the file, `gl_reload_texture_pack()` if `g_texpack`, `hdmovie_rescan()`). Setting `hd_pack_download` (default 1), menu item `L"&Download the HD pack (textures and movies)"` toggling it and starting `hd-pack.exe` when turned on. New strings added to `lang/*.txt` the way `T("HD textures: %d%%")` is.

- [ ] **Step 1: Write the test** `t_rank.c`: `hdpack_dir_rank("rom") == 0`, `("hd-pack") == 1`, `("hd") == 2`, `("HD-PACK") == 1`.
- [ ] **Step 2: Run** `sh tests/hdpack/run.sh`: fails.
- [ ] **Step 3: Implement** the above.
- [ ] **Step 4: Run** `sh tests/hdpack/run.sh`: passes. Then `cmd /c "set DQIX_NOPAUSE=1&& J:\DQIX-wt\graph-hd-pack-download\BUILD.bat"` (subagent `dqix-build-check`): exit 0. Do not start the game.
- [ ] **Step 5: Commit** `HD pack: the game downloads it in the background (bottom-left bar), then uses it`.

### Task 7: Build, docs, credits

**Files:**
- Modify: `tools/build-game.sh` (build `hd-pack.exe` on MINGW/MSYS every time, like `textures-hd`, not gated by `textures-hd-outil`), `README.md` (HD section: pack downloaded by the game; credits), `docs/roles/graphismes.md`, `docs/TECHNICAL.md` (hd-pack.exe, files, priority)

- [ ] **Step 1: Run** BUILD.bat in the worktree (subagent `dqix-build-check`): exit 0 and `game\hd-pack.exe` exists.
- [ ] **Step 2: Commit** `HD pack: built with the game; README credits and docs`.
- [ ] **Step 3: Ask the coordinator** to note the shared files in `docs/SUIVI.md`; push the branch and open the PR only with recompsmith's go-ahead.

### Task 8: Publish the pack (after the 6 movies, with recompsmith's go-ahead)

- [ ] **Step 1:** `python tools/make_hd_pack.py J:\DQIX-Enhanced\game\textures\hd J:\DQIX-HD-staging\movies J:\DQIX-HD-staging\release`, check the manifest lines.
- [ ] **Step 2:** End-to-end with `HDPACK_SOURCE=J:\DQIX-HD-staging\release` on a copy of a game folder (not the play folder): installed texture count = 12,942, movies present.
- [ ] **Step 3:** With recompsmith's explicit yes: `gh release create hd-pack-1 ... --title "HD pack 1" --notes-file <notes with credits, and that the movies were made and checked on the USA version; EU not checked yet>` uploading the zips and `hd-pack.txt`. Then a run without `HDPACK_URL` on the copy folder downloads from GitHub and installs.
- [ ] **Step 4:** recompsmith checks in game himself: the bar at the bottom left, then textures and movies in HD.
