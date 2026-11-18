"""Exercise the shell logic embedded in the release workflows."""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
PUBLISH_ACTION = REPO_ROOT / ".github" / "actions" / "publish-mod-branches"
COMPILE_ACTION = REPO_ROOT / ".github" / "actions" / "compile-mod"


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


def run_script_verbose(
    script: str,
    cwd: Path,
    variables: dict[str, str],
    extra_env: dict[str, str] | None = None,
) -> tuple[subprocess.CompletedProcess[str], dict[str, str], str]:
    """Run a workflow script and return its process, outputs and summary."""

    output = cwd.parent / f"{cwd.name}-output.txt"
    summary = cwd.parent / f"{cwd.name}-summary.md"
    output.write_text("", encoding="utf-8")
    summary.write_text("", encoding="utf-8")

    header = ["set -euo pipefail", f"GITHUB_OUTPUT='{output.as_posix()}'"]
    header.append(f"GITHUB_STEP_SUMMARY='{summary.as_posix()}'")
    header.append(f"GITHUB_ACTION_PATH='{PUBLISH_ACTION.as_posix()}'")
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
    return proc, values, summary.read_text(encoding="utf-8")


def run_script(
    script: str,
    cwd: Path,
    variables: dict[str, str],
    extra_env: dict[str, str] | None = None,
) -> tuple[int, dict[str, str], str]:
    """Run a workflow script with GitHub output files in a temporary root."""

    proc, values, summary = run_script_verbose(script, cwd, variables, extra_env)
    return proc.returncode, values, summary


def git(cwd: Path, *args: str) -> str:
    proc = subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise AssertionError(f"git {' '.join(args)} failed: {proc.stderr}")
    return proc.stdout.strip()


MOD_TEMPLATE = """\
// ==WindhawkMod==
// @id              {mod_id}
// @name            {mod_id}
// @version         {version}
// @author          Tester
// @github          https://github.com/CYoJkoY
// ==/WindhawkMod==

int entry()
{{
    return {value};
}}
"""


def bundle_text(mod_id: str, version: str, value: int = 0) -> str:
    return MOD_TEMPLATE.format(mod_id=mod_id, version=version, value=value)


def make_gh_stub(
    directory: Path, existing: str = "", edit_failure: str = ""
) -> tuple[Path, dict[str, str]]:
    """Fake `gh` that records calls and answers pr list/create/edit.

    `edit_failure` makes `pr edit` fail with that message on stderr.
    """

    directory.mkdir(parents=True, exist_ok=True)
    script = directory / "gh"
    script.write_text(
        "\n".join(
            [
                "#!/usr/bin/env bash",
                'printf \'%s\\n\' "$*" >> "$GH_LOG"',
                'if [ "$1" = "pr" ] && [ "$2" = "list" ]; then',
                f'  printf \'%s\' "{existing}"',
                'elif [ "$1" = "pr" ] && [ "$2" = "create" ]; then',
                '  echo "https://github.com/ramensoftware/windhawk-mods/pull/321"',
                'elif [ "$1" = "pr" ] && [ "$2" = "edit" ]; then',
                f'  if [ -n "{edit_failure}" ]; then',
                f'    printf \'%s\\n\' "{edit_failure}" >&2',
                "    exit 1",
                "  fi",
                "fi",
                "exit 0",
                "",
            ]
        ),
        encoding="utf-8",
        newline="\n",
    )
    os.chmod(script, 0o755)
    log = directory / "gh.log"
    log.write_text("", encoding="utf-8")
    env = {
        "PATH": os.pathsep.join([str(directory), os.environ.get("PATH", "")]),
        "GH_LOG": log.as_posix(),
    }
    return log, env


