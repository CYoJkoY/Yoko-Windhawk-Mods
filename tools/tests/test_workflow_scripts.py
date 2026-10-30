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
    run_seen = False
    for line in lines[start + 1 :]:
        if line.startswith("      - name:") or line.startswith("    - name:"):
            break
        if not run_seen:
            if line.strip() == "run: |":
                run_seen = True
            continue
        if not line.strip():
            body.append("")
            continue
        if not line.startswith("        "):
            break
        body.append(line[8:])
    if not run_seen:
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

    def make_tree(self, name: str, target: bytes | None) -> tuple[Path, Path, Path]:
        root = self.root / name
        source = root / "source"
        target_root = root / "target"
        (source / "dist").mkdir(parents=True)
        (target_root / "mods").mkdir(parents=True)
        (source / "dist" / "demo.wh.cpp").write_bytes(b"bundle\n")
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


class BundleComparisonTests(unittest.TestCase):
    """A push must publish only when the regenerated bundles differ."""

    @classmethod
    def setUpClass(cls) -> None:
        cls._temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls._temp.name)
        workflow = REPO_ROOT / ".github" / "workflows" / "validate-mods.yml"
        cls.compare = extract_run(workflow, "Compare regenerated bundles")

    @classmethod
    def tearDownClass(cls) -> None:
        cls._temp.cleanup()

    def make_repo(self, name: str) -> tuple[Path, str]:
        repo = self.root / name
        repo.mkdir(parents=True)
        git(repo, "init", "-q", "-b", "main")
        git(repo, "config", "user.email", "t@example.com")
        git(repo, "config", "user.name", "T")
        (repo / "dist").mkdir()
        (repo / "dist" / "demo.wh.cpp").write_text("bundle\n", encoding="utf-8")
        git(repo, "add", "-A")
        git(repo, "commit", "-qm", "initial")
        return repo, git(repo, "rev-parse", "HEAD")

    def test_identical_bundle_and_untouched_dist_skip_publish(self) -> None:
        repo, base = self.make_repo("identical")
        (repo / "README.md").write_text("docs\n", encoding="utf-8")
        git(repo, "add", "-A")
        git(repo, "commit", "-qm", "docs")
        code, values, summary = run_script(
            self.compare, repo, {"BASE_SHA": base}
        )
        self.assertEqual(code, 0)
        self.assertEqual(values.get("regenerated"), "false")
        self.assertEqual(values.get("dist_changed"), "false")
        self.assertIn("no commit is published", summary)

    def test_identical_bundle_with_dist_change_still_syncs(self) -> None:
        repo, base = self.make_repo("dist-only")
        (repo / "dist" / "demo.wh.cpp").write_text("bundle v2\n", encoding="utf-8")
        git(repo, "add", "-A")
        git(repo, "commit", "-qm", "dist")
        code, values, _ = run_script(self.compare, repo, {"BASE_SHA": base})
        self.assertEqual(code, 0)
        self.assertEqual(values.get("regenerated"), "false")
        self.assertEqual(values.get("dist_changed"), "true")

    def test_regenerated_bundle_without_committed_dist_is_flagged(self) -> None:
        repo, base = self.make_repo("regenerated")
        (repo / "README.md").write_text("docs\n", encoding="utf-8")
        git(repo, "add", "-A")
        git(repo, "commit", "-qm", "docs")
        # The bundle step writes its output but commits nothing; this dirty
        # file is the regenerated content the comparison must notice.
        (repo / "dist" / "demo.wh.cpp").write_text("regenerated\n", encoding="utf-8")
        code, values, summary = run_script(self.compare, repo, {"BASE_SHA": base})
        self.assertEqual(code, 0)
        self.assertEqual(values.get("regenerated"), "true")
        self.assertEqual(values.get("dist_changed"), "true")
        self.assertIn("demo.wh.cpp", summary)

    def test_new_bundle_is_detected_as_untracked(self) -> None:
        repo, _ = self.make_repo("new")
        (repo / "dist" / "added.wh.cpp").write_text("new\n", encoding="utf-8")
        code, values, _ = run_script(self.compare, repo, {"BASE_SHA": ""})
        self.assertEqual(code, 0)
        self.assertEqual(values.get("regenerated"), "true")
        self.assertEqual(values.get("dist_changed"), "true")


