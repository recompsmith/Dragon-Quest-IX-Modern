#!/usr/bin/env python3
"""Release command line of Dragon Quest IX Modern (used by the release-manager
agent). Python 3 standard library only; runs on Windows and Linux.

    python tools/release.py prepare vX
    python tools/release.py check vX
    python tools/release.py publish vX --title "vX: ..." [--dry-run]
    python tools/release.py publish vX-rcN --prerelease --title "..."
    python tools/release.py verify vX
    python tools/release.py announce vX [--file PATH]
    python tools/release.py delete-rc vX-rcN

--dry-run prints each mutating git/gh command instead of running it (read-only
commands still run). Every git/gh call goes through run(), which the tests
replace. Messages to the operator are in French."""
import argparse
import datetime
import http.client
import json
import os
import re
import shlex
import subprocess
import sys
import time
import urllib.error
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

from release_lib import (bump_version_h, merged_prs, forbidden_tracked, check_notes,  # noqa: E402
                         check_announce, is_rc_tag, parse_cloud_report, NOTE_LANGS)

PUBLIC_REPO = "recompsmith/Dragon-Quest-IX-Modern"
PRIVATE_REPO = "recompsmith/dqix-test-prive"
WATCH_TIMEOUT = 15 * 60
SIGNATURE = "\U0001F916 Generated with [Claude Code](https://claude.com/claude-code)"
DEFAULT_ANNOUNCE_DIR = r"J:\DQIX-Passation\annonces"

DRY = False


class Fail(Exception):
    """A stop with a message for the operator (exit code 1)."""


def run(cmd, check=True, timeout=None):
    """Run a command, return the CompletedProcess (text output). The one
    function the tests replace."""
    return subprocess.run(cmd, cwd=ROOT, check=check, timeout=timeout, capture_output=True,
                          text=True, encoding="utf-8", errors="replace")


def mutate(cmd):
    """A command that changes something: only printed under --dry-run."""
    if DRY:
        print("[dry-run] " + " ".join(shlex.quote(c) for c in cmd))
        return subprocess.CompletedProcess(cmd, 0, "", "")
    return run(cmd)


def out_of(cmd):
    return run(cmd).stdout.strip()


def load_json(text, what):
    """json.loads for gh output: a clean Fail instead of a traceback."""
    try:
        return json.loads(text)
    except ValueError:
        raise Fail("Réponse de gh illisible (%s)." % what)


def base_version(tag):
    """vX for both vX and vX-rcN."""
    return re.sub(r"-rc\d+$", "", tag)


def branch_of(tag):
    return "integ/release-" + base_version(tag)


def load_notes(version):
    files = {}
    paths = ["release-notes/%s.md" % version]
    paths += ["release-notes/%s/%s.md" % (lang, version) for lang in NOTE_LANGS]
    for rel in paths:
        try:
            with open(os.path.join(ROOT, rel), encoding="utf-8-sig") as f:
                files[rel] = f.read()
        except OSError:
            pass
    return files


def bump_file(version):
    path = os.path.join(ROOT, "runtime", "version.h")
    with open(path, encoding="utf-8", newline="") as f:
        text = f.read()
    new = bump_version_h(text, version, datetime.date.today().isoformat())
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(new)


# --- prepare ---------------------------------------------------------------

