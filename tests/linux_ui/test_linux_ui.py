"""ROM-free Linux UI regressions. Requires a C compiler and SDL2 headers.

    python3 -m unittest discover -s tests/linux_ui -v

All fixtures and helper executables live in a temporary directory. Dialog
helpers are replaced with test programs; no display or user interaction is
needed, and the player's game directory is never read or written.
"""
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent


@unittest.skipUnless(sys.platform.startswith("linux"), "Linux UI regressions")
class LinuxUiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="dqix-linux-ui-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.work = Path(cls.temp.name)
        cls.cc = shlex.split(os.environ.get("CC", "cc"))
        cls.sdl = shlex.split(subprocess.check_output(
            ["sdl2-config", "--cflags", "--libs"], text=True))
        cls.font = cls.build("font_probe", ["text.c"], ["-lm", "-Wl,--wrap=fopen"])
        cls.controls = cls.build("controls_probe", ["controls.c"], cls.sdl)
        cls.dialog = cls.build("dialog_probe", ["menu_win32.c"], cls.sdl)

    @classmethod
    def build(cls, name, runtime, libraries):
        binary = cls.work / name
        subprocess.run(cls.cc + ["-ffunction-sections", "-fdata-sections", "-I", str(ROOT / "runtime"),
                                str(HERE / (name + ".c"))]
                       + [str(ROOT / "runtime" / source) for source in runtime]
                       + libraries + ["-Wl,--gc-sections", "-o", str(binary)], check=True)
        return binary

    def setUp(self):
        self.case = Path(tempfile.mkdtemp(dir=self.work))

    def test_bundled_fonts_render_journal_text_without_system_fonts(self):
        fonts = self.case / "fonts"
        fonts.mkdir()
        for name in ("SourceSans3-Regular.ttf", "SourceSans3.ttf"):
            with self.subTest(font=name):
                target = fonts / name
                shutil.copyfile(ROOT / "fonts" / name, target)
                subprocess.run([str(self.font), "1"], cwd=self.case, check=True)
                target.unlink()

    def test_missing_fonts_fail_without_drawing(self):
        subprocess.run([str(self.font), "0"], cwd=self.case, check=True)

    def test_ds_bindings_are_available_to_menus(self):
        (self.case / "controls.ini").write_text(
            "up = W | Up | dpup\nleft = A | Left | dpleft\n"
            "down = S | Down | dpdown\nright = D | Right | dpright\n"
            "x = E | none | y\ny = F | none | x\nr = R | none | rightshoulder\n")
        subprocess.run([str(self.controls)], cwd=self.case, check=True)

    def dialog_env(self, *helpers):
        folder = self.case / "bin"
        folder.mkdir()
        helper = f"#!{sys.executable}\n" + (
            "import json, os, sys\n"
            "with open(os.environ['DIALOG_TEST_LOG'], 'a') as f:\n"
            "    f.write(json.dumps(sys.argv, ensure_ascii=False) + '\\n')\n"
            "sys.stdout.write(os.environ.get('DIALOG_TEST_VALUE', '') + '\\n')\n"
            "sys.exit(int(os.environ.get('DIALOG_TEST_STATUS', '0')))\n")
        for name in helpers:
            path = folder / name
            path.write_text(helper)
            path.chmod(0o755)
        # Only the test helpers can be found, even if a desktop tool is installed.
        return dict(os.environ, PATH=str(folder), DIALOG_TEST_LOG=str(self.case / "calls.jsonl"),
                    DIALOG_TEST_VALUE="50000")

    def calls(self):
        return [json.loads(line) for line in (self.case / "calls.jsonl").read_text().splitlines()]

    def test_dialog_accepts_amounts_empty_input_and_literal_text(self):
        env = self.dialog_env("kdialog", "zenity")
        for value in ("50000", "50 000", "", "Été ; $(printf faux)\nseconde ligne"):
            with self.subTest(value=value):
                subprocess.run([str(self.dialog)], cwd=self.case,
                               env=dict(env, DIALOG_TEST_VALUE=value), check=True)
        for call in self.calls():
            self.assertEqual(Path(call[0]).name, "kdialog")
            self.assertEqual(call[1:], ["--title", "Cheats", "--inputbox",
                                        "Montant d'or : $(touch SHOULD_NOT_EXIST)"])
        self.assertFalse((self.case / "SHOULD_NOT_EXIST").exists())

    def test_cancel_does_not_open_a_second_dialog(self):
        env = self.dialog_env("kdialog", "zenity")
        subprocess.run([str(self.dialog)], cwd=self.case,
                       env=dict(env, DIALOG_TEST_CANCEL="1", DIALOG_TEST_STATUS="1"), check=True)
        self.assertEqual(len(self.calls()), 1)

    def test_missing_kdialog_falls_back_to_zenity(self):
        subprocess.run([str(self.dialog)], cwd=self.case,
                       env=self.dialog_env("zenity"), check=True)
        self.assertEqual(Path(self.calls()[0][0]).name, "zenity")

    def test_missing_helpers_leave_the_prompt_cancelled(self):
        subprocess.run([str(self.dialog)], cwd=self.case,
                       env=dict(self.dialog_env(), DIALOG_TEST_CANCEL="1"), check=True)

    def test_dialog_works_when_the_launcher_closes_stdout(self):
        subprocess.run([str(self.dialog)], cwd=self.case,
                       env=dict(self.dialog_env("kdialog"), DIALOG_TEST_CLOSE_STDOUT="1"), check=True)


if __name__ == "__main__":
    unittest.main()
