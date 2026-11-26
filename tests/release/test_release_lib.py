# Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later
"""Tests for tools/release_lib.py (pure functions of the release automation).

    python -m unittest discover -s tests/release -v"""
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))

from release_lib import (bump_version_h, merged_prs, forbidden_tracked, check_notes,  # noqa: E402
                         check_announce, is_rc_tag, parse_cloud_report)

with open(os.path.join(ROOT, "runtime", "version.h"), encoding="utf-8") as _f:
    V21 = _f.read()

LANGS = ("fr", "de", "it", "es")


CREDIT = "Fork of [dqix-enhanced](https://github.com/sackchief/dqix-enhanced) by sackchief.\n"


def notes_files(version, **overrides):
    files = {"release-notes/%s.md" % version: "**How to install**\n- a\n\n" + CREDIT}
    for lang in LANGS:
        files["release-notes/%s/%s.md" % (lang, version)] = "- a\n- b\n"
    files.update(overrides)
    return files


class BumpVersionH(unittest.TestCase):
    def test_bump_keeps_v48_comment_first(self):
        out = bump_version_h(V21, "v3", "2026-11-15")
        lines = [l for l in out.splitlines() if "#define DQIX_VERSION " in l]
        self.assertIn('"v48 (v3)"', lines[0])
        self.assertIn('"v3"', lines[1])

    def test_bump_sets_date(self):
        out = bump_version_h(V21, "v3", "2026-11-15")
        self.assertIn('#define DQIX_VERSION_DATE "2026-11-15"', out)
        self.assertNotIn("2026-11-13", out)

    def test_bump_missing_line_raises(self):
        for needle in ('#define DQIX_VERSION "', "#define DQIX_VERSION_DATE", '"v48 ('):
            broken = "\n".join(l for l in V21.splitlines() if needle not in l)
            with self.assertRaises(ValueError, msg=needle):
                bump_version_h(broken, "v3", "2026-11-15")


class MergedPrs(unittest.TestCase):
    def test_merged_prs(self):
        self.assertEqual(merged_prs("Merge pull request #146 from a/b\nMerge pull request #145 from c/d"),
                         [146, 145])

    def test_ignores_other_lines(self):
        self.assertEqual(merged_prs("Merge branch 'x'\n\nMerge pull request #7 from a/b\n"), [7])

    def test_empty(self):
        self.assertEqual(merged_prs(""), [])


class Forbidden(unittest.TestCase):
    def test_forbidden(self):
        self.assertEqual(forbidden_tracked(["rom/x.nds", "game/ui/title-back.png", "tools/ui/equip-shop.png",
                                            "runtime/main.c"]),
                         ["rom/x.nds", "game/ui/title-back.png"])

    def test_extensions_case_insensitive(self):
        paths = ["a.NDS", "b.3ds", "c.cia", "d.cci", "e.cxi", "f.SAV", "g.txt"]
        self.assertEqual(forbidden_tracked(paths), paths[:6])

    def test_prefixes(self):
        self.assertEqual(forbidden_tracked(["build/x.o", "game/a", "gen/b.h", "buildx/y", "runtime/game/z"]),
                         ["build/x.o", "game/a", "gen/b.h"])

    def test_title_back_anywhere(self):
        self.assertEqual(forbidden_tracked(["ui/title-back.png"]), ["ui/title-back.png"])

    def test_asset_dirs_outside_tools(self):
        self.assertEqual(forbidden_tracked(["textures/a.png", "x/movies/b.mp4", "trophies/c.png",
                                            "tools/textures/d.png", "tools/ui/trophies/e.png"]),
                         ["textures/a.png", "x/movies/b.mp4", "trophies/c.png"])

    def test_generated_c(self):
        self.assertEqual(forbidden_tracked(["recomp-out/a.c", "recomp-out/a.h", "gen/x/y.c", "recomp/a.c"]),
                         ["recomp-out/a.c", "gen/x/y.c"])

    def test_order_kept_and_clean(self):
        self.assertEqual(forbidden_tracked(["b.sav", "a.nds"]), ["b.sav", "a.nds"])
        self.assertEqual(forbidden_tracked(["runtime/main.c", "docs/a.md"]), [])


