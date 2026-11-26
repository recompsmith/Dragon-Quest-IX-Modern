# Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later
"""Decode a DS Mobiclip .mods into RGB frames, as the game shows them (YCoCg, chroma not interpolated)."""
import os, struct, subprocess, numpy as np
def frames(path):
    p = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-f", "rawvideo", "-pix_fmt", "yuv420p", "-"], capture_output=True).stdout
    fs = 256 * 192 * 3 // 2
    raw = np.frombuffer(p, np.uint8)[: len(p) // fs * fs].reshape(-1, fs)
    for r in raw:
        r = r.astype(np.int16)
        Y = r[:49152].reshape(192, 256)
        Cg = r[49152:61440].reshape(96, 128).repeat(2, 0).repeat(2, 1) - 128
        Co = r[61440:].reshape(96, 128).repeat(2, 0).repeat(2, 1) - 128
        t = Y - Cg
        yield np.clip(np.stack([t + Co, Y + Cg, t - Co], -1), 0, 255).astype(np.uint8)


def write_movie(out_dir, name, src_frames, hd_frames):
    """Write <out_dir>/<name>.mpg (1024x768 MPEG-1, one picture per frame, no B-frames) and
    <name>.sig ("DQMS", u32 frames, per frame 16x12 RGB: the mean of each 16x16 block of the
    256x192 source frame). Returns the frame count."""
    os.makedirs(out_dir, exist_ok=True)
    sigs = []
    enc = subprocess.Popen(["ffmpeg", "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", "1024x768", "-r", "24", "-i", "-",
                            "-c:v", "mpeg1video", "-q:v", "3", "-g", "24", "-bf", "0", "-f", "mpeg1video", f"{out_dir}/{name}.tmp"], stdin=subprocess.PIPE)
    for f, o in zip(src_frames, hd_frames):
        sigs.append(f.reshape(12, 16, 16, 16, 3).mean(axis=(1, 3)).round().astype(np.uint8).tobytes())
        if o.shape != (768, 1024, 3):
            enc.stdin.close(); enc.wait()
            raise ValueError(f"{name}: HD frame is {o.shape}, expected (768, 1024, 3)")
        enc.stdin.write(np.ascontiguousarray(o, np.uint8).tobytes())
    enc.stdin.close()
    if enc.wait() != 0:
        raise RuntimeError(f"ffmpeg failed encoding {name}")
    with open(f"{out_dir}/{name}.sig", "wb") as s:
        s.write(b"DQMS" + struct.pack("<I", len(sigs)) + b"".join(sigs))
    os.replace(f"{out_dir}/{name}.tmp", f"{out_dir}/{name}.mpg")
    return len(sigs)
