"""Tests for tools/bundle.py."""

from __future__ import annotations

import json
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.bundle import BundleError, bundle_all  # noqa: E402


METADATA = """\
// ==WindhawkMod==
// @id              test-mod
// @name            Test Mod
// @version         1.2.3
// @author          Tester
// @license         MIT
// ==/WindhawkMod==
"""


def write(root: Path, relative: str, content: str) -> Path:
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(textwrap.dedent(content), encoding="utf-8")
    return path


class BundleTestCase(unittest.TestCase):
    def setUp(self) -> None:
        self._temp = tempfile.TemporaryDirectory()
        self.root = Path(self._temp.name)
        write(
            self.root,
            "mods/test-mod/mod.json",
            json.dumps({"id": "test-mod", "version": "1.2.3"}),
        )

    def tearDown(self) -> None:
        self._temp.cleanup()

    def main(self, body: str) -> None:
        write(self.root, "mods/test-mod/src/main.cpp", METADATA + "\n" + body)

    def build(self) -> tuple[str, object]:
        results = bundle_all(self.root)
        self.assertEqual(len(results), 1)
        output = self.root / "dist" / "test-mod.wh.cpp"
        return output.read_text(encoding="utf-8"), results[0]


class ExpansionTests(BundleTestCase):
    def test_nested_local_includes_are_inlined(self) -> None:
        write(
            self.root,
            "mods/test-mod/src/a.h",
            "#pragma once\n#include \"b.h\"\nint a();\n",
        )
        write(self.root, "mods/test-mod/src/b.h", "#pragma once\nint b();\n")
        self.main('#include "a.h"\nint main() { return b(); }\n')

        text, stats = self.build()
        self.assertIn("int b();", text)
        self.assertIn("int a();", text)
        self.assertLess(text.index("int b();"), text.index("int a();"))
        self.assertEqual(
            stats.files,
            (
                "mods/test-mod/src/main.cpp",
                "mods/test-mod/src/a.h",
                "mods/test-mod/src/b.h",
            ),
        )

    def test_common_header_is_inlined_once(self) -> None:
        write(self.root, "mods/test-mod/src/common.h", "#pragma once\nint common();\n")
        write(
            self.root,
            "mods/test-mod/src/a.h",
            '#pragma once\n#include "common.h"\nint a();\n',
        )
        write(
            self.root,
            "mods/test-mod/src/b.h",
            '#pragma once\n#include "common.h"\nint b();\n',
        )
        self.main('#include "a.h"\n#include "b.h"\n')

        text, _ = self.build()
        self.assertEqual(text.count("int common();"), 1)

    def test_pragma_once_is_removed(self) -> None:
        write(self.root, "mods/test-mod/src/a.h", "#pragma once\nint a();\n")
        self.main('#include "a.h"\n')

        text, _ = self.build()
        self.assertNotIn("#pragma once", text)

    def test_matching_include_guard_is_removed(self) -> None:
        write(
            self.root,
            "mods/test-mod/src/guarded.hpp",
            "#ifndef GUARDED_HPP\n#define GUARDED_HPP\nint guarded();\n#endif\n",
        )
        self.main('#include "guarded.hpp"\n')

        text, _ = self.build()
        self.assertNotIn("GUARDED_HPP", text)
        self.assertIn("int guarded();", text)

    def test_nonmatching_conditional_is_preserved(self) -> None:
        write(
            self.root,
            "mods/test-mod/src/conditional.h",
            "#ifndef OTHER_GUARD\n#define OTHER_GUARD\nint one();\n#endif\nint two();\n",
        )
        self.main('#include "conditional.h"\n')

        text, _ = self.build()
        self.assertIn("#ifndef OTHER_GUARD", text)
        self.assertIn("#endif", text)

    def test_angle_includes_are_preserved(self) -> None:
        self.main(
            "#include <windows.h>\n"
            "#include <windhawk_utils.h>\n"
            '#include "local.h"\n',
        )
        write(self.root, "mods/test-mod/src/local.h", "#pragma once\nint local();\n")

        text, _ = self.build()
        self.assertIn("#include <windows.h>", text)
        self.assertIn("#include <windhawk_utils.h>", text)
        self.assertIn("int local();", text)

    def test_subdirectory_include_resolves_relative_to_includer(self) -> None:
        write(self.root, "mods/test-mod/src/hooks/inner.h", "#pragma once\nint inner();\n")
        write(
            self.root,
            "mods/test-mod/src/hooks/outer.h",
            '#pragma once\n#include "inner.h"\nint outer();\n',
        )
        self.main('#include "hooks/outer.h"\n')

        text, _ = self.build()
        self.assertIn("int inner();", text)
        self.assertIn("int outer();", text)

    def test_quoted_include_inside_block_comment_is_ignored(self) -> None:
        self.main('/*\n#include "missing.h"\n*/\nint main() { return 0; }\n')

        text, _ = self.build()
        self.assertIn("#include \"missing.h\"", text)


