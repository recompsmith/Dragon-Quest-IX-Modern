#!/usr/bin/env python3
# Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later
"""Pure helpers of the release automation (tools/release.py). No I/O, so each
one is tested on its own (tests/release/test_release_lib.py).

    python -m unittest discover -s tests/release -v

bump_version_h        runtime/version.h text -> the same for a new version
merged_prs            `git log --merges --format=%s` output -> PR numbers
forbidden_tracked     tracked paths that must never be in the public repo
check_notes           release notes present, translated, not cut
check_announce        the Discord announcement fits and links only our invite
is_rc_tag             "v3-rc1" yes, "v3" no
parse_cloud_report    the cloud test report -> (commit, passed)"""
import re

DISCORD_INVITE = "z9X9PH8AH4"
NOTE_LANGS = ("fr", "de", "it", "es")
FORBIDDEN_EXTS = (".nds", ".3ds", ".cia", ".cci", ".cxi", ".sav")
FORBIDDEN_PREFIXES = ("build/", "game/", "gen/")
FORBIDDEN_DIRS = ("textures", "movies", "trophies")


def bump_version_h(text, version, date):
    """Set the version in runtime/version.h: the comment line (the first
    DQIX_VERSION line, read by old updaters, "v48" first), DQIX_VERSION and
    DQIX_VERSION_DATE. ValueError when one of the three lines is missing."""
    patterns = [
        (re.compile(r'(#define DQIX_VERSION )"v48 \([^"]*\)"'), r'\1"v48 (%s)"' % version),
        (re.compile(r'^(#define DQIX_VERSION )"(?!v48 \()[^"]*"', re.M), r'\1"%s"' % version),
        (re.compile(r'^(#define DQIX_VERSION_DATE )"[^"]*"', re.M), r'\1"%s"' % date),
    ]
    for pat, repl in patterns:
        text, n = pat.subn(repl, text)
        if n == 0:
            raise ValueError("version.h: no line matches %s" % pat.pattern)
    return text


def merged_prs(log):
    """The numbers of the merged pull requests, in the order of the log."""
    return [int(n) for n in re.findall(r"^Merge pull request #(\d+)", log, re.M)]


def _forbidden(path):
    low = path.lower()
    if low.endswith(FORBIDDEN_EXTS) or low.startswith(FORBIDDEN_PREFIXES):
        return True
    parts = path.split("/")
    if parts[-1] == "title-back.png":
        return True
    if parts[0] != "tools" and any(p in FORBIDDEN_DIRS for p in parts[:-1]):
        return True
    return low.endswith(".c") and low.startswith(("gen/", "recomp-out/"))


def forbidden_tracked(paths):
    """The paths (git ls-files) that must not be tracked, in input order."""
    return [p for p in paths if _forbidden(p)]


# recompsmith, 2026-11-25: every release credits the original project.
ORIGIN_CREDIT = "github.com/sackchief/dqix-enhanced"


def check_notes(version, files):
    """Errors in the release notes of a version. `files` maps repo-relative
    paths (release-notes/v3.md, release-notes/fr/v3.md) to their text."""
    errors = []
    english = "release-notes/%s.md" % version
    paths = [english] + ["release-notes/%s/%s.md" % (lang, version) for lang in NOTE_LANGS]
    for path in paths:
        text = files.get(path)
        if text is None:
            errors.append("%s: missing" % path)
            continue
        if path == english and ORIGIN_CREDIT not in text:
            errors.append("%s: no credit to the original project (%s)" % (path, ORIGIN_CREDIT))
        if path != english and "**How to install**" in text:
            errors.append("%s: contains the English install block" % path)
        in_list = False
        for n, line in enumerate(text.splitlines(), 1):
            if in_list and line.strip() and not line.startswith(("- ", "**")):
                errors.append("%s:%d: list item cut: %r" % (path, n, line[:40]))
            in_list = line.startswith("- ") or (in_list and line.strip() != "" and not line.startswith("**"))
    return errors


def check_announce(text):
    """Errors in the Discord announcement: 2000 characters is the limit, and
    the only invite allowed is ours."""
    errors = []
    if len(text) >= 2000:
        errors.append("announcement is %d characters (must be under 2000)" % len(text))
    for code in re.findall(r"discord\.gg/([A-Za-z0-9_-]*)", text):
        if code != DISCORD_INVITE:
            errors.append("announcement has another Discord invite: discord.gg/%s" % code)
    return errors


def is_rc_tag(tag):
    return re.fullmatch(r"v\d+(\.\d+)?-rc\d+", tag) is not None


def parse_cloud_report(text):
    """(commit sha, passed). The contract: line 1 is `commit: <sha40>`, line 2
    is `result: PASS|FAIL`. ValueError otherwise."""
    lines = text.lstrip("﻿").splitlines()
    first = lines[0].strip() if lines else ""
    second = lines[1].strip() if len(lines) > 1 else ""
    commit = re.fullmatch(r"commit: ([0-9a-f]{40})", first)
    result = re.fullmatch(r"result: (PASS|FAIL)", second)
    if not commit:
        raise ValueError("cloud report: line 1 must be 'commit: <40 hex>'")
    if not result:
        raise ValueError("cloud report: line 2 must be 'result: PASS|FAIL'")
    return commit.group(1), result.group(1) == "PASS"
