#!/usr/bin/env python3
"""Run the supplemental Linux x86_64 packaged Release Phase 0 route matrix."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import shutil
import signal
import shlex
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import run_phase0_evidence_macos as route_contract


TASK_ID = "QT0-EVIDENCE-AUTOMATION"
RUNNER_TASK_ID = "QT0-LINUX-SUPPLEMENTAL-ROUTE-MATRIX-01"
RUN_SCHEMA = "classmngr-phase0-evidence-run-v1"
ROUTE_SCHEMA = "classmngr-phase0-route-v1"
OFFICIAL_SUPPORTED_PLATFORMS = ("windows-x64", "macos-universal")
SUPPLEMENTAL_PLATFORMS = ("linux-x64",)
DEFERRED_PLATFORMS = ("windows-arm64", "linux")
ROUTES = route_contract.ROUTES
ROUTE_BY_ID = route_contract.ROUTE_BY_ID
ALL_ROUTE_IDS = route_contract.ALL_ROUTE_IDS
DEFAULT_TIMEOUT_SECONDS = 900
DEFAULT_PARALLEL = max(1, min(os.cpu_count() or 1, 8))
ELF_MACHINE_X86_64 = 62


class RunnerError(RuntimeError):
    pass


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def _path_text(path: Path) -> str:
    return str(path.expanduser().absolute())


def _route_paths(root: Path, artifact_path: str) -> Path:
    return root.joinpath(*artifact_path.split("/"))


def _xvfb_command(executable: str, arguments: list[str]) -> list[str]:
    return [
        shutil.which("xvfb-run") or "xvfb-run",
        "--auto-servernum",
        "--server-args=-screen 0 1920x1080x24",
        executable,
        *arguments,
    ]


def route_invocation(
    route_id: str,
    evidence_root: Path,
    application_path: Path,
    test_executable: Path,
    repository_root: Path | None = None,
) -> dict[str, Any]:
    """Reuse the established Phase 0 invocation map for every canonical route."""
    invocation = route_contract.route_invocation(
        route_id,
        evidence_root,
        application_path,
        test_executable,
        repository_root,
    )
    invocation["environmentOverrides"]["QT_QPA_PLATFORM"] = "xcb"
    invocation["environmentOverrides"]["QT_QPA_PLATFORM_PLUGIN_PATH"] = str(
        application_path.absolute().parent.parent / "plugins" / "platforms"
    )
    return invocation


def _platform_metadata() -> dict[str, Any]:
    return {
        "supportedPlatform": "linux-x64",
        "supportedPlatforms": list(OFFICIAL_SUPPORTED_PLATFORMS),
        "supplementalPlatforms": list(SUPPLEMENTAL_PLATFORMS),
        "deferredPlatforms": list(DEFERRED_PLATFORMS),
        "platform": {
            "name": "linux-x64",
            "supported": True,
            "supplemental": True,
            "acceptanceScope": (
                "Supplemental packaged Release Linux x86_64 baseline; this evidence "
                "does not add Linux to the official Phase 0 exit gate."
            ),
        },
    }


def _inside(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def _validate_evidence_root(root: Path, repository_root: Path, *, create: bool) -> Path:
    candidate = root.expanduser().absolute()
    repository = repository_root.resolve()
    if candidate == Path(candidate.anchor):
        raise RunnerError("Evidence root cannot be a filesystem root.")
    if _inside(candidate, repository):
        raise RunnerError(f"Evidence root must be outside the repository: {candidate}")
    if not candidate.parent.is_dir():
        raise RunnerError(f"Evidence root parent does not exist: {candidate.parent}")
    cursor = candidate.parent
    while True:
        if cursor.is_symlink():
            raise RunnerError(f"Evidence path traverses a symlink: {cursor}")
        if cursor.parent == cursor:
            break
        cursor = cursor.parent
    if candidate.exists() or candidate.is_symlink():
        raise RunnerError(f"Evidence root already exists and will not be reused: {candidate}")
    if create:
        try:
            candidate.mkdir()
        except FileExistsError as error:
            raise RunnerError(f"Evidence root already exists and will not be reused: {candidate}") from error
    return candidate


def _is_x86_64_elf(path: Path) -> bool:
    try:
        with path.open("rb") as stream:
            header = stream.read(20)
    except OSError:
        return False
    if len(header) < 20 or header[:4] != b"\x7fELF" or header[4] != 2:
        return False
    byte_order = "little" if header[5] == 1 else "big" if header[5] == 2 else None
    if byte_order is None:
        return False
    return int.from_bytes(header[18:20], byte_order) == ELF_MACHINE_X86_64


def _validate_package_root(package_root: Path) -> tuple[Path, dict[str, Any]]:
    package = package_root.expanduser().absolute()
    if package.is_symlink() or not package.is_dir():
        raise RunnerError(f"Staged Linux package root must be a real directory: {package}")
    application = package / "bin" / "ClassMngr"
    if application.is_symlink() or not application.is_file() or not os.access(application, os.X_OK):
        raise RunnerError(f"Staged package executable is missing or not executable: {application}")
    if not _is_x86_64_elf(application):
        raise RunnerError(f"Staged package executable is not an ELF x86_64 binary: {application}")
    platform_plugin = package / "plugins" / "platforms" / "libqxcb.so"
    if platform_plugin.is_symlink() or not platform_plugin.is_file():
        raise RunnerError(f"Staged package xcb platform plugin is missing: {platform_plugin}")
    if not _is_x86_64_elf(platform_plugin):
        raise RunnerError(f"Staged package xcb platform plugin is not an ELF x86_64 binary: {platform_plugin}")
    digest = hashlib.sha256(application.read_bytes()).hexdigest()
    return application, {
        "applicationSha256": digest,
        "applicationSizeBytes": application.stat().st_size,
        "applicationArchitecture": "x86_64",
        "packageRoot": str(package),
        "platformPluginBackend": "xcb",
        "platformPluginPath": str(platform_plugin),
        "platformPluginSha256": hashlib.sha256(platform_plugin.read_bytes()).hexdigest(),
        "platformPluginArchitecture": "x86_64",
    }


def _preflight(
    repository_root: Path,
    package_root: Path,
    qt_prefix: Path,
) -> tuple[Path, dict[str, Any], dict[str, Any]]:
    if platform.system() != "Linux":
        raise RunnerError("This runner requires Linux.")
    architecture = platform.machine().lower()
    if architecture not in {"x86_64", "amd64"}:
        raise RunnerError(f"This run requires Linux x86_64; host architecture is {architecture!r}.")
    repository = repository_root.expanduser().absolute()
    if not repository.is_dir() or not (repository / "CMakeLists.txt").is_file():
        raise RunnerError(f"Repository root is not a ClassMngr source tree: {repository}")
    application, package_audit = _validate_package_root(package_root)
    qt = qt_prefix.expanduser().absolute()
    required_paths = (
        qt / "lib" / "cmake" / "Qt6" / "Qt6Config.cmake",
        qt / "lib" / "cmake" / "Qt6Test" / "Qt6TestConfig.cmake",
    )
    for required in required_paths:
        if not required.is_file():
            raise RunnerError(f"Required Qt 6.12 build/runtime component is missing: {required}")
    for executable in ("cmake", "ninja", "git", "xvfb-run", "Xvfb", "xauth"):
        if shutil.which(executable) is None:
            raise RunnerError(f"Required executable is not on PATH: {executable}")
    host = {
        "system": platform.system(),
        "release": platform.release(),
        "machine": platform.machine(),
        "python": sys.version,
    }
    return application, package_audit, {
        "recordedAtUtc": utc_now(),
        "platform": f"{platform.system()} {platform.release()} {platform.machine()}",
        "machine": platform.machine(),
        "host": host,
        "testHarnessQtPrefix": str(qt),
    }


def _git_snapshot(repository_root: Path) -> dict[str, Any]:
    commit = subprocess.run(
        ["git", "-C", str(repository_root), "rev-parse", "HEAD"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    status = subprocess.run(
        ["git", "-C", str(repository_root), "status", "--porcelain", "--untracked-files=all"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.splitlines()
    return {
        "commit": commit,
        "statusPorcelain": status,
        "workingTreeClean": not status,
    }


def _write_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(value, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)


def _new_manifest(
    evidence_root: Path,
    repository_root: Path,
    package_root: Path,
    application_path: Path,
    qt_prefix: Path,
    test_build_directory: Path,
    source_snapshot: dict[str, Any],
    package_audit: dict[str, Any],
    timeout_seconds: int,
    parallel: int,
) -> dict[str, Any]:
    test_executable = evidence_root / "tools" / "ClassMngrStartupPerformanceTests"
    return {
        "schema": RUN_SCHEMA,
        "taskId": TASK_ID,
        "runnerTaskId": RUNNER_TASK_ID,
        "recordedAtUtc": utc_now(),
        "status": "running",
        "scenario": "Supplemental Linux x86_64 packaged Release Phase 0 baseline",
        "fixture": "packaged Release Linux x86_64",
        **_platform_metadata(),
        "source": source_snapshot["source"],
        "host": source_snapshot["host"],
        "paths": {
            "repositoryRoot": str(repository_root),
            "packageRoot": str(package_root),
            "applicationPath": str(application_path),
            "testHarnessQtPrefix": str(qt_prefix),
            "testBuildDirectory": str(test_build_directory),
            "testExecutable": str(test_executable),
            "evidenceRoot": str(evidence_root),
        },
        "packageAudit": package_audit,
        "options": {
            "plan": False,
            "routes": ["all"],
            "parallel": parallel,
            "timeoutSeconds": timeout_seconds,
            "officialExitGateIncludesLinux": False,
        },
        "requestedRoutes": list(ALL_ROUTE_IDS),
        "requiredOptInEnvironmentVariables": [
            "QT_QPA_PLATFORM",
            "QT_QPA_PLATFORM_PLUGIN_PATH",
            "CLASSMNGR_TEST_APP_PATH",
            "CLASSMNGR_SETTINGS_ROOT",
            "CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH",
            "CLASSMNGR_STARTUP_PDF_CAPTURE_OUTPUT_DIR",
            "CLASSMNGR_VISUAL_BASELINE_OUTPUT_DIR",
            "CLASSMNGR_STARTUP_RESOURCE_TRACE_PATH",
        ],
        "memoryContract": {
            "legacyResidentTargetBytes": 250 * 1024 * 1024,
            "legacyResidentTargetMiB": 250,
            "temporaryDiagnosticCeilingBytes": 512 * 1024 * 1024,
            "temporaryDiagnosticCeilingMiB": 512,
            "legacy250MiBIsPhase0Failure": False,
            "temporary512MiBIsPhase0Failure": False,
        },
        "build": {"status": "pending"},
        "packageLaunchSmoke": {"status": "not-started"},
        "routeExecution": {
            "status": "not-started",
            "requestedRouteCount": len(ALL_ROUTE_IDS),
            "executedRouteIds": [],
            "failedRouteIds": [],
        },
        "routes": [],
        "commands": [],
        "failures": [],
        "warnings": [],
    }


def _cmake_commands(
    repository_root: Path,
    evidence_root: Path,
    application_path: Path,
    qt_prefix: Path,
    test_build_directory: Path,
    timeout_seconds: int,
    parallel: int,
) -> dict[str, Any]:
    cmake = shutil.which("cmake") or "cmake"
    configure = [
        cmake,
        "--fresh",
        "-S",
        str(repository_root),
        "-B",
        str(test_build_directory),
        "-G",
        "Ninja",
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DCMAKE_CXX_STANDARD=23",
        "-DCMAKE_CXX_STANDARD_REQUIRED=ON",
        "-DCMAKE_CXX_EXTENSIONS=OFF",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-DBUILD_TESTING=ON",
        f"-DCMAKE_PREFIX_PATH={qt_prefix}",
    ]
    build = [
        cmake,
        "--build",
        str(test_build_directory),
        "--target",
        "ClassMngrStartupPerformanceTests",
        "--parallel",
        str(parallel),
    ]
    smoke_metrics = evidence_root / "package-launch-smoke" / "startup-metrics.json"
    smoke_command = _xvfb_command(
        str(application_path),
        [
            "--startup-performance-test",
            "--startup-performance-scenario",
            "minimal",
            "--startup-performance-settle-ms",
            "1000",
            "--startup-performance-output",
            str(smoke_metrics),
        ],
    )
    return {
        "debugHarnessConfigure": configure,
        "debugHarnessBuild": build,
        "packageLaunchSmoke": smoke_command,
        "timeoutSeconds": timeout_seconds,
    }


def build_plan(
    repository_root: Path,
    evidence_root: Path,
    package_root: Path,
    qt_prefix: Path,
    timeout_seconds: int,
    parallel: int,
) -> dict[str, Any]:
    repository = repository_root.expanduser().absolute()
    evidence = evidence_root.expanduser().absolute()
    package = package_root.expanduser().absolute()
    qt = qt_prefix.expanduser().absolute()
    application, package_audit = _validate_package_root(package)
    test_build_directory = evidence.with_name(evidence.name + "-linux-debug-build")
    if test_build_directory.exists() or test_build_directory.is_symlink():
        raise RunnerError(f"Isolated test build directory already exists: {test_build_directory}")
    commands = _cmake_commands(
        repository,
        evidence,
        application,
        qt,
        test_build_directory,
        timeout_seconds,
        parallel,
    )
    route_commands = []
    planned_test_executable = evidence / "tools" / "ClassMngrStartupPerformanceTests"
    for route in ROUTES:
        invocation = route_invocation(
            route.route_id,
            evidence,
            application,
            planned_test_executable,
            repository,
        )
        route_commands.append(
            {
                "routeId": route.route_id,
                "filePath": invocation["filePath"],
                "command": _xvfb_command(
                    invocation["filePath"], invocation["arguments"]
                ),
                "arguments": invocation["arguments"],
                "environmentOverrides": invocation["environmentOverrides"],
            }
        )
    return {
        "scenario": "Supplemental Linux x86_64 packaged Release Phase 0 baseline",
        "platform": _platform_metadata(),
        "evidenceRoot": str(evidence),
        "repositoryRoot": str(repository),
        "packageRoot": str(package),
        "applicationPath": str(application),
        "testHarnessQtPrefix": str(qt),
        "testBuildDirectory": str(test_build_directory),
        "testExecutable": str(planned_test_executable),
        "packageAudit": package_audit,
        "packageLaunchEnvironment": _recorded_environment(
            _isolated_environment(evidence, application, qt)
        ),
        "timeoutSeconds": timeout_seconds,
        "parallel": parallel,
        "requestedRouteCount": len(ALL_ROUTE_IDS),
        "requestedRoutes": list(ALL_ROUTE_IDS),
        "commands": commands,
        "routeCommands": route_commands,
    }


def _append_path(parts: list[str], value: str | None) -> None:
    if value and value not in parts:
        parts.append(value)


def _isolated_environment(
    evidence_root: Path,
    application_path: Path,
    qt_prefix: Path,
    overrides: dict[str, str] | None = None,
) -> dict[str, str]:
    root = evidence_root.absolute()
    temporary = root / "tmp"
    env = {
        key: value
        for key, value in os.environ.items()
        if not key.startswith(("CLASSMNGR_", "QT_", "LD_"))
        and key not in {
            "HOME",
            "TMP",
            "TEMP",
            "TMPDIR",
            "XDG_CONFIG_HOME",
            "XDG_DATA_HOME",
            "XDG_CACHE_HOME",
            "XDG_STATE_HOME",
            "CMAKE_PREFIX_PATH",
        }
    }
    path_parts: list[str] = []
    _append_path(path_parts, str(application_path.absolute().parent))
    qt_root = qt_prefix.expanduser().absolute()
    for part in os.environ.get("PATH", "").split(os.pathsep):
        if not part:
            continue
        candidate = Path(part).expanduser().absolute()
        if candidate == qt_root or _inside(candidate, qt_root):
            continue
        _append_path(path_parts, part)
    env.update(
        {
            "PATH": os.pathsep.join(path_parts),
            "HOME": str(root / "runtime" / "home"),
            "TMPDIR": str(temporary),
            "TMP": str(temporary),
            "TEMP": str(temporary),
            "XDG_CONFIG_HOME": str(root / "runtime" / "xdg-config"),
            "XDG_DATA_HOME": str(root / "runtime" / "xdg-data"),
            "XDG_CACHE_HOME": str(root / "runtime" / "xdg-cache"),
            "XDG_STATE_HOME": str(root / "runtime" / "xdg-state"),
            "QT_QPA_PLATFORM": "xcb",
            "QT_QPA_PLATFORM_PLUGIN_PATH": str(
                application_path.absolute().parent.parent / "plugins" / "platforms"
            ),
            "CLASSMNGR_TEST_APP_PATH": str(application_path.absolute()),
            "PYTHONDONTWRITEBYTECODE": "1",
        }
    )
    if overrides:
        env.update(overrides)
    return env


def _recorded_environment(env: dict[str, str]) -> dict[str, str]:
    return {
        key: env[key]
        for key in sorted(env)
        if key in {
            "PATH",
            "HOME",
            "TMPDIR",
            "TMP",
            "TEMP",
            "XDG_CONFIG_HOME",
            "XDG_DATA_HOME",
            "XDG_CACHE_HOME",
            "XDG_STATE_HOME",
            "QT_QPA_PLATFORM",
            "QT_QPA_PLATFORM_PLUGIN_PATH",
            "PYTHONDONTWRITEBYTECODE",
        }
        or key.startswith(("CLASSMNGR_", "CCACHE_"))
    }


def _build_environment(evidence_root: Path, qt_prefix: Path) -> dict[str, str]:
    root = evidence_root.absolute()
    env = {
        key: value
        for key, value in os.environ.items()
        if not key.startswith(("CLASSMNGR_", "QT_", "LD_", "CCACHE_"))
        and key not in {"CMAKE_PREFIX_PATH"}
    }
    path_parts: list[str] = []
    _append_path(path_parts, str(qt_prefix.absolute() / "bin"))
    for part in os.environ.get("PATH", "").split(os.pathsep):
        _append_path(path_parts, part)
    env.update(
        {
            "PATH": os.pathsep.join(path_parts),
            "QT_LINUX_PREFIX": str(qt_prefix.absolute()),
            "CCACHE_DIR": str(root / "tmp" / "ccache"),
            "HOME": str(root / "runtime" / "build-home"),
            "TMPDIR": str(root / "tmp"),
            "PYTHONDONTWRITEBYTECODE": "1",
        }
    )
    return env


def _process_record(
    label: str,
    command: list[str],
    cwd: Path,
    env: dict[str, str],
    log_directory: Path,
    timeout_seconds: int,
) -> dict[str, Any]:
    log_directory.mkdir(parents=True, exist_ok=True)
    stdout_path = log_directory / "runner-stdout.txt"
    stderr_path = log_directory / "runner-stderr.txt"
    command = [str(item) for item in command]
    record: dict[str, Any] = {
        "label": label,
        "command": command,
        "commandLine": shlex.join(command),
        "workingDirectory": str(cwd),
        "environment": _recorded_environment(env),
        "startedAtUtc": utc_now(),
        "stdoutPath": str(stdout_path),
        "stderrPath": str(stderr_path),
        "processFinished": False,
        "exitStatus": "not-started",
        "exitCode": None,
        "timedOut": False,
        "durationSeconds": 0.0,
    }
    print(f"[{label}] start: {record['commandLine']}", flush=True)
    start = time.monotonic()
    process: subprocess.Popen[bytes] | None = None
    try:
        with stdout_path.open("wb") as stdout_stream, stderr_path.open("wb") as stderr_stream:
            process = subprocess.Popen(
                command,
                cwd=str(cwd),
                env=env,
                stdin=subprocess.DEVNULL,
                stdout=stdout_stream,
                stderr=stderr_stream,
                start_new_session=True,
            )
            record["processId"] = process.pid
            try:
                process.wait(timeout=timeout_seconds)
            except subprocess.TimeoutExpired:
                record["timedOut"] = True
                record["terminationSignal"] = "SIGTERM"
                try:
                    os.killpg(process.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    record["terminationSignal"] = "SIGKILL"
                    try:
                        os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    process.wait()
        record["processFinished"] = process.poll() is not None
        record["exitCode"] = process.returncode
        record["exitStatus"] = "timeout" if record["timedOut"] else "normal"
    except Exception as error:
        record["exitStatus"] = "failed-to-start"
        record["error"] = f"{type(error).__name__}: {error}"
        stdout_path.touch(exist_ok=True)
        with stderr_path.open("a", encoding="utf-8") as stream:
            stream.write(record["error"] + "\n")
    finally:
        if process is not None and process.poll() is None:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait()
        record["durationSeconds"] = round(time.monotonic() - start, 3)
        record["finishedAtUtc"] = utc_now()
        print(
            f"[{label}] finished: exitCode={record['exitCode']} "
            f"timedOut={record['timedOut']} durationSeconds={record['durationSeconds']}",
            flush=True,
        )
    return record


def _is_success(record: dict[str, Any]) -> bool:
    return (
        record.get("processFinished") is True
        and record.get("exitStatus") == "normal"
        and record.get("exitCode") == 0
        and record.get("timedOut") is False
    )


def _invoke(
    manifest: dict[str, Any],
    command_id: str,
    kind: str,
    command: list[str],
    cwd: Path,
    env: dict[str, str],
    log_directory: Path,
    evidence_root: Path,
    timeout_seconds: int,
) -> dict[str, Any]:
    record = _process_record(command_id, command, cwd, env, log_directory, timeout_seconds)
    record["id"] = command_id
    record["kind"] = kind
    record["status"] = "completed" if _is_success(record) else "failed"
    manifest["commands"].append(record)
    _write_json(evidence_root / "run-manifest.json", manifest)
    return record


def _require_success(record: dict[str, Any]) -> None:
    if not _is_success(record):
        raise RunnerError(
            f"{record.get('id', record.get('label'))} failed "
            f"(exitCode={record.get('exitCode')}, timedOut={record.get('timedOut')}). "
            f"See {record.get('stderrPath')}."
        )


def _write_deterministic_settings(settings_root: Path) -> None:
    settings_directory = settings_root / "PaperCloud"
    settings_directory.mkdir(parents=True, exist_ok=True)
    settings_path = settings_directory / "ClassMngr.ini"
    contents = (
        "[options]\n"
        "theme=1\n"
        "fontSize=2\n"
        "language=1\n"
        "saveMode=0\n"
        "documentPageSpacing=2\n"
        "documentViewerBackground=1\n"
        "sidebarTooltipsEnabled=true\n"
        "sidebarMarqueeEnabled=false\n"
        "\n"
        "[updates]\n"
        "automaticChecksEnabled=false\n"
    )
    with settings_path.open("x", encoding="utf-8", newline="\n") as stream:
        stream.write(contents)


def _prepare_isolated_runtime(evidence_root: Path) -> None:
    root = evidence_root.absolute()
    for relative in (
        "runtime/home",
        "runtime/xdg-config",
        "runtime/xdg-data",
        "runtime/xdg-cache",
        "runtime/xdg-state",
        "tmp",
    ):
        (root / relative).mkdir(parents=True, exist_ok=True)


def _execute_package_launch_smoke(
    manifest: dict[str, Any],
    evidence_root: Path,
    repository_root: Path,
    qt_prefix: Path,
    application_path: Path,
    timeout_seconds: int,
) -> bool:
    _prepare_isolated_runtime(evidence_root)
    smoke_root = evidence_root / "package-launch-smoke"
    metrics_path = smoke_root / "startup-metrics.json"
    smoke_root.mkdir(parents=True, exist_ok=False)
    settings_root = evidence_root / "runtime" / "settings-package-launch-smoke"
    _write_deterministic_settings(settings_root)
    command = _xvfb_command(
        str(application_path),
        [
            "--startup-performance-test",
            "--startup-performance-scenario",
            "minimal",
            "--startup-performance-settle-ms",
            "1000",
            "--startup-performance-output",
            str(metrics_path),
        ],
    )
    env = _isolated_environment(
        evidence_root,
        application_path,
        qt_prefix,
        {"CLASSMNGR_SETTINGS_ROOT": str(settings_root)},
    )
    process = _process_record(
        "linux-installed-package-launch-smoke",
        command,
        repository_root,
        env,
        smoke_root / "runner",
        timeout_seconds,
    )
    status = "completed" if _is_success(process) else "failed"
    manifest["packageLaunchSmoke"] = {
        "status": status,
        "applicationPath": str(application_path),
        "platformPluginBackend": "xcb",
        "platformPluginPath": str(
            application_path.absolute().parent.parent / "plugins" / "platforms"
        ),
        "metricsPath": "package-launch-smoke/startup-metrics.json",
        "invocation": process,
    }
    command_record = dict(process)
    command_record.update(
        {"id": "linux-installed-package-launch-smoke", "kind": "package-launch-smoke", "status": status}
    )
    manifest["commands"].append(command_record)
    if status != "completed":
        manifest["failures"].append(
            {
                "code": "package-launch-smoke-failed",
                "message": (
                    f"Installed-package launch smoke exited abnormally: "
                    f"exitStatus={process.get('exitStatus')}, exitCode={process.get('exitCode')}, "
                    f"timedOut={process.get('timedOut')}."
                ),
                "stdoutPath": process["stdoutPath"],
                "stderrPath": process["stderrPath"],
            }
        )
    _write_json(evidence_root / "run-manifest.json", manifest)
    return status == "completed"


def _execute_routes(
    manifest: dict[str, Any],
    evidence_root: Path,
    repository_root: Path,
    qt_prefix: Path,
    application_path: Path,
    test_executable: Path,
    timeout_seconds: int,
) -> list[str]:
    (evidence_root / "runtime").mkdir(parents=True, exist_ok=True)
    (evidence_root / "tmp").mkdir(parents=True, exist_ok=True)
    _prepare_isolated_runtime(evidence_root)
    _write_deterministic_settings(evidence_root / "runtime" / "representative-settings")
    executed: list[str] = []
    failed: list[str] = []
    route_records: list[dict[str, Any]] = []

    for definition in ROUTES:
        invocation_plan = route_invocation(
            definition.route_id,
            evidence_root,
            application_path,
            test_executable,
            repository_root,
        )
        route_root = _route_paths(evidence_root, definition.artifact_path)
        if route_root.exists() or route_root.is_symlink():
            raise RunnerError(f"Route output path already exists inside new evidence root: {route_root}")
        route_root.mkdir(parents=True)
        settings_value = invocation_plan["environmentOverrides"].get("CLASSMNGR_SETTINGS_ROOT")
        if settings_value:
            settings_root = Path(settings_value)
            settings_path = settings_root / "PaperCloud" / "ClassMngr.ini"
            if not settings_path.exists():
                _write_deterministic_settings(settings_root)
        env = _isolated_environment(
            evidence_root,
            application_path,
            qt_prefix,
            invocation_plan["environmentOverrides"],
        )
        command = _xvfb_command(
            invocation_plan["filePath"], invocation_plan["arguments"]
        )
        process = _process_record(
            definition.route_id,
            command,
            repository_root,
            env,
            route_root / "runner",
            timeout_seconds,
        )
        status = "completed" if _is_success(process) else "failed"
        route_manifest = {
            "schema": ROUTE_SCHEMA,
            "taskId": TASK_ID,
            "routeId": definition.route_id,
            "category": definition.category,
            "scenario": definition.scenario,
            "fixture": definition.fixture,
            "artifactPath": definition.artifact_path,
            "status": status,
            "invocation": process,
        }
        _write_json(route_root / "route-manifest.json", route_manifest)
        command_record = dict(process)
        command_record.update({"id": definition.route_id, "kind": "route", "status": status})
        manifest["commands"].append(command_record)
        route_records.append(route_manifest)
        executed.append(definition.route_id)
        if status != "completed":
            failed.append(definition.route_id)
            manifest["failures"].append(
                {
                    "code": "route-process-failed",
                    "routeId": definition.route_id,
                    "message": (
                        f"Route exited abnormally: exitStatus={process.get('exitStatus')}, "
                        f"exitCode={process.get('exitCode')}, timedOut={process.get('timedOut')}."
                    ),
                    "stdoutPath": process["stdoutPath"],
                    "stderrPath": process["stderrPath"],
                }
            )
        manifest["routes"] = route_records
        manifest["routeExecution"] = {
            "status": "running",
            "requestedRouteCount": len(ALL_ROUTE_IDS),
            "executedRouteIds": executed,
            "failedRouteIds": failed,
        }
        _write_json(evidence_root / "run-manifest.json", manifest)
    return failed


def _validate_output(
    manifest: dict[str, Any],
    evidence_root: Path,
    repository_root: Path,
    qt_prefix: Path,
    timeout_seconds: int,
) -> bool:
    validator_path = repository_root / "scripts" / "phase0" / "validate_phase0_evidence.py"
    summary_path = evidence_root / "validation-summary.json"
    record = _invoke(
        manifest,
        "validate-linux-x64-supplemental-evidence",
        "validation",
        [
            sys.executable,
            str(validator_path),
            "--evidence-root",
            str(evidence_root),
            "--expected-platform",
            "linux-x64",
            "--json-out",
            str(summary_path),
        ],
        repository_root,
        _build_environment(evidence_root, qt_prefix),
        evidence_root / "logs" / "validation",
        evidence_root,
        timeout_seconds,
    )
    try:
        summary = json.loads(summary_path.read_text(encoding="utf-8"))
        summary_status = summary.get("status")
    except (OSError, UnicodeError, json.JSONDecodeError):
        summary_status = "missing-or-invalid"
    passed = _is_success(record) and summary_status == "pass"
    manifest["validation"] = {
        "status": "passed" if passed else "failed",
        "summaryPath": str(summary_path),
        "summaryStatus": summary_status,
        "process": record,
    }
    _write_json(evidence_root / "run-manifest.json", manifest)
    return passed


def _run(
    evidence_root: Path,
    repository_root: Path,
    package_root: Path,
    qt_prefix: Path,
    timeout_seconds: int,
    parallel: int,
) -> int:
    root = _validate_evidence_root(evidence_root, repository_root, create=False)
    application_path, package_audit = _validate_package_root(package_root)
    test_build_directory = root.with_name(root.name + "-linux-debug-build")
    if test_build_directory.exists() or test_build_directory.is_symlink():
        raise RunnerError(f"Isolated test build directory already exists: {test_build_directory}")
    root = _validate_evidence_root(evidence_root, repository_root, create=True)
    try:
        application_path, package_audit, source_snapshot = _preflight(
            repository_root,
            package_root,
            qt_prefix,
        )
    except (RunnerError, OSError, subprocess.SubprocessError) as error:
        try:
            source_snapshot = {"source": _git_snapshot(repository_root)}
        except (OSError, subprocess.SubprocessError):
            source_snapshot = {
                "source": {
                    "commit": None,
                    "statusPorcelain": [],
                    "workingTreeClean": False,
                }
            }
        source_snapshot.update(
            {
                "recordedAtUtc": utc_now(),
                "platform": f"{platform.system()} {platform.release()} {platform.machine()}",
                "machine": platform.machine(),
                "host": {
                    "system": platform.system(),
                    "release": platform.release(),
                    "machine": platform.machine(),
                    "python": sys.version,
                },
            }
        )
        manifest = _new_manifest(
            root,
            repository_root,
            package_root.expanduser().absolute(),
            application_path,
            qt_prefix.expanduser().absolute(),
            test_build_directory,
            source_snapshot,
            package_audit,
            timeout_seconds,
            parallel,
        )
        manifest["status"] = "failed"
        manifest["preflight"] = {"status": "failed", "message": f"{type(error).__name__}: {error}"}
        manifest["failures"].append(
            {
                "code": "linux-preflight-failed",
                "message": manifest["preflight"]["message"],
                "executedRouteIds": [],
            }
        )
        _write_json(root / "run-manifest.json", manifest)
        _validate_output(manifest, root, repository_root, qt_prefix, timeout_seconds)
        print(f"ERROR: {error}", file=sys.stderr)
        return 2

    source_snapshot["source"] = _git_snapshot(repository_root)
    manifest = _new_manifest(
        root,
        repository_root,
        package_root.expanduser().absolute(),
        application_path,
        qt_prefix.expanduser().absolute(),
        test_build_directory,
        source_snapshot,
        package_audit,
        timeout_seconds,
        parallel,
    )
    _write_json(root / "run-manifest.json", manifest)
    build_root_created = False
    try:
        (root / "runtime" / "build-home").mkdir(parents=True, exist_ok=True)
        (root / "tmp").mkdir(parents=True, exist_ok=True)
        (root / "tmp" / "ccache").mkdir(parents=True, exist_ok=True)
        build_commands = _cmake_commands(
            repository_root,
            root,
            application_path,
            qt_prefix,
            test_build_directory,
            timeout_seconds,
            parallel,
        )
        build_root_created = True
        configure_record = _invoke(
            manifest,
            "configure-linux-debug-startup-harness",
            "configure",
            build_commands["debugHarnessConfigure"],
            repository_root,
            _build_environment(root, qt_prefix),
            root / "logs" / "configure-debug-startup-harness",
            root,
            timeout_seconds,
        )
        _require_success(configure_record)
        build_record = _invoke(
            manifest,
            "build-linux-debug-startup-harness",
            "build",
            build_commands["debugHarnessBuild"],
            repository_root,
            _build_environment(root, qt_prefix),
            root / "logs" / "build-debug-startup-harness",
            root,
            timeout_seconds,
        )
        _require_success(build_record)
        built_test_executable = test_build_directory / "ClassMngrStartupPerformanceTests"
        if not built_test_executable.is_file() or not os.access(built_test_executable, os.X_OK):
            raise RunnerError(f"CMake did not produce the startup test harness: {built_test_executable}")
        test_executable = root / "tools" / "ClassMngrStartupPerformanceTests"
        test_executable.parent.mkdir(parents=True, exist_ok=False)
        shutil.copy2(built_test_executable, test_executable)
        manifest["build"] = {
            "status": "completed",
            "configuration": "Debug",
            "target": "ClassMngrStartupPerformanceTests",
            "testExecutableSha256": hashlib.sha256(test_executable.read_bytes()).hexdigest(),
            "testExecutableSizeBytes": test_executable.stat().st_size,
            "temporaryBuildDirectoryRetained": False,
        }
        _write_json(root / "run-manifest.json", manifest)

        smoke_ok = _execute_package_launch_smoke(
            manifest,
            root,
            repository_root,
            qt_prefix,
            application_path,
            timeout_seconds,
        )
        failed_routes = _execute_routes(
            manifest,
            root,
            repository_root,
            qt_prefix,
            application_path,
            test_executable,
            timeout_seconds,
        )
        manifest["status"] = "completed"
        manifest["recordedAtUtc"] = utc_now()
        manifest["routeExecution"] = {
            "status": "completed",
            "requestedRouteCount": len(ALL_ROUTE_IDS),
            "executedRouteIds": list(ALL_ROUTE_IDS),
            "failedRouteIds": failed_routes,
        }
        _write_json(root / "run-manifest.json", manifest)
        validation_ok = _validate_output(
            manifest,
            root,
            repository_root,
            qt_prefix,
            timeout_seconds,
        )
        return 0 if smoke_ok and not failed_routes and validation_ok else 1
    except Exception as error:
        manifest["status"] = "failed"
        manifest["recordedAtUtc"] = utc_now()
        manifest["failures"].append(
            {
                "code": "linux-runner-failed",
                "message": f"{type(error).__name__}: {error}",
                "executedRouteIds": list(manifest.get("routeExecution", {}).get("executedRouteIds", [])),
            }
        )
        _write_json(root / "run-manifest.json", manifest)
        print(f"ERROR: {error}", file=sys.stderr)
        return 1
    finally:
        if build_root_created and test_build_directory.is_dir() and not test_build_directory.is_symlink():
            shutil.rmtree(test_build_directory)


def _default_repository_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _default_qt_prefix() -> Path:
    value = os.environ.get("QT_LINUX_PREFIX")
    if value:
        return Path(value).expanduser().absolute()
    return Path.home() / "Qt" / "6.12.0" / "gcc_64"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence-root", type=Path, required=True, help="new evidence root, outside the repository")
    parser.add_argument("--package-root", type=Path, required=True, help="staged Release package root containing bin/ClassMngr")
    parser.add_argument("--repository-root", type=Path, default=_default_repository_root())
    parser.add_argument("--qt-prefix", type=Path, default=_default_qt_prefix())
    parser.add_argument("--timeout-seconds", type=int, default=DEFAULT_TIMEOUT_SECONDS)
    parser.add_argument("--parallel", type=int, default=DEFAULT_PARALLEL)
    parser.add_argument("--plan", action="store_true", help="print exact commands and all 24 route mappings without writing or executing")
    args = parser.parse_args(argv)
    if args.timeout_seconds < 1:
        parser.error("--timeout-seconds must be positive")
    if args.parallel < 1:
        parser.error("--parallel must be positive")
    repository_root = args.repository_root.expanduser().absolute()
    evidence_root = args.evidence_root.expanduser().absolute()
    package_root = args.package_root.expanduser().absolute()
    qt_prefix = args.qt_prefix.expanduser().absolute()
    try:
        _validate_evidence_root(evidence_root, repository_root, create=False)
        _validate_package_root(package_root)
        if args.plan:
            _preflight(repository_root, package_root, qt_prefix)
            plan = build_plan(
                repository_root,
                evidence_root,
                package_root,
                qt_prefix,
                args.timeout_seconds,
                args.parallel,
            )
            print(json.dumps(plan, indent=2, ensure_ascii=False))
            return 0
        return _run(
            evidence_root,
            repository_root,
            package_root,
            qt_prefix,
            args.timeout_seconds,
            args.parallel,
        )
    except (RunnerError, OSError, subprocess.SubprocessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
