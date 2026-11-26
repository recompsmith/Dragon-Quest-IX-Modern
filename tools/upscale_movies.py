# Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later
"""Upscale DQIX's Mobiclip movies 4x with Real-ESRGAN (realesr-animevideov3) into MPEG-1 files
the game plays instead (runtime/hdmovie.c), plus each movie's signature file.

usage: upscale_movies.py <model.pth> <movie_dir with *.mods> <out_dir>

out_dir/<name>.mpg  1024x768 MPEG-1, one picture per movie frame, no B-frames
out_dir/<name>.sig  "DQMS", u32 frames, then per frame 16x12 RGB (each the mean of an
                    16x16 block of the frame as the game shows it, 8 bits per channel)
"""
import os, sys, time
import numpy as np, torch, spandrel
sys.path.insert(0, os.path.dirname(__file__))
from movie_frames import frames, write_movie

model = spandrel.ModelLoader().load_from_file(sys.argv[1]).model.eval()
src, out = sys.argv[2], sys.argv[3]
os.makedirs(out, exist_ok=True)
names = sorted(n[:-5] for n in os.listdir(src) if n.endswith(".mods"))
if len(sys.argv) > 4: names = [n for n in names if n in sys.argv[4:]]
for name in names:
    if os.path.exists(f"{out}/{name}.mpg"): continue
    t0 = time.time()

    def upscaled():
        for i, f in enumerate(frames(f"{src}/{name}.mods")):
            with torch.no_grad():
                o = model(torch.from_numpy(f).float().div(255).permute(2, 0, 1)[None])
            if i % 100 == 0: print(name, i, f"{time.time() - t0:.0f}s", flush=True)
            yield (o[0].permute(1, 2, 0).clamp(0, 1).numpy() * 255).round().astype(np.uint8)
    n = write_movie(out, name, frames(f"{src}/{name}.mods"), upscaled())
    print(name, "done", n, "frames", flush=True)