class ValidationGateTests(unittest.TestCase):
    """The fallback sync accepts only bundles a successful run covered."""

    @classmethod
    def setUpClass(cls) -> None:
        cls._temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls._temp.name)
        workflow = REPO_ROOT / ".github" / "workflows" / "sync-mods.yml"
        cls.gate = extract_run(workflow, "Require successful validation")

    @classmethod
    def tearDownClass(cls) -> None:
        cls._temp.cleanup()

    def make_source(self, name: str, advance: bool = True) -> tuple[Path, str, str]:
        source = self.root / name / "source"
        source.mkdir(parents=True)
        git(source, "init", "-q", "-b", "main")
        git(source, "config", "user.email", "t@example.com")
        git(source, "config", "user.name", "T")
        (source / "README.md").write_text("readme\n", encoding="utf-8")
        git(source, "add", "-A")
        git(source, "commit", "-qm", "initial")
        validated = git(source, "rev-parse", "HEAD")
        head = validated
        if advance:
            (source / "README.md").write_text("docs\n", encoding="utf-8")
            git(source, "add", "-A")
            git(source, "commit", "-qm", "docs")
            head = git(source, "rev-parse", "HEAD")
        return source, validated, head

    def variables(self, source: Path) -> dict[str, str]:
        return {
            "GITHUB_WORKSPACE": source.parent.as_posix(),
            "GITHUB_REPOSITORY": "owner/repo",
            "DRY_RUN": "false",
        }

    def test_dry_run_bypasses_gate(self) -> None:
        source, _, _ = self.make_source("dry")
        variables = self.variables(source)
        variables["DRY_RUN"] = "true"
        code, values, _ = run_script(self.gate, source, variables)
        self.assertEqual(code, 0)
        self.assertEqual(values.get("ready"), "true")

    def test_covered_commit_passes(self) -> None:
        source, validated, _ = self.make_source("covered", advance=False)
        runs = self.root / "covered" / "runs.json"
        runs.write_text(f"success\t{validated}\n", encoding="utf-8")
        code, values, _ = run_script(
            self.gate, source, self.variables(source), self.fake_gh(runs)
        )
        self.assertEqual(code, 0)
        self.assertEqual(values.get("ready"), "true")

    def test_unvalidated_commit_after_success_blocks(self) -> None:
        source, validated, head = self.make_source("after")
        runs = self.root / "after" / "runs.json"
        runs.write_text(f"success\t{validated}\n", encoding="utf-8")
        code, values, _ = run_script(
            self.gate, source, self.variables(source), self.fake_gh(runs)
        )
        self.assertEqual(code, 0)
        self.assertNotEqual(validated, head)
        self.assertEqual(values.get("ready"), "false")

    def test_failed_validation_blocks(self) -> None:
        source, validated, _ = self.make_source("failed")
        runs = self.root / "failed" / "runs.json"
        runs.write_text(f"failure\t{validated}\n", encoding="utf-8")
        code, values, summary = run_script(
            self.gate, source, self.variables(source), self.fake_gh(runs)
        )
        self.assertEqual(code, 0)
        self.assertEqual(values.get("ready"), "false")
        self.assertIn("Sync skipped", summary)

    def fake_gh(self, runs: Path) -> dict[str, str]:
        """Put a stub gh on PATH that returns one conclusion and head SHA."""

        bindir = runs.parent / "bin"
        bindir.mkdir(exist_ok=True)
        gh = bindir / "gh"
        gh.write_text('#!/usr/bin/env bash\ncat "$FAKE_RUNS"\n', encoding="utf-8", newline="\n")
        gh.chmod(0o755)
        return {
            "PATH": f"{bindir.as_posix()}{os.pathsep}{os.environ['PATH']}",
            "FAKE_RUNS": runs.as_posix(),
        }


