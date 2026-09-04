#!/usr/bin/env python3
"""Run and validate the Phase 2 portable-engine exit gate.

The runner intentionally uses only the Python standard library.  It can run a
single CI lane (``run``) or validate the reports downloaded from all lanes
(``validate``).
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import re
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable


ENGINE_LANES = (
    "windows-x64-winui-debug",
    "windows-x64-winui-release",
    "windows-x86-winui-debug",
    "windows-x86-winui-release",
)
QT_LANES = (
    "windows-qt-6.12-x64",
    "linux-qt-6.12-x64",
    "macos-qt-6.12-universal",
)
REQUIRED_LANES = ENGINE_LANES + QT_LANES
ENGINE_TEST_PATTERN = r"^ClassMngrEngine"
RETAINED_QT_TEST = "ClassMngrDatabasePortFixtureTests"
RETAINED_QT_GENERATOR = "ClassMngrDatabasePortFixtureGenerator"
DATABASE_ENGINE_TEST = "ClassMngrEngineDatabaseFixtureRoundTripTests"
REQUIRED_COVERAGE = (
    "invalid-input",
    "rollback",
    "migration",
    "busy/locked database",
    "partial failure",
)
REQUIRED_ARTIFACTS = (
    "configure_log",
    "build_log",
    "ctest_log",
    "ctest_junit",
    "inventory_log",
    "inventory_json",
)
SOURCE_UNTRACKED_ROOTS = {
    ".github",
    "cmake",
    "docs",
    "licenses",
    "plans",
    "resources",
    "scripts",
    "src",
    "tests",
}


def repository_root() -> Path:
    return Path(__file__).resolve().parents[1]


def capture(command: list[str], cwd: Path) -> str:
    completed = subprocess.run(
        command,
        cwd=cwd,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    return completed.stdout.strip() if completed.returncode == 0 else ""


def source_fingerprint(repository: Path) -> str:
    tracked = capture(["git", "ls-files", "-z"], repository)
    untracked = capture(
        ["git", "ls-files", "--others", "--exclude-standard", "-z"],
        repository,
    )
    tracked_names = {name for name in tracked.split("\0") if name}
    relevant_untracked = {
        name
        for name in untracked.split("\0")
        if name
        and not name.replace("\\", "/").endswith(".depends")
        and name.replace("\\", "/").split("/", 1)[0] in SOURCE_UNTRACKED_ROOTS
    }
    digest = hashlib.sha256()
    for relative_name in sorted(tracked_names | relevant_untracked):
        path = repository / relative_name
        if not path.is_file():
            continue
        digest.update(relative_name.replace("\\", "/").encode("utf-8"))
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def as_relative(path: Path, base: Path) -> str:
    try:
        return path.resolve().relative_to(base.resolve()).as_posix()
    except ValueError:
        return str(path)


def run_logged(command: list[str], cwd: Path, log_path: Path) -> tuple[int, float]:
    log_path.parent.mkdir(parents=True, exist_ok=True)
    print(f"+ {' '.join(command)}", flush=True)
    started = time.perf_counter()
    with log_path.open("w", encoding="utf-8", newline="") as log:
        log.write(f"$ {' '.join(command)}\n\n")
        log.flush()
        completed = subprocess.run(
            command,
            cwd=cwd,
            check=False,
            stdout=log,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        log.write(f"\n[exit code: {completed.returncode}]\n")
    return completed.returncode, time.perf_counter() - started


def configuration_args(configuration: str | None) -> list[str]:
    return ["-C", configuration] if configuration else []


def build_configuration_args(configuration: str | None) -> list[str]:
    return ["--config", configuration] if configuration else []


def resolve_executable(
    command: Iterable[str],
    test_name: str,
    build_dir: Path,
    executable_paths: dict[str, Path] | None = None,
    executable_name: str | None = None,
) -> Path | None:
    tokens = list(command)
    expected_names = {test_name.lower(), f"{test_name.lower()}.exe"}
    if executable_name:
        expected_names.update(
            {executable_name.lower(), f"{executable_name.lower()}.exe"}
        )
    candidates: list[Path] = []
    for token in tokens:
        if not token or token.startswith("-"):
            continue
        candidate = Path(token)
        if not candidate.is_absolute():
            candidate = build_dir / candidate
        if candidate.name.lower() in expected_names:
            candidates.append(candidate)
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
        for with_suffix in (candidate.with_suffix(".exe"), candidate.with_suffix("")):
            if with_suffix.is_file():
                return with_suffix.resolve()
    if executable_paths is not None:
        for expected_name in expected_names:
            if executable := executable_paths.get(expected_name):
                return executable
    return None


def executable_index(build_dir: Path) -> dict[str, Path]:
    return {
        candidate.name.lower(): candidate.resolve()
        for candidate in build_dir.rglob("*.exe")
        if candidate.is_file()
    }


def parse_json_inventory(raw: str, build_dir: Path) -> list[dict[str, Any]] | None:
    try:
        document = json.loads(raw)
    except json.JSONDecodeError:
        return None
    tests = document.get("tests") if isinstance(document, dict) else None
    if not isinstance(tests, list):
        return None
    inventory = []
    for test in tests:
        if not isinstance(test, dict) or not isinstance(test.get("name"), str):
            continue
        name = test["name"]
        command = test.get("command", [])
        if not isinstance(command, list):
            command = []
        executable = resolve_executable(
            command,
            name,
            build_dir,
            executable_name=(
                RETAINED_QT_GENERATOR if name == RETAINED_QT_TEST else None
            ),
        )
        inventory.append(
            {
                "name": name,
                "command": [str(item) for item in command],
                "executable": str(executable) if executable else None,
                "executable_present": executable is not None,
            }
        )
    return inventory


def parse_fallback_inventory(raw: str, build_dir: Path) -> list[dict[str, Any]]:
    names = []
    for match in re.finditer(r"Test #\d+:\s*(\S+)", raw):
        if match.group(1) not in names:
            names.append(match.group(1))
    executable_paths = executable_index(build_dir)
    return [
        {
            "name": name,
            "command": [],
            "executable": str(executable)
            if (
                executable := resolve_executable(
                    [],
                    name,
                    build_dir,
                    executable_paths,
                    executable_name=(
                        RETAINED_QT_GENERATOR if name == RETAINED_QT_TEST else None
                    ),
                )
            )
            else None,
            "executable_present": executable is not None,
        }
        for name in names
    ]


def engine_inventory(inventory: Iterable[dict[str, Any]]) -> list[dict[str, Any]]:
    return [item for item in inventory if item["name"].startswith("ClassMngrEngine")]


def collect_inventory(
    build_dir: Path,
    configuration: str | None,
    logs_dir: Path,
) -> tuple[int, str, list[dict[str, Any]], str]:
    json_log = logs_dir / "inventory-json.log"
    command = ["ctest", "--test-dir", str(build_dir), *configuration_args(configuration), "--show-only=json-v1"]
    code, _ = run_logged(command, repository_root(), json_log)
    raw = json_log.read_text(encoding="utf-8", errors="replace") if json_log.exists() else ""
    raw = raw.split("\n\n", 1)[1] if "\n\n" in raw else raw
    raw_without_footer = raw.split("\n[exit code:", 1)[0]
    inventory = parse_json_inventory(raw_without_footer, build_dir) if code == 0 else None
    source = "json-v1"
    if inventory is None:
        fallback_log = logs_dir / "inventory-fallback.log"
        fallback_command = ["ctest", "--test-dir", str(build_dir), *configuration_args(configuration), "-N", "-V"]
        fallback_code, _ = run_logged(fallback_command, repository_root(), fallback_log)
        fallback_raw = fallback_log.read_text(encoding="utf-8", errors="replace") if fallback_log.exists() else ""
        fallback_raw = fallback_raw.split("\n\n", 1)[1] if "\n\n" in fallback_raw else fallback_raw
        fallback_raw = fallback_raw.split("\n[exit code:", 1)[0]
        inventory = parse_fallback_inventory(fallback_raw, build_dir)
        source = "fallback"
        # A nonzero JSON probe is expected on older CTest versions; the
        # fallback result is authoritative whenever it is used.
        code = fallback_code
    normalized = {
        "format": "phase2-ctest-inventory-v1",
        "source": source,
        "tests": inventory,
    }
    (logs_dir / "inventory.json").write_text(json.dumps(normalized, indent=2) + "\n", encoding="utf-8")
    return code, source, inventory, str(logs_dir / "inventory.json")


def junit_summary(path: Path) -> dict[str, Any] | None:
    if not path.is_file() or path.stat().st_size == 0:
        return None
    try:
        root = ET.parse(path).getroot()
    except ET.ParseError:
        return None
    cases = list(root.iter("testcase"))
    failed = []
    for case in cases:
        if case.find("failure") is not None or case.find("error") is not None:
            failed.append(case.attrib.get("name", "<unnamed>"))
    def integer(name: str, default: int) -> int:
        try:
            return int(root.attrib.get(name, default))
        except (TypeError, ValueError):
            return default
    return {
        "total": integer("tests", len(cases)),
        "failures": integer("failures", len(failed)),
        "errors": integer("errors", 0),
        "skipped": integer("skipped", integer("disabled", 0)),
        "test_names": [case.attrib.get("name", "<unnamed>") for case in cases],
        "failed_tests": failed,
    }


def artifact_map(logs_dir: Path, report_path: Path) -> dict[str, str]:
    return {
        "configure_log": as_relative(logs_dir / "configure.log", report_path.parent),
        "build_log": as_relative(logs_dir / "build.log", report_path.parent),
        "ctest_log": as_relative(logs_dir / "ctest.log", report_path.parent),
        "ctest_junit": as_relative(logs_dir / "ctest.junit.xml", report_path.parent),
        "inventory_log": as_relative(logs_dir / "inventory-json.log", report_path.parent),
        "inventory_json": as_relative(logs_dir / "inventory.json", report_path.parent),
    }


def probe_qt_version(prefix: Path) -> tuple[str | None, str | None]:
    candidates = (
        prefix / "bin" / "qmake.exe",
        prefix / "bin" / "qmake6.exe",
        prefix / "bin" / "qmake",
        prefix / "bin" / "qmake6",
    )
    for executable in candidates:
        if not executable.is_file():
            continue
        completed = subprocess.run(
            [str(executable), "-query", "QT_VERSION"],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        if completed.returncode == 0:
            version = completed.stdout.strip()
            if re.fullmatch(r"\d+\.\d+\.\d+", version):
                return version, str(executable)
    return None, None


def qt_metadata(args: argparse.Namespace) -> tuple[dict[str, Any], list[str]]:
    prefix_env = args.qt_prefix_env
    prefix = os.environ.get(prefix_env, "") if prefix_env else ""
    metadata = {
        "required_version": args.qt_version,
        "version": None,
        "detected_version": None,
        "architecture": args.qt_architecture,
        "prefix_env": prefix_env,
        "prefix": prefix,
        "version_probe": None,
        "runtime_tested": False,
        "compile_only": False,
        "blocked": False,
    }
    failures = []
    if not args.qt_version:
        failures.append("Qt version metadata is missing")
    if args.qt_version != "6.12.0":
        failures.append(f"Qt version must be exactly 6.12.0, got {args.qt_version!r}")
    if not args.qt_architecture:
        failures.append("Qt architecture metadata is missing")
    if not prefix_env or not prefix:
        failures.append(f"{prefix_env or 'Qt prefix environment'} is not set")
    elif not Path(prefix).is_dir():
        failures.append(f"Qt prefix does not exist: {prefix}")
    else:
        detected_version, version_probe = probe_qt_version(Path(prefix))
        metadata["version"] = detected_version
        metadata["detected_version"] = detected_version
        metadata["version_probe"] = version_probe
        if detected_version is None:
            failures.append("Qt version could not be probed from the installed Qt prefix")
        elif detected_version != args.qt_version:
            failures.append(
                f"Detected Qt version must be exactly {args.qt_version}, got {detected_version!r}"
            )
    return metadata, failures


def write_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {path}", flush=True)


def run_lane(args: argparse.Namespace) -> int:
    repository = repository_root()
    report_path = args.output if args.output.is_absolute() else repository / args.output
    logs_dir = args.logs_dir if args.logs_dir.is_absolute() else repository / args.logs_dir
    build_dir = args.build_dir if args.build_dir.is_absolute() else repository / args.build_dir
    logs_dir.mkdir(parents=True, exist_ok=True)
    commit_start = capture(["git", "rev-parse", "HEAD"], repository)
    fingerprint_start = source_fingerprint(repository)
    dirty_start = bool(capture(["git", "status", "--porcelain"], repository))
    failures: list[str] = []
    qt_info: dict[str, Any] | None = None
    if args.lane_type == "retained-qt":
        qt_info, qt_failures = qt_metadata(args)
        failures.extend(qt_failures)

    configure_command = ["cmake", "--fresh", "--preset", args.configure_preset]
    configure_code, configure_seconds = run_logged(configure_command, repository, logs_dir / "configure.log")
    inventory_code = -1
    inventory_source = None
    inventory: list[dict[str, Any]] = []
    inventory_path = None
    if configure_code == 0:
        inventory_code, inventory_source, inventory, inventory_path = collect_inventory(build_dir, args.configuration, logs_dir)

    build_parallel = "1" if args.lane_type == "retained-qt" else "2"
    build_command = [
        "cmake",
        "--build",
        str(args.build_dir),
        *build_configuration_args(args.configuration),
        "--clean-first",
        "--parallel",
        build_parallel,
    ]
    if args.lane_type == "retained-qt":
        build_command.extend(["--target", RETAINED_QT_GENERATOR])
    if os.name == "nt":
        build_command.extend(["--", "/p:TrackFileAccess=false"])
    build_code = -1
    build_seconds = 0.0
    if configure_code == 0:
        build_code, build_seconds = run_logged(build_command, repository, logs_dir / "build.log")
        if inventory_code == 0:
            inventory_code, inventory_source, inventory, inventory_path = collect_inventory(build_dir, args.configuration, logs_dir)

    test_pattern = ENGINE_TEST_PATTERN if args.lane_type == "engine" else rf"^{RETAINED_QT_TEST}$"
    junit_path = logs_dir / "ctest.junit.xml"
    test_command = [
        "ctest",
        "--test-dir",
        str(args.build_dir),
        *configuration_args(args.configuration),
        "-R",
        test_pattern,
        "--output-on-failure",
        "--output-junit",
        str(junit_path),
        "--timeout",
        str(args.test_timeout),
    ]
    test_code = -1
    test_seconds = 0.0
    if build_code == 0:
        test_code, test_seconds = run_logged(test_command, repository, logs_dir / "ctest.log")
    summary = junit_summary(junit_path)
    registered_names = {item["name"] for item in inventory}
    relevant = engine_inventory(inventory) if args.lane_type == "engine" else []
    missing_executables = [item["name"] for item in relevant if not item["executable_present"]]
    required_test = DATABASE_ENGINE_TEST if args.lane_type == "engine" else RETAINED_QT_TEST
    executed_names = set(summary["test_names"]) if summary else set()
    if configure_code != 0:
        failures.append(f"configure failed with exit code {configure_code}")
    if build_code != 0:
        failures.append(f"build failed with exit code {build_code}")
    if inventory_code != 0:
        failures.append(f"CTest inventory failed with exit code {inventory_code}")
    if args.lane_type == "engine":
        if not relevant:
            failures.append("no registered ClassMngrEngine tests were found")
        if missing_executables:
            failures.append("missing registered engine executables: " + ", ".join(sorted(missing_executables)))
    missing_executables = sorted(missing_executables)
    if required_test not in registered_names:
        failures.append(f"required test is not registered: {required_test}")
    required_entry = next((item for item in inventory if item["name"] == required_test), None)
    if required_entry is None or not required_entry["executable_present"]:
        failures.append(f"required test executable is missing: {required_test}")
    if required_test not in executed_names:
        failures.append(f"required test did not run: {required_test}")
    if test_code != 0:
        failures.append(f"CTest failed with exit code {test_code}")
    if summary is None:
        failures.append("CTest JUnit evidence is missing or invalid")
    elif summary["failures"] or summary["errors"]:
        failures.append("CTest reported failed or errored tests")
    if args.lane_type == "engine":
        coverage = {
            category: {"status": "covered", "evidence_tests": [DATABASE_ENGINE_TEST]}
            for category in REQUIRED_COVERAGE
        }
    else:
        coverage = {}
        if qt_info is not None:
            qt_info["runtime_tested"] = test_code == 0 and required_test in executed_names
            if not qt_info["runtime_tested"]:
                qt_info["compile_only"] = build_code == 0
    commit_end = capture(["git", "rev-parse", "HEAD"], repository)
    fingerprint_end = source_fingerprint(repository)
    dirty_end = bool(capture(["git", "status", "--porcelain"], repository))
    source_stable = commit_start == commit_end and fingerprint_start == fingerprint_end
    if not source_stable:
        failures.append("source changed during the lane")
    if qt_info is not None:
        failures.extend(
            message
            for message in (
                []
                if qt_info["runtime_tested"]
                else ["retained Qt fixture was not runtime-tested"]
            )
        )
    report = {
        "format": "phase2-exit-gate-report-v1",
        "lane_id": args.lane_id,
        "lane_type": args.lane_type,
        "status": "PASS" if not failures else "FAIL",
        "recorded_at_utc": datetime.now(timezone.utc).isoformat(),
        "platform": platform.platform(),
        "git_commit": commit_end,
        "git_commit_start": commit_start,
        "git_commit_end": commit_end,
        "git_dirty_start": dirty_start,
        "git_dirty_end": dirty_end,
        "source_fingerprint_start": fingerprint_start,
        "source_fingerprint_end": fingerprint_end,
        "source_stable_during_run": source_stable,
        "preset": args.configure_preset,
        "build_dir": str(args.build_dir),
        "configuration": args.configuration,
        "commands": {
            "configure": configure_command,
            "build": build_command,
            "inventory": ["ctest", "--test-dir", str(args.build_dir), *configuration_args(args.configuration), "--show-only=json-v1"],
            "test": test_command,
        },
        "results": {
            "configure_exit_code": configure_code,
            "build_exit_code": build_code,
            "inventory_exit_code": inventory_code,
            "test_exit_code": test_code,
            "test_pattern": test_pattern,
            "inventory_source": inventory_source,
            "registered_tests": [item["name"] for item in inventory],
            "registered_engine_tests": [item["name"] for item in relevant],
            "missing_engine_executables": sorted(missing_executables),
            "required_test": required_test,
            "tests": summary,
        },
        "coverage": coverage,
        "qt": qt_info,
        "artifacts": artifact_map(logs_dir, report_path),
        "failures": failures,
        "duration_seconds": {
            "configure": round(configure_seconds, 3),
            "build": round(build_seconds, 3),
            "tests": round(test_seconds, 3),
        },
    }
    write_json(report_path, report)
    return 0 if not failures else 1


def required_artifacts(report: dict[str, Any]) -> tuple[dict[str, str], list[str]]:
    artifacts = report.get("artifacts")
    if not isinstance(artifacts, dict):
        return {}, ["artifacts section is missing"]

    failures = []
    paths: dict[str, str] = {}
    for key in REQUIRED_ARTIFACTS:
        value = artifacts.get(key)
        if not isinstance(value, str) or not value:
            failures.append(f"required artifact path is missing: {key}")
            continue
        paths[key] = value
    return paths, failures


def validate_report(report: dict[str, Any], report_path: Path) -> list[str]:
    failures: list[str] = []
    if report.get("format") != "phase2-exit-gate-report-v1":
        failures.append("report format is not phase2-exit-gate-report-v1")
    lane_id = report.get("lane_id")
    lane_type = report.get("lane_type")
    expected_type = "engine" if lane_id in ENGINE_LANES else "retained-qt"
    if lane_type != expected_type:
        failures.append(f"lane_type is {lane_type!r}, expected {expected_type!r}")
    if report.get("status") != "PASS":
        failures.append(f"lane status is {report.get('status')!r}")
    if not isinstance(report.get("git_commit"), str) or not report.get("git_commit"):
        failures.append("git commit identity is missing")
    if report.get("source_stable_during_run") is not True:
        failures.append("source was not stable during the lane")
    artifact_paths, artifact_failures = required_artifacts(report)
    failures.extend(artifact_failures)
    for key, path in artifact_paths.items():
        artifact = (report_path.parent / path).resolve()
        try:
            artifact.relative_to(report_path.parent.resolve())
        except ValueError:
            failures.append(f"artifact path escapes the lane directory: {key}")
            continue
        if not artifact.is_file() or artifact.stat().st_size == 0:
            failures.append(f"missing required artifact: {key}: {path}")
    results = report.get("results")
    if not isinstance(results, dict):
        failures.append("results section is missing")
        return failures
    for key in ("configure_exit_code", "build_exit_code", "inventory_exit_code", "test_exit_code"):
        if results.get(key) != 0:
            failures.append(f"{key} is {results.get(key)!r}")
    tests = results.get("tests")
    required_test = DATABASE_ENGINE_TEST if lane_type == "engine" else RETAINED_QT_TEST
    if required_test not in results.get("registered_tests", []):
        failures.append(f"required test is not in the registered inventory: {required_test}")
    if not isinstance(tests, dict) or required_test not in tests.get("test_names", []):
        failures.append(f"required test evidence is missing: {required_test}")
    if not isinstance(tests, dict) or tests.get("failures", 0) or tests.get("errors", 0):
        failures.append("test summary contains failures/errors")
    if isinstance(tests, dict) and tests.get("skipped", 0):
        failures.append("test summary contains skipped tests")
    if lane_type == "engine":
        if results.get("test_pattern") != ENGINE_TEST_PATTERN:
            failures.append("engine test pattern is not the complete ClassMngrEngine selection")
        if not results.get("registered_engine_tests"):
            failures.append("registered engine test inventory is empty")
        if results.get("missing_engine_executables"):
            failures.append("registered engine executables are missing")
        if DATABASE_ENGINE_TEST not in results.get("registered_engine_tests", []):
            failures.append("database fixture engine test is not registered")
        coverage = report.get("coverage")
        for category in REQUIRED_COVERAGE:
            entry = coverage.get(category) if isinstance(coverage, dict) else None
            if not isinstance(entry, dict) or entry.get("status") != "covered" or DATABASE_ENGINE_TEST not in entry.get("evidence_tests", []):
                failures.append(f"required coverage evidence is missing: {category}")
    else:
        qt = report.get("qt")
        if not isinstance(qt, dict):
            failures.append("Qt metadata is missing")
        else:
            for key in (
                "required_version",
                "version",
                "detected_version",
                "architecture",
                "prefix_env",
                "prefix",
                "version_probe",
            ):
                if not qt.get(key):
                    failures.append(f"Qt metadata is missing: {key}")
            if (
                qt.get("required_version") != "6.12.0"
                or qt.get("version") != "6.12.0"
                or qt.get("detected_version") != "6.12.0"
            ):
                failures.append("Qt metadata does not identify exact version 6.12.0")
            expected_qt = {
                "windows-qt-6.12-x64": ("QT_MSVC_X64_PREFIX", "x64"),
                "linux-qt-6.12-x64": ("QT_LINUX_PREFIX", "x64"),
                "macos-qt-6.12-universal": ("QT_MACOS_PREFIX", "universal"),
            }.get(str(lane_id))
            if expected_qt and (qt.get("prefix_env"), qt.get("architecture")) != expected_qt:
                failures.append("Qt prefix environment or architecture does not match the declared lane")
            if qt.get("compile_only") or qt.get("blocked") or not qt.get("runtime_tested"):
                failures.append("Qt lane is compile-only, blocked, or not runtime-tested")
            if lane_id == "windows-qt-6.12-x64" and qt.get("architecture") != "x64":
                failures.append("Windows retained Qt lane is not x64")
    return failures


def validate_aggregate(reports_dir: Path, output: Path) -> int:
    # Inventory JSON is itself a required lane artifact, not another lane
    # report.  Lane reports have stable, lane-specific filenames.
    report_paths = (
        sorted(path for path in reports_dir.rglob("*.json") if path.name != "inventory.json")
        if reports_dir.exists()
        else []
    )
    reports: list[tuple[Path, dict[str, Any]]] = []
    failures: list[str] = []
    for path in report_paths:
        try:
            document = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            failures.append(f"invalid report {path}: {error}")
            continue
        if not isinstance(document, dict) or not document.get("lane_id"):
            failures.append(f"report has no lane_id: {path}")
            continue
        reports.append((path, document))
    by_lane: dict[str, list[tuple[Path, dict[str, Any]]]] = {}
    for path, report in reports:
        by_lane.setdefault(str(report["lane_id"]), []).append((path, report))
    found = sorted(by_lane)
    if found != sorted(REQUIRED_LANES):
        failures.append(f"lane set mismatch; expected {list(REQUIRED_LANES)}, found {found}")
    for lane_id in REQUIRED_LANES:
        entries = by_lane.get(lane_id, [])
        if not entries:
            failures.append(f"missing lane report: {lane_id}")
            continue
        if len(entries) != 1:
            failures.append(f"duplicate lane reports: {lane_id}")
            continue
        path, report = entries[0]
        failures.extend(f"{lane_id}: {failure}" for failure in validate_report(report, path))
    commits = {
        str(report.get("git_commit"))
        for _, report in reports
        if report.get("git_commit")
    }
    if len(commits) != 1:
        failures.append(f"lane reports do not share one commit: {sorted(commits)}")
    aggregate = {
        "format": "phase2-exit-gate-aggregate-v1",
        "status": "PASS" if not failures else "FAIL",
        "validated_at_utc": datetime.now(timezone.utc).isoformat(),
        "required_lanes": list(REQUIRED_LANES),
        "found_lanes": found,
        "lane_reports": {lane: str(entries[0][0]) for lane, entries in by_lane.items() if entries},
        "failures": failures,
    }
    write_json(output, aggregate)
    if failures:
        print("Phase 2 exit gate failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
    return 0 if not failures else 1


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(description=__doc__)
    commands = root.add_subparsers(dest="command", required=True)
    run = commands.add_parser("run", help="run one CI lane and write its report")
    run.add_argument("--lane-id", required=True, choices=REQUIRED_LANES)
    run.add_argument("--lane-type", required=True, choices=("engine", "retained-qt"))
    run.add_argument("--configure-preset", required=True)
    run.add_argument("--build-dir", required=True, type=Path)
    run.add_argument("--configuration")
    run.add_argument("--test-timeout", type=int, default=120)
    run.add_argument("--output", required=True, type=Path)
    run.add_argument("--logs-dir", required=True, type=Path)
    run.add_argument("--qt-version")
    run.add_argument("--qt-architecture")
    run.add_argument("--qt-prefix-env")
    validate = commands.add_parser("validate", help="validate all downloaded lane reports")
    validate.add_argument("--reports-dir", required=True, type=Path)
    validate.add_argument("--output", required=True, type=Path)
    return root


def main(argv: list[str] | None = None) -> int:
    args = parser().parse_args(argv)
    if args.command == "run":
        if args.test_timeout < 1:
            parser().error("--test-timeout must be at least 1")
        return run_lane(args)
    return validate_aggregate(args.reports_dir, args.output)


if __name__ == "__main__":
    sys.exit(main())
