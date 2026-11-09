#!/usr/bin/env python3
"""Focused tests for baseline Git provenance."""

from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path

from record_refactoring_baseline import (
    GitInspectionError,
    evaluate_provenance,
    inspect_git_state,
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


if __name__ == "__main__":
    unittest.main()
