from __future__ import annotations

import hashlib
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from validate_p908_retention_diagnostic import EvidenceError, _verify_fixture_hashes


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


class FixturePhysicalHashTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_directory = tempfile.TemporaryDirectory()
        self.run_directory = Path(self.temp_directory.name)
        self.master_path = self.run_directory / "representative-master.tps"
        self.copy_path = self.run_directory / "process-01" / "representative.tps"
        self.snapshot_path = self.run_directory / "process-01" / "representative.pre-launch.tps"
        self.copy_path.parent.mkdir()

        self.master_bytes = b"source 96-class fixture"
        self.prepared_bytes = b"weekday transform with 30 package classes"
        self.exit_bytes = b"weekday transform plus app-persisted settings"
        self.master_path.write_bytes(self.master_bytes)
        self.snapshot_path.write_bytes(self.prepared_bytes)
        self.copy_path.write_bytes(self.exit_bytes)

        self.master_sha = _sha256(self.master_bytes)
        self.prepared_sha = _sha256(self.prepared_bytes)
        self.exit_sha = _sha256(self.exit_bytes)
        self.fixture = {"masterPath": str(self.master_path), "sha256": self.master_sha}
        self.fixture_copy = {
            "processInstanceId": "process-01",
            "path": str(self.copy_path),
            "sourceFixtureSha256": self.master_sha,
            "sha256BeforeLaunch": self.prepared_sha,
            "preLaunchSnapshotPath": str(self.snapshot_path),
            "preLaunchSnapshotSha256": self.prepared_sha,
            "outputPreparation": {
                "sourceSha256": self.master_sha,
                "preparedSha256": self.prepared_sha,
            },
        }
        self.process = {
            "fixturePathPassedToApp": str(self.copy_path),
            "fixturePreLaunchSnapshotPath": str(self.snapshot_path),
            "fixturePreLaunchSnapshotSha256": self.prepared_sha,
            "fixtureSha256PassedToApp": self.prepared_sha,
            "fixtureHashAtExit": self.exit_sha,
        }

    def tearDown(self) -> None:
        self.temp_directory.cleanup()

    def test_matching_transformed_snapshot_and_runtime_exit_hash_pass(self) -> None:
        hashes = _verify_fixture_hashes(
            self.fixture, self.fixture_copy, self.process, self.run_directory
        )

        self.assertEqual(
            hashes,
            {
                "master": self.master_sha,
                "preLaunch": self.prepared_sha,
                "postExit": self.exit_sha,
            },
        )

    def test_mismatching_physical_prelaunch_snapshot_fails_closed(self) -> None:
        self.snapshot_path.write_bytes(b"tampered transformed snapshot")

        with self.assertRaisesRegex(EvidenceError, "physical pre-launch transformed fixture"):
            _verify_fixture_hashes(
                self.fixture, self.fixture_copy, self.process, self.run_directory
            )

    def test_mismatching_physical_postexit_fixture_fails_closed(self) -> None:
        self.copy_path.write_bytes(b"tampered app working fixture")

        with self.assertRaisesRegex(EvidenceError, "physical post-exit fixture"):
            _verify_fixture_hashes(
                self.fixture, self.fixture_copy, self.process, self.run_directory
            )


if __name__ == "__main__":
    unittest.main()
