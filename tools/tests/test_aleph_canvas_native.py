"""Compile and execute Aleph Canvas regressions with the Windhawk SDK.

Set WINDHAWK_ROOT to a portable Windhawk installation when it is not in a
standard location. WH_EDITING uses the SDK's inert settings/logging helpers;
the harness calls production algorithms without loading the injected engine.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
FIXTURE = Path(__file__).with_name("fixtures") / "aleph_canvas_native.cpp"
COMPILE_TIMEOUT_SECONDS = 120
CASE_TIMEOUT_SECONDS = 10
KNOWN_ROOTS = (
    Path.home() / "scoop/apps/windhawk-dev/current",
    Path.home() / "scoop/apps/windhawk/current",
    Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Windhawk",
)
COMPILER_FLAGS = (
    "--target=x86_64-w64-mingw32",
    "-std=c++20",
    "-O2",
    "-DUNICODE",
    "-D_UNICODE",
    "-DNOMINMAX",
    "-DWINVER=0x0A00",
    "-D_WIN32_WINNT=0x0A00",
    "-DWH_MOD",
    "-DWH_EDITING",
    "-ffunction-sections",
    "-fdata-sections",
    "-Wl,--gc-sections",
    "-static",
)
WINDOWS_LIBRARIES = ("-ldwmapi", "-luser32", "-lshell32", "-lole32", "-lshcore")


def windhawk_roots() -> list[Path]:
    roots: list[Path] = []
    configured = os.environ.get("WINDHAWK_ROOT")
    if configured:
        roots.append(Path(configured))
    scoop = os.environ.get("SCOOP")
    if scoop is not None:
        roots.extend(Path(scoop) / "apps" / name / "current"
                     for name in ("windhawk-dev", "windhawk"))
    for command in ("windhawk-cli.exe", "windhawk.exe"):
        executable = shutil.which(command)
        if executable is not None:
            roots.append(Path(executable).resolve().parent)
    return roots + list(KNOWN_ROOTS)


def find_windhawk_root() -> Path:
    if os.name != "nt":
        raise unittest.SkipTest("Aleph native regressions require Windows")
    for root in windhawk_roots():
        compiler = root / "Compiler/bin/clang++.exe"
        sdk = root / "Compiler/include/windhawk_api.h"
        if compiler.is_file() and sdk.is_file():
            return root
    raise unittest.SkipTest(
        "Windhawk clang++ and SDK not found; set WINDHAWK_ROOT to the "
        "Windhawk installation (the bundle-only CI job has no compiler)"
    )


def run_process(command: list[str], timeout: int) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout,
        check=False,
    )


class AlephCanvasNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        root = find_windhawk_root()
        temporary = tempfile.TemporaryDirectory(prefix="aleph-native-")
        cls.addClassCleanup(temporary.cleanup)
        cls.executable = Path(temporary.name) / "aleph_canvas_native.exe"
        command = [str(root / "Compiler/bin/clang++.exe"), *COMPILER_FLAGS]
        command.extend(["-I", str(root / "Compiler/include"), "-I", str(REPO_ROOT)])
        command.extend([str(FIXTURE), "-o", str(cls.executable), *WINDOWS_LIBRARIES])
        result = run_process(command, COMPILE_TIMEOUT_SECONDS)
        if result.returncode != 0:
            raise AssertionError(
                f"Native harness compilation failed ({result.returncode}):\n"
                f"{result.stdout}{result.stderr}"
            )

    def run_case(self, name: str) -> None:
        try:
            result = run_process([str(self.executable), name], CASE_TIMEOUT_SECONDS)
        except subprocess.TimeoutExpired as error:
            self.fail(f"Native case {name!r} exceeded {CASE_TIMEOUT_SECONDS}s: {error}")
        self.assertEqual(
            result.returncode, 0,
            f"Native case {name!r} failed:\n{result.stdout}{result.stderr}",
        )
        self.assertEqual(result.stdout.strip(), f"PASS {name}")

    def test_snap_positive_delta(self) -> None:
        self.run_case("snap_positive")

    def test_snap_negative_delta(self) -> None:
        self.run_case("snap_negative")

    def test_disabled_snap_keeps_position(self) -> None:
        self.run_case("snap_disabled")

    def test_snap_ignores_self_and_distant_cards(self) -> None:
        self.run_case("snap_unmatched")

    def test_zoom_anchor_stays_fixed_during_animation(self) -> None:
        self.run_case("zoom_anchor")

    def test_minimap_click_centers_world_point(self) -> None:
        self.run_case("minimap_center")

    def test_degenerate_minimap_keeps_camera(self) -> None:
        self.run_case("minimap_empty")

    def test_malformed_json_terminates_and_rejects_partial_data(self) -> None:
        self.run_case("json_malformed")

    def test_disabled_layout_saving_does_not_schedule_writes(self) -> None:
        self.run_case("layout_save_disabled")

    def test_nonfinite_json_geometry_is_rejected(self) -> None:
        self.run_case("json_finite")

    def test_json_fields_are_order_independent(self) -> None:
        self.run_case("json_order")

    def test_json_roundtrip_preserves_unicode_and_geometry(self) -> None:
        self.run_case("json_roundtrip")

    def test_hotkeys_require_complete_function_key_suffix(self) -> None:
        self.run_case("hotkey_suffix")

    def test_duplicate_window_fingerprints_survive_close_and_reopen(self) -> None:
        self.run_case("fingerprint_lifetime")

    def test_fingerprint_ordinals_do_not_depend_on_scan_order(self) -> None:
        self.run_case("fingerprint_ordinals")

    def test_destroyed_or_reused_window_does_not_match_old_card(self) -> None:
        self.run_case("window_identity")

    def test_scan_limit_keeps_existing_and_largest_candidates(self) -> None:
        self.run_case("scan_candidate_selection")

    def test_header_is_28_dip_with_close_and_pin_buttons(self) -> None:
        self.run_case("header_chrome")

    def test_pinned_cards_draw_and_hit_test_on_top(self) -> None:
        self.run_case("pinned_order")

    def test_saved_pin_reaches_the_merged_card(self) -> None:
        self.run_case("pinned_layout_sync")

    def test_pin_flag_survives_serialization(self) -> None:
        self.run_case("pin_persistence")

    def test_layout_cap_evicts_the_oldest_unpinned_entry(self) -> None:
        self.run_case("layout_entry_eviction")

    def test_pin_survives_drag_and_readd_without_a_scan(self) -> None:
        self.run_case("pin_survives_layout_move")

    def test_layout_recency_follows_vector_order(self) -> None:
        self.run_case("layout_recency_order")

    def test_layout_capacity_refuses_new_entries_when_all_are_pinned(self) -> None:
        self.run_case("layout_pinned_capacity")

    def test_dismissed_windows_free_their_candidate_slots(self) -> None:
        self.run_case("dismissed_windows_skip_cut")

    def test_dismissed_window_stays_off_the_canvas(self) -> None:
        self.run_case("dismissed_cards")

    def test_other_desktop_window_is_pulled_onto_the_canvas_desktop(self) -> None:
        self.run_case("desktop_pull_moves")

    def test_window_already_on_the_canvas_desktop_is_not_moved(self) -> None:
        self.run_case("desktop_pull_same")

    def test_unusable_pull_inputs_leave_the_window_untouched(self) -> None:
        self.run_case("desktop_pull_null")

    def test_unreadable_canvas_desktop_blocks_the_pull(self) -> None:
        self.run_case("desktop_pull_reference_failure")

    def test_refused_pull_reports_its_error(self) -> None:
        self.run_case("desktop_pull_rejected")

    def test_shell_icon_candidates_start_at_the_jumbo_list(self) -> None:
        self.run_case("icon_source_order")

    def test_largest_available_shell_icon_wins(self) -> None:
        self.run_case("icon_source_largest")

    def test_shell_icon_lookup_falls_back_to_smaller_lists(self) -> None:
        self.run_case("icon_source_fallback")

    def test_missing_shell_icon_reports_no_source(self) -> None:
        self.run_case("icon_source_missing")

    def test_every_canvas_string_exists_for_every_language(self) -> None:
        self.run_case("localization_strings")

    def test_localized_badges_keep_their_translations(self) -> None:
        self.run_case("localization_badges")

    def test_language_option_table_covers_every_language(self) -> None:
        self.run_case("localization_codes")

    def test_unknown_or_auto_language_setting_falls_back_safely(self) -> None:
        self.run_case("localization_fallback")

    def test_out_of_range_localization_lookups_return_nothing(self) -> None:
        self.run_case("localization_out_of_range")

    def test_card_state_selects_the_matching_badge(self) -> None:
        self.run_case("localization_badge_selection")

if __name__ == "__main__":
    unittest.main()
