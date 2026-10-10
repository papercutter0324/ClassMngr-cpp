"""Focused regressions for the Windows Phase 0 evidence runner."""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
RUNNER = ROOT / "scripts" / "phase0" / "run_phase0_evidence.ps1"


@unittest.skipUnless(os.name == "nt", "Windows PowerShell runner regression")
class WindowsRunnerManifestTests(unittest.TestCase):
    def test_one_requested_route_remains_a_json_array(self) -> None:
        powershell = shutil.which("powershell") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is not installed")

        with tempfile.TemporaryDirectory(prefix="classmngr-one-route-") as temp:
            root = Path(temp)
            application = root / "ClassMngr.exe"
            test_executable = root / "ClassMngrStartupPerformanceTests.exe"
            application.write_bytes(b"skip-run placeholder")
            test_executable.write_bytes(b"skip-run placeholder")
            command = [
                powershell,
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(RUNNER),
                "-EvidenceParent",
                str(root),
                "-RunDirectoryName",
                "single-route",
                "-ApplicationPath",
                str(application),
                "-TestExecutable",
                str(test_executable),
                "-SkipBuild",
                "-SkipRun",
                "-SkipValidation",
                "-Routes",
                "lifecycle-sub-prep",
            ]
            completed = subprocess.run(
                command,
                cwd=ROOT,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(
                completed.returncode,
                0,
                msg=f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}",
            )

            manifest_path = root / "single-route" / "run-manifest.json"
            self.assertTrue(manifest_path.is_file())
            manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
            self.assertEqual(manifest["requestedRoutes"], ["lifecycle-sub-prep"])
            self.assertIn(
                "CLASSMNGR_STARTUP_PDF_LIFECYCLE_GRAB_ARM",
                manifest["requiredOptInEnvironmentVariables"],
            )


if __name__ == "__main__":
    unittest.main()
