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

from tools.changed_mods import (  # noqa: E402
    MATRIX_PLACEHOLDER,
    SelectionError,
    known_mods,
    main,
    removed_mods,
    restrict_mods,
    select_mods,
)


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


class RemovedModsTests(unittest.TestCase):
    """A mod directory that disappeared leaves a branch to delete."""

    def test_deleted_directory_reports_the_mod(self) -> None:
        paths = ["mods/gone/mod.json", "mods/gone/src/main.cpp"]
        self.assertEqual(removed_mods(paths, ["a"]), ["gone"])

    def test_known_mod_is_never_reported(self) -> None:
        self.assertEqual(removed_mods(["mods/a/src/main.cpp"], ["a"]), [])

    def test_removed_docs_alone_report_nothing(self) -> None:
        self.assertEqual(removed_mods(["mods/gone/doc/README.md"], ["a"]), [])

    def test_invalid_directory_name_is_ignored(self) -> None:
        self.assertEqual(removed_mods(["mods/Bad_Name/src/main.cpp"], ["a"]), [])

    def test_unrelated_paths_report_nothing(self) -> None:
        self.assertEqual(removed_mods(["README.md", "shared/helper.h"], ["a"]), [])


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


class RestrictModsTests(unittest.TestCase):
    """An explicit id list narrows the selection without widening it."""

    def test_explicit_ids_narrow_a_change_selection(self) -> None:
        self.assertEqual(restrict_mods(["a", "b"], ["b"], ["a", "b"]), ["b"])

    def test_explicit_ids_replace_an_all_selection(self) -> None:
        self.assertEqual(restrict_mods(None, ["b", "a"], ["a", "b"]), ["a", "b"])

    def test_explicit_ids_keep_an_empty_selection_empty(self) -> None:
        self.assertEqual(restrict_mods([], ["a"], ["a"]), [])

    def test_missing_ids_leave_the_selection_untouched(self) -> None:
        self.assertEqual(restrict_mods(["a"], [], ["a"]), ["a"])

    def test_unknown_ids_are_an_error(self) -> None:
        with self.assertRaises(SelectionError):
            restrict_mods(None, ["a", "ghost"], ["a"])


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
        self.assertEqual(json.loads(stdout), {"all": False, "mods": ["a"], "removed": []})
        self.assertEqual(
            values,
            {"publish_all": "false", "mods": "a", "removed": "", "count": "1"},
        )

    def test_shared_change_writes_every_mod(self) -> None:
        self.write_paths("shared/helper.h")
        code, stdout, values = self.run_main("--trigger", "shared")
        self.assertEqual(code, 0)
        self.assertEqual(
            json.loads(stdout), {"all": True, "mods": ["a", "b"], "removed": []}
        )
        self.assertEqual(
            values,
            {"publish_all": "true", "mods": "a b", "removed": "", "count": "2"},
        )

    def test_all_flag_ignores_changed_paths(self) -> None:
        self.write_paths("README.md")
        code, stdout, values = self.run_main("--all")
        self.assertEqual(code, 0)
        self.assertEqual(
            json.loads(stdout), {"all": True, "mods": ["a", "b"], "removed": []}
        )
        self.assertEqual(values.get("count"), "2")
        self.assertEqual(values.get("removed"), "")

    def test_docs_only_change_writes_zero_mods(self) -> None:
        self.write_paths("README.md", "docs/guide.md")
        code, stdout, values = self.run_main()
        self.assertEqual(code, 0)
        self.assertEqual(json.loads(stdout), {"all": False, "mods": [], "removed": []})
        self.assertEqual(
            values,
            {"publish_all": "false", "mods": "", "removed": "", "count": "0"},
        )

    def test_deleted_mod_writes_no_publish_count_and_one_removal(self) -> None:
        self.write_paths("mods/gone/mod.json", "mods/gone/src/main.cpp")
        code, stdout, values = self.run_main()
        self.assertEqual(code, 0)
        self.assertEqual(
            json.loads(stdout), {"all": False, "mods": [], "removed": ["gone"]}
        )
        self.assertEqual(
            values,
            {"publish_all": "false", "mods": "", "removed": "gone", "count": "0"},
        )

    def test_all_flag_never_reports_removals(self) -> None:
        self.write_paths("mods/gone/mod.json")
        code, _, values = self.run_main("--all")
        self.assertEqual(code, 0)
        self.assertEqual(values.get("removed"), "")

    def test_missing_paths_file_fails(self) -> None:
        code, _, values = self.run_main()
        self.assertEqual(code, 1)
        self.assertEqual(values, {})
        self.assertIn("cannot read changed paths", self.last_stderr)

    def test_only_limits_a_shared_change(self) -> None:
        self.write_paths("shared/helper.h")
        code, stdout, values = self.run_main("--trigger", "shared", "--only", "b")
        self.assertEqual(code, 0)
        self.assertEqual(json.loads(stdout), {"all": False, "mods": ["b"], "removed": []})
        self.assertEqual(values.get("mods"), "b")

    def test_unknown_only_id_fails(self) -> None:
        self.write_paths("mods/a/src/main.cpp")
        code, _, values = self.run_main("--only", "ghost")
        self.assertEqual(code, 1)
        self.assertEqual(values, {})
        self.assertIn("unknown mod id", self.last_stderr)

    def test_matrix_lists_the_selected_mods(self) -> None:
        self.write_paths("mods/b/src/main.cpp")
        code, _, values = self.run_main("--matrix")
        self.assertEqual(code, 0)
        self.assertEqual(json.loads(values["matrix"]), ["b"])

    def test_matrix_never_expands_to_nothing(self) -> None:
        """GitHub rejects a matrix with no values, so it gets a placeholder."""

        self.write_paths("README.md")
        code, _, values = self.run_main("--matrix")
        self.assertEqual(code, 0)
        self.assertEqual(values.get("count"), "0")
        self.assertEqual(json.loads(values["matrix"]), [MATRIX_PLACEHOLDER])

    def test_matrix_is_absent_without_the_flag(self) -> None:
        self.write_paths("mods/a/src/main.cpp")
        code, _, values = self.run_main()
        self.assertEqual(code, 0)
        self.assertNotIn("matrix", values)


if __name__ == "__main__":
    unittest.main()
