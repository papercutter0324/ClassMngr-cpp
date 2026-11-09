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


class GitInspectionError(RuntimeError):
    """Raised when Git cannot provide trustworthy provenance information."""


def git_capture(arguments: list[str], cwd: Path) -> str:
    command = ["git", *arguments]
    try:
        completed = subprocess.run(
            command,
            cwd=cwd,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
    except OSError as error:
        raise GitInspectionError(f"could not run {' '.join(command)}: {error}") from error

    if completed.returncode != 0:
        detail = completed.stderr.strip()
        suffix = f": {detail}" if detail else ""
        raise GitInspectionError(
            f"{' '.join(command)} exited with code {completed.returncode}{suffix}"
        )
    return completed.stdout


def count_lines(path: Path) -> int:
    with path.open("r", encoding="utf-8") as source:
        return sum(1 for _ in source)


def source_fingerprint(repository: Path) -> str:
    listed = git_capture(
        ["ls-files", "--cached", "--others", "--exclude-standard", "-z"],
        repository,
    )
    digest = hashlib.sha256()
    for relative_name in sorted(name for name in listed.split("\0") if name):
        path = repository / relative_name
        if not path.is_file():
            continue
        digest.update(relative_name.replace("\\", "/").encode("utf-8"))
        digest.update(b"\0")
        try:
            digest.update(path.read_bytes())
        except OSError as error:
            raise GitInspectionError(
                f"could not fingerprint {relative_name!r}: {error}"
            ) from error
        digest.update(b"\0")
    return digest.hexdigest()


def resolve_expected_commit(repository: Path, expected_commit: str) -> tuple[str, str]:
    resolved_commit = git_capture(
        ["rev-parse", "--verify", "--end-of-options", f"{expected_commit}^{{commit}}"],
        repository,
    ).strip()
    if not resolved_commit:
        raise GitInspectionError("Git returned an empty expected commit identity")
    expected_tree = git_capture(
        ["rev-parse", "--verify", "--end-of-options", f"{resolved_commit}^{{tree}}"],
        repository,
    ).strip()
    if not expected_tree:
        raise GitInspectionError("Git returned an empty expected tree identity")
    return resolved_commit, expected_tree


def inspect_git_state(repository: Path) -> dict[str, Any]:
    head = git_capture(["rev-parse", "--verify", "HEAD"], repository).strip()
    tree = git_capture(["rev-parse", "--verify", "HEAD^{tree}"], repository).strip()
    if not head or not tree:
        raise GitInspectionError("Git returned an empty HEAD or tree identity")
    status = git_capture(
        ["status", "--porcelain=v1", "--untracked-files=all"], repository
    ).rstrip("\r\n")
    status_entries = status.splitlines() if status else []
    dirty_paths = [entry[3:] for entry in status_entries if len(entry) >= 4]
    fingerprint = source_fingerprint(repository)
    return {
        "head": head,
        "tree": tree,
        "status_entries": status_entries,
        "dirty_paths": dirty_paths,
        "fingerprint": fingerprint,
    }


def evaluate_provenance(
    start: dict[str, Any] | None,
    end: dict[str, Any] | None,
    expected_commit: str | None,
    expected_tree: str | None,
) -> tuple[bool, list[str]]:
    errors: list[str] = []
    if start is None or end is None:
        return False, ["Git state could not be inspected at both run boundaries"]

    if expected_commit is not None:
        if start["head"] != expected_commit:
            errors.append("HEAD at start does not match the expected commit")
        if end["head"] != expected_commit:
            errors.append("HEAD at end does not match the expected commit")
    if expected_tree is not None:
        if start["tree"] != expected_tree:
            errors.append("tree at start does not match the expected commit tree")
        if end["tree"] != expected_tree:
            errors.append("tree at end does not match the expected commit tree")
    if start["head"] != end["head"]:
        errors.append("HEAD changed during the run")
    if start["tree"] != end["tree"]:
        errors.append("HEAD tree changed during the run")
    if start["dirty_paths"]:
        errors.append("working tree was dirty at start")
    if end["dirty_paths"]:
        errors.append("working tree was dirty at end")
    if start["fingerprint"] != end["fingerprint"]:
        errors.append("tracked and nonignored untracked source changed during the run")
    return not errors, errors


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


def find_executable(build_dir: Path, stem: str) -> Path | None:
    names = {stem, f"{stem}.exe"}
    candidates = [
        path
        for path in build_dir.rglob(f"{stem}*")
        if path.is_file() and path.name in names
    ]
    if not candidates:
        return None
    return max(candidates, key=lambda path: path.stat().st_mtime_ns)


def resource_pack_metrics(build_dir: Path) -> dict[str, Any]:
    entries = []
    pack_directory = build_dir / "resource-packs"
    if pack_directory.is_dir():
        for path in sorted(pack_directory.glob("*.rcc")):
            if path.is_file():
                entries.append(
                    {
                        "id": path.stem,
                        "path": str(path.relative_to(build_dir)),
                        "size_bytes": path.stat().st_size,
                    }
                )
    return {
        "packs": entries,
        "total_size_bytes": sum(entry["size_bytes"] for entry in entries),
    }


def qt_module_metrics(build_dir: Path) -> dict[str, Any] | None:
    report_path = build_dir / "reports" / "qt-module-links.json"
    if not report_path.is_file():
        return None
    report = json.loads(report_path.read_text(encoding="utf-8"))
    return report.get("targets") if isinstance(report, dict) else None


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


def publish_build_exit_code(build_code: int) -> None:
    output_file = os.environ.get("GITHUB_OUTPUT")
    if not output_file:
        return
    with Path(output_file).open("a", encoding="utf-8", newline="\n") as output:
        output.write(f"build_exit_code={build_code}\n")


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
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--configuration", help="CTest configuration for multi-config generators")
    # Let the Windows generator build more independent test targets together;
    # CMakeLists.txt caps each MSVC Debug target at two compiler processes.
    default_parallel = 4 if sys.platform == "win32" else 2
    parser.add_argument(
        "--parallel",
        type=int,
        default=default_parallel,
        help=f"Maximum parallel build jobs (default: {default_parallel})",
    )
    parser.add_argument(
        "--test-timeout",
        type=int,
        default=120,
        help="Maximum seconds allowed for each CTest test (default: 120)",
    )
    parser.add_argument(
        "--expected-commit",
        default=os.environ.get("GITHUB_SHA"),
        help="Workflow commit that the checked-out repository must match",
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
    expected_commit_input = args.expected_commit.strip() if args.expected_commit else None
    expected_commit = None
    expected_tree = None
    start_state = None
    end_state = None
    inspection_errors: list[str] = []

    try:
        start_state = inspect_git_state(repository)
        if expected_commit_input:
            expected_commit, expected_tree = resolve_expected_commit(
                repository, expected_commit_input
            )
    except GitInspectionError as error:
        inspection_errors.append(f"start: {error}")

    configure_code = -1
    configure_seconds = 0.0
    build_code = -1
    build_seconds = 0.0
    test_code = -1
    test_seconds = 0.0
    junit_path = build_dir / "Testing" / "refactoring-baseline.junit.xml"
    if start_state is not None and not inspection_errors:
        junit_path.unlink(missing_ok=True)
        configure_code, configure_seconds = run(
            [
                "cmake",
                "--fresh",
                "--preset",
                args.configure_preset,
                "-B",
                str(build_dir),
            ],
            repository,
        )

        if configure_code == 0:
            build_command = [
                "cmake",
                "--build",
                str(build_dir),
                "--clean-first",
                "--parallel",
                str(args.parallel),
            ]
            if args.configuration:
                build_command.extend(["--config", args.configuration])
            build_code, build_seconds = run(build_command, repository)

    publish_build_exit_code(build_code)

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

    try:
        end_state = inspect_git_state(repository)
    except GitInspectionError as error:
        inspection_errors.append(f"end: {error}")

    provenance_valid, provenance_errors = evaluate_provenance(
        start_state, end_state, expected_commit, expected_tree
    )
    if inspection_errors:
        provenance_valid = False
        provenance_errors = inspection_errors + provenance_errors

    source_stable = bool(
        start_state is not None
        and end_state is not None
        and start_state["head"] == end_state["head"]
        and start_state["tree"] == end_state["tree"]
        and start_state["fingerprint"] == end_state["fingerprint"]
    )
    executable = find_application(build_dir) if build_code == 0 else None
    next_executable = find_executable(build_dir, "ClassMngrNext") if build_code == 0 else None
    cmake_version_output = capture(["cmake", "--version"], repository)
    cmake_version = cmake_version_output.splitlines()[0] if cmake_version_output else None

    def state_value(state: dict[str, Any] | None, key: str) -> Any:
        return state[key] if state is not None else None

    def state_report(state: dict[str, Any] | None) -> dict[str, Any] | None:
        if state is None:
            return None
        return {
            "head": state["head"],
            "tree": state["tree"],
            "clean": not state["dirty_paths"],
            "dirty_paths": state["dirty_paths"],
            "status_entries": state["status_entries"],
            "source_fingerprint": state["fingerprint"],
        }

    report = {
        "recorded_at_utc": datetime.now(timezone.utc).isoformat(),
        "git_commit": state_value(end_state, "head"),
        "git_commit_start": state_value(start_state, "head"),
        "git_commit_end": state_value(end_state, "head"),
        "git_tree_start": state_value(start_state, "tree"),
        "git_tree_end": state_value(end_state, "tree"),
        "git_dirty_start": (
            bool(start_state["dirty_paths"]) if start_state is not None else None
        ),
        "git_dirty_end": bool(end_state["dirty_paths"]) if end_state is not None else None,
        "git_dirty_paths_start": state_value(start_state, "dirty_paths"),
        "git_dirty_paths_end": state_value(end_state, "dirty_paths"),
        "git_status_entries_start": state_value(start_state, "status_entries"),
        "git_status_entries_end": state_value(end_state, "status_entries"),
        "source_fingerprint_start": state_value(start_state, "fingerprint"),
        "source_fingerprint_end": state_value(end_state, "fingerprint"),
        "source_stable_during_run": source_stable,
        "provenance": {
            "expected_commit_input": expected_commit_input,
            "expected_commit": expected_commit,
            "expected_tree": expected_tree,
            "start": state_report(start_state),
            "end": state_report(end_state),
            "source_stable_during_run": source_stable,
            "valid": provenance_valid,
            "errors": provenance_errors,
            "git_inspection_error": "; ".join(inspection_errors) or None,
        },
        "platform": platform.platform(),
        "python": platform.python_version(),
        "cmake": cmake_version,
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
        "next_executable": {
            "path": str(next_executable.relative_to(repository))
            if next_executable
            else None,
            "size_bytes": next_executable.stat().st_size
            if next_executable
            else None,
        },
        "resource_packs": resource_pack_metrics(build_dir),
        "linked_qt_modules": qt_module_metrics(build_dir),
        "compilation": compilation_metrics(repository, build_dir),
    }
    write_report(output, report)

    return (
        0
        if configure_code == build_code == test_code == 0 and provenance_valid
        else 1
    )


if __name__ == "__main__":
    sys.exit(main())