class CheckNotes(unittest.TestCase):
    def test_ok(self):
        self.assertEqual(check_notes("v3", notes_files("v3")), [])

    def test_missing_english(self):
        files = notes_files("v3")
        del files["release-notes/v3.md"]
        errs = check_notes("v3", files)
        self.assertEqual(len(errs), 1)
        self.assertIn("release-notes/v3.md", errs[0])

    def test_missing_translation(self):
        files = notes_files("v3")
        del files["release-notes/de/v3.md"]
        errs = check_notes("v3", files)
        self.assertEqual(len(errs), 1)
        self.assertIn("release-notes/de/v3.md", errs[0])

    def test_translation_with_install_block(self):
        errs = check_notes("v3", notes_files("v3", **{"release-notes/it/v3.md": "- a\n**How to install**\n"}))
        self.assertEqual(len(errs), 1)
        self.assertIn("release-notes/it/v3.md", errs[0])

    def test_english_may_have_install_block(self):
        self.assertEqual(check_notes("v3", notes_files("v3")), [])

    def test_cut_list_line(self):
        errs = check_notes("v3", notes_files("v3", **{"release-notes/fr/v3.md": "- a long item\ncut here\n- b\n"}))
        self.assertEqual(len(errs), 1)
        self.assertIn("release-notes/fr/v3.md", errs[0])

    def test_non_cut_lines(self):
        text = "- a\n\n**Title**\n- b\n- c\n"
        self.assertEqual(check_notes("v3", notes_files("v3", **{"release-notes/es/v3.md": text})), [])

    def test_english_without_origin_credit(self):
        errs = check_notes("v3", notes_files("v3", **{"release-notes/v3.md": "**How to install**\n- a\n"}))
        self.assertEqual(len(errs), 1)
        self.assertIn("original project", errs[0])

    def test_cut_line_in_english_too(self):
        errs = check_notes("v3", notes_files("v3", **{"release-notes/v3.md": "- a\nbroken\n\n" + CREDIT}))
        self.assertEqual(len(errs), 1)
        self.assertIn("release-notes/v3.md", errs[0])


class CheckAnnounce(unittest.TestCase):
    def test_announce_2000_refused(self):
        self.assertTrue(check_announce("a" * 2000))
        self.assertFalse(check_announce("a" * 1999))

    def test_good_invite(self):
        self.assertEqual(check_announce("join https://discord.gg/z9X9PH8AH4 now"), [])

    def test_other_invite(self):
        self.assertTrue(check_announce("join https://discord.gg/abcdef"))
        self.assertTrue(check_announce("discord.gg/z9X9PH8AH4x"))
        self.assertTrue(check_announce("https://discord.gg/z9X9PH8AH4 and discord.gg/other"))


class RcTag(unittest.TestCase):
    def test_rc_refuses_real_tag(self):
        self.assertFalse(is_rc_tag("v3"))
        self.assertTrue(is_rc_tag("v3-rc1"))

    def test_variants(self):
        self.assertTrue(is_rc_tag("v2.1-rc12"))
        self.assertFalse(is_rc_tag("v3-rc"))
        self.assertFalse(is_rc_tag("v3-rc1-x"))
        self.assertFalse(is_rc_tag("xv3-rc1"))


class CloudReport(unittest.TestCase):
    SHA = "a" * 40

    def test_cloud_report(self):
        self.assertEqual(parse_cloud_report("commit: " + self.SHA + "\nresult: PASS\n"), (self.SHA, True))

    def test_fail(self):
        self.assertEqual(parse_cloud_report("commit: " + self.SHA + "\n" + "result: FAIL" + "\n"), (self.SHA, False))

    def test_crlf_and_bom(self):
        text = "\ufeffcommit: " + self.SHA + "\r\n" + "result: PASS" + "\r\n"
        self.assertEqual(parse_cloud_report(text), (self.SHA, True))

    def test_lines_further_down_are_refused(self):
        for text in ("noise" + "\n" + "result: FAIL" + "\n" + "commit: " + self.SHA,
                     "result: PASS" + "\n" + "commit: " + self.SHA + "\n",
                     "commit: " + self.SHA + "\n" + "\n" + "result: PASS" + "\n",
                     "# Report" + "\n" + "commit: " + self.SHA + "\n" + "result: PASS" + "\n"):
            with self.assertRaises(ValueError, msg=text):
                parse_cloud_report(text)

    def test_errors(self):
        for text in ("result: PASS\n", "commit: " + self.SHA + "\n", "commit: " + self.SHA + "\nresult: MAYBE\n",
                     "commit: " + "A" * 40 + "\nresult: PASS\n", "commit: abc123\nresult: PASS\n", ""):
            with self.assertRaises(ValueError, msg=text):
                parse_cloud_report(text)


if __name__ == "__main__":
    unittest.main()
