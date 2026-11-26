# Copyright (C) 2026 recompsmith. SPDX-License-Identifier: GPL-3.0-or-later
"""The smoke test must not be stopped by the What's new box: its settings.ini
has to carry the current version (no game needed to check that)."""
import os
import re
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import run

VERSION_H = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "runtime", "version.h")


class SettingsTest(unittest.TestCase):
    def test_last_version_is_the_current_version(self):
        with open(VERSION_H, encoding="utf-8") as f:
            version = re.search(r'^#define DQIX_VERSION "([^"]+)"', f.read(), re.M).group(1)
        self.assertIn(f"\nlast_version = {version}\n", run.settings_text())

    def test_other_settings_are_kept(self):
        text = run.settings_text()
        for line in run.SETTINGS.splitlines():
            self.assertIn(line, text.splitlines())


if __name__ == "__main__":
    unittest.main()
