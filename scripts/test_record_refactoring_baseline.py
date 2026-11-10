#!/usr/bin/env python3
"""Focused tests for baseline Git provenance."""

from __future__ import annotations

import json
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from record_refactoring_baseline import (
    GitInspectionError,
    evaluate_provenance,
    inspect_git_state,
    main,
    resolve_expected_commit,
)


class TemporaryGitRepository:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.git("init", "--quiet")
        self.git("config", "user.name", "Baseline Provenance Test")
        self.git("config", "user.email", "baseline-test@example.invalid")

    def git(self, *arguments: str) -> str:
        completed = subprocess.run(
            ["git", *arguments],
            cwd=self.path,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
        )
        return completed.stdout.strip()

    def commit_all(self, message: str) -> str:
        self.git("add", "--all")
        self.git("commit", "--quiet", "-m", message)
        return self.git("rev-parse", "HEAD")


class RecordRefactoringBaselineProvenanceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.repository = Path(self.temporary_directory.name)
        self.git = TemporaryGitRepository(self.repository)
        (self.repository / "tracked.txt").write_text("original\n", encoding="utf-8")
        self.initial_commit = self.git.commit_all("initial source")

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def snapshot(self) -> dict[str, object]:
        return inspect_git_state(self.repository)

    def test_clean_repository_matches_expected_commit_and_tree(self) -> None:
        expected_commit, expected_tree = resolve_expected_commit(
            self.repository, self.initial_commit
        )
        start = self.snapshot()
        end = self.snapshot()

        self.assertEqual(start["dirty_paths"], [])
        self.assertTrue(evaluate_provenance(start, end, expected_commit, expected_tree)[0])

    def test_modified_and_deleted_tracked_files_are_reported(self) -> None:
        deleted_path = self.repository / "deleted.txt"
        deleted_path.write_text("to be removed\n", encoding="utf-8")
        self.git.commit_all("add file to delete")
        start = self.snapshot()

        (self.repository / "tracked.txt").write_text("modified\n", encoding="utf-8")
        deleted_path.unlink()
        end = self.snapshot()

        self.assertIn("tracked.txt", end["dirty_paths"])
        self.assertIn("deleted.txt", end["dirty_paths"])
        self.assertNotEqual(start["fingerprint"], end["fingerprint"])
        self.assertFalse(evaluate_provenance(start, end, None, None)[0])

    def test_added_and_removed_nonignored_untracked_files_change_provenance(self) -> None:
        start = self.snapshot()
        untracked_path = self.repository / "local-source.txt"
        untracked_path.write_text("added during the run\n", encoding="utf-8")
        added = self.snapshot()
        untracked_path.unlink()
        removed = self.snapshot()

        self.assertIn("local-source.txt", added["dirty_paths"])
        self.assertEqual(removed["dirty_paths"], [])
        self.assertNotEqual(start["fingerprint"], added["fingerprint"])
        self.assertNotEqual(added["fingerprint"], removed["fingerprint"])
        self.assertFalse(evaluate_provenance(start, added, None, None)[0])
        self.assertFalse(evaluate_provenance(added, removed, None, None)[0])

    def test_ignored_build_and_artifact_output_does_not_dirty_source(self) -> None:
        (self.repository / ".gitignore").write_text(
            "build/\nartifacts/\n", encoding="utf-8"
        )
        self.git.commit_all("ignore generated output")
        start = self.snapshot()

        (self.repository / "build").mkdir()
        (self.repository / "build" / "cache.bin").write_bytes(b"build output")
        (self.repository / "artifacts").mkdir()
        (self.repository / "artifacts" / "baseline.json").write_text(
            "{}\n", encoding="utf-8"
        )
        end = self.snapshot()

        self.assertEqual(end["dirty_paths"], [])
        self.assertEqual(start["fingerprint"], end["fingerprint"])
        expected_commit, expected_tree = resolve_expected_commit(
            self.repository, self.git.git("rev-parse", "HEAD")
        )
        self.assertTrue(evaluate_provenance(start, end, expected_commit, expected_tree)[0])

    def test_expected_commit_mismatch_fails_even_when_worktree_is_clean(self) -> None:
        expected_commit = self.initial_commit
        (self.repository / "tracked.txt").write_text("second version\n", encoding="utf-8")
        other_commit = self.git.commit_all("second source")
        self.git.git("checkout", "--quiet", expected_commit)
        start = self.snapshot()
        end = self.snapshot()
        resolved_commit, expected_tree = resolve_expected_commit(
            self.repository, other_commit
        )

        valid, errors = evaluate_provenance(start, end, resolved_commit, expected_tree)

        self.assertFalse(valid)
        self.assertIn("HEAD at start does not match the expected commit", errors)
        self.assertIn("tree at start does not match the expected commit tree", errors)

    def test_git_inspection_failure_raises_instead_of_returning_empty_state(self) -> None:
        with tempfile.TemporaryDirectory() as non_repository:
            with self.assertRaises(GitInspectionError):
                inspect_git_state(Path(non_repository))

    def run_recorder(
        self,
        configure_code: int,
        build_code: int,
        test_code: int,
        c53_code: int = 0,
    ) -> tuple[int, str, dict[str, object], list[str], bool]:
        with tempfile.TemporaryDirectory() as temporary_directory:
            repository = Path(temporary_directory)
            scripts = repository / "scripts"
            scripts.mkdir()
            module_path = scripts / "record_refactoring_baseline.py"
            module_path.touch()
            (repository / "CMakeLists.txt").write_text(
                "cmake_minimum_required()\n", encoding="utf-8"
            )
            output_file = repository / "github-output"
            report_file = repository / "artifacts" / "baseline.json"
            build_dir = repository / "build"
            state = {
                "head": "a" * 40,
                "tree": "b" * 40,
                "status_entries": [],
                "dirty_paths": [],
                "fingerprint": "source-fingerprint",
            }
            commands: list[str] = []
            output_was_published_before_tests = False

            def fake_run(command: list[str], cwd: Path) -> tuple[int, float]:
                nonlocal output_was_published_before_tests
                executable = command[0]
                if executable == "ctest":
                    commands.append("ctest")
                    output_was_published_before_tests = (
                        output_file.read_text(encoding="utf-8")
                        == "build_exit_code=0\n"
                    )
                    return test_code, 3.0
                if len(command) > 1 and "capture_sub_prep_c53_parity.py" in command[1]:
                    commands.append("c53")
                    return c53_code, 4.0
                if "--fresh" in command:
                    commands.append("configure")
                    return configure_code, 1.0
                if "--build" in command:
                    commands.append("build")
                    return build_code, 2.0
                self.fail(f"unexpected recorder command: {command}")

            argv = [
                "record_refactoring_baseline.py",
                "--configure-preset",
                "test-preset",
                "--build-dir",
                str(build_dir),
                "--output",
                str(report_file),
            ]
            with (
                patch("record_refactoring_baseline.__file__", str(module_path)),
                patch(
                    "record_refactoring_baseline.inspect_git_state",
                    side_effect=[state, state],
                ),
                patch(
                    "record_refactoring_baseline.capture",
                    return_value="cmake version test",
                ),
                patch("record_refactoring_baseline.run", side_effect=fake_run),
                patch("sys.argv", argv),
                patch.dict(
                    "os.environ",
                    {"GITHUB_OUTPUT": str(output_file), "GITHUB_SHA": ""},
                ),
            ):
                result = main()

            output = output_file.read_text(encoding="utf-8")
            report = json.loads(report_file.read_text(encoding="utf-8"))
            return result, output, report, commands, output_was_published_before_tests

    def test_successful_build_publishes_output_before_failed_ctest(self) -> None:
        result, output, report, commands, published_before_tests = self.run_recorder(
            configure_code=0, build_code=0, test_code=1
        )

        self.assertEqual(result, 1)
        self.assertEqual(output, "build_exit_code=0\n")
        self.assertEqual(report["results"]["build_exit_code"], 0)
        self.assertEqual(report["results"]["test_exit_code"], 1)
        self.assertEqual(report["results"]["c53_capture_exit_code"], 0)
        self.assertEqual(report["results"]["c53_capture_status"], "passed")
        self.assertEqual(report["duration_seconds"]["c53_capture"], 4.0)
        self.assertEqual(commands, ["configure", "build", "ctest", "c53"])
        self.assertTrue(published_before_tests)

    def test_failed_build_publishes_nonzero_output_and_skips_ctest(self) -> None:
        result, output, report, commands, _ = self.run_recorder(
            configure_code=0, build_code=2, test_code=0
        )

        self.assertEqual(result, 1)
        self.assertEqual(output, "build_exit_code=2\n")
        self.assertEqual(report["results"]["build_exit_code"], 2)
        self.assertEqual(report["results"]["test_exit_code"], -1)
        self.assertIsNone(report["results"]["c53_capture_exit_code"])
        self.assertEqual(report["results"]["c53_capture_status"], "not_run")
        self.assertEqual(commands, ["configure", "build"])

    def test_failed_configure_publishes_nonzero_output_and_skips_build_and_ctest(
        self,
    ) -> None:
        result, output, report, commands, _ = self.run_recorder(
            configure_code=1, build_code=0, test_code=0
        )

        self.assertEqual(result, 1)
        self.assertEqual(output, "build_exit_code=-1\n")
        self.assertEqual(report["results"]["configure_exit_code"], 1)
        self.assertEqual(report["results"]["build_exit_code"], -1)
        self.assertEqual(report["results"]["test_exit_code"], -1)
        self.assertIsNone(report["results"]["c53_capture_exit_code"])
        self.assertEqual(report["results"]["c53_capture_status"], "not_run")
        self.assertEqual(commands, ["configure"])

    def test_failed_c53_capture_is_recorded_and_fails_the_baseline(self) -> None:
        result, _, report, commands, _ = self.run_recorder(
            configure_code=0, build_code=0, test_code=0, c53_code=1
        )

        self.assertEqual(result, 1)
        self.assertEqual(report["results"]["test_exit_code"], 0)
        self.assertEqual(report["results"]["c53_capture_exit_code"], 1)
        self.assertEqual(report["results"]["c53_capture_status"], "failed")
        self.assertEqual(commands, ["configure", "build", "ctest", "c53"])


if __name__ == "__main__":
    unittest.main()