class PublishModBranchesActionTests(unittest.TestCase):
    """Every mod is published as one atomic commit on its own branch."""

    @classmethod
    def setUpClass(cls) -> None:
        cls._temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls._temp.name)
        action = PUBLISH_ACTION / "action.yml"
        cls.branches = extract_run(action, "Publish mod branches")
        cls.pull_requests = extract_run(action, "Open or refresh upstream pull requests")
        cls.report = extract_run(action, "Report failures")

    @classmethod
    def tearDownClass(cls) -> None:
        cls._temp.cleanup()

    def make_repositories(self, name: str) -> tuple[Path, Path, str]:
        base = self.root / name
        upstream = base / "upstream.git"
        target = base / "target.git"
        seed = base / "seed"
        git(self.root, "init", "-q", "--bare", "-b", "main", str(upstream))
        git(self.root, "init", "-q", "--bare", "-b", "main", str(target))
        git(self.root, "clone", "-q", str(upstream), str(seed))
        git(seed, "config", "user.email", "t@example.com")
        git(seed, "config", "user.name", "T")
        (seed / "mods").mkdir()
        (seed / "mods" / "demo.wh.cpp").write_text(bundle_text("demo", "1.0.0"), encoding="utf-8")
        (seed / "mods" / "other.wh.cpp").write_text(bundle_text("other", "2.0.0"), encoding="utf-8")
        (seed / "README.md").write_text("readme\n", encoding="utf-8")
        (seed / ".gitattributes").write_text("*.wh.cpp text eol=lf\n", encoding="utf-8")
        git(seed, "add", "-A")
        git(seed, "commit", "-qm", "seed: upstream mods")
        git(seed, "push", "-q", "origin", "main")
        git(seed, "push", "-q", str(target), "main")
        return upstream, target, git(seed, "rev-parse", "HEAD")

    def make_bundles(self, name: str, files: dict[str, str]) -> Path:
        bundle_dir = self.root / name / "bundles"
        bundle_dir.mkdir(parents=True)
        for filename, content in files.items():
            (bundle_dir / filename).write_text(content, encoding="utf-8", newline="\n")
        return bundle_dir

    def variables(
        self,
        bundle_dir: Path,
        target: Path,
        upstream: Path,
        runner_temp: Path,
        **overrides: str,
    ) -> dict[str, str]:
        values = {
            "BUNDLE_DIR": bundle_dir.as_posix(),
            "TARGET_REPO": "CYoJkoY/windhawk-mods",
            "UPSTREAM_REPO": "ramensoftware/windhawk-mods",
            "TARGET_URL": target.as_posix(),
            "UPSTREAM_URL": upstream.as_posix(),
            "TOKEN": "",
            "BRANCH_PREFIX": "mods/",
            "BASE_BRANCH": "main",
            "MOD_FILTER": "",
            "CHANGELOG": "feat(demo): refresh demo",
            "DRY_RUN": "false",
            "RUNNER_TEMP": runner_temp.as_posix(),
        }
        values.update(overrides)
        return values

    def runner_temp(self, name: str) -> Path:
        path = self.root / name / "runner"
        path.mkdir(parents=True, exist_ok=True)
        return path

    def run_report(
        self,
        runner: Path,
        branch_values: dict[str, str],
        pr_failed: str = "",
        branch_outcome: str = "success",
        pr_outcome: str = "success",
    ) -> tuple[int, str]:
        code, _, summary = run_script(
            self.report,
            runner,
            {
                "BRANCH_FAILED": branch_values.get("failed", ""),
                "PR_FAILED": pr_failed,
                "BRANCH_OUTCOME": branch_outcome,
                "PR_OUTCOME": pr_outcome,
                "RUNNER_TEMP": runner.as_posix(),
            },
        )
        return code, summary

    def run_pull_requests(
        self,
        name: str,
        runner: Path,
        variables: dict[str, str],
        status_file: str,
        existing: str = "",
        edit_failure: str = "",
    ) -> tuple[int, str, dict[str, str], str]:
        """Run the pull request step with a stubbed `gh`."""

        log, env = make_gh_stub(
            self.root / name / "bin", existing=existing, edit_failure=edit_failure
        )
        values = {
            "STATUS_FILE": status_file,
            "TARGET_REPO": variables["TARGET_REPO"],
            "UPSTREAM_REPO": variables["UPSTREAM_REPO"],
            "BASE_BRANCH": variables["BASE_BRANCH"],
            "TOKEN": variables["TOKEN"],
            "AUTHORSHIP": "The submitter, with AI assistance",
            "CHANGELOG": variables["CHANGELOG"],
            "RUNNER_TEMP": variables["RUNNER_TEMP"],
        }
        code, outputs, summary = run_script(
            self.pull_requests, runner, values, extra_env=env
        )
        return code, summary, outputs, log.read_text(encoding="utf-8")

    def pull_request_body(self, variables: dict[str, str], mod_id: str) -> str:
        return (
            Path(variables["RUNNER_TEMP"])
            / "publish-mod-branches"
            / f"pr-body.{mod_id}.md"
        ).read_text(encoding="utf-8")

    def test_new_mod_becomes_one_commit_on_the_upstream_tip(self) -> None:
        upstream, target, base_sha = self.make_repositories("new")
        bundle_dir = self.make_bundles(
            "new", {"demo.wh.cpp": bundle_text("demo", "1.1.0", 1)}
        )
        runner = self.runner_temp("new")
        code, values, summary = run_script(
            self.branches,
            runner,
            self.variables(bundle_dir, target, upstream, runner),
        )
        self.assertEqual(code, 0, summary)
        self.assertEqual(values.get("pushed"), "1")
        self.assertEqual(values.get("failed"), "0")

        head = git(target, "rev-parse", "mods/demo")
        self.assertEqual(git(target, "rev-list", "--count", f"{base_sha}..{head}"), "1")
        self.assertEqual(git(target, "rev-parse", f"{head}^"), base_sha)
        self.assertEqual(
            git(target, "diff", "--name-only", base_sha, head).splitlines(),
            ["mods/demo.wh.cpp"],
        )
        self.assertEqual(git(target, "show", f"{head}:mods/other.wh.cpp"), bundle_text("other", "2.0.0").rstrip("\n"))
        self.assertEqual(git(target, "show", f"{head}:mods/demo.wh.cpp"), bundle_text("demo", "1.1.0", 1).rstrip("\n"))
        self.assertEqual(
            git(target, "log", "-1", "--format=%s", head),
            "Update mods/demo.wh.cpp to v1.1.0",
        )
        self.assertIn("feat(demo): refresh demo", git(target, "log", "-1", "--format=%b", head))
        self.assertIn("| `demo` | 1.1.0 | `mods/demo` | pushed", summary)

    def test_new_mod_commit_subject_names_the_addition(self) -> None:
        upstream, target, _ = self.make_repositories("added")
        bundle_dir = self.make_bundles(
            "added", {"fresh-mod.wh.cpp": bundle_text("fresh-mod", "0.1.0")}
        )
        runner = self.runner_temp("added")
        code, values, _ = run_script(
            self.branches, runner, self.variables(bundle_dir, target, upstream, runner)
        )
        self.assertEqual(code, 0)
        head = git(target, "rev-parse", "mods/fresh-mod")
        self.assertEqual(
            git(target, "log", "-1", "--format=%s", head), "Add mod: fresh-mod (0.1.0)"
        )
        self.assertEqual(
            git(target, "diff", "--name-only", "main", head).splitlines(),
            ["mods/fresh-mod.wh.cpp"],
        )

    def test_repeated_publish_keeps_the_branch_untouched(self) -> None:
        upstream, target, base_sha = self.make_repositories("repeat")
        bundle_dir = self.make_bundles(
            "repeat", {"demo.wh.cpp": bundle_text("demo", "1.1.0", 1)}
        )
        runner = self.runner_temp("repeat")
        first = run_script(
            self.branches, runner, self.variables(bundle_dir, target, upstream, runner)
        )
        self.assertEqual(first[0], 0)
        head = git(target, "rev-parse", "mods/demo")

        second = run_script(
            self.branches, runner, self.variables(bundle_dir, target, upstream, runner)
        )
        self.assertEqual(second[0], 0)
        self.assertEqual(second[1].get("pushed"), "0")
        self.assertEqual(second[1].get("unchanged"), "1")
        self.assertEqual(git(target, "rev-parse", "mods/demo"), head)
        self.assertEqual(git(target, "rev-parse", f"{head}^"), base_sha)

    def test_updated_bundle_replaces_the_branch_commit(self) -> None:
        upstream, target, base_sha = self.make_repositories("update")
        first_dir = self.make_bundles(
            "update", {"demo.wh.cpp": bundle_text("demo", "1.1.0", 1)}
        )
        runner = self.runner_temp("update")
        first = run_script(
            self.branches, runner, self.variables(first_dir, target, upstream, runner)
        )
        self.assertEqual(first[0], 0)
        first_head = git(target, "rev-parse", "mods/demo")

        second_dir = self.root / "update" / "bundles-v2"
        second_dir.mkdir(parents=True)
        (second_dir / "demo.wh.cpp").write_text(
            bundle_text("demo", "1.2.0", 2), encoding="utf-8", newline="\n"
        )
        second = run_script(
            self.branches, runner, self.variables(second_dir, target, upstream, runner)
        )
        self.assertEqual(second[0], 0)
        self.assertEqual(second[1].get("pushed"), "1")
        second_head = git(target, "rev-parse", "mods/demo")
        self.assertNotEqual(second_head, first_head)
        self.assertEqual(git(target, "rev-parse", f"{second_head}^"), base_sha)
        self.assertEqual(git(target, "rev-list", "--count", "main..mods/demo"), "1")
        self.assertEqual(
            git(target, "log", "-1", "--format=%s", second_head),
            "Update mods/demo.wh.cpp to v1.2.0",
        )

    def test_branch_left_on_an_old_base_is_rebuilt(self) -> None:
        upstream, target, _ = self.make_repositories("stale")
        bundle_dir = self.make_bundles(
            "stale", {"demo.wh.cpp": bundle_text("demo", "1.1.0", 1)}
        )
        runner = self.runner_temp("stale")
        code, _, _ = run_script(
            self.branches, runner, self.variables(bundle_dir, target, upstream, runner)
        )
        self.assertEqual(code, 0)
        old_head = git(target, "rev-parse", "mods/demo")

        movert = self.root / "stale" / "movert"
        git(self.root, "clone", "-q", str(upstream), str(movert))
        git(movert, "config", "user.email", "t@example.com")
        git(movert, "config", "user.name", "T")
        (movert / "README.md").write_text("readme v2\n", encoding="utf-8")
        git(movert, "add", "-A")
        git(movert, "commit", "-qm", "docs: newer upstream")
        git(movert, "push", "-q", "origin", "main")
        new_base = git(movert, "rev-parse", "HEAD")

        code, values, _ = run_script(
            self.branches, runner, self.variables(bundle_dir, target, upstream, runner)
        )
        self.assertEqual(code, 0)
        self.assertEqual(values.get("pushed"), "1")
        new_head = git(target, "rev-parse", "mods/demo")
        self.assertNotEqual(new_head, old_head)
        self.assertEqual(git(target, "rev-parse", f"{new_head}^"), new_base)
        self.assertEqual(
            git(target, "diff", "--name-only", new_base, new_head).splitlines(),
            ["mods/demo.wh.cpp"],
        )
        self.assertEqual(git(target, "show", f"{new_head}:README.md"), "readme v2")

    def test_bundle_matching_upstream_creates_no_branch(self) -> None:
        upstream, target, _ = self.make_repositories("current")
        bundle_dir = self.make_bundles(
            "current", {"demo.wh.cpp": bundle_text("demo", "1.0.0")}
        )
        runner = self.runner_temp("current")
        code, values, summary = run_script(
            self.branches, runner, self.variables(bundle_dir, target, upstream, runner)
        )
        self.assertEqual(code, 0)
        self.assertEqual(values.get("current"), "1")
        self.assertEqual(values.get("pushed"), "0")
        self.assertEqual(git(target, "branch", "--list", "mods/demo"), "")
        self.assertIn("already upstream", summary)

    def test_mod_filter_limits_the_branches(self) -> None:
        upstream, target, _ = self.make_repositories("filter")
        bundle_dir = self.make_bundles(
            "filter",
            {
                "demo.wh.cpp": bundle_text("demo", "1.1.0", 1),
                "other.wh.cpp": bundle_text("other", "2.1.0", 2),
            },
        )
        runner = self.runner_temp("filter")
        code, values, summary = run_script(
            self.branches,
            runner,
            self.variables(bundle_dir, target, upstream, runner, MOD_FILTER="other"),
        )
        self.assertEqual(code, 0)
        self.assertEqual(values.get("pushed"), "1")
        self.assertEqual(git(target, "branch", "--list", "mods/demo"), "")
        self.assertEqual(git(target, "branch", "--list", "mods/other"), "mods/other")
        self.assertNotIn("| `demo` |", summary)

    def test_reused_version_fails_before_pushing(self) -> None:
        upstream, target, _ = self.make_repositories("version")
        bundle_dir = self.make_bundles(
            "version", {"demo.wh.cpp": bundle_text("demo", "1.0.0", 9)}
        )
        runner = self.runner_temp("version")
        code, values, summary = run_script(
            self.branches, runner, self.variables(bundle_dir, target, upstream, runner)
        )
        self.assertEqual(code, 0)
        self.assertEqual(values.get("failed"), "1")
        self.assertIn("version already used", summary)
        self.assertEqual(git(target, "branch", "--list", "mods/demo"), "")
        self.assertNotEqual(self.run_report(runner, values)[0], 0)

    def test_dry_run_reports_without_pushing(self) -> None:
        upstream, target, _ = self.make_repositories("dry")
        bundle_dir = self.make_bundles(
            "dry", {"demo.wh.cpp": bundle_text("demo", "1.1.0", 1)}
        )
        runner = self.runner_temp("dry")
        code, values, summary = run_script(
            self.branches,
            runner,
            self.variables(bundle_dir, target, upstream, runner, DRY_RUN="true"),
        )
        self.assertEqual(code, 0)
        self.assertEqual(values.get("pushed"), "1")
        self.assertEqual(git(target, "branch", "--list", "mods/demo"), "")
        self.assertIn("dry run", summary)

    def test_one_failing_mod_does_not_block_the_others(self) -> None:
        upstream, target, _ = self.make_repositories("partial")
        bundle_dir = self.make_bundles(
            "partial",
            {
                "demo.wh.cpp": bundle_text("demo", "1.0.0", 1),
                "other.wh.cpp": bundle_text("other", "2.1.0", 2),
            },
        )
        runner = self.runner_temp("partial")
        code, values, _ = run_script(
            self.branches, runner, self.variables(bundle_dir, target, upstream, runner)
        )
        self.assertEqual(code, 0)
        self.assertEqual(values.get("failed"), "1")
        self.assertEqual(values.get("pushed"), "1")
        self.assertEqual(git(target, "branch", "--list", "mods/demo"), "")
        self.assertEqual(git(target, "branch", "--list", "mods/other"), "mods/other")

    def test_report_step_succeeds_when_nothing_failed(self) -> None:
        code, _ = self.run_report(self.runner_temp("report-ok"), {"failed": "0"}, "0")
        self.assertEqual(code, 0)

    def install_pre_receive(self, target: Path, body: str) -> Path:
        """Install a hook that records every push attempt in a log file.

        `body` may use `%s` once for the attempt log path.
        """

        hook = target / "hooks" / "pre-receive"
        log = target / "push-attempts.log"
        hook.write_text(
            "\n".join(
                [
                    "#!/usr/bin/env bash",
                    f'printf \'attempt\\n\' >> "{log.as_posix()}"',
                    body % log.as_posix() if "%s" in body else body,
                    "",
                ]
            ),
            encoding="utf-8",
            newline="\n",
        )
        os.chmod(hook, 0o755)
        return log

    def test_policy_rejection_is_reported_once_with_its_reason(self) -> None:
        upstream, target, _ = self.make_repositories("policy")
        log = self.install_pre_receive(
            target,
            "echo 'refusing to allow a token to update a workflow file' >&2\nexit 1",
        )
        bundle_dir = self.make_bundles(
            "policy", {"demo.wh.cpp": bundle_text("demo", "1.1.0", 1)}
        )
        runner = self.runner_temp("policy")
        code, values, summary = run_script(
            self.branches, runner, self.variables(bundle_dir, target, upstream, runner)
        )

        self.assertEqual(code, 0, summary)
        self.assertEqual(values.get("failed"), "1")
        self.assertEqual(values.get("pushed"), "0")
        self.assertEqual(len(log.read_text(encoding="utf-8").splitlines()), 1)
        self.assertIn("refusing to allow a token", summary)
        self.assertNotIn("push failed", summary)
        self.assertEqual(git(target, "branch", "--list", "mods/demo"), "")

    def test_transient_push_failure_is_retried(self) -> None:
        upstream, target, _ = self.make_repositories("transient")
        log = self.install_pre_receive(
            target,
            'if [[ "$(wc -l < %s)" -eq 1 ]]; then\n'
            "  echo 'fatal: the remote end hung up unexpectedly' >&2\n"
            "  exit 1\n"
            "fi\n"
            "exit 0",
        )
        bundle_dir = self.make_bundles(
            "transient", {"demo.wh.cpp": bundle_text("demo", "1.1.0", 1)}
        )
        runner = self.runner_temp("transient")
        code, values, summary = run_script(
            self.branches, runner, self.variables(bundle_dir, target, upstream, runner)
        )

        self.assertEqual(code, 0, summary)
        self.assertEqual(values.get("pushed"), "1")
        self.assertEqual(values.get("failed"), "0")
        self.assertEqual(len(log.read_text(encoding="utf-8").splitlines()), 2)
        self.assertEqual(git(target, "branch", "--list", "mods/demo"), "mods/demo")

    def test_exhausted_retries_report_the_refusal_not_the_remote_url(self) -> None:
        upstream, target, _ = self.make_repositories("transient-all")
        log = self.install_pre_receive(target, "exit 1")
        bundle_dir = self.make_bundles(
            "transient-all", {"demo.wh.cpp": bundle_text("demo", "1.1.0", 1)}
        )
        runner = self.runner_temp("transient-all")
        code, values, summary = run_script(
            self.branches, runner, self.variables(bundle_dir, target, upstream, runner)
        )

        self.assertEqual(code, 0, summary)
        self.assertEqual(values.get("failed"), "1")
        self.assertEqual(len(log.read_text(encoding="utf-8").splitlines()), 3)
        self.assertIn("[remote rejected]", summary)
        self.assertNotIn(target.as_posix(), summary)

    def test_reported_reason_escapes_workflow_command_percent(self) -> None:
        upstream, target, _ = self.make_repositories("percent")
        self.install_pre_receive(
            target,
            "echo 'refusing to allow a token 100% without a scope' >&2\nexit 1",
        )
        bundle_dir = self.make_bundles(
            "percent", {"demo.wh.cpp": bundle_text("demo", "1.1.0", 1)}
        )
        runner = self.runner_temp("percent")
        proc, _, summary = run_script_verbose(
            self.branches, runner, self.variables(bundle_dir, target, upstream, runner)
        )

        self.assertEqual(proc.returncode, 0, summary)
        annotation = next(
            line for line in proc.stdout.splitlines() if line.startswith("::error::")
        )
        self.assertIn("100%25", annotation)

    def test_both_steps_share_the_summary_helpers(self) -> None:
        """A second copy of one_line would drift from the first."""

        for script in (self.branches, self.pull_requests):
            self.assertIn('source "$GITHUB_ACTION_PATH/summary.sh"', script)
            self.assertNotIn("one_line() {", script)
            self.assertNotIn("command_text() {", script)
        source = (PUBLISH_ACTION / "summary.sh").read_text(encoding="utf-8")
        self.assertIn("one_line() {", source)
        self.assertIn("command_text() {", source)

    def test_report_step_fails_after_a_branch_failure(self) -> None:
        code, _ = self.run_report(self.runner_temp("report-branch"), {"failed": "2"})
        self.assertNotEqual(code, 0)

    def test_report_step_fails_after_a_pull_request_failure(self) -> None:
        code, _ = self.run_report(self.runner_temp("report-pr"), {"failed": "0"}, "1")
        self.assertNotEqual(code, 0)

    def test_report_step_fails_when_a_step_died_before_reporting(self) -> None:
        code, _ = self.run_report(
            self.runner_temp("report-crash"), {"failed": ""}, branch_outcome="failure"
        )
        self.assertNotEqual(code, 0)

    def test_invalid_bundle_name_is_rejected(self) -> None:
        upstream, target, _ = self.make_repositories("invalid")
        bundle_dir = self.make_bundles(
            "invalid", {"Demo.wh.cpp": bundle_text("Demo", "1.0.0")}
        )
        runner = self.runner_temp("invalid")
        code, values, _ = run_script(
            self.branches, runner, self.variables(bundle_dir, target, upstream, runner)
        )
        self.assertNotEqual(code, 0)
        self.assertEqual(values, {})
        self.assertEqual(git(target, "branch", "--list", "mods/*"), "")

    def test_invalid_branch_prefix_is_rejected(self) -> None:
        upstream, target, _ = self.make_repositories("prefix")
        bundle_dir = self.make_bundles(
            "prefix", {"demo.wh.cpp": bundle_text("demo", "1.1.0", 1)}
        )
        runner = self.runner_temp("prefix")
        code, values, _ = run_script(
            self.branches,
            runner,
            self.variables(bundle_dir, target, upstream, runner, BRANCH_PREFIX="-bad"),
        )
        self.assertNotEqual(code, 0)
        self.assertEqual(values, {})

    def test_multiline_changelog_keeps_only_the_first_line(self) -> None:
        upstream, target, _ = self.make_repositories("multi")
        bundle_dir = self.make_bundles(
            "multi", {"demo.wh.cpp": bundle_text("demo", "1.1.0", 1)}
        )
        runner = self.runner_temp("multi")
        code, _, _ = run_script(
            self.branches,
            runner,
            self.variables(
                bundle_dir,
                target,
                upstream,
                runner,
                CHANGELOG="feat(demo): headline\n\nBody line\nCo-authored-by: X <x@e>",
            ),
        )
        self.assertEqual(code, 0)
        body = git(target, "log", "-1", "--format=%b", "mods/demo")
        self.assertTrue(body.startswith("feat(demo): headline"))
        self.assertNotIn("Body line", body)

    def test_empty_bundle_directory_is_rejected(self) -> None:
        upstream, target, _ = self.make_repositories("empty")
        bundle_dir = self.make_bundles("empty", {"notes.txt": "ignored\n"})
        runner = self.runner_temp("empty")
        code, values, _ = run_script(
            self.branches, runner, self.variables(bundle_dir, target, upstream, runner)
        )
        self.assertNotEqual(code, 0)
        self.assertEqual(values, {})

    def test_opened_pull_request_satisfies_the_upstream_rules(self) -> None:
        upstream, target, _ = self.make_repositories("pr-new")
        bundle_dir = self.make_bundles(
            "pr-new", {"demo.wh.cpp": bundle_text("demo", "1.1.0", 1)}
        )
        runner = self.runner_temp("pr-new")
        variables = self.variables(
            bundle_dir, target, upstream, runner, TOKEN="test-token"
        )
        code, values, _ = run_script(self.branches, runner, variables)
        self.assertEqual(code, 0)

        code, summary, _, calls = self.run_pull_requests(
            "pr-new", runner, variables, values["status-file"]
        )
        self.assertEqual(code, 0, summary)
        self.assertIn("pr list --repo ramensoftware/windhawk-mods --head CYoJkoY:mods/demo", calls)
        self.assertIn("--state open", calls)
        self.assertIn("pr create --repo ramensoftware/windhawk-mods --base main", calls)
        self.assertIn("--head CYoJkoY:mods/demo", calls)
        self.assertIn("--title Update mods/demo.wh.cpp to v1.1.0", calls)
        body = self.pull_request_body(variables, "demo")
        self.assertTrue(body.startswith("<!-- changelog:start -->"))
        self.assertIn("feat(demo): refresh demo (v1.1.0)", body)
        self.assertIn("<!-- changelog:end -->", body)
        self.assertNotIn("## Mod authorship", body)
        self.assertIn("opened 321", summary)

    def test_new_mod_pull_request_declares_authorship(self) -> None:
        upstream, target, _ = self.make_repositories("pr-author")
        bundle_dir = self.make_bundles(
            "pr-author", {"fresh-mod.wh.cpp": bundle_text("fresh-mod", "0.1.0")}
        )
        runner = self.runner_temp("pr-author")
        variables = self.variables(
            bundle_dir, target, upstream, runner, TOKEN="test-token"
        )
        code, values, _ = run_script(self.branches, runner, variables)
        self.assertEqual(code, 0)

        code, summary, _, calls = self.run_pull_requests(
            "pr-author", runner, variables, values["status-file"]
        )
        self.assertEqual(code, 0, summary)
        self.assertIn("--title Add mod: fresh-mod", calls)
        body = self.pull_request_body(variables, "fresh-mod")
        self.assertIn("## Mod authorship", body)
        self.assertIn("- [x] The submitter, with AI assistance", body)

    def test_refreshed_pull_request_is_edited_not_duplicated(self) -> None:
        upstream, target, _ = self.make_repositories("pr-edit")
        bundle_dir = self.make_bundles(
            "pr-edit", {"demo.wh.cpp": bundle_text("demo", "1.1.0", 1)}
        )
        runner = self.runner_temp("pr-edit")
        variables = self.variables(
            bundle_dir, target, upstream, runner, TOKEN="test-token"
        )
        code, values, _ = run_script(self.branches, runner, variables)
        self.assertEqual(code, 0)

        code, summary, _, calls = self.run_pull_requests(
            "pr-edit", runner, variables, values["status-file"], existing="77\n"
        )
        self.assertEqual(code, 0, summary)
        self.assertIn("pr edit 77 --repo ramensoftware/windhawk-mods", calls)
        self.assertNotIn("pr reopen", calls)
        self.assertNotIn("pr create", calls)
        self.assertIn("refreshed #77", summary)

    def test_failed_pull_request_update_reports_the_gh_reason(self) -> None:
        upstream, target, _ = self.make_repositories("pr-edit-fail")
        bundle_dir = self.make_bundles(
            "pr-edit-fail", {"demo.wh.cpp": bundle_text("demo", "1.1.0", 1)}
        )
        runner = self.runner_temp("pr-edit-fail")
        variables = self.variables(
            bundle_dir, target, upstream, runner, TOKEN="test-token"
        )
        code, values, _ = run_script(self.branches, runner, variables)
        self.assertEqual(code, 0)

        code, summary, outputs, _ = self.run_pull_requests(
            "pr-edit-fail",
            runner,
            variables,
            values["status-file"],
            existing="77\n",
            edit_failure="HTTP 403: Resource not accessible by integration",
        )
        self.assertEqual(code, 0, summary)
        self.assertEqual(outputs.get("pr-failed"), "1")
        self.assertIn("HTTP 403", summary)
        self.assertIn("error: #77: HTTP 403", summary)

    def test_pull_requests_are_skipped_without_a_token(self) -> None:
        upstream, target, _ = self.make_repositories("pr-skip")
        bundle_dir = self.make_bundles(
            "pr-skip", {"demo.wh.cpp": bundle_text("demo", "1.1.0", 1)}
        )
        runner = self.runner_temp("pr-skip")
        variables = self.variables(bundle_dir, target, upstream, runner)
        code, values, _ = run_script(self.branches, runner, variables)
        self.assertEqual(code, 0)

        code, _, _, calls = self.run_pull_requests(
            "pr-skip", runner, variables | {"TOKEN": ""}, values["status-file"]
        )
        self.assertEqual(code, 0)
        self.assertEqual(calls, "")


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


