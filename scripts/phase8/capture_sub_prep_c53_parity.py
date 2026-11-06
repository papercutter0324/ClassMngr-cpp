#!/usr/bin/env python3
"""Capture matched-SQL Legacy/Next Sub Prep output evidence on one host."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys


REPOSITORY = Path(__file__).resolve().parents[2]
REFERENCE_DATE = "2026-09-14"
PACKAGE_DIRECTORY = "Teacher 01 (14 - 18 Sep 2026)"
AUDITOR = Path(
    "docs/qt-rewrite/visual-baseline/next/phase-7/sub-prep-c53/"
    "writer-probe/check_used_codes.py"
)


def find_executable(build_dir: Path, stem: str) -> Path:
    names = {stem, f"{stem}.exe"}
    matches = [
        path for path in build_dir.rglob(f"{stem}*")
        if path.is_file() and path.name in names
    ]
    if not matches:
        raise FileNotFoundError(f"Could not find {stem} under {build_dir}")
    return max(matches, key=lambda path: path.stat().st_mtime_ns)


def run(command: list[str], env: dict[str, str]) -> None:
    print(f"+ {shlex.join(command)}", flush=True)
    subprocess.run(command, cwd=REPOSITORY, env=env, check=True)


def read_json(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"Expected a JSON object in {path}")
    return value


def qt_environment() -> dict[str, str]:
    env = os.environ.copy()
    env["QT_QPA_PLATFORM"] = "offscreen"
    env["LC_ALL"] = "en_US.UTF-8"
    env["LANG"] = "en_US.UTF-8"

    if sys.platform == "win32":
        prefix_value = env.get("QT_MSVC_X64_PREFIX", "")
    elif sys.platform == "darwin":
        prefix_value = env.get("QT_MACOS_PREFIX", "")
    else:
        prefix_value = env.get("QT_LINUX_PREFIX", "")

    if not prefix_value:
        variable = {
            "win32": "QT_MSVC_X64_PREFIX",
            "darwin": "QT_MACOS_PREFIX",
        }.get(sys.platform, "QT_LINUX_PREFIX")
        raise RuntimeError(f"{variable} is required for the C53 capture")

    prefix = Path(prefix_value).resolve()
    plugins = prefix / "plugins"
    platform_plugins = plugins / "platforms"
    if not prefix.is_dir() or not platform_plugins.is_dir():
        raise FileNotFoundError(f"Qt prefix or platform plugins are missing: {prefix}")
    env["QT_PLUGIN_PATH"] = str(plugins)
    env["QT_QPA_PLATFORM_PLUGIN_PATH"] = str(platform_plugins)
    env["PATH"] = str(prefix / "bin") + os.pathsep + env.get("PATH", "")
    if sys.platform == "darwin":
        env["DYLD_LIBRARY_PATH"] = str(prefix / "lib") + os.pathsep + env.get("DYLD_LIBRARY_PATH", "")
    elif sys.platform != "win32":
        env["LD_LIBRARY_PATH"] = str(prefix / "lib") + os.pathsep + env.get("LD_LIBRARY_PATH", "")

    return env


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--report", required=True)
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()

    build_dir = args.build_dir
    if not build_dir.is_absolute():
        build_dir = REPOSITORY / build_dir
    build_dir = build_dir.resolve()
    output_root = args.output_dir or (
        REPOSITORY / "artifacts" / "sub-prep-output" / args.report
    )
    if not output_root.is_absolute():
        output_root = REPOSITORY / output_root
    output_root = output_root.resolve()
    if output_root.exists() and any(output_root.iterdir()):
        raise FileExistsError(
            f"C53 evidence directory must be new or empty: {output_root}"
        )
    output_root.mkdir(parents=True, exist_ok=True)

    legacy_capture = output_root / "legacy-capture"
    next_capture = output_root / "next-capture"
    next_capture.mkdir()
    env = qt_environment()
    app_path = find_executable(build_dir, "ClassMngr")
    startup_tests = find_executable(build_dir, "ClassMngrStartupPerformanceTests")
    next_capture_exe = find_executable(
        build_dir, "ClassMngrNextSubPrepSqlParityCapture"
    )

    legacy_env = env.copy()
    legacy_env["CLASSMNGR_C53_SQL_GATE_REFERENCE_DIR"] = str(legacy_capture)
    legacy_env["CLASSMNGR_STARTUP_SUB_PREP_OUTPUT_REFERENCE_DATE"] = REFERENCE_DATE
    legacy_env["CLASSMNGR_TEST_APP_PATH"] = str(app_path)
    run([str(startup_tests), "capturesMatchedSqlSubPrepForC53"], legacy_env)

    legacy_manifest_path = legacy_capture / "legacy-capture.json"
    legacy_manifest = read_json(legacy_manifest_path)
    database_hash = legacy_manifest["preparedDatabase"][
        "sha256BeforeLegacyGeneration"
    ]
    if legacy_manifest["preparedDatabase"]["retainedSnapshotSha256"] != database_hash:
        raise ValueError("The retained C53 SQL snapshot differs from the prepared input")

    database = legacy_capture / "generated-large-sub-prep-output.tps"
    after_legacy_database = legacy_capture / legacy_manifest["preparedDatabase"][
        "workspaceAfterGenerationPath"
    ]
    oracle = output_root / "sql-oracle.json"
    run(
        [
            sys.executable,
            str(REPOSITORY / "scripts/phase7/c53_sub_prep_sql_oracle.py"),
            "--database", str(database),
            "--after-legacy-database", str(after_legacy_database),
            "--expected-database-sha256", database_hash,
            "--output", str(oracle),
        ],
        env,
    )

    legacy_package = legacy_capture / "output-target" / PACKAGE_DIRECTORY
    legacy_roster = legacy_package / "Rosters - By Day.pdf"
    run(
        [
            str(next_capture_exe), "--matched-sql", str(database),
            str(legacy_roster), str(legacy_manifest_path), str(oracle),
            str(next_capture),
        ],
        env,
    )

    run(
        [
            sys.executable, str(REPOSITORY / AUDITOR),
            "--output", str(output_root / "legacy-used-code-audit.json"),
            str(legacy_package / "Sub Prep.pdf"), str(legacy_roster),
        ],
        env,
    )
    next_package = next_capture / "next-output" / PACKAGE_DIRECTORY
    run(
        [
            sys.executable, str(REPOSITORY / AUDITOR), "--require-valid",
            "--output", str(output_root / "next-used-code-audit.json"),
            str(next_package / "Rosters - By Day.pdf"),
        ],
        env,
    )

    comparison = read_json(next_capture / "pdf-comparison.json")
    documents = comparison.get("documents", [])
    expected_documents = {"Sub Prep.pdf": 12, "Rosters - By Day.pdf": 5}
    actual_documents = {
        document.get("name"): document.get("legacyPageCount")
        for document in documents
    }
    if comparison.get("result") != "pass" or actual_documents != expected_documents:
        raise ValueError(
            "C53 output capture did not pass the 12 Sub Prep/5 roster page gate"
        )

    summary = {
        "schema": "classmngr-phase8-sub-prep-c53-platform-capture-v1",
        "report": args.report,
        "host": sys.platform,
        "buildDirectory": build_dir.relative_to(REPOSITORY).as_posix(),
        "nextCapture": "next-capture/pdf-comparison.json",
        "legacyCapture": "legacy-capture/legacy-capture.json",
        "sqlOracle": "sql-oracle.json",
        "comparisonResult": comparison["result"],
        "documentPageCounts": actual_documents,
        "changedPixelTotal": comparison["changedPixelTotal"],
        "allPagesPixelIdentical": comparison["allPagesPixelIdentical"],
        "sameFontResourceProvenance": comparison["sameFontResourceProvenance"],
    }
    (output_root / "platform-capture.json").write_text(
        json.dumps(summary, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(summary, indent=2), flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, FileExistsError, OSError, ValueError, KeyError,
            subprocess.CalledProcessError, RuntimeError) as error:
        print(f"C53 platform capture failed: {error}", file=sys.stderr, flush=True)
        raise SystemExit(1)