class PublishDistTests(unittest.TestCase):
    """The publish job must commit only validated dist and never clobber newer sources."""

    @classmethod
    def setUpClass(cls) -> None:
        cls._temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls._temp.name)
        workflow = REPO_ROOT / ".github" / "workflows" / "validate-mods.yml"
        cls.publish = extract_run(workflow, "Commit regenerated dist")

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
        (work / "dist").mkdir()
        (work / "dist" / "demo.wh.cpp").write_text("old bundle\n", encoding="utf-8")
        (work / "README.md").write_text("readme\n", encoding="utf-8")
        git(work, "add", "-A")
        git(work, "commit", "-qm", "[Issue-64] feat(demo): add demo mod")
        git(work, "push", "-q", "origin", "main")
        return origin, work, git(work, "rev-parse", "HEAD")

    def make_artifact(self, name: str, content: str) -> Path:
        runner_temp = self.root / name / "runner"
        artifact = runner_temp / "mod-bundles"
        artifact.mkdir(parents=True)
        (artifact / "demo.wh.cpp").write_text(content, encoding="utf-8")
        return runner_temp

    def advance_remote(self, origin: Path, path: str, content: str, message: str) -> str:
        other = self.root / origin.parent.name / "other"
        git(self.root, "clone", "-q", str(origin), str(other))
        git(other, "config", "user.email", "t@example.com")
        git(other, "config", "user.name", "T")
        (other / path).write_text(content, encoding="utf-8")
        git(other, "add", "-A")
        git(other, "commit", "-qm", message)
        git(other, "push", "-q", "origin", "main")
        return git(other, "rev-parse", "HEAD")

    def test_publishes_dist_only_commit(self) -> None:
        origin, work, validated = self.make_remote("publish")
        runner_temp = self.make_artifact("publish", "new bundle\n")
        code, values, summary = run_script(
            self.publish, work, {"RUNNER_TEMP": runner_temp.as_posix()}
        )
        self.assertEqual(code, 0)
        self.assertEqual(values.get("published"), "true")
        published = values.get("sha")
        self.assertIsNotNone(published)
        self.assertNotEqual(published, validated)
        self.assertEqual(git(origin, "show", f"{published}:dist/demo.wh.cpp"), "new bundle")
        changed = git(origin, "diff", "--name-only", f"{validated}", published).splitlines()
        self.assertEqual(changed, ["dist/demo.wh.cpp"])
        self.assertIn("chore(dist): #64", git(origin, "log", "-1", "--format=%s", published))
        self.assertIn("Published dist", summary)

    def test_replays_on_newer_docs_commit(self) -> None:
        origin, work, validated = self.make_remote("replay")
        newer = self.advance_remote(origin, "README.md", "updated readme\n", "docs: update readme")
        runner_temp = self.make_artifact("replay", "new bundle\n")
        code, values, _ = run_script(
            self.publish, work, {"RUNNER_TEMP": runner_temp.as_posix()}
        )
        self.assertEqual(code, 0)
        self.assertEqual(values.get("published"), "true")
        published = values.get("sha")
        self.assertEqual(git(origin, "rev-parse", f"{published}^"), newer)
        self.assertEqual(git(origin, "show", f"{published}:README.md"), "updated readme")
        self.assertEqual(git(origin, "show", f"{published}:dist/demo.wh.cpp"), "new bundle")

    def test_refuses_to_overwrite_newer_sources(self) -> None:
        origin, work, _ = self.make_remote("superseded")
        newer = self.advance_remote(
            origin, "mods/demo/src/main.cpp", "// newer source\n", "feat: newer source"
        )
        runner_temp = self.make_artifact("superseded", "new bundle\n")
        code, values, summary = run_script(
            self.publish, work, {"RUNNER_TEMP": runner_temp.as_posix()}
        )
        self.assertEqual(code, 0)
        self.assertEqual(values.get("published"), "false")
        self.assertEqual(git(origin, "rev-parse", "main"), newer)
        self.assertIn("superseded", summary)

    def test_reuses_main_that_already_has_the_bundle(self) -> None:
        origin, work, _ = self.make_remote("already")
        newer = self.advance_remote(
            origin, "dist/demo.wh.cpp", "new bundle\n", "chore(dist): already published"
        )
        runner_temp = self.make_artifact("already", "new bundle\n")
        code, values, _ = run_script(
            self.publish, work, {"RUNNER_TEMP": runner_temp.as_posix()}
        )
        self.assertEqual(code, 0)
        self.assertEqual(values.get("published"), "true")
        self.assertEqual(values.get("sha"), newer)
        self.assertEqual(git(origin, "rev-parse", "main"), newer)

    def test_refuses_to_overwrite_a_newer_direct_dist_commit(self) -> None:
        origin, work, _ = self.make_remote("dist-race")
        newer = self.advance_remote(
            origin, "dist/demo.wh.cpp", "hand edited\n", "fix: hand edit dist"
        )
        runner_temp = self.make_artifact("dist-race", "new bundle\n")
        code, values, summary = run_script(
            self.publish, work, {"RUNNER_TEMP": runner_temp.as_posix()}
        )
        self.assertEqual(code, 0)
        self.assertEqual(values.get("published"), "false")
        self.assertEqual(git(origin, "rev-parse", "main"), newer)
        self.assertEqual(git(origin, "show", "main:dist/demo.wh.cpp"), "hand edited")
        self.assertIn("superseded", summary)


