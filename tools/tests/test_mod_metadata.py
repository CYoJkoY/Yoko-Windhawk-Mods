"""Guard mod metadata against Windhawk's tool mod process rules.

Windhawk rejects a tool mod whose ``@include`` list is not one of the
process combinations it can inject into. The check runs only under
``windhawk-cli source meta --strict`` on Windows, so these tests read the
bundles the same way the compiler does and assert the same rule.
"""

from __future__ import annotations

import re
import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
DIST = REPO_ROOT / "dist"

INCLUDE_RE = re.compile(r"^//\s*@include\s+(\S+)\s*$")
TOOL_MARKERS = (
    "WhTool_ModInit",
    "WhTool_ModUninit",
    "WhTool_ModSettingsChanged",
    "-tool-mod",
)
ALLOWED_INCLUDES = (
    frozenset({"windhawk.exe"}),
    frozenset({"windhawk.exe", "windhawk-mod.exe"}),
    frozenset({"windhawk.exe", "windhawk-mod.exe", "windhawk-mod-uiaccess.exe"}),
    frozenset({"windhawk.exe", "windhawk-mod.exe", "windhawk-mod-elevated.exe"}),
    frozenset(
        {
            "windhawk.exe",
            "windhawk-mod.exe",
            "windhawk-mod-uiaccess.exe",
            "windhawk-mod-elevated.exe",
        }
    ),
)


def parse_includes(text: str) -> list[str]:
    """Return the ``@include`` targets declared in one bundled mod."""

    return [match.group(1) for line in text.splitlines() if (match := INCLUDE_RE.match(line))]


def is_tool_mod(text: str) -> bool:
    """Report whether one bundled mod is a tool mod."""

    if any(marker in text for marker in TOOL_MARKERS):
        return True
    return any(include.startswith("windhawk-") for include in parse_includes(text))


class ModMetadataTests(unittest.TestCase):
    def bundles(self) -> list[Path]:
        found = sorted(DIST.glob("*.wh.cpp"))
        if not found:
            self.skipTest("dist/ is missing; run python tools/bundle.py first")
        return found

    def test_every_bundle_declares_processes(self) -> None:
        for bundle in self.bundles():
            with self.subTest(bundle=bundle.name):
                self.assertTrue(parse_includes(bundle.read_text(encoding="utf-8")))

    def test_tool_mod_includes_are_a_valid_combination(self) -> None:
        for bundle in self.bundles():
            text = bundle.read_text(encoding="utf-8")
            with self.subTest(bundle=bundle.name):
                if is_tool_mod(text):
                    self.assertIn(frozenset(parse_includes(text)), ALLOWED_INCLUDES)


if __name__ == "__main__":
    unittest.main()