def cmd_prepare(args):
    version = args.version
    if is_rc_tag(version):
        raise Fail("prepare prend la version (v3), pas une pré-version.")
    git_dir = out_of(["git", "rev-parse", "--git-dir"])
    common_dir = out_of(["git", "rev-parse", "--git-common-dir"])
    if os.path.realpath(os.path.join(ROOT, git_dir)) == os.path.realpath(os.path.join(ROOT, common_dir)):
        raise Fail("prepare ne s'exécute pas dans le clone principal (il doit rester sur main). "
                   "Créer un worktree : git -C J:\\DQIX-Enhanced-repo worktree add "
                   "J:\\DQIX-wt\\integ-release-%s origin/main, puis relancer prepare dedans." % version)
    if run(["gh", "auth", "status"], check=False).returncode:
        raise Fail("gh n'est pas connecté : lancer `gh auth login`.")
    if out_of(["git", "status", "--porcelain"]):
        raise Fail("L'arbre de travail n'est pas propre : commiter ou ranger les changements.")
    run(["git", "fetch", "origin"])
    tags = [t for t in out_of(["git", "tag", "--list", "v*", "--sort=-creatordate"]).split()
            if not is_rc_tag(t)]
    last = tags[0] if tags else None
    log_cmd = ["git", "log", "--merges", "--format=%s"]
    log = out_of(log_cmd + [("%s..origin/main" % last) if last else "origin/main"])
    prs = merged_prs(log)
    mutate(["git", "switch", "--no-track", "-c", branch_of(version), "origin/main"])
    if DRY:
        print("[dry-run] runtime/version.h : version %s, date du jour" % version)
    else:
        bump_file(version)
    print("Branche %s créée depuis origin/main ; runtime/version.h mis à jour (non commité)."
          % branch_of(version))
    if prs:
        print("PR fusionnées depuis %s : %s" % (last or "le début",
                                                ", ".join("#%d" % n for n in prs)))
    else:
        print("Aucune PR fusionnée depuis %s : pas de release." % (last or "le début"))
    print("Suite : écrire release-notes/%s.md (+ fr/de/it/es), puis commiter." % version)


# --- check -----------------------------------------------------------------

def fetch_report(base):
    path = "captures/release-%s/report.md" % base
    res = run(["gh", "api", "repos/%s/contents/%s?ref=tests/release-%s" % (PRIVATE_REPO, path, base),
               "-H", "Accept: application/vnd.github.raw"], check=False)
    if res.returncode:
        raise Fail("Rapport cloud introuvable (%s, branche tests/release-%s du dépôt privé)."
                   % (path, base))
    return res.stdout


def cmd_check(args):
    tag = args.version
    base = base_version(tag)
    branch = branch_of(tag)
    current = out_of(["git", "rev-parse", "--abbrev-ref", "HEAD"])
    if current != branch:
        raise Fail("La branche courante est %s, pas %s." % (current, branch))
    head = out_of(["git", "rev-parse", "HEAD"])
    run(["git", "fetch", "origin"])
    if run(["git", "merge-base", "--is-ancestor", "origin/main", "HEAD"], check=False).returncode:
        raise Fail("La branche ne contient pas origin/main : rebaser la branche sur origin/main "
                   "et relancer le test cloud.")
    if out_of(["git", "status", "--porcelain"]):
        raise Fail("L'arbre de travail n'est pas propre : commiter ou ranger les changements "
                   "(le rapport cloud doit couvrir exactement ce qui est publié).")
    bad = forbidden_tracked(out_of(["git", "ls-files"]).splitlines())
    if bad:
        raise Fail("Fichiers interdits suivis par git : " + ", ".join(bad))
    errors = check_notes(base, load_notes(base))
    if errors:
        raise Fail("Notes de release invalides :\n  " + "\n  ".join(errors))
    if run([sys.executable, "tools/lang_check.py"], check=False).returncode:
        raise Fail("tools/lang_check.py a échoué.")
    if run([sys.executable, "-m", "unittest", "discover", "-s", "tests/release"],
           check=False).returncode:
        raise Fail("Les tests tests/release ont échoué.")
    try:
        sha, passed = parse_cloud_report(fetch_report(base))
    except ValueError as e:
        raise Fail(str(e))
    if sha != head:
        raise Fail("Le rapport cloud est celui de %s, la branche est sur %s : "
                   "relancer le test cloud sur %s." % (sha, head, head))
    if not passed:
        raise Fail("Le test cloud a échoué (result: FAIL).")
    print("check %s : OK (commit %s, test cloud PASS)." % (tag, head))
    return head


# --- publish ---------------------------------------------------------------

def create_release(tag, target, title, notes, latest):
    cmd = ["gh", "release", "create", tag, "--target", target, "--title", title,
           "--notes-file", notes, "--repo", PUBLIC_REPO]
    cmd.append("--latest" if latest else "--prerelease")
    mutate(cmd)