class SyncFreshnessTests(unittest.TestCase):
    """A queued sync must not mirror dist that main has already replaced."""

    @classmethod
    def setUpClass(cls) -> None:
        cls._temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls._temp.name)
        workflow = REPO_ROOT / ".github" / "workflows" / "validate-mods.yml"
        cls.fresh = extract_run(workflow, "Confirm main still has this dist")

    @classmethod
    def tearDownClass(cls) -> None:
        cls._temp.cleanup()

    def make_checkout(self, name: str) -> tuple[Path, Path, Path]:
        origin = self.root / name / "origin.git"
        author = self.root / name / "author"
        runner = self.root / name / "runner"
        git(self.root, "init", "-q", "--bare", "-b", "main", str(origin))
        git(self.root, "clone", "-q", str(origin), str(author))
        git(author, "config", "user.email", "t@example.com")
        git(author, "config", "user.name", "T")
        (author / "dist").mkdir()
        (author / "dist" / "demo.wh.cpp").write_text("bundle\n", encoding="utf-8")
        git(author, "add", "-A")
        git(author, "commit", "-qm", "initial")
        git(author, "push", "-q", "origin", "main")
        git(
            self.root,
            "clone",
            "-q",
            "--depth",
            "1",
            f"file://{origin.as_posix()}",
            str(runner),
        )
        return origin, author, runner

    def test_skips_when_main_has_newer_bundles(self) -> None:
        origin, author, runner = self.make_checkout("newer")
        (author / "dist" / "demo.wh.cpp").write_text("newer\n", encoding="utf-8")
        git(author, "add", "-A")
        git(author, "commit", "-qm", "chore(dist): newer")
        git(author, "push", "-q", "origin", "main")
        code, values, summary = run_script(self.fresh, runner, {})
        self.assertEqual(code, 0)
        self.assertEqual(values.get("current"), "false")
        self.assertIn("newer validation run", summary)

    def test_proceeds_when_main_still_has_this_dist(self) -> None:
        _, author, runner = self.make_checkout("same")
        (author / "README.md").write_text("docs\n", encoding="utf-8")
        git(author, "add", "-A")
        git(author, "commit", "-qm", "docs")
        git(author, "push", "-q", "origin", "main")
        code, values, _ = run_script(self.fresh, runner, {})
        self.assertEqual(code, 0)
        self.assertEqual(values.get("current"), "true")


if __name__ == "__main__":
    unittest.main()