def job_block(path: Path, job: str) -> list[str]:
    """Return the lines of one job, from its name to the next job."""

    lines = path.read_text(encoding="utf-8").splitlines()
    start = lines.index(f"  {job}:")
    body: list[str] = []
    for line in lines[start + 1 :]:
        if line.startswith("  ") and not line.startswith("   ") and line.rstrip().endswith(":"):
            break
        body.append(line)
    return body


class ValidateModsWorkflowTests(unittest.TestCase):
    """Each mod owns its compile and publish, so one mod cannot block another."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.path = REPO_ROOT / ".github" / "workflows" / "validate-mods.yml"
        cls.text = cls.path.read_text(encoding="utf-8")

    def test_build_runs_one_matrix_entry_per_selected_mod(self) -> None:
        build = "\n".join(job_block(self.path, "build"))
        self.assertIn("strategy:", build)
        self.assertIn("mod: ${{ fromJSON(needs.select.outputs.matrix) }}", build)
        self.assertIn("fail-fast: false", build)

    def test_build_does_not_wait_for_a_shared_compile_job(self) -> None:
        build = "\n".join(job_block(self.path, "build"))
        self.assertIn("needs: [bundle, select]", build)
        self.assertNotIn("needs: [bundle, compile", build)

    def test_publish_receives_only_its_own_mod(self) -> None:
        build = "\n".join(job_block(self.path, "build"))
        self.assertIn("mods: ${{ matrix.mod }}", build)
        self.assertNotIn("needs.select.outputs.mods", build)

    def test_release_still_waits_for_every_mod(self) -> None:
        release = "\n".join(job_block(self.path, "release"))
        self.assertIn("needs: [bundle, build]", release)
        self.assertIn("needs.build.result == 'success'", release)

    def test_bundle_wide_inputs_still_select_every_mod(self) -> None:
        self.assertIn("--trigger shared", self.text)
        self.assertIn("--trigger tools/bundle.py", self.text)
        self.assertIn("--matrix", self.text)

    def test_manual_run_can_name_mods_without_comma_splitting(self) -> None:
        self.assertIn('read -r -a wanted <<< "$requested"', self.text)
        self.assertIn('extra=(--all --only "${wanted[@]}")', self.text)


class CompileModActionTests(unittest.TestCase):
    """The compile action owns one bundle and refuses another mod's bundle."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.path = COMPILE_ACTION / "action.yml"
        cls.script = extract_run(cls.path, "Compile mod with every Windhawk version")

    def test_script_is_valid_bash(self) -> None:
        with tempfile.TemporaryDirectory() as name:
            script = Path(name) / "compile.sh"
            script.write_text(self.script + "\n", encoding="utf-8", newline="\n")
            proc = subprocess.run(
                [_bash(), "-n", script.as_posix()], capture_output=True, text=True
            )
        self.assertEqual(proc.returncode, 0, proc.stderr)

    def test_bundle_must_belong_to_the_requested_mod(self) -> None:
        self.assertIn('if [[ "$name" != "$MOD_ID" ]]', self.script)

    def test_every_version_is_compiled_for_the_one_bundle(self) -> None:
        self.assertIn("for version in $versions; do", self.script)
        self.assertIn('echo "Compiling $BUNDLE with Windhawk $version"', self.script)
        # The action never globs the bundle directory: it owns exactly one file.
        self.assertNotIn("find dist", self.script)

    def test_architectures_are_complete_before_success(self) -> None:
        self.assertIn('for arch in 32 64 arm64; do', self.script)
        self.assertIn('echo "::error::missing $arch build for $name at $out_dir"', self.script)


if __name__ == "__main__":
    unittest.main()
