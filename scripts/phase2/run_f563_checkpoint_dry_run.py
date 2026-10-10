#!/usr/bin/env python3
"""Run the Debug PDF lifecycle route and service fake F563 checkpoint acks.

This controller launches the existing QtTest lifecycle-sub-prep route, which
creates its deterministic workspace fixture and launches the Debug app. It
only validates marker files and writes acknowledgements; it never invokes WPR.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


CHECKPOINTS = (
    (1, "after-render"),
    (2, "after-document-close"),
    (3, "after-view-destroyed"),
)
TEST_SLOT = "capturesLargeSubPrepBoundaryWhenConfigured"


class ControllerError(RuntimeError):
    pass


def default_executable(candidates: tuple[Path, ...], label: str) -> Path:
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    choices = "\n".join(f"  {path}" for path in candidates)
    raise ControllerError(
        f"Could not find the Debug {label}. Pass its path explicitly.\n{choices}"
    )


def atomic_write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    data = (json.dumps(payload, indent=2, ensure_ascii=False) + "\n").encode(
        "utf-8"
    )
    with tempfile.NamedTemporaryFile(
        mode="wb", dir=path.parent, prefix=f".{path.name}.", suffix=".tmp", delete=False
    ) as temporary:
        temporary.write(data)
        temporary.flush()
        os.fsync(temporary.fileno())
        temporary_path = Path(temporary.name)
    os.replace(temporary_path, path)


def read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ControllerError(f"Could not read valid JSON from {path}: {error}") from error
    if not isinstance(value, dict):
        raise ControllerError(f"Expected a JSON object in {path}.")
    return value


def process_is_alive(process_id: int) -> bool:
    if os.name == "nt":
        import ctypes

        process_query_limited_information = 0x1000
        still_active = 259
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        open_process = kernel32.OpenProcess
        open_process.argtypes = [ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
        open_process.restype = ctypes.c_void_p
        get_exit_code = kernel32.GetExitCodeProcess
        get_exit_code.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_ulong)]
        get_exit_code.restype = ctypes.c_int
        close_handle = kernel32.CloseHandle
        close_handle.argtypes = [ctypes.c_void_p]
        close_handle.restype = ctypes.c_int

        handle = open_process(
            process_query_limited_information,
            False,
            process_id,
        )
        if not handle:
            return False
        try:
            exit_code = ctypes.c_ulong()
            return bool(get_exit_code(handle, ctypes.byref(exit_code))) and (
                exit_code.value == still_active
            )
        finally:
            close_handle(handle)

    try:
        os.kill(process_id, 0)
    except OSError:
        return False
    return True


def validate_marker(
    marker: dict[str, Any],
    order: int,
    checkpoint: str,
    prior_timestamp: float,
    expected_run_id: str | None,
    expected_process_id: int | None,
) -> tuple[str, int, float]:
    if marker.get("schema") != "classmngr-f563-checkpoint-v1":
        raise ControllerError(f"Unexpected marker schema at {checkpoint}.")
    run_id = marker.get("runId")
    process_id_value = marker.get("processId")
    timestamp_value = marker.get("monotonicTimestampNanoseconds")
    if not isinstance(run_id, str) or not run_id:
        raise ControllerError(f"Marker {checkpoint} has no run ID.")
    if expected_run_id is not None and run_id != expected_run_id:
        raise ControllerError(f"Run ID changed at checkpoint {checkpoint}.")
    if (
        isinstance(process_id_value, bool)
        or not isinstance(process_id_value, (int, float))
        or not math.isfinite(float(process_id_value))
        or int(process_id_value) <= 0
    ):
        raise ControllerError(f"Marker {checkpoint} has an invalid process ID.")
    process_id = int(process_id_value)
    if expected_process_id is not None and process_id != expected_process_id:
        raise ControllerError(f"Process ID changed at checkpoint {checkpoint}.")
    if (
        isinstance(timestamp_value, bool)
        or not isinstance(timestamp_value, (int, float))
        or not math.isfinite(float(timestamp_value))
        or float(timestamp_value) <= prior_timestamp
    ):
        raise ControllerError(
            f"Marker {checkpoint} has a missing, invalid, or out-of-order monotonic timestamp."
        )
    if marker.get("checkpoint") != checkpoint:
        raise ControllerError(f"Marker checkpoint name/order mismatch at {checkpoint}.")
    if marker.get("checkpointOrder") != order:
        raise ControllerError(f"Marker checkpoint order mismatch at {checkpoint}.")
    if marker.get("documentAlive") is not True:
        raise ControllerError(f"The document is not alive at {checkpoint}.")

    if checkpoint == "after-render":
        count = marker.get("readyPageRenderCallbackCount")
        if (
            marker.get("documentStatus") != "Ready"
            or marker.get("viewAlive") is not True
            or marker.get("viewAttachedToDocument") is not True
            or marker.get("viewDestroyed") is not False
            or marker.get("guiProcessingDrained") is not True
            or isinstance(count, bool)
            or not isinstance(count, int)
            or count < 1
        ):
            raise ControllerError("The after-render marker does not prove a Ready render/view state.")
    elif checkpoint == "after-document-close":
        if (
            marker.get("documentStatus") != "Null"
            or marker.get("viewAlive") is not True
            or marker.get("viewAttachedToDocument") is not True
            or marker.get("viewDestroyed") is not False
            or marker.get("guiProcessingDrained") is not True
        ):
            raise ControllerError(
                "The after-document-close marker does not prove Null status with the same view attached."
            )
    elif checkpoint == "after-view-destroyed":
        if (
            marker.get("documentStatus") != "Null"
            or marker.get("viewAlive") is not False
            or marker.get("viewAttachedToDocument") is not False
            or marker.get("viewDestroyed") is not True
        ):
            raise ControllerError(
                "The after-view-destroyed marker does not prove view teardown with a live Null document."
            )
    else:
        raise ControllerError(f"Unknown checkpoint {checkpoint}.")

    return run_id, process_id, float(timestamp_value)


def write_ack(
    control_directory: Path,
    marker_path: Path,
    marker_bytes: bytes,
    marker: dict[str, Any],
    order: int,
    checkpoint: str,
) -> Path:
    ack_path = control_directory / f"ack-{order:02d}-{checkpoint}.json"
    if ack_path.exists():
        raise ControllerError(f"Refusing to replace existing ack {ack_path}.")
    atomic_write_json(
        ack_path,
        {
            "schema": "classmngr-f563-checkpoint-ack-v1",
            "runId": marker["runId"],
            "processId": int(marker["processId"]),
            "checkpoint": checkpoint,
            "checkpointOrder": order,
            "markerSha256": hashlib.sha256(marker_bytes).hexdigest(),
            "acknowledgedAtUtc": datetime.now(timezone.utc).isoformat(),
        },
    )
    return ack_path


def terminate_process_tree(process: subprocess.Popen[bytes]) -> None:
    if process.poll() is not None:
        return
    if os.name == "nt":
        subprocess.run(
            ["taskkill", "/PID", str(process.pid), "/T", "/F"],
            check=False,
            capture_output=True,
        )
    else:
        process.terminate()
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=10)


def run_controller(args: argparse.Namespace) -> Path:
    repository_root = Path(__file__).resolve().parents[2]
    application = (
        args.application.resolve()
        if args.application
        else default_executable(
            (
                repository_root / "build/windows-x64-debug-ninja/ClassMngr.exe",
                repository_root / "build/windows-x64-debug/Debug/ClassMngr.exe",
                repository_root / "build/windows-x64-debug/ClassMngr.exe",
            ),
            "ClassMngr application",
        )
    )
    test_executable = (
        args.test_executable.resolve()
        if args.test_executable
        else default_executable(
            (
                repository_root
                / "build/windows-x64-debug-ninja/ClassMngrStartupPerformanceTests.exe",
                repository_root
                / "build/windows-x64-debug/Debug/ClassMngrStartupPerformanceTests.exe",
                repository_root
                / "build/windows-x64-debug/ClassMngrStartupPerformanceTests.exe",
            ),
            "startup performance test executable",
        )
    )
    if not application.is_file():
        raise ControllerError(f"Debug application does not exist: {application}")
    if not test_executable.is_file():
        raise ControllerError(f"Debug lifecycle runner does not exist: {test_executable}")
    if application == test_executable:
        raise ControllerError("The app and lifecycle test executable must be different files.")

    local_app_data = os.environ.get("LOCALAPPDATA", "").strip()
    if not local_app_data:
        raise ControllerError("LOCALAPPDATA is not set; cannot place dry-run artifacts under LocalAppData\\Temp.")
    artifact_parent = Path(local_app_data) / "Temp"
    artifact_parent.mkdir(parents=True, exist_ok=True)
    run_stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    artifact_root = artifact_parent / (
        f"ClassMngr-F563-Checkpoint-DryRun-{run_stamp}-{os.getpid()}"
    )
    artifact_root.mkdir()
    control_directory = artifact_root / "control"
    control_directory.mkdir()

    route_output = artifact_root / "lifecycle-sub-prep"
    route_output.mkdir()
    fixture_path = route_output / "large_startup.tps"
    environment = os.environ.copy()
    environment.update(
        {
            "CLASSMNGR_TEST_APP_PATH": str(application),
            "CLASSMNGR_LARGE_SUB_PREP_BOUNDARY_REFERENCE_DIR": str(route_output),
            "CLASSMNGR_LARGE_STARTUP_FIXTURE_OUTPUT_PATH": str(fixture_path),
            "CLASSMNGR_STARTUP_PDF_LIFECYCLE_GRAB_ARM": "without-grabs",
            "CLASSMNGR_STARTUP_PDF_LIFECYCLE_INNER_BOUNDARIES": "1",
            "CLASSMNGR_STARTUP_PDF_LIFECYCLE_PAGE_RENDER_OBSERVER": "1",
            "CLASSMNGR_STARTUP_PDF_LIFECYCLE_CHECKPOINT_HANDSHAKE": "1",
            "CLASSMNGR_STARTUP_PDF_LIFECYCLE_CHECKPOINT_CONTROL_DIR": str(
                control_directory
            ),
            "QT_QPA_PLATFORM": "offscreen",
        }
    )
    qt_bin = args.qt_bin or Path(r"C:\Qt\6.12.0\msvc2022_64\bin")
    if qt_bin.is_dir():
        environment["PATH"] = str(qt_bin.resolve()) + os.pathsep + environment.get(
            "PATH", ""
        )

    stdout_path = artifact_root / "controller-stdout.txt"
    stderr_path = artifact_root / "controller-stderr.txt"
    summary_path = artifact_root / "controller-summary.json"
    command = [str(test_executable), TEST_SLOT]
    checkpoint_records: list[dict[str, Any]] = []
    expected_run_id: str | None = None
    expected_process_id: int | None = None
    prior_timestamp = 0.0
    process: subprocess.Popen[bytes] | None = None

    try:
        with stdout_path.open("wb") as stdout_log, stderr_path.open("wb") as stderr_log:
            process = subprocess.Popen(
                command,
                cwd=repository_root,
                env=environment,
                stdout=stdout_log,
                stderr=stderr_log,
            )
            for order, checkpoint in CHECKPOINTS:
                marker_path = control_directory / f"ready-{order:02d}-{checkpoint}.json"
                deadline = time.monotonic() + args.timeout_seconds
                while not marker_path.is_file():
                    failure_path = control_directory / "failure.json"
                    if failure_path.is_file():
                        failure = read_json(failure_path)
                        raise ControllerError(
                            f"App checkpoint failure: {failure.get('error', 'unknown error')}"
                        )
                    if process.poll() is not None:
                        raise ControllerError(
                            f"Lifecycle runner exited with code {process.returncode} before {checkpoint}."
                        )
                    if time.monotonic() >= deadline:
                        raise ControllerError(f"Timed out waiting for marker {marker_path}.")
                    time.sleep(0.025)

                marker_bytes = marker_path.read_bytes()
                marker = read_json(marker_path)
                run_id, process_id, prior_timestamp = validate_marker(
                    marker,
                    order,
                    checkpoint,
                    prior_timestamp,
                    expected_run_id,
                    expected_process_id,
                )
                if not process_is_alive(process_id):
                    raise ControllerError(
                        f"ClassMngr PID {process_id} was not alive at {checkpoint}."
                    )
                expected_run_id = run_id
                expected_process_id = process_id
                ack_path = write_ack(
                    control_directory,
                    marker_path,
                    marker_bytes,
                    marker,
                    order,
                    checkpoint,
                )
                checkpoint_records.append(
                    {
                        "checkpoint": checkpoint,
                        "checkpointOrder": order,
                        "processId": process_id,
                        "documentStatus": marker["documentStatus"],
                        "viewAlive": marker["viewAlive"],
                        "documentAlive": marker["documentAlive"],
                        "monotonicTimestampNanoseconds": prior_timestamp,
                        "markerPath": str(marker_path),
                        "ackPath": str(ack_path),
                    }
                )

            try:
                exit_code = process.wait(timeout=args.timeout_seconds)
            except subprocess.TimeoutExpired as error:
                raise ControllerError(
                    "The lifecycle test runner did not finish after the third ack."
                ) from error
            if exit_code != 0:
                raise ControllerError(
                    f"Debug lifecycle-sub-prep runner exited with code {exit_code}."
                )

        atomic_write_json(
            summary_path,
            {
                "schema": "classmngr-f563-checkpoint-dry-run-v1",
                "createdAtUtc": datetime.now(timezone.utc).isoformat(),
                "application": str(application),
                "testExecutable": str(test_executable),
                "testSlot": TEST_SLOT,
                "route": "Debug lifecycle-sub-prep with PDF page-render observer opt-in",
                "runId": expected_run_id,
                "processId": expected_process_id,
                "runnerExitCode": 0,
                "checkpoints": checkpoint_records,
                "wprInvoked": False,
                "artifacts": str(artifact_root),
            },
        )
        print(f"F563 dry-run passed. Artifacts: {artifact_root}")
        return artifact_root
    except Exception as error:
        try:
            atomic_write_json(
                control_directory / "controller-error.json",
                {
                    "schema": "classmngr-f563-checkpoint-controller-error-v1",
                    "error": str(error),
                    "createdAtUtc": datetime.now(timezone.utc).isoformat(),
                },
            )
        except OSError:
            pass
        if process is not None:
            terminate_process_tree(process)
        atomic_write_json(
            summary_path,
            {
                "schema": "classmngr-f563-checkpoint-dry-run-v1",
                "createdAtUtc": datetime.now(timezone.utc).isoformat(),
                "application": str(application),
                "testExecutable": str(test_executable),
                "testSlot": TEST_SLOT,
                "runId": expected_run_id,
                "processId": expected_process_id,
                "runnerExitCode": process.returncode if process else None,
                "checkpoints": checkpoint_records,
                "wprInvoked": False,
                "error": str(error),
                "artifacts": str(artifact_root),
            },
        )
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--application", type=Path, help="Debug ClassMngr.exe path")
    parser.add_argument(
        "--test-executable",
        type=Path,
        help="Debug ClassMngrStartupPerformanceTests.exe path",
    )
    parser.add_argument(
        "--qt-bin",
        type=Path,
        help="Optional Qt bin directory prepended to PATH",
    )
    parser.add_argument(
        "--timeout-seconds",
        type=int,
        default=90,
        help="Maximum wait for each marker and final runner completion (default: 90)",
    )
    args = parser.parse_args()
    if args.timeout_seconds < 1 or args.timeout_seconds > 300:
        parser.error("--timeout-seconds must be between 1 and 300.")

    try:
        run_controller(args)
        return 0
    except (ControllerError, OSError, subprocess.SubprocessError) as error:
        print(f"F563 dry-run failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
