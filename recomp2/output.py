"""Writing the generated files.

- one function per table entry, `per_file` entries per .c file, named
  <module>_<NN>.c in table order (no file for a module without entries);
- a file whose content did not change is not rewritten (make keeps it);
- each file is written to <name>.tmp then renamed (no truncated file after an
  interrupted build);
- generated files from an earlier run that are no longer produced are
  removed; nothing else in the folder is touched;
- "\\n" line ends everywhere, no time, path or hash-order dependence.
"""
import os
import re

GENERATED = re.compile(r"^(?:[a-z0-9]+_[0-9]{2,}\.c|funcs7?\.h|dispatch7?_table\.c|report7?\.txt|recomp_ops\.h)$")


def c_string(s):
    out = []
    for ch in s:
        o = ord(ch)
        if ch in "\\\"":
            out.append("\\" + ch)
        elif 32 <= o < 127:
            out.append(ch)
        else:
            out.append("\\%03o" % (o & 0xFF))
    return '"' + "".join(out) + '"'


def write_file(path, text):
    """Write `text` unless the file already holds exactly it. Returns True if written."""
    data = text.encode("utf-8")
    try:
        with open(path, "rb") as f:
            if f.read() == data:
                return False
    except OSError:
        pass
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        f.write(data)
    os.replace(tmp, path)
    return True


def write_all(out_dir, files):
    """files: {name: text}. Writes them and removes stale generated files."""
    os.makedirs(out_dir, exist_ok=True)
    written = 0
    for name in sorted(files):
        if write_file(os.path.join(out_dir, name), files[name]):
            written += 1
    for name in sorted(os.listdir(out_dir)):
        if GENERATED.match(name) and name not in files:
            os.remove(os.path.join(out_dir, name))
    return written


def chunks(entries, n):
    """Split table entries by module into runs of at most n: [(module name, [entries])]."""
    out = []
    for e in entries:
        if not out or out[-1][0] != e.mod.name or len(out[-1][1]) >= n:
            out.append((e.mod.name, []))
        out[-1][1].append(e)
    return out


def report(stats, problems):
    lines = ["%s: %d" % (k, stats[k]) for k in sorted(stats)]
    lines.append("")
    lines.append("problems (%d):" % len(problems))
    lines.extend(problems)
    return "\n".join(lines) + "\n"
