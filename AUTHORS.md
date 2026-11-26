# Authors / Auteurs

## Original project / Projet d'origine

**sackchief** — [dqix-enhanced](https://github.com/sackchief/dqix-enhanced)
(commits by `dqix-recomp`, 2026-10-12 to 2026-10-19): the static recompiler
(`recomp/`), the DS runtime (CPU, memory, 2D, 3D, sound, cartridge, input),
the wireless chip, online multiplayer, the relay, the built-in Nintendo WFC
and DQVC, and the first SDL/OpenGL front end. This repository is a fork of it;
since 2026-10-19 (his v31) it no longer takes his updates. / Ce dépôt en est un
fork ; depuis le 19 octobre 2026 (sa v31), il ne reprend plus ses mises à jour.

## This fork / Ce fork (since 2026-10-19)

**Romuald** ([romusan495](https://github.com/romusan495)), with
**TitanFr199** (Tsu, Dr.Stone), helped by Claude. Each file that is entirely
ours starts with a `Copyright (C) 2026 ...` line naming its authors; under the
GPL, anyone reusing these files must keep it.

| Feature / Fonction | Files | Authors |
|---|---|---|
| EU ROM support, languages (fr/de/it/es) | `tools/region.py`, `tools/rom_files.py`, `runtime/uitext.c`, `lang/` | Romuald |
| Modern window style, in-game menu, What's new | `runtime/gamemenu.c`, `runtime/whatsnew.c`, `runtime/bootskip.c` | Romuald |
| Map markers (monsters, gather points, chests) | `runtime/mapmarks.c` | Romuald, TitanFr199 |
| Journal (quests, party, alchemy) | `runtime/journal*.c` | Romuald |
| Use several items at once | `runtime/multiuse.c` | TitanFr199, Romuald |
| Hints | `runtime/hints.c` | TitanFr199, Romuald |
| Cheats, gold farm | `runtime/cheats.c`, `runtime/farm.c` | Romuald, TitanFr199 |
| Trophies | `runtime/trophies.c`, `tools/trophy_badges.py` | Romuald |
| Autosave, safe save writes | `runtime/autosave.c`, `runtime/savefile.c` | Romuald |
| HD text, HD sprites, HD movies | `runtime/hdtext.c`, `runtime/hdsprite.h`, `runtime/hdmovie.c`, `tools/*movies*.py` | Romuald |
| HD texture pack (build, download, install) | `runtime/hdpack.c`, `tools/hd-pack.c`, `tools/hdpack_core.h`, `tools/make_hd_pack.py`, `tools/textures-*.c` | Romuald |
| 3D townspeople | `runtime/npc3d.c`, `tools/npc3d/` | Romuald |
| Vocation icons | `tools/job_icons.py`, `tools/job_icons_svg/` | Romuald, TitanFr199 |
| Launcher (install, updates, HD pack, controls) | `launcher/` | Romuald |
| Release tooling | `tools/release.py`, `tools/release_lib.py`, `.github/workflows/` | Romuald |

Files shared with the original code (for example `runtime/frontend_sdl.c`,
`runtime/gl_render.c`, `runtime/gpu2d.c`, `runtime/menu_win32.c`) contain work
by both; `git blame` shows who wrote each line.

Also: function names from the public-domain DQIX decompilation project
(`config/`), melonDS research for the wireless chip, 3D and sound, and the
HD pack's tools (FatalPixels, UltraSharp, SeedVR2) — see README > Credits.

The CPU core in `runtime/base/` adapts code from [melonDS](https://github.com/melonDS-emu/melonDS) 1.1 (commit b86390e, GPL-3.0-or-later) for the hardware blocks (ARM interpreter, CP15, memory map, I/O, timers, DMA, IPC, maths unit, SPI, firmware, RTC, direct boot); each such file names its melonDS source in its header.