def cmd_publish(args):
    tag = args.version
    if args.prerelease and not is_rc_tag(tag):
        raise Fail("--prerelease n'est permis que pour un tag vX-rcN.")
    if is_rc_tag(tag) and not args.prerelease:
        raise Fail("Un tag vX-rcN se publie avec --prerelease.")
    base = base_version(tag)
    branch = branch_of(tag)
    notes = "release-notes/%s.md" % base
    sha = cmd_check(args)
    mutate(["git", "push", "-u", "origin", branch])
    if args.prerelease:
        create_release(tag, branch, args.title, notes, latest=False)
        print("Pré-version %s publiée (stable n'a pas bougé)." % tag)
        return
    body = ("Release %s.\n\nNotes: release-notes/%s.md. Tested by the cloud run on commit %s.\n\n%s"
            % (tag, tag, sha, SIGNATURE))
    mutate(["gh", "pr", "create", "--base", "main", "--head", branch,
            "--title", "Release " + tag, "--body", body, "--repo", PUBLIC_REPO])
    mutate(["gh", "pr", "merge", branch, "--merge", "--repo", PUBLIC_REPO])
    mutate(["git", "fetch", "origin"])
    if DRY:
        print("[dry-run] git diff --quiet %s origin/main" % sha)
    elif run(["git", "diff", "--quiet", sha, "origin/main"], check=False).returncode:
        raise Fail("origin/main n'a pas le même contenu que la branche testée (%s) : "
                   "arrêt avant stable, rien n'est publié." % sha)
    try:
        mutate(["git", "push", "origin", "origin/main:stable"])
    except subprocess.CalledProcessError:
        raise Fail("Le push de stable a échoué (pas en avance rapide ?) : arrêt, "
                   "aucune release créée. Pas de --force.")
    create_release(tag, "stable", args.title, notes, latest=True)
    print("Release %s publiée. Suite : python tools/release.py verify %s" % (tag, tag))


# --- verify ----------------------------------------------------------------

def find_run(tag):
    for attempt in range(6):
        runs = load_json(out_of(["gh", "run", "list", "--workflow", "release-zip.yml", "--json",
                                 "databaseId,headBranch,event,status,conclusion",
                                 "--limit", "20", "--repo", PUBLIC_REPO]) or "[]", "run list")
        for r in runs:
            if r.get("headBranch") == tag:
                return r["databaseId"]
        if attempt < 5:
            time.sleep(10)
    raise Fail("Aucun run release-zip.yml trouvé pour %s." % tag)


def cmd_verify(args):
    tag = args.version
    run_id = find_run(tag)
    try:
        res = run(["gh", "run", "watch", str(run_id), "--exit-status", "--repo", PUBLIC_REPO], check=False,
                  timeout=WATCH_TIMEOUT)
    except subprocess.TimeoutExpired:
        raise Fail("Le workflow release-zip.yml n'a pas fini en 15 min (run %s). "
                   "La release reste publiée ; pas de nouvelle tentative." % run_id)
    if res.returncode:
        raise Fail("Le workflow release-zip.yml est en échec (run %s). "
                   "La release reste publiée ; à corriger puis relancer à la main." % run_id)
    info = load_json(out_of(["gh", "release", "view", tag, "--json",
                             "assets,isPrerelease", "--repo", PUBLIC_REPO]), "release view")
    # gh release view has no isLatest field; gh release list has
    latest = load_json(out_of(["gh", "release", "list", "--json", "tagName,isLatest",
                               "--repo", PUBLIC_REPO]), "release list")
    is_latest = any(r.get("tagName") == tag and r.get("isLatest") for r in latest)
    names = {a["name"] for a in info.get("assets", [])}
    for osname in ("Windows", "Linux"):
        want = "Dragon-Quest-IX-Modern-%s-%s-x64.zip" % (tag, osname)
        if want not in names:
            raise Fail("Zip manquant dans la release %s : %s" % (tag, want))
    if is_rc_tag(tag):
        if not info.get("isPrerelease") or is_latest:
            raise Fail("%s doit être une pré-version et pas latest." % tag)
    elif not is_latest or info.get("isPrerelease"):
        raise Fail("%s n'est pas marquée latest (RecompLauncher garderait l'ancienne version)." % tag)
    print("verify %s : OK (2 zips, drapeaux corrects)." % tag)


# --- announce --------------------------------------------------------------

