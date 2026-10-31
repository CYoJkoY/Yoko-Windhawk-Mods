"""Exercise the shell logic embedded in the release workflows."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def _bash() -> str:
    """Find Git Bash on Windows, or use the system bash elsewhere."""

    if os.name != "nt":
        return shutil.which("bash") or "bash"
    candidates = [
        shutil.which("bash"),
        r"C:\Program Files\Git\bin\bash.exe",
        r"E:\Scoop\Apps\git\current\bin\bash.exe",
    ]
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return candidate
    return "bash"


def extract_run(path: Path, step: str) -> str:
    """Return the script of one named step from a workflow or action file."""

    lines = path.read_text(encoding="utf-8").splitlines()
    start = next(
        (index for index, line in enumerate(lines) if line.strip() == f"- name: {step}"),
        None,
    )
    if start is None:
        raise AssertionError(f"step not found: {step} in {path}")
    body: list[str] = []
    run_indent: int | None = None
    for line in lines[start + 1 :]:
        if line.startswith("      - name:") or line.startswith("    - name:"):
            break
        if run_indent is None:
            if line.strip() == "run: |":
                run_indent = len(line) - len(line.lstrip()) + 2
            continue
        if not line.strip():
            body.append("")
            continue
        indent = len(line) - len(line.lstrip())
        if indent < run_indent:
            break
        body.append(line[run_indent:])
    if run_indent is None:
        raise AssertionError(f"step has no run block: {step}")
    return "\n".join(body)


def run_script(
    script: str,
    cwd: Path,
    variables: dict[str, str],
    extra_env: dict[str, str] | None = None,
) -> tuple[int, dict[str, str], str]:
    """Run a workflow script with GitHub output files in a temporary root."""

    output = cwd.parent / f"{cwd.name}-output.txt"
    summary = cwd.parent / f"{cwd.name}-summary.md"
    output.write_text("", encoding="utf-8")
    summary.write_text("", encoding="utf-8")

    header = ["set -euo pipefail", f"GITHUB_OUTPUT='{output.as_posix()}'"]
    header.append(f"GITHUB_STEP_SUMMARY='{summary.as_posix()}'")
    for key, value in variables.items():
        header.append(f"{key}='{value}'")

    script_path = cwd.parent / f"{cwd.name}-step.sh"
    script_path.write_text("\n".join(header) + "\n" + script + "\n", encoding="utf-8", newline="\n")
    proc = subprocess.run(
        [_bash(), script_path.as_posix()],
        cwd=cwd,
        capture_output=True,
        text=True,
        env={**os.environ, **(extra_env or {})},
    )
    values: dict[str, str] = {}
    for line in output.read_text(encoding="utf-8").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    return proc.returncode, values, summary.read_text(encoding="utf-8")


def git(cwd: Path, *args: str) -> str:
    proc = subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise AssertionError(f"git {' '.join(args)} failed: {proc.stderr}")
    return proc.stdout.strip()


class SyncDistActionTests(unittest.TestCase):
    """The composite action must skip identical targets and commit real changes."""

    @classmethod
    def setUpClass(cls) -> None:
        cls._temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls._temp.name)
        cls.action = REPO_ROOT / ".github" / "actions" / "sync-dist" / "action.yml"
        cls.detect = extract_run(cls.action, "Detect target differences")
        cls.mirror = extract_run(cls.action, "Mirror dist to target mods")

    @classmethod
    def tearDownClass(cls) -> None:
        cls._temp.cleanup()

    def make_tree(
        self, name: str, target: bytes | None, root_only: bool = False
    ) -> tuple[Path, Path, Path]:
        root = self.root / name
        source = root / "source"
        target_root = root / "target"
        bundle_dir = source if root_only else source / "dist"
        bundle_dir.mkdir(parents=True)
        (target_root / "mods").mkdir(parents=True)
        (bundle_dir / "demo.wh.cpp").write_bytes(b"bundle\n")
        if target is not None:
            (target_root / "mods" / "demo.wh.cpp").write_bytes(target)
        git(source, "init", "-q", "-b", "main")
        git(source, "config", "user.email", "t@example.com")
        git(source, "config", "user.name", "T")
        git(source, "add", "-A")
        git(source, "commit", "-qm", "source")
        git(target_root, "init", "-q", "-b", "main")
        git(target_root, "config", "user.email", "t@example.com")
        git(target_root, "config", "user.name", "T")
        (target_root / "README.md").write_text("keep\n", encoding="utf-8")
        git(target_root, "add", "-A")
        git(target_root, "commit", "-qm", "target")
        return source, target_root, root

    def variables(self, source: Path, target: Path) -> dict[str, str]:
        return {
            "SOURCE_ROOT": source.as_posix(),
            "TARGET_ROOT": target.as_posix(),
            "TARGET_BRANCH": "main",
            "FILE_GLOB": "*.wh.cpp",
            "PRUNE": "false",
            "DRY_RUN": "true",
        }

    def test_identical_target_reports_no_change(self) -> None:
        source, target, root = self.make_tree("same", b"bundle\n")
        code, values, summary = run_script(self.detect, root, self.variables(source, target))
        self.assertEqual(code, 0)
        self.assertEqual(values.get("changed"), "false")
        self.assertIn("No differences", summary)

    def test_stale_target_reports_change(self) -> None:
        source, target, root = self.make_tree("stale", b"old\n")
        code, values, _ = run_script(self.detect, root, self.variables(source, target))
        self.assertEqual(code, 0)
        self.assertEqual(values.get("changed"), "true")

    def test_missing_target_reports_change(self) -> None:
        source, target, root = self.make_tree("missing", None)
        code, values, _ = run_script(self.detect, root, self.variables(source, target))
        self.assertEqual(code, 0)
        self.assertEqual(values.get("changed"), "true")

    def test_dry_run_copies_without_committing(self) -> None:
        source, target, root = self.make_tree("dry", b"old\n")
        code, values, summary = run_script(self.mirror, root, self.variables(source, target))
        self.assertEqual(code, 0)
        self.assertEqual(values.get("pushed"), "false")
        self.assertEqual((target / "mods" / "demo.wh.cpp").read_bytes(), b"bundle\n")
        self.assertEqual(git(target, "status", "--porcelain"), "M mods/demo.wh.cpp")
        self.assertIn("demo.wh.cpp", summary)

    def test_release_root_without_dist_is_supported(self) -> None:
        source, target, root = self.make_tree("release-root", b"old\n", root_only=True)
        code, values, _ = run_script(self.detect, root, self.variables(source, target))
        self.assertEqual(code, 0)
        self.assertEqual(values.get("changed"), "true")


class PublishReleaseActionTests(unittest.TestCase):
    """The release branch contains only validated bundles and nothing else."""

    @classmethod
    def setUpClass(cls) -> None:
        cls._temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls._temp.name)
        action = REPO_ROOT / ".github" / "actions" / "publish-release" / "action.yml"
        cls.publish = extract_run(action, "Publish release snapshot")

    @classmethod
    def tearDownClass(cls) -> None:
        cls._temp.cleanup()

    def make_remote(self, name: str) -> tuple[Path, Path, str]:
        origin = self.root / name / "origin.git"
        work = self.root / name / "work"
        git(self.root, "init", "-q", "--bare", "-b", "main", str(origin))
        git(self.root, "clone", "-q", str(origin), str(work))
        git(work, "config", "user.email", "t@example.com")
        git(work, "config", "user.name", "T")
        (work / "mods" / "demo" / "src").mkdir(parents=True)
        (work / "mods" / "demo" / "src" / "main.cpp").write_text("// src\n", encoding="utf-8")
        (work / "shared").mkdir()
        (work / "shared" / "helper.h").write_text("// helper\n", encoding="utf-8")
        (work / ".gitattributes").write_text("*.wh.cpp text eol=lf\n", encoding="utf-8")
        (work / "README.md").write_text("readme\n", encoding="utf-8")
        git(work, "add", "-A")
        git(work, "commit", "-qm", "[Issue-66] feat(demo): add demo mod")
        git(work, "push", "-q", "origin", "main")
        return origin, work, git(work, "rev-parse", "HEAD")

    def make_artifact(
        self, name: str, files: dict[str, str], run: str = "runner"
    ) -> Path:
        runner_temp = self.root / name / run
        artifact = runner_temp / "mod-bundles"
        artifact.mkdir(parents=True)
        for filename, content in files.items():
            (artifact / filename).write_text(content, encoding="utf-8")
        return runner_temp

    def advance_main(self, origin: Path, path: str, content: str, message: str) -> str:
        other = self.root / origin.parent.name / "other"
        git(self.root, "clone", "-q", str(origin), str(other))
        git(other, "config", "user.email", "t@example.com")
        git(other, "config", "user.name", "T")
        (other / path).write_text(content, encoding="utf-8")
        git(other, "add", "-A")
        git(other, "commit", "-qm", message)
        git(other, "push", "-q", "origin", "main")
        return git(other, "rev-parse", "HEAD")

    def run_publish(
        self, work: Path, runner_temp: Path, validated: str
    ) -> tuple[int, dict[str, str], str]:
        return run_script(
            self.publish,
            work,
            {
                "BUNDLE_DIR": (runner_temp / "mod-bundles").as_posix(),
                "VALIDATED_SHA": validated,
                "RELEASE_BRANCH": "release",
                "RUNNER_TEMP": runner_temp.as_posix(),
            },
        )

    def test_publishes_root_only_snapshot_as_orphan(self) -> None:
        origin, work, validated = self.make_remote("publish")
        runner_temp = self.make_artifact(
            "publish", {"demo.wh.cpp": "new bundle\n", "notes.txt": "ignore me\n"}
        )
        code, values, summary = self.run_publish(work, runner_temp, validated)
        self.assertEqual(code, 0)
        self.assertEqual(values.get("published"), "true")
        self.assertEqual(values.get("changed"), "true")
        release = values.get("sha")
        self.assertIsNotNone(release)
        self.assertEqual(
            git(origin, "ls-tree", "-r", "--name-only", release).splitlines(),
            [".gitignore", "demo.wh.cpp"],
        )
        self.assertEqual(git(origin, "show", f"{release}:demo.wh.cpp"), "new bundle")
        self.assertEqual(
            len(git(origin, "rev-list", "--parents", "-n", "1", release).split()), 1
        )
        self.assertEqual(
            git(origin, "log", "-1", "--format=%s", release),
            "chore(release): #66 publish bundles from " + validated[:7],
        )
        self.assertEqual(git(origin, "rev-parse", "main"), validated)
        self.assertIn("Release publish", summary)

    def test_identical_artifact_does_not_create_an_empty_commit(self) -> None:
        origin, work, validated = self.make_remote("identical")
        runner_temp = self.make_artifact("identical", {"demo.wh.cpp": "bundle\n"})
        first_code, first_values, _ = self.run_publish(work, runner_temp, validated)
        self.assertEqual(first_code, 0)
        first_release = first_values.get("sha")
        second_code, second_values, _ = self.run_publish(work, runner_temp, validated)
        self.assertEqual(second_code, 0)
        self.assertEqual(second_values.get("published"), "true")
        self.assertEqual(second_values.get("changed"), "false")
        self.assertEqual(second_values.get("sha"), first_release)
        self.assertEqual(git(origin, "rev-parse", "release"), first_release)

    def test_release_snapshot_removes_stale_bundles(self) -> None:
        origin, work, validated = self.make_remote("replace")
        first_runner = self.make_artifact(
            "replace", {"alpha.wh.cpp": "alpha\n", "demo.wh.cpp": "demo v1\n"}
        )
        code, first_values, _ = self.run_publish(work, first_runner, validated)
        self.assertEqual(code, 0)
        first_release = first_values.get("sha")
        second_runner = self.make_artifact(
            "replace", {"demo.wh.cpp": "demo v2\n"}, run="runner-2"
        )
        code, second_values, _ = self.run_publish(work, second_runner, validated)
        self.assertEqual(code, 0)
        second_release = second_values.get("sha")
        self.assertEqual(git(origin, "rev-parse", f"{second_release}^"), first_release)
        self.assertEqual(
            git(origin, "ls-tree", "-r", "--name-only", second_release).splitlines(),
            [".gitignore", "demo.wh.cpp"],
        )
        self.assertEqual(git(origin, "show", f"{second_release}:demo.wh.cpp"), "demo v2")

    def test_newer_main_skips_publication(self) -> None:
        origin, work, validated = self.make_remote("superseded")
        newer = self.advance_main(
            origin, "mods/demo/src/main.cpp", "// newer source\n", "feat: newer source"
        )
        runner_temp = self.make_artifact("superseded", {"demo.wh.cpp": "new bundle\n"})
        code, values, summary = self.run_publish(work, runner_temp, validated)
        self.assertEqual(code, 0)
        self.assertEqual(values.get("published"), "false")
        self.assertEqual(values.get("changed"), "false")
        self.assertIn("superseded", summary)
        self.assertEqual(git(origin, "rev-parse", "main"), newer)
        self.assertEqual(git(origin, "show", "main:mods/demo/src/main.cpp"), "// newer source")
        branches = git(origin, "branch", "--list", "release")
        self.assertEqual(branches, "")

    def test_empty_artifact_is_rejected(self) -> None:
        origin, work, validated = self.make_remote("empty")
        runner_temp = self.make_artifact("empty", {})
        code, values, _ = self.run_publish(work, runner_temp, validated)
        self.assertNotEqual(code, 0)
        self.assertEqual(values, {})
        self.assertEqual(git(origin, "branch", "--list", "release"), "")

    def test_invalid_bundle_name_is_rejected(self) -> None:
        origin, work, validated = self.make_remote("invalid")
        runner_temp = self.make_artifact("invalid", {"Demo.wh.cpp": "bad name\n"})
        code, values, _ = self.run_publish(work, runner_temp, validated)
        self.assertNotEqual(code, 0)
        self.assertEqual(values, {})
        self.assertEqual(git(origin, "branch", "--list", "release"), "")


if __name__ == "__main__":
    unittest.main()
