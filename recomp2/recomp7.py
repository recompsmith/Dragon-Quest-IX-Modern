"""ARM7 driver: read the inputs, then (later tasks) find, translate and write."""
import inputs


def run(a):
    if a.hle:
        inputs.read_list(a.hle, inputs.HLE7)
    if a.seeds:
        inputs.read_list(a.seeds, inputs.SEEDS)
    return 0
