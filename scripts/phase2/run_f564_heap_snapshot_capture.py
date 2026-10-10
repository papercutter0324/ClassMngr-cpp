#!/usr/bin/env python3
"""Capture WPR heap snapshots at the F563 PDF lifecycle checkpoints.

The image-name WPR setting applies to new ClassMngr.exe processes. This
controller refuses a pre-enabled setting, enables it only for the capture,
acknowledges each application checkpoint only after its snapshot succeeds, and
disables the setting during cleanup. Use --dry-run to exercise the Release
application handshake without invoking WPR or changing system configuration.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import run_f563_checkpoint_dry_run as f563


PROCESS_IMAGE = "ClassMngr.exe"
CHECKPOINT_NAMES = (
    "after-render",
    "after-document-close",
    "after-view-destroyed",
)
SNAPSHOT_CONFIG_QUERY = [
    "-snapshotconfig",
    "heap",
    "-name",
    PROCESS_IMAGE,
]


class CaptureError(RuntimeError):
    pass


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def default_wpr_executable() -> Path:
    candidate = shutil.which("wpr.exe") or shutil.which("wpr")
    if not candidate:
        raise CaptureError("Could not find wpr.exe on PATH; pass --wpr-executable.")
    return Path(candidate).resolve()


def run_wpr(
    executable: Path,
    arguments: list[str],
    records: list[dict[str, Any]],
    timeout_seconds: int = 120,
) -> subprocess.CompletedProcess[str]:
    command = [str(executable), *arguments]
    started_at = utc_now()
    try:
        result = subprocess.run(
            command,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout_seconds,
            check=False,
        )
    except (OSError, subprocess.SubprocessError) as error:
        records.append(
            {
                "startedAtUtc": started_at,
                "arguments": arguments,
                "error": str(error),
            }
        )
        raise CaptureError(f"Could not complete wpr {' '.join(arguments)}: {error}") from error

    records.append(
        {
            "startedAtUtc": started_at,
            "completedAtUtc": utc_now(),
            "arguments": arguments,
            "exitCode": result.returncode,
            "stdout": result.stdout.strip(),
            "stderr": result.stderr.strip(),
        }
    )
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip() or "no WPR output"
        raise CaptureError(
            f"wpr {' '.join(arguments)} exited {result.returncode}: {detail}"
        )
    return result


def snapshot_config_state(
    executable: Path,
    records: list[dict[str, Any]],
) -> str:
    result = run_wpr(executable, SNAPSHOT_CONFIG_QUERY, records)
    response = f"{result.stdout}\n{result.stderr}".casefold()
    if "disabled" in response:
        return "disabled"
    if "enabled" in response:
        return "enabled"
    raise CaptureError(
        "WPR snapshot configuration query did not report enabled or disabled."
    )


def wpr_is_recording(
    executable: Path,
    records: list[dict[str, Any]],
) -> bool:
    result = run_wpr(executable, ["-status"], records)
    response = f"{result.stdout}\n{result.stderr}".casefold()
    if "wpr is not recording" in response or "no recording is in progress" in response:
        return False
    if (
        "wpr is recording" in response
        or "recording is in progress" in response
    ):
        return True
    raise CaptureError("WPR status did not clearly report whether a recording is active.")


def make_artifact_root(repository_root: Path, requested: Path | None) -> Path:
    if requested is not None:
        root = requested.resolve()
    else:
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        root = (
            repository_root
            / "build"
            / f"f564-heap-snapshot-{stamp}-{os.getpid()}"
        )
    if root.exists():
        raise CaptureError(f"Refusing to overwrite existing artifact directory: {root}")
    root.mkdir(parents=True)
    return root


def run_capture(args: argparse.Namespace) -> Path:
    repository_root = Path(__file__).resolve().parents[2]
    application = (
        args.application.resolve()
        if args.application
        else f563.default_executable(
            (
                repository_root / "dist/f564-current-source-windows-x64/ClassMngr.exe",
                repository_root / "build/windows-x64-release-ninja/ClassMngr.exe",
            ),
            "Release ClassMngr application",
        )
    )
    test_executable = (
        args.test_executable.resolve()
        if args.test_executable
        else f563.default_executable(
            (
                repository_root
                / "build/windows-x64-debug-ninja/ClassMngrStartupPerformanceTests.exe",
                repository_root
                / "build/windows-x64-debug/Debug/ClassMngrStartupPerformanceTests.exe",
            ),
            "Debug startup performance test executable",
        )
    )
    if not application.is_file():
        raise CaptureError(f"Release application does not exist: {application}")
    if not test_executable.is_file():
        raise CaptureError(f"Debug lifecycle runner does not exist: {test_executable}")
    if application == test_executable:
        raise CaptureError("The application and lifecycle runner must be different files.")
    if args.timeout_seconds < 1 or args.timeout_seconds > 300:
        raise CaptureError("--timeout-seconds must be between 1 and 300.")

    wpr_executable = (
        args.wpr_executable.resolve()
        if args.wpr_executable
        else default_wpr_executable()
    )
    if not wpr_executable.is_file():
        raise CaptureError(f"WPR executable does not exist: {wpr_executable}")

    artifact_root = make_artifact_root(repository_root, args.artifact_root)
    trace_path = artifact_root / "classmngr-pdf-heap-snapshots.etl"
    incomplete_trace_path = artifact_root / "classmngr-pdf-heap-snapshots-incomplete.etl"
    wpr_commands: list[dict[str, Any]] = []
    snapshots: list[dict[str, Any]] = []
    cleanup_errors: list[str] = []
    initial_configuration: str | None = None
    final_configuration: str | None = None
    configuration_may_be_enabled = False
    recording_may_be_active = False
    route_artifacts: Path | None = None
    controller_summary: dict[str, Any] | None = None
    error_text: str | None = None
    trace_saved = False

    def snapshot_before_ack(marker: dict[str, Any], marker_bytes: bytes) -> None:
        order = int(marker["checkpointOrder"])
        checkpoint = str(marker["checkpoint"])
        if order != len(snapshots) + 1:
            raise CaptureError(f"Unexpected snapshot order {order} at {checkpoint}.")
        if checkpoint != CHECKPOINT_NAMES[order - 1]:
            raise CaptureError(f"Unexpected checkpoint name at order {order}: {checkpoint}.")
        if args.dry_run:
            snapshot_records = {
                "checkpoint": checkpoint,
                "checkpointOrder": order,
                "processId": int(marker["processId"]),
                "markerSha256": hashlib.sha256(marker_bytes).hexdigest(),
                "snapshotCommandSucceeded": False,
                "dryRun": True,
                "recordedAtUtc": utc_now(),
            }
        else:
            run_wpr(
                wpr_executable,
                [
                    "-singlesnapshot",
                    "heap",
                    str(int(marker["processId"])),
                ],
                wpr_commands,
                timeout_seconds=25,
            )
            snapshot_records = {
                "checkpoint": checkpoint,
                "checkpointOrder": order,
                "processId": int(marker["processId"]),
                "markerSha256": hashlib.sha256(marker_bytes).hexdigest(),
                "snapshotCommandSucceeded": True,
                "dryRun": False,
                "recordedAtUtc": utc_now(),
            }
        snapshots.append(snapshot_records)

    try:
        if not args.dry_run:
            if wpr_is_recording(wpr_executable, wpr_commands):
                raise CaptureError(
                    "Refusing to capture because another WPR recording is already active."
                )
            initial_configuration = snapshot_config_state(wpr_executable, wpr_commands)
            if initial_configuration != "disabled":
                raise CaptureError(
                    f"Refusing to capture because {PROCESS_IMAGE} heap snapshots "
                    "were already enabled before this run."
                )

            # The prior state is known to be disabled, so cleanup owns any
            # configuration change attempted from this point onward.
            configuration_may_be_enabled = True
            run_wpr(
                wpr_executable,
                [
                    "-snapshotconfig",
                    "heap",
                    "-name",
                    PROCESS_IMAGE,
                    "enable",
                ],
                wpr_commands,
            )
            enabled_state = snapshot_config_state(wpr_executable, wpr_commands)
            if enabled_state != "enabled":
                raise CaptureError("WPR did not confirm that heap snapshots were enabled.")

            run_wpr(
                wpr_executable,
                ["-start", "heapsnapshot", "-filemode"],
                wpr_commands,
            )
            recording_may_be_active = True
            if not wpr_is_recording(wpr_executable, wpr_commands):
                raise CaptureError("WPR did not confirm that heap snapshot recording started.")

        controller_args = argparse.Namespace(
            application=application,
            test_executable=test_executable,
            qt_bin=args.qt_bin,
            timeout_seconds=args.timeout_seconds,
        )
        route_artifacts = f563.run_controller(
            controller_args,
            before_ack=snapshot_before_ack,
        )
        summary_path = route_artifacts / "controller-summary.json"
        controller_summary = f563.read_json(summary_path)
        if controller_summary.get("runnerExitCode") != 0:
            raise CaptureError("The F563 lifecycle runner did not exit successfully.")
        if len(controller_summary.get("checkpoints", [])) != len(CHECKPOINT_NAMES):
            raise CaptureError("The F563 lifecycle runner did not acknowledge all checkpoints.")
        if len(snapshots) != len(CHECKPOINT_NAMES):
            raise CaptureError("A heap snapshot was not captured at every checkpoint.")
        if args.dry_run and any(
            snapshot["snapshotCommandSucceeded"] for snapshot in snapshots
        ):
            raise CaptureError("Dry-run unexpectedly reports a WPR snapshot command.")

        if not args.dry_run:
            run_wpr(wpr_executable, ["-stop", str(trace_path)], wpr_commands)
            recording_may_be_active = False
            if wpr_is_recording(wpr_executable, wpr_commands):
                recording_may_be_active = True
                raise CaptureError("WPR remained active after stopping the snapshot trace.")
            if not trace_path.is_file() or trace_path.stat().st_size <= 0:
                raise CaptureError("WPR stop completed without a non-empty ETL file.")
            trace_saved = True
    except Exception as error:
        error_text = f"{type(error).__name__}: {error}"
    finally:
        if not args.dry_run and recording_may_be_active:
            try:
                run_wpr(
                    wpr_executable,
                    ["-stop", str(incomplete_trace_path)],
                    wpr_commands,
                )
                recording_may_be_active = False
            except Exception as error:
                cleanup_errors.append(f"Could not stop WPR: {error}")

        if not args.dry_run and configuration_may_be_enabled:
            try:
                run_wpr(
                    wpr_executable,
                    [
                        "-snapshotconfig",
                        "heap",
                        "-name",
                        PROCESS_IMAGE,
                        "disable",
                    ],
                    wpr_commands,
                )
            except Exception as error:
                cleanup_errors.append(f"Could not disable WPR snapshot configuration: {error}")
            try:
                final_configuration = snapshot_config_state(wpr_executable, wpr_commands)
                if final_configuration != "disabled":
                    cleanup_errors.append(
                        f"Final WPR configuration state is {final_configuration}, not disabled."
                    )
            except Exception as error:
                cleanup_errors.append(f"Could not verify final WPR configuration: {error}")

        if args.dry_run:
            final_configuration = "not_changed"

        status = (
            "passed"
            if error_text is None
            and not cleanup_errors
            and len(snapshots) == len(CHECKPOINT_NAMES)
            and (args.dry_run or (trace_saved and final_configuration == "disabled"))
            else "failed"
        )
        summary: dict[str, Any] = {
            "schema": "classmngr-f564-heap-snapshot-capture-v1",
            "createdAtUtc": utc_now(),
            "status": status,
            "dryRun": bool(args.dry_run),
            "processImage": PROCESS_IMAGE,
            "application": str(application),
            "applicationSha256": sha256_file(application),
            "testExecutable": str(test_executable),
            "testExecutableSha256": sha256_file(test_executable),
            "wprExecutable": str(wpr_executable),
            "initialSnapshotConfiguration": initial_configuration,
            "finalSnapshotConfiguration": final_configuration,
            "configurationChangeAttempted": configuration_may_be_enabled,
            "routeArtifacts": str(route_artifacts) if route_artifacts else None,
            "runId": controller_summary.get("runId") if controller_summary else None,
            "processId": controller_summary.get("processId") if controller_summary else None,
            "checkpoints": snapshots,
            "wprCommands": wpr_commands,
            "tracePath": str(trace_path) if trace_saved else None,
            "traceBytes": trace_path.stat().st_size if trace_saved else None,
            "traceSha256": sha256_file(trace_path) if trace_saved else None,
            "error": error_text,
            "cleanupErrors": cleanup_errors,
            "artifacts": str(artifact_root),
        }
        f563.atomic_write_json(artifact_root / "capture-summary.json", summary)

    if status != "passed":
        details = error_text or "; ".join(cleanup_errors) or "capture acceptance failed"
        raise CaptureError(f"F564 capture {status}: {details}. Artifacts: {artifact_root}")

    print(f"F564 heap-snapshot {'dry-run' if args.dry_run else 'capture'} passed.")
    print(f"Artifacts: {artifact_root}")
    if trace_saved:
        print(f"ETL: {trace_path} ({trace_path.stat().st_size} bytes)")
    return artifact_root


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--application", type=Path, help="Current-source Release ClassMngr.exe")
    parser.add_argument(
        "--test-executable",
        type=Path,
        help="Debug ClassMngrStartupPerformanceTests.exe",
    )
    parser.add_argument("--qt-bin", type=Path, help="Optional Qt bin directory prepended to PATH")
    parser.add_argument("--wpr-executable", type=Path, help="Optional wpr.exe path")
    parser.add_argument("--artifact-root", type=Path, help="Optional new artifact directory")
    parser.add_argument(
        "--timeout-seconds",
        type=int,
        default=90,
        help="Maximum wait for each marker and final runner completion (default: 90)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Exercise the checkpoint route without invoking WPR or changing configuration",
    )
    args = parser.parse_args()
    try:
        run_capture(args)
        return 0
    except (CaptureError, f563.ControllerError, OSError, subprocess.SubprocessError) as error:
        print(f"F564 heap-snapshot capture failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
