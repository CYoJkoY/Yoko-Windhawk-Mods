"""Tests for tools/changed_mods.py."""

from __future__ import annotations

import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.changed_mods import SelectionError, known_mods, main, select_mods  # noqa: E402


class SelectModsTests(unittest.TestCase):
    """A mod change republishes that mod; a shared change republishes all."""

    def test_mod_directory_selects_that_mod(self) -> None:
        selected = select_mods(["mods/a/src/main.cpp"], ["a", "b"], ["shared"])
        self.assertEqual(selected, ["a"])

    def test_several_mods_are_sorted(self) -> None:
        paths = ["mods/b/src/hook.cpp", "mods/a/mod.json", "mods/b/src/extra.h"]
        self.assertEqual(select_mods(paths, ["a", "b"], []), ["a", "b"])

    def test_mod_notes_do_not_republish(self) -> None:
        self.assertEqual(
            select_mods(["mods/a/doc/README.md", "mods/a/doc/preview.png"], ["a"], []),
            [],
        )

    def test_shared_change_selects_every_mod(self) -> None:
        self.assertIsNone(select_mods(["shared/helper.h"], ["a", "b"], ["shared"]))

    def test_trigger_file_selects_every_mod(self) -> None:
        self.assertIsNone(
            select_mods(["tools/bundle.py"], ["a", "b"], ["shared", "tools/bundle.py"])
        )

    def test_trigger_matches_whole_segments_only(self) -> None:
        self.assertEqual(
            select_mods(["shared-extra/x.h"], ["a"], ["shared"]),
            [],
        )

    def test_unrelated_and_unknown_paths_select_nothing(self) -> None:
        paths = ["README.md", "mods/removed/src/main.cpp", "tools/bundle.py"]
        self.assertEqual(select_mods(paths, ["a"], ["shared"]), [])

    def test_backslashes_and_blank_lines_are_tolerated(self) -> None:
        self.assertEqual(
            select_mods(["mods\\a\\src\\main.cpp", "", "  "], ["a"], []), ["a"]
        )


class KnownModsTests(unittest.TestCase):
    """Only directories carrying mod.json count as mods."""

    def setUp(self) -> None:
        self._temp = tempfile.TemporaryDirectory()
        self.root = Path(self._temp.name)

    def tearDown(self) -> None:
        self._temp.cleanup()

    def test_lists_sorted_manifest_directories(self) -> None:
        for name in ["b", "a"]:
            (self.root / "mods" / name).mkdir(parents=True)
            (self.root / "mods" / name / "mod.json").write_text("{}\n", encoding="utf-8")
        (self.root / "mods" / "notes").mkdir()
        self.assertEqual(known_mods(self.root), ["a", "b"])

    def test_missing_mods_directory_is_an_error(self) -> None:
        with self.assertRaises(SelectionError):
            known_mods(self.root)


class MainTests(unittest.TestCase):
    """The CLI reports JSON and writes the workflow outputs."""

    def setUp(self) -> None:
        self._temp = tempfile.TemporaryDirectory()
        self.root = Path(self._temp.name)
        for name in ["a", "b"]:
            (self.root / "mods" / name).mkdir(parents=True)
            (self.root / "mods" / name / "mod.json").write_text("{}\n", encoding="utf-8")
        self.paths = self.root / "changed.txt"

    def tearDown(self) -> None:
        self._temp.cleanup()

    def write_paths(self, *paths: str) -> None:
        self.paths.write_text("\n".join(paths) + "\n", encoding="utf-8")

    def run_main(self, *extra: str) -> tuple[int, str, dict[str, str]]:
        output = self.root / "github-output.txt"
        output.write_text("", encoding="utf-8")
        argv = [
            "--root",
            str(self.root),
            "--paths",
            str(self.paths),
            "--github-output",
            str(output),
            *extra,
        ]
        buffer = io.StringIO()
        errors = io.StringIO()
        with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(errors):
            code = main(argv)
        self.last_stderr = errors.getvalue()
        values: dict[str, str] = {}
        for line in output.read_text(encoding="utf-8").splitlines():
            key, _, value = line.partition("=")
            values[key] = value
        return code, buffer.getvalue(), values

    def test_single_mod_change_writes_one_mod(self) -> None:
        self.write_paths("mods/a/src/main.cpp")
        code, stdout, values = self.run_main()
        self.assertEqual(code, 0)
        self.assertEqual(json.loads(stdout), {"all": False, "mods": ["a"]})
        self.assertEqual(values, {"publish_all": "false", "mods": "a", "count": "1"})

    def test_shared_change_writes_every_mod(self) -> None:
        self.write_paths("shared/helper.h")
        code, stdout, values = self.run_main("--trigger", "shared")
        self.assertEqual(code, 0)
        self.assertEqual(json.loads(stdout), {"all": True, "mods": ["a", "b"]})
        self.assertEqual(values, {"publish_all": "true", "mods": "a b", "count": "2"})

    def test_all_flag_ignores_changed_paths(self) -> None:
        self.write_paths("README.md")
        code, stdout, values = self.run_main("--all")
        self.assertEqual(code, 0)
        self.assertEqual(json.loads(stdout), {"all": True, "mods": ["a", "b"]})
        self.assertEqual(values.get("count"), "2")

    def test_docs_only_change_writes_zero_mods(self) -> None:
        self.write_paths("README.md", "docs/guide.md")
        code, stdout, values = self.run_main()
        self.assertEqual(code, 0)
        self.assertEqual(json.loads(stdout), {"all": False, "mods": []})
        self.assertEqual(values, {"publish_all": "false", "mods": "", "count": "0"})

    def test_missing_paths_file_fails(self) -> None:
        code, _, values = self.run_main()
        self.assertEqual(code, 1)
        self.assertEqual(values, {})
        self.assertIn("cannot read changed paths", self.last_stderr)


if __name__ == "__main__":
    unittest.main()