class FailureTests(BundleTestCase):
    def test_missing_include_is_rejected(self) -> None:
        self.main('#include "nope.h"\n')
        with self.assertRaises(BundleError) as context:
            bundle_all(self.root)
        self.assertIn("missing include", str(context.exception))

    def test_include_cycle_is_rejected(self) -> None:
        write(self.root, "mods/test-mod/src/a.h", '#pragma once\n#include "b.h"\n')
        write(self.root, "mods/test-mod/src/b.h", '#pragma once\n#include "a.h"\n')
        self.main('#include "a.h"\n')
        with self.assertRaises(BundleError) as context:
            bundle_all(self.root)
        self.assertIn("include cycle", str(context.exception))

    def test_absolute_include_is_rejected(self) -> None:
        self.main('#include "C:/Windows/win.h"\n')
        with self.assertRaises(BundleError) as context:
            bundle_all(self.root)
        self.assertIn("absolute include", str(context.exception))

    def test_parent_traversal_is_rejected(self) -> None:
        self.main('#include "../../../../outside.h"\n')
        with self.assertRaises(BundleError) as context:
            bundle_all(self.root)
        self.assertIn("escapes the repository", str(context.exception))

    def test_missing_manifest_is_rejected(self) -> None:
        self.main("int main() { return 0; }\n")
        (self.root / "mods/test-mod/mod.json").unlink()
        with self.assertRaises(BundleError) as context:
            bundle_all(self.root)
        self.assertIn("missing mod manifest", str(context.exception))

    def test_version_mismatch_is_rejected(self) -> None:
        self.main("int main() { return 0; }\n")
        write(
            self.root,
            "mods/test-mod/mod.json",
            json.dumps({"id": "test-mod", "version": "9.9.9"}),
        )
        with self.assertRaises(BundleError) as context:
            bundle_all(self.root)
        self.assertIn("mod.json version", str(context.exception))

    def test_metadata_id_mismatch_is_rejected(self) -> None:
        write(
            self.root,
            "mods/test-mod/src/main.cpp",
            METADATA.replace("test-mod", "other-mod", 1)
            + "\nint main() { return 0; }\n",
        )
        with self.assertRaises(BundleError) as context:
            bundle_all(self.root)
        self.assertIn("metadata @id", str(context.exception))

    def test_missing_metadata_block_is_rejected(self) -> None:
        write(self.root, "mods/test-mod/src/main.cpp", "int main() { return 0; }\n")
        with self.assertRaises(BundleError) as context:
            bundle_all(self.root)
        self.assertIn("metadata block", str(context.exception))


class OutputTests(BundleTestCase):
    def test_output_starts_with_metadata_block(self) -> None:
        write(self.root, "mods/test-mod/src/a.h", "#pragma once\nint a();\n")
        self.main('#include "a.h"\n')

        text, _ = self.build()
        self.assertTrue(text.startswith("// ==WindhawkMod==\n"))

    def test_output_uses_lf_and_final_newline(self) -> None:
        self.main("int main() { return 0; }")
        self.build()
        raw = (self.root / "dist" / "test-mod.wh.cpp").read_bytes()
        self.assertNotIn(b"\r", raw)
        self.assertTrue(raw.endswith(b"\n"))
        self.assertFalse(raw.endswith(b"\n\n"))

    def test_output_is_deterministic(self) -> None:
        write(self.root, "mods/test-mod/src/a.h", "#pragma once\nint a();\n")
        self.main('#include "a.h"\n')
        first, stats = self.build()
        self.assertTrue(stats.changed)
        second, stats = self.build()
        self.assertEqual(first, second)
        self.assertFalse(stats.changed)

    def test_stats_report_line_counts(self) -> None:
        write(self.root, "mods/test-mod/src/a.h", "#pragma once\nint a();\n")
        self.main('#include "a.h"\nint main() { return a(); }\n')

        _, stats = self.build()
        self.assertGreater(stats.source_lines, stats.output_lines)
        self.assertGreater(stats.ratio, 0)
        self.assertEqual(stats.entry, "mods/test-mod/src/main.cpp")
        self.assertEqual(stats.output, "dist/test-mod.wh.cpp")

    def test_check_mode_detects_stale_output(self) -> None:
        self.main("int main() { return 0; }\n")
        bundle_all(self.root)

        results = bundle_all(self.root, check=True)
        self.assertFalse(results[0].changed)

        self.main("int main() { return 1; }\n")
        with self.assertRaises(BundleError) as context:
            bundle_all(self.root, check=True)
        self.assertIn("stale bundle", str(context.exception))

    def test_check_mode_reports_missing_output(self) -> None:
        self.main("int main() { return 0; }\n")
        with self.assertRaises(BundleError) as context:
            bundle_all(self.root, check=True)
        self.assertIn("stale bundle", str(context.exception))


class RepositoryIntegrationTests(unittest.TestCase):
    def test_repository_bundles_are_up_to_date(self) -> None:
        if not any((REPO_ROOT / "mods").glob("*/src/main.cpp")):
            self.skipTest("repository has no modular mods yet")
        results = bundle_all(REPO_ROOT, check=True)
        self.assertTrue(results)
        for stats in results:
            self.assertFalse(stats.changed)
            self.assertGreater(stats.source_lines, 0)
            self.assertIn(f"mods/{stats.mod_id}/src/main.cpp", stats.files)


if __name__ == "__main__":
    unittest.main()