def cmd_announce(args):
    path = args.file
    if not path:
        if os.name != "nt":
            raise Fail("Hors Windows, --file est obligatoire.")
        path = os.path.join(DEFAULT_ANNOUNCE_DIR, args.version + ".txt")
    try:
        with open(path, encoding="utf-8-sig") as f:
            text = f.read()
    except OSError as e:
        raise Fail("Annonce illisible (%s) : %s" % (path, e))
    errors = check_announce(text)
    if errors:
        raise Fail("Annonce refusée (le texte reste dans %s) :\n  %s" % (path, "\n  ".join(errors)))
    url = os.environ.get("DISCORD_WEBHOOK_URL")
    if not url:
        print("DISCORD_WEBHOOK_URL absent : l'annonce reste dans %s." % path)
        return
    if DRY:
        print("[dry-run] POST webhook (%d caractères)" % len(text))
        return
    # The URL is a secret: never put it, or an exception that quotes it, in a message.
    try:
        req = urllib.request.Request(
            url, data=json.dumps({"content": text, "allowed_mentions": {"parse": []}}).encode("utf-8"), method="POST",
            headers={"Content-Type": "application/json", "User-Agent": "DQIX-Modern-release"})
        with urllib.request.urlopen(req, timeout=30):
            pass
    except urllib.error.HTTPError as e:
        raise Fail("Discord a répondu une erreur (HTTP %s). Le texte est conservé dans %s."
                   % (e.code, path)) from None
    except (ValueError, http.client.HTTPException, urllib.error.URLError, OSError) as e:
        raise Fail("Envoi à Discord impossible (%s). Le texte est conservé dans %s."
                   % (type(e).__name__, path)) from None
    print("Annonce postée sur Discord (fichier conservé : %s)." % path)


# --- delete-rc -------------------------------------------------------------

def cmd_delete_rc(args):
    tag = args.version
    if not is_rc_tag(tag):
        raise Fail("%s n'est pas une pré-version vX-rcN : suppression refusée." % tag)
    info = load_json(out_of(["gh", "release", "view", tag, "--json", "isPrerelease",
                             "--repo", PUBLIC_REPO]), "release view")
    if not info.get("isPrerelease"):
        raise Fail("%s n'est pas marquée pré-version : suppression refusée." % tag)
    mutate(["gh", "release", "delete", tag, "--cleanup-tag", "--yes", "--repo", PUBLIC_REPO])
    print("Pré-version %s supprimée (release et tag)." % tag)


# --- main ------------------------------------------------------------------

def build_parser():
    p = argparse.ArgumentParser(prog="release.py", description=__doc__.split("\n")[0])
    p.add_argument("--dry-run", action="store_true",
                   help="print mutating git/gh commands instead of running them")
    sub = p.add_subparsers(dest="command", required=True)
    for name in ("prepare", "check", "publish", "verify", "announce", "delete-rc"):
        s = sub.add_parser(name)
        s.add_argument("version", help="vX (vX-rcN for pre-releases)")
        s.add_argument("--dry-run", action="store_true", default=argparse.SUPPRESS)
        if name == "publish":
            s.add_argument("--title", required=True)
            s.add_argument("--prerelease", action="store_true")
        if name == "announce":
            s.add_argument("--file")
    return p


COMMANDS = {"prepare": cmd_prepare, "check": cmd_check, "publish": cmd_publish,
            "verify": cmd_verify, "announce": cmd_announce, "delete-rc": cmd_delete_rc}


def main(argv=None):
    global DRY
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):  # Windows consoles default to a legacy codepage
            stream.reconfigure(encoding="utf-8", errors="replace")
    args = build_parser().parse_args(argv)
    DRY = args.dry_run
    if not (re.fullmatch(r"v\d+(\.\d+)?", args.version) or is_rc_tag(args.version)):
        print("ERREUR : version invalide « %s » (attendu vX, vX.Y ou vX-rcN)." % args.version,
              file=sys.stderr)
        return 1
    try:
        COMMANDS[args.command](args)
    except Fail as e:
        print("ERREUR : %s" % e, file=sys.stderr)
        return 1
    except subprocess.CalledProcessError as e:
        print("ERREUR : commande en échec : %s\n%s" % (" ".join(e.cmd), (e.stderr or "").strip()),
              file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
