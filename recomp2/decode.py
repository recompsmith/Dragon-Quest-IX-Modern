"""Decode the instruction at an address of a module, in ARM or Thumb state."""
from decode_arm import decode_arm
from decode_thumb import decode_thumb


def decode_at(mod, addr, thumb):
    if thumb:
        if not mod.contains(addr, 2):
            return None
        nxt = mod.half(addr + 2) if mod.contains(addr + 2, 2) else None
        return decode_thumb(mod.half(addr), nxt, addr)
    if not mod.contains(addr, 4):
        return None
    return decode_arm(mod.word(addr), addr)
