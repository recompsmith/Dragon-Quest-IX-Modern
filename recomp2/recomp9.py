"""ARM9 driver: read the inputs, then (later tasks) find, translate and write."""
import inputs


def read_inputs(a):
    lists = {}
    for key, path, fields in (("hle", a.hle, inputs.HLE), ("interpret", a.interpret, inputs.INTERPRET),
                              ("patch", a.patch, inputs.PATCHES), ("hooks", a.hooks, inputs.HOOKS),
                              ("tags", a.tags, inputs.TAGS)):
        lists[key] = inputs.read_list(path, fields) if path else []
    mods = inputs.load_modules(a.extract, a.config)
    return mods, lists


def run(a):
    read_inputs(a)
    return 0
