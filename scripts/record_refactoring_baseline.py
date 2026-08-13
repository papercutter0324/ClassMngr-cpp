#!/usr/bin/env python3
"""Run a clean build/test cycle and write comparable refactoring metrics."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


def run(command: list[str], cwd: Path) -> tuple[int, float]:
    print(f"+ {' '.join(command)}", flush=True)
    started = time.perf_counter()
    completed = subprocess.run(command, cwd=cwd, check=False)
    return completed.returncode, time.perf_counter() - started


def capture(command: list[str], cwd: Path) -> str:
    completed = subprocess.run(
        command,
        cwd=cwd,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
    )
    return completed.stdout.strip() if completed.returncode == 0 else ""


def count_lines(path: Path) -> int:
    with path.open("r", encoding="utf-8") as source:
        return sum(1 for _ in source)


def source_fingerprint(repository: Path) -> str:
    listed = capture(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
        repository,
    )
    digest = hashlib.sha256()
    for relative_name in sorted(name for name in listed.split("\0") if name):
        path = repository / relative_name
        if not path.is_file():
            continue
        digest.update(relative_name.replace("\\", "/").encode("utf-8"))
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def find_application(build_dir: Path) -> Path | None:
    names = {"ClassMngr", "ClassMngr.exe"}
    candidates = [
        path
        for path in build_dir.rglob("ClassMngr*")
        if path.is_file() and path.name in names
    ]
    if not candidates:
        return None
    return max(candidates, key=lambda path: path.stat().st_mtime_ns)


def compilation_metrics(repository: Path, build_dir: Path) -> dict[str, Any]:
    database = build_dir / "compile_commands.json"
    if not database.exists():
        return {
            "compile_commands": None,
            "production_entries": None,
            "unique_production_sources": None,
            "recompiled_production_sources": None,
        }

    entries = json.loads(database.read_text(encoding="utf-8"))
    source_root = (repository / "src").resolve()
    production: list[str] = []
    for entry in entries:
        source = Path(entry["file"])
        if not source.is_absolute():
            source = Path(entry.get("directory", build_dir)) / source
        source = source.resolve()
        try:
            source.relative_to(source_root)
        except ValueError:
            continue
        if source.suffix.lower() in {".c", ".cc", ".cpp", ".cxx"}:
            production.append(os.path.normcase(str(source)))

    unique_sources = set(production)
    return {
        "compile_commands": str(database.relative_to(repository)),
        "production_entries": len(production),
        "unique_production_sources": len(unique_sources),
        "recompiled_production_sources": len(production) - len(unique_sources),
    }


def write_report(path: Path, report: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {path}", flush=True)


def test_summary(junit_path: Path) -> dict[str, Any] | None:
    if not junit_path.exists():
        return None
    root = ET.parse(junit_path).getroot()
    failed_tests = []
    for test_case in root.iter("testcase"):
        if (
            test_case.find("failure") is not None
            or test_case.find("error") is not None
        ):
            failed_tests.append(test_case.attrib.get("name", "<unnamed>"))

    return {
        "total": int(root.attrib.get("tests", 0)),
        "failures": int(root.attrib.get("failures", 0)),
        "errors": int(root.attrib.get("errors", 0)),
        "skipped": int(root.attrib.get("skipped", root.attrib.get("disabled", 0))),
        "failed_tests": failed_tests,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--configure-preset", required=True)
    parser.add_argument("--build-preset")
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--configuration", help="CTest configuration for multi-config generators")
    parser.add_argument(
        "--parallel",
        type=int,
        default=2,
        help="Maximum parallel build jobs (default: 2 for reproducible local baselines)",
    )
    parser.add_argument(
        "--test-timeout",
        type=int,
        default=120,
        help="Maximum seconds allowed for each CTest test (default: 120)",
    )
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    if args.parallel < 1:
        parser.error("--parallel must be at least 1")
    if args.test_timeout < 1:
        parser.error("--test-timeout must be at least 1")

    repository = Path(__file__).resolve().parents[1]
    build_dir = args.build_dir
    if not build_dir.is_absolute():
        build_dir = repository / build_dir
    output = args.output
    if not output.is_absolute():
        output = repository / output
    build_preset = args.build_preset or args.configure_preset
    commit_start = capture(["git", "rev-parse", "HEAD"], repository)
    dirty_start = bool(capture(["git", "status", "--porcelain"], repository))
    fingerprint_start = source_fingerprint(repository)

    configure_code, configure_seconds = run(
        ["cmake", "--fresh", "--preset", args.configure_preset], repository
    )
    build_code = -1
    build_seconds = 0.0
    test_code = -1
    test_seconds = 0.0
    junit_path = build_dir / "Testing" / "refactoring-baseline.junit.xml"
    junit_path.unlink(missing_ok=True)

    if configure_code == 0:
        build_code, build_seconds = run(
            [
                "cmake",
                "--build",
                "--preset",
                build_preset,
                "--clean-first",
                "--parallel",
                str(args.parallel),
            ],
            repository,
        )

    if build_code == 0:
        test_command = [
            "ctest",
            "--test-dir",
            str(build_dir),
            "--output-on-failure",
            "--output-junit",
            str(junit_path),
            "--timeout",
            str(args.test_timeout),
        ]
        if args.configuration:
            test_command.extend(["--build-config", args.configuration])
        test_code, test_seconds = run(test_command, repository)

    executable = find_application(build_dir) if build_code == 0 else None
    commit_end = capture(["git", "rev-parse", "HEAD"], repository)
    dirty_end = bool(capture(["git", "status", "--porcelain"], repository))
    fingerprint_end = source_fingerprint(repository)
    source_stable = (
        commit_start == commit_end and fingerprint_start == fingerprint_end
    )
    report = {
        "recorded_at_utc": datetime.now(timezone.utc).isoformat(),
        "git_commit": commit_end,
        "git_commit_start": commit_start,
        "git_commit_end": commit_end,
        "git_dirty_start": dirty_start,
        "git_dirty_end": dirty_end,
        "source_fingerprint_start": fingerprint_start,
        "source_fingerprint_end": fingerprint_end,
        "source_stable_during_run": source_stable,
        "platform": platform.platform(),
        "python": platform.python_version(),
        "cmake": capture(["cmake", "--version"], repository).splitlines()[0],
        "preset": args.configure_preset,
        "configuration": args.configuration,
        "parallel_build_jobs": args.parallel,
        "test_timeout_seconds": args.test_timeout,
        "results": {
            "configure_exit_code": configure_code,
            "build_exit_code": build_code,
            "test_exit_code": test_code,
            "tests": test_summary(junit_path),
        },
        "duration_seconds": {
            "configure": round(configure_seconds, 3),
            "clean_build": round(build_seconds, 3),
            "tests": round(test_seconds, 3),
        },
        "root_cmake_lines": count_lines(repository / "CMakeLists.txt"),
        "application": {
            "path": str(executable.relative_to(repository)) if executable else None,
            "size_bytes": executable.stat().st_size if executable else None,
        },
        "compilation": compilation_metrics(repository, build_dir),
    }
    write_report(output, report)

    return 0 if configure_code == build_code == test_code == 0 and source_stable else 1


if __name__ == "__main__":
    sys.exit(main())
