#!/usr/bin/env python3
"""Validate P9-17 packaged large-roster editing evidence and acceptance gates."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import re
import statistics
import sys
from pathlib import Path
from typing import Any

SCHEMA = "classmngr-phase9-p9-17-packaged-large-roster-v1"
FIXTURE_SCHEMA = "classmngr-phase9-p9-17-large-roster-fixture-v1"
WORKING_SET_TARGET = 268_435_456
WORKING_SET_CEILING = 471_859_200
OPERATION_PEAK_ADVISORY_LIMIT = 536_870_912
P917_BUILD_RECEIPT_SCHEMA = "classmngr-phase9-p9-17-exact-release-build-v3"
P917_BUILD_HELPER_RELATIVE = "scripts/phase9/p9_17_build_current_source_release.ps1"
P917_RELEASE_GENERATOR = "Visual Studio 17 2022"
P917_RELEASE_PLATFORM = "x64"
P917_RELEASE_TOOLSET = "v143"
P917_RELEASE_GENERATOR_INSTANCE = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
P917_RELEASE_TOOLSET_VERSION = "14.44.35207"
P917_RELEASE_COMPILER_VERSION = "19.44.35230.0"
P917_RELEASE_COMPILER_PATH = (
    P917_RELEASE_GENERATOR_INSTANCE
    + r"\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe"
)
P917_RELEASE_WINDOWS_SDK_VERSION = "10.0.26100.0"
P917_RELEASE_WINDOWS_SDK_INCLUDE_ROOT = (
    r"C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0"
)
P917_ROSTER_ABSENCE_PROOF_KIND = "fresh-active-tree-roster-absence"
P917_ROSTER_SELECTION_READBACK_SOURCE = "fresh SelectionItemPattern.IsSelected readback after section leave"
WINDOWS_MEDIUM_INTEGRITY_RID = 8192
BASE_SHA256 = "85c23437621fe67361b38157bb53acdb19e78573ebaa0297fad769b71ad0c9e9"
SEED_SHA256 = "1b2f09be03cce809e239112b3f7b4f2af405bad83a420db0c6ffe5ef0eb597e3"
P916_MANIFEST_RELATIVE = "artifacts/phase9/packaged-next-p9-16-repeated-workspace/20261121T023320451Z-9b576dcbfdc84dd0996f279db5ef67e9/manifest.json"
P917_BLOCKED_MANIFEST_RELATIVE = "artifacts/phase9/packaged-next-p9-17-large-roster/20261121T033132802Z-f79c212166cc47378a4c70d10c2f42b1/manifest.json"
P917_BLOCKED_BUILD_LOG_RELATIVE = "artifacts/phase9/packaged-next-p9-17-large-roster/20261121T033132802Z-f79c212166cc47378a4c70d10c2f42b1/logs/local-release-build.log"
P917_OWNED_SOURCES = (
    "scripts/phase9/prepare_p9_17_large_roster.py",
    "scripts/phase9/test_prepare_p9_17_large_roster.py",
    P917_BUILD_HELPER_RELATIVE,
    "scripts/phase9/run_packaged_next_p9_17_large_roster.ps1",
    "scripts/phase9/p9_17_heap_snapshot_elevated_worker.ps1",
    "scripts/phase9/validate_packaged_next_p9_17_large_roster.py",
    "scripts/phase9/test_validate_packaged_next_p9_17_large_roster.py",
)
P917_HARNESS_SOURCES = P917_OWNED_SOURCES + (
    "scripts/phase9/p9_14_packaged_lifecycle_common.ps1",
)
P917_AUTHORIZED_HARNESS_SOURCE_CHANGES = (
    {
        "path": "scripts/phase9/p9_14_packaged_lifecycle_common.ps1",
        "beforeSizeBytes": 176010,
        "beforeSha256": "868d92279cd14fea6eefdcac1cc35a554a28084e9932efb140581ed60dd7d263",
        "afterSizeBytes": 176293,
        "afterSha256": "24170ce41bcf03bdcc7c897d815dba716c729cede910b44a53e80913e829ae8c",
    },
)
P917_AUTHORIZED_PRODUCT_SOURCE_CHANGES = (
    {
        "path": "src/core/process_memory_snapshot.cpp",
        "beforeSizeBytes": 8776,
        "beforeSha256": "3b5cf0737340dbb6b308c9df9e517d853944602bbe71f189ff7266b519d057c4",
        "afterSizeBytes": 11156,
        "afterSha256": "8600b0c07259ccee97da754a36a60fcfbec853e253eae5e53886e5e6471a1be7",
    },
    {
        "path": "src/core/process_memory_snapshot.h",
        "beforeSizeBytes": 2397,
        "beforeSha256": "812976afbc797872c22087c39018302f0947eb0d490fcf844fec9a556d5226cc",
        "afterSizeBytes": 3025,
        "afterSha256": "8fd8731c3c4d52a370ea7ac308fc014b16ed5b18efc51fb1506efae76c30aca4",
    },
    {
        "path": "src/data/repositories/roster_repository.cpp",
        "beforeSizeBytes": 146469,
        "beforeSha256": "41c594afab83cbe6635cadc7f4c800e125843fb2336ed66b63c8ae6d33bbf719",
        "afterSizeBytes": 154045,
        "afterSha256": "b4805038b7589b18eaad8c7eee93b4424af187ce652521f44e24cbcc220b0fdb",
    },
    {
        "path": "src/data/repositories/roster_repository.h",
        "beforeSizeBytes": 11811,
        "beforeSha256": "2081c277989f19cffcdbcf063efaaf0935ed1e285401263dcb5a9b746c5c4bd7",
        "afterSizeBytes": 13045,
        "afterSha256": "7e0185a49c19184099c581ff26b412c807a9a4a176f5043693e803a683c0fbce",
    },
    {
        "path": "src/next/bootstrap/application_bootstrap.cpp",
        "beforeSizeBytes": 311829,
        "beforeSha256": "d16a8768306778ca88aa6067066311827f8e0811fc79772282a19599ca812edd",
        "afterSizeBytes": 316874,
        "afterSha256": "7a166e8674496ad42333d28418d5c0508e2236445c27258d1665ad912e396d7b",
    },
    {
        "path": "src/next/bootstrap/qt_page_lifecycle_diagnostics.cpp",
        "beforeSizeBytes": 28531,
        "beforeSha256": "32548730b42428addc4330b6c978b08a91d97d76cd9e169408f4b3cddcf45865",
        "afterSizeBytes": 38085,
        "afterSha256": "8488ccfddc6438f27597c20b6eba24a998a21e8ace2f91961e0261a2317dd354",
    },
    {
        "path": "src/next/bootstrap/qt_page_lifecycle_diagnostics.h",
        "beforeSizeBytes": 4968,
        "beforeSha256": "7df91471cbef0a211db5d169cc6955c7bc8b5278f8a7f425318818ee134e07bc",
        "afterSizeBytes": 7140,
        "afterSha256": "05071ab23f76d38c45696414cfd56af840ef5936a0d3af0ee1b73ebed95fc046",
    },
    {
        "path": "src/next/application/roster_save_preparation.h",
        "beforeSizeBytes": 19568,
        "beforeSha256": "196e28c3b42dd20906e6c6f84b1e54c6e0ececd2716f008c220cf51de64a889a",
        "afterSizeBytes": 24741,
        "afterSha256": "7ab0e34ddd5dfd44425226cf9558377d92aaf0dbba1e9e49fe5a3ed0d72afba5",
    },
    {
        "path": "src/next/application/roster_save_use_case.h",
        "beforeSizeBytes": 5554,
        "beforeSha256": "c3ce3b174398f27f2f77b943b9213dcc97a30af6ba2ea714a80157bbba245e66",
        "afterSizeBytes": 7537,
        "afterSha256": "1dad3866a1405ae8e1c493cb4c3009f862680c6bd789d7a7beda6eb046d23984",
    },
    {
        "path": "src/next/platform/workspace_store_roster_actions.cpp",
        "beforeSizeBytes": 33903,
        "beforeSha256": "822f5415a5d3a7b7d1dab9ea54b53e0cdf662bd9e327bfe16505c0107ac50993",
        "afterSizeBytes": 38957,
        "afterSha256": "2637b3fb8a07a6b399fc19ab4bb2e9170131a6e05839f3d2ec94ae62f2e76443",
    },
    {
        "path": "src/next/platform/workspace_store_roster_actions.h",
        "beforeSizeBytes": 1541,
        "beforeSha256": "9c5d440ce9459cca08ce92297cd54e7437803e49a24e2393617a8ab669c1521c",
        "afterSizeBytes": 1756,
        "afterSha256": "e86ca771dcc5143466047a0c2c6740e8601cdaca38c93708e70e4f18195e1825",
    },
    {
        "path": "src/next/ui/roster_editor_widget.cpp",
        "beforeSizeBytes": 53975,
        "beforeSha256": "83f3879273819377efd3994d6ff009d4343635b9471784132238d75a390138aa",
        "afterSizeBytes": 58395,
        "afterSha256": "58f09304b094cd4be93b4e3ed71def9b108304d59128ef8c77bc5aa954b53dae",
    },
    {
        "path": "src/next/ui/roster_editor_widget.h",
        "beforeSizeBytes": 5292,
        "beforeSha256": "c190060c12c6c9dcff8cec7f9b3f26b0229156fce41e079a0b844bccd1200b48",
        "afterSizeBytes": 6658,
        "afterSha256": "b9852b472f9dde4d10be3b1b46b77a0c4546af6c216ba7522803b513e4a44639",
    },
    {
        "path": "src/next/ui/roster_table_model.cpp",
        "beforeSizeBytes": 24480,
        "beforeSha256": "98678d18665d6cce5de53413f9c0ba9a145dc3537da6787cad2feb0480a13192",
        "afterSizeBytes": 28080,
        "afterSha256": "4f0d56113ec8113f4ec2a81e91d8279162fa46911b3c268bc1bd8ffe0a0d8ff4",
    },
    {
        "path": "src/next/ui/roster_table_model.h",
        "beforeSizeBytes": 3633,
        "beforeSha256": "5087fb8af1d81b06fa726a8ea0b7759ea078c44649607b1ba94836b92f162f33",
        "afterSizeBytes": 3745,
        "afterSha256": "1184aa2156364f0af6adcfb7d14754c07029fda7a3da1a158b56f51c0e98ea64",
    },
    {
        "path": "src/next/ui/selected_class_editor.cpp",
        "beforeSizeBytes": 82573,
        "beforeSha256": "da5e2d80217c684b2b81a509ff72bb8f4162eb560a387e5bd9230772c8ee13f4",
        "afterSizeBytes": 83736,
        "afterSha256": "63f6e66755ba9c9ac33f6ce50ccbd7bfe7a67d9a292bafc0faea36786598fc52",
    },
    {
        "path": "src/next/ui/selected_class_editor.h",
        "beforeSizeBytes": 10470,
        "beforeSha256": "ea871923bc440ea7b1e0fa4bca699386eb09662c3765398e01df5ab57ba293ff",
        "afterSizeBytes": 10631,
        "afterSha256": "2bbdb7cdf6c9680817573be97fa4987e9b916710b0d3a6eca9294f9cc8f194c3",
    },
)
P917_TEST_ONLY_SOURCE_BOUNDARY_EVIDENCE = (
    {
        "path": "cmake/next_app/CMakeLists.txt",
        "lines": "17-22,120-143",
        "finding": "The standalone-test source change is absent from the ClassMngrNext production executable source and link closure.",
    },
    {
        "path": "cmake/next_resources.cmake",
        "lines": "21-58,172,258-263,367-379",
        "finding": "The standalone-test source change is absent from application resource inputs and installed package resources.",
    },
)
P917_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES = (
    {
        "path": "tests/next/sub_prep_page_tests.cpp",
        "beforeSizeBytes": 20782,
        "beforeSha256": "70f5f2ff168432ea558729442cf727b15c8295ec439687c78ee51e4352943516",
        "afterSizeBytes": 20782,
        "afterSha256": "af4f8325afdec691fa8c6130d4e31540b6f4d91919114ee395b575c5668862ae",
        "target": "ClassMngrNextSubPrepPageTests",
        "cmakeLines": "4014-4023,4059-4070",
    },
    {
        "path": "tests/next/shared_staff_directory_visual_capture.cpp",
        "beforeSizeBytes": 31510,
        "beforeSha256": "b0a216be29787583e84093f0acf5e987bbca38f05c4515d057f3822ca41d0d78",
        "afterSizeBytes": 31518,
        "afterSha256": "916b828dba3b01ea7b3748ca6dc5f471e3265fa092b2f27bc228043ef68ad6a3",
        "target": "ClassMngrNextSharedStaffDirectoryVisualCapture",
        "cmakeLines": "997-998,1035-1036",
    },
    {
        "path": "tests/next/korean_teacher_directory_visual_capture.cpp",
        "beforeSizeBytes": 37762,
        "beforeSha256": "b68062eef5318a151ed9d25527f9c2b0bf3ae22f0950bcc3144b5fb77369893d",
        "afterSizeBytes": 37778,
        "afterSha256": "f69e1ca39365e50f111c3bfb81cbe56aa6ac665a75c3ba414f7651361fb56c89",
        "target": "ClassMngrNextTeacherDirectoryVisualCapture",
        "cmakeLines": "1085-1086,1144-1145",
    },
    {
        "path": "tests/next/upcoming_birthdays_visual_capture.cpp",
        "beforeSizeBytes": 20421,
        "beforeSha256": "65c42f543a550c27d763d80bdff914854647f175843f5b7df1b10f5ac2488221",
        "afterSizeBytes": 20429,
        "afterSha256": "cc59fae5e2886aa48804acd0d0e50e4ba4cab7371cb44e26eb8244f97a40f3c9",
        "target": "ClassMngrNextUpcomingBirthdaysVisualCapture",
        "cmakeLines": "5281-5282,5310-5311",
    },
    {
        "path": "cmake/tests/next.cmake",
        "beforeSizeBytes": 296864,
        "beforeSha256": "675e7a1fc7c2877074108fc05ed4fbe6937bd058132d77aeba5c5059e289bc8e",
        "afterSizeBytes": 296959,
        "afterSha256": "6ba82e3c77e84b0ff425f2c071d9119d71a0b29895a7ee9579d6b17aa801c6dd",
        "target": "ClassMngrNextRosterEditorTests",
        "cmakeLines": "4398-4406",
    },
    {
        "path": "tests/next/page_host_tests.cpp",
        "beforeSizeBytes": 92258,
        "beforeSha256": "3bf6863e0a73bc96ef62f5aefefa3b493a3d54529cd82398a20eb2e16fd5353f",
        "afterSizeBytes": 105901,
        "afterSha256": "9dd0a582970000707ec805a25530d44f7049eea653f4c8b1d1b5875ca43a56c4",
        "target": "ClassMngrNextPageHostTests",
        "cmakeLines": "843-894",
    },
    {
        "path": "tests/next/roster_editor_tests.cpp",
        "beforeSizeBytes": 89727,
        "beforeSha256": "5f810ba8d2d3138e3a323ac5f791b00652772dd2c41993412b86255348357ad6",
        "afterSizeBytes": 126850,
        "afterSha256": "b02cb7f7e7fc294643d94287df8a9ce6c146ecbd57a02279b6f22919ac98dc08",
        "target": "ClassMngrNextRosterEditorTests",
        "cmakeLines": "4398-4446",
    },
    {
        "path": "tests/next_application_roster_save_use_case_tests.cpp",
        "beforeSizeBytes": 26048,
        "beforeSha256": "70980d2f4529b7a9dfeea89a160e1429f63797c55935baf575a30465e836695c",
        "afterSizeBytes": 35484,
        "afterSha256": "b5a3c4b93b11a46946567678e4e12ce0e1004652ecd4dfd7f18d15b311c8c426",
        "target": "ClassMngrNextApplicationRosterSaveUseCaseTests",
        "cmakeLines": "5937-5944",
    },
    {
        "path": "tests/next/classes_navigator_page_tests.cpp",
        "beforeSizeBytes": 111698,
        "beforeSha256": "88bcd4267dfa89b8b7b5fadeb43e6ab5af90833fddcb15cf8c0f02fc37ba4fc0",
        "afterSizeBytes": 118993,
        "afterSha256": "415c57ba38205959e438f2bfbe88baabcbd9b8d4406c0b3e8dc66e9202b6cdc2",
        "target": "ClassMngrNextClassesNavigatorPageTests",
        "cmakeLines": "4264-4277",
    },
    {
        "path": "tests/next_platform_application_services_roster_save_port_tests.cpp",
        "beforeSizeBytes": 38542,
        "beforeSha256": "471e010aac3912c2cdf140dbd18f8097cf69a3e146c107dbeb25ca76c7ac1dca",
        "afterSizeBytes": 46186,
        "afterSha256": "2694a4d2d77640481e5e53c20f98669d002069a3f01a758e23e8c30862e9cbbb",
        "target": "ClassMngrNextPlatformApplicationServicesRosterSavePortTests",
        "cmakeLines": "7308-7316",
    },
)
P917_NEW_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES = P917_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES[-5:]
for _test_source_change in P917_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES:
    _cmake_boundary_finding = (
        f"The pinned CMake delta adds implementation sources only to standalone {_test_source_change['target']}."
        if _test_source_change["path"] == "cmake/tests/next.cmake"
        else f"The changed test source is compiled only into standalone {_test_source_change['target']}."
    )
    _test_source_change["cmakeBoundaryEvidence"] = (
        {
            "path": "cmake/tests/next.cmake",
            "lines": _test_source_change["cmakeLines"],
            "finding": _cmake_boundary_finding,
        },
        *P917_TEST_ONLY_SOURCE_BOUNDARY_EVIDENCE,
    )
P914_HARNESS_SOURCES = (
    "scripts/phase9/p9_14_packaged_lifecycle_common.ps1",
    "scripts/phase9/prepare_p9_14_staff_ai_workspaces.py",
    "scripts/phase9/run_packaged_next_p9_14_staff_directory_lifecycle.ps1",
    "scripts/phase9/run_packaged_next_p9_14_ai_batch_review_lifecycle.ps1",
    "scripts/phase9/validate_packaged_next_p9_14_lifecycle.py",
    "scripts/phase9/test_validate_packaged_next_p9_14_lifecycle.py",
)
P915_HARNESS_SOURCES = (
    "scripts/phase9/prepare_p9_15_schedule_workspace.py",
    "scripts/phase9/run_packaged_next_p9_15_schedule_locale_theme.ps1",
    "scripts/phase9/validate_packaged_next_p9_15_schedule_locale_theme.py",
    "scripts/phase9/test_validate_packaged_next_p9_15_schedule_locale_theme.py",
)
P916_HARNESS_SOURCES = (
    "scripts/phase9/prepare_p9_16_workspace.py",
    "scripts/phase9/test_prepare_p9_16_workspace.py",
    "scripts/phase9/run_packaged_next_p9_16_repeated_workspace.ps1",
    "scripts/phase9/validate_packaged_next_p9_16_repeated_workspace.py",
    "scripts/phase9/test_validate_packaged_next_p9_16_repeated_workspace.py",
)
SOURCE_CACHE_DIRECTORIES = {"__pycache__", ".pytest_cache", ".mypy_cache"}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def ordinal_lower_path_sort_key(path: str) -> tuple[bytes, bytes]:
    """Match .NET StringComparer.Ordinal over ToLowerInvariant UTF-16 code units."""
    return (
        path.lower().encode("utf-16-be", errors="surrogatepass"),
        path.encode("utf-16-be", errors="surrogatepass"),
    )


def canonical_inventory_fingerprint(files: list[dict[str, Any]]) -> str:
    ordered = sorted(
        files,
        key=lambda row: ordinal_lower_path_sort_key(str(row["path"])),
    )
    canonical = "\n".join(
        f"{row['path']}\t{int(row['sizeBytes'])}\t{str(row['sha256']).lower()}"
        for row in ordered
    )
    return hashlib.sha256((canonical + "\n").encode("utf-8")).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def canonicalize_exact_path_records(records: Any, description: str) -> list[dict[str, Any]]:
    require(isinstance(records, list), f"{description} is not a list.")
    by_path: dict[str, dict[str, Any]] = {}
    for record in records:
        require(isinstance(record, dict), f"{description} contains a non-object record.")
        path = record.get("path")
        require(isinstance(path, str) and bool(path), f"{description} contains an invalid path.")
        key = path.casefold()
        require(key not in by_path, f"{description} contains a duplicate path: {path}.")
        by_path[key] = record
    return [by_path[key] for key in sorted(by_path)]


def require_exact_path_records(actual: Any, expected: Any, message: str) -> None:
    actual_rows = canonicalize_exact_path_records(actual, "Recorded P9-17 source deltas")
    expected_rows = canonicalize_exact_path_records(expected, "Pinned P9-17 source deltas")
    expected_by_path = {str(row["path"]).casefold(): row for row in expected_rows}
    for row in actual_rows:
        pinned = expected_by_path.get(str(row["path"]).casefold())
        if pinned is None:
            continue
        for field, value in pinned.items():
            if type(value) is bool:
                require(
                    type(row.get(field)) is bool,
                    f"Recorded P9-17 field {field} for {row['path']} is not a JSON boolean.",
                )
            elif type(value) is int:
                require(
                    type(row.get(field)) is int,
                    f"Recorded P9-17 field {field} for {row['path']} is not a JSON integer.",
                )
    require(
        actual_rows == expected_rows,
        message,
    )


def canonicalize_exact_paths(paths: Any, description: str) -> list[str]:
    require(isinstance(paths, list), f"{description} is not a list.")
    by_path: dict[str, str] = {}
    for path in paths:
        require(isinstance(path, str) and bool(path), f"{description} contains an invalid path.")
        key = path.casefold()
        require(key not in by_path, f"{description} contains a duplicate path: {path}.")
        by_path[key] = path
    return [by_path[key] for key in sorted(by_path)]


def require_exact_paths(actual: Any, expected: Any, message: str) -> None:
    require(
        canonicalize_exact_paths(actual, "Recorded P9-17 source paths")
        == canonicalize_exact_paths(expected, "Pinned P9-17 source paths"),
        message,
    )


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"Expected a JSON object at {path}.")
    return value


def resolve_evidence_path(evidence_root: Path, relative_path: str) -> Path:
    path = (evidence_root / relative_path).resolve(strict=True)
    if evidence_root.resolve() not in path.parents:
        raise ValueError(f"Evidence path escaped its run directory: {relative_path}")
    return path


def verify_recorded_file(
    evidence_root: Path,
    record: dict[str, Any],
    path_key: str = "relativePath",
    hash_key: str = "sha256",
) -> Path:
    relative = str(record.get(path_key, ""))
    if not relative:
        raise ValueError(f"Evidence file record omits {path_key}.")
    path = resolve_evidence_path(evidence_root, relative)
    actual_hash = sha256_file(path)
    if actual_hash != str(record.get(hash_key, "")):
        raise ValueError(f"Evidence file hash mismatch: {relative}")
    if "sizeBytes" in record and int(record["sizeBytes"]) != path.stat().st_size:
        raise ValueError(f"Evidence file size mismatch: {relative}")
    return path


def expected_roster_headers() -> list[str]:
    return [
        "English", "Korean", "Winter", "Speech Contest", "Summer", "Fall", "Student", "School", "Notes",
        *(f"Stress Field {index:03d}" for index in range(4, 151)),
    ]


def parse_utc_timestamp(value: Any, label: str) -> dt.datetime:
    text = str(value or "")
    require(bool(text), f"{label} timestamp is missing.")
    try:
        parsed = dt.datetime.fromisoformat(text.replace("Z", "+00:00"))
    except ValueError as exc:
        raise ValueError(f"{label} timestamp is not valid ISO-8601 UTC: {text}.") from exc
    require(parsed.tzinfo is not None, f"{label} timestamp does not include a timezone.")
    return parsed.astimezone(dt.timezone.utc)


def validate_natural_process_close_action(
    route: dict[str, Any], process_exit: dict[str, Any]
) -> dict[str, Any]:
    actions = route.get("actions")
    require(isinstance(actions, list), "Natural close evidence lacks route actions.")
    close_actions = [
        action for action in actions
        if isinstance(action, dict)
        and action.get("actionKind") == "close-main-window-naturally"
    ]
    require(len(close_actions) == 1, "Natural close evidence requires exactly one close-main-window-naturally action.")
    action = close_actions[0]
    require(action.get("method") == "WindowPattern.Close", "Natural close action did not use WindowPattern.Close.")
    require(action.get("forcedTermination") is False, "Natural close action records forced termination.")

    issued = parse_utc_timestamp(action.get("issuedAtUtc"), "Natural close action issued")
    close_returned = parse_utc_timestamp(action.get("closeReturnedAtUtc"), "Natural close return")
    completed = parse_utc_timestamp(action.get("completedAtUtc"), "Natural close action completed")
    require(issued <= close_returned <= completed,
            "Natural close action close-return/completion timestamps are missing, reversed, or out of order.")

    exit_evidence = action.get("processExitEvidence")
    require(isinstance(exit_evidence, dict), "Natural close action lacks process-exit evidence.")
    require(exit_evidence.get("observed") is True and exit_evidence.get("normalExit") is True,
            "Natural close action process-exit evidence is not a successful normal exit.")
    require(type(exit_evidence.get("exitCode")) is int and exit_evidence.get("exitCode") == 0,
            "Natural close action process-exit evidence does not record exit code zero.")
    require(exit_evidence.get("closeMethod") == "WindowPattern.Close"
            and exit_evidence.get("forcedTermination") is False,
            "Natural close action process-exit evidence records a forced or mismatched close method.")
    require(process_exit.get("observed") is True and process_exit.get("normalExit") is True,
            "Natural process exit was not observed as a normal exit.")
    require(type(process_exit.get("exitCode")) is int and process_exit.get("exitCode") == 0,
            "Natural process exit code is not zero.")
    require(process_exit.get("closeMethod") == "WindowPattern.Close"
            and process_exit.get("forcedTermination") is False,
            "Process did not exit through an unforced natural UIA window close.")
    exited = parse_utc_timestamp(process_exit.get("exitedAtUtc"), "Natural process exit")
    require(completed < exited, "Natural process exit did not occur after close-action completion.")

    matching_fields = (
        "observed", "normalExit", "exitCode", "closeMethod", "forcedTermination", "exitedAtUtc",
    )
    require(all(exit_evidence.get(field) == process_exit.get(field) for field in matching_fields),
            "Natural close action process-exit evidence differs from the manifest process-exit record.")
    return {
        "actionKind": "close-main-window-naturally",
        "issuedAtUtc": action["issuedAtUtc"],
        "closeReturnedAtUtc": action["closeReturnedAtUtc"],
        "completedAtUtc": action["completedAtUtc"],
        "exitedAtUtc": process_exit["exitedAtUtc"],
    }


def _memory_window_timestamp_bounds(window: dict[str, Any], label: str) -> tuple[dt.datetime, dt.datetime]:
    started = parse_utc_timestamp(window.get("startedAtUtc"), f"{label} started")
    completed = parse_utc_timestamp(window.get("completedAtUtc"), f"{label} completed")
    require(started < completed, f"{label} has an empty or reversed timestamp interval.")
    samples = window.get("samples")
    require(isinstance(samples, list) and bool(samples), f"{label} has no timestamped samples.")
    sample_times = [
        parse_utc_timestamp(sample.get("capturedAtUtc"), f"{label} sample {index} captured")
        for index, sample in enumerate(samples)
    ]
    require(all(started <= sample_time <= completed for sample_time in sample_times),
            f"{label} sample timestamps fall outside the recorded memory window.")
    require(sample_times == sorted(sample_times) and len(set(sample_times)) == len(sample_times),
            f"{label} sample timestamps are missing, duplicated, or out of order.")
    require(started == sample_times[0],
            f"{label} startedAtUtc must equal the first sample timestamp.")
    return started, completed


def validate_cleanup_memory_window_chronology(
    memory: dict[str, Any], route: dict[str, Any], by_id: dict[str, dict[str, Any]]
) -> dict[str, Any]:
    actions = route.get("actions")
    require(isinstance(actions, list), "Memory-window chronology lacks route actions.")
    previous_action_issued: dt.datetime | None = None
    previous_action_completed: dt.datetime | None = None
    for index, action in enumerate(actions):
        require(isinstance(action, dict), f"Route action {index} is not an object.")
        issued = parse_utc_timestamp(action.get("issuedAtUtc"), f"Route action {index} issued")
        completed = parse_utc_timestamp(action.get("completedAtUtc"), f"Route action {index} completed")
        require(issued <= completed,
                f"Route action {index} issuedAtUtc is later than completedAtUtc.")
        if previous_action_issued is not None and previous_action_completed is not None:
            require(previous_action_issued <= issued and previous_action_completed <= completed
                    and previous_action_completed <= issued,
                    f"Route action {index} timestamps are not monotonic after action {index - 1}.")
        previous_action_issued = issued
        previous_action_completed = completed

    def action_index(action_kind: str, label: str) -> int:
        matches = [index for index, action in enumerate(actions)
                   if str(action.get("actionKind", "")) == action_kind]
        require(len(matches) == 1, f"Memory-window chronology requires exactly one {label} route action.")
        return matches[0]

    def action_time(index: int, field: str, label: str) -> dt.datetime:
        return parse_utc_timestamp(actions[index].get(field), f"{label} route action {field}")

    def bounded_window(window_id: str, label: str) -> tuple[dt.datetime, dt.datetime]:
        window = by_id.get(window_id)
        require(isinstance(window, dict), f"{label} memory window is missing: {window_id}.")
        return _memory_window_timestamp_bounds(window, label)

    def checkpoint_evidence_time(checkpoint: Any, label: str) -> dt.datetime:
        require(isinstance(checkpoint, dict), f"{label} checkpoint is missing.")
        times = []
        for field in ("observedAtUtc", "screenshotCapturedAtUtc"):
            value = checkpoint.get(field)
            if value is not None and str(value).strip():
                times.append(parse_utc_timestamp(value, f"{label} {field}"))
        require(bool(times), f"{label} checkpoint has neither observedAtUtc nor screenshotCapturedAtUtc evidence.")
        return max(times)

    cold_baseline_start, cold_baseline_end = bounded_window("matched-baseline", "Cold matched baseline")
    target_selection_index = action_index("select-target-class", "cold-baseline target selection")
    initial_roster_index = action_index("open-roster-tab", "cold-baseline roster open")
    require(target_selection_index < initial_roster_index
            and action_time(target_selection_index, "completedAtUtc", "Cold-baseline target selection")
            < cold_baseline_start < cold_baseline_end
            < action_time(initial_roster_index, "issuedAtUtc", "Cold-baseline roster open"),
            "Cold matched-baseline window falls outside the interval after target selection and before initial roster open.")

    suspension_index = route.get("sectionSuspensionActionIndex")
    require(type(suspension_index) is int and 0 <= suspension_index < len(actions),
            "Cold cleanup return has no exact section-suspension action index.")
    suspension_checkpoint = route.get("sectionSuspensionCheckpoint") or {}
    require(type(suspension_checkpoint.get("leaveActionIndex")) is int
            and suspension_checkpoint["leaveActionIndex"] == suspension_index
            and str(actions[suspension_index].get("actionKind", "")) == "leave-roster-section",
            "Cold cleanup return is not linked to its roster-section leave action.")
    next_action_index = suspension_index + 1
    require(next_action_index < len(actions)
            and str(actions[next_action_index].get("actionKind", "")) == "navigate-my_workspace",
            "Cold cleanup return is not bounded by the following Workspace navigation action.")
    cold_return_start, cold_return_end = bounded_window("matched-after-suspension", "Cold suspension return")
    cold_return_lower_bound = action_time(suspension_index, "completedAtUtc", "Cold roster-section leave")
    cold_return_lower_bound = max(
        cold_return_lower_bound,
        checkpoint_evidence_time(suspension_checkpoint, "Cold roster-clear"),
    )
    require(cold_return_lower_bound < cold_return_start < cold_return_end
            < action_time(next_action_index, "issuedAtUtc", "Post-priming Workspace navigation"),
            "Cold suspension-return window falls outside the interval after roster-section suspension and before Workspace navigation.")

    baseline_ids = memory.get("plateauBaselineWindowIds")
    require(isinstance(baseline_ids, list) and baseline_ids == [
        f"plateau-baseline-{index:02d}" for index in range(1, 6)
    ], "Post-priming baseline chronology requires five ordered baseline window IDs.")
    checkpoint = route.get("postPrimingClearCheckpoint") or {}
    checkpoint_time = checkpoint_evidence_time(checkpoint, "Post-priming roster-clear")
    first_cycle_open_index = action_index("cycle-01-open-roster", "first measured-cycle roster open")
    first_cycle_open_time = action_time(first_cycle_open_index, "issuedAtUtc", "First measured-cycle roster open")
    previous_baseline_end = checkpoint_time
    for window_id in baseline_ids:
        started, completed = bounded_window(str(window_id), f"Post-priming baseline {window_id}")
        require(previous_baseline_end < started < completed < first_cycle_open_time,
                f"Post-priming baseline window {window_id} falls outside the interval after the post-primer checkpoint and before the first measured cycle.")
        previous_baseline_end = completed

    cycle_records = route.get("cycleRecords")
    cycle_window_ids = memory.get("plateauCycleWindowIds")
    require(isinstance(cycle_records, list) and isinstance(cycle_window_ids, list),
            "Measured-cycle return chronology lacks cycle records or window identifiers.")
    require(len(cycle_records) == 10,
            "Measured-cycle return chronology requires exactly ten cycle records.")
    final_reentry_index = action_index("cycle-final-reenter-roster", "final measured-cycle re-entry")
    cycle_returns: list[dict[str, Any]] = []
    for cycle_number, record in enumerate(cycle_records, start=1):
        tag = f"{cycle_number:02d}"
        window_id = f"cycle-{tag}-return"
        require(record.get("postSuspensionWindowId") == window_id and window_id in cycle_window_ids,
                f"Cycle {cycle_number} return window is not linked to its cycle record.")
        checkpoint = record.get("suspensionCheckpoint") or {}
        leave_index = checkpoint.get("leaveActionIndex")
        require(type(leave_index) is int and 0 <= leave_index < len(actions)
                and str(actions[leave_index].get("actionKind", "")) == f"cycle-{tag}-leave-roster",
                f"Cycle {cycle_number} return window is not linked to its roster-section leave action.")
        expected_next_action = (
            f"cycle-{cycle_number + 1:02d}-open-roster"
            if cycle_number < len(cycle_records)
            else "cycle-final-reenter-roster"
        )
        next_index = leave_index + 1
        require(next_index < len(actions)
                and str(actions[next_index].get("actionKind", "")) == expected_next_action,
                f"Cycle {cycle_number} return window is not bounded by the next expected route action.")
        if cycle_number == len(cycle_records):
            require(next_index == final_reentry_index,
                    "Cycle 10 return window is not bounded by the unique final roster re-entry action.")
        checkpoint_time = checkpoint_evidence_time(checkpoint, f"Cycle {cycle_number} roster-clear")
        leave_completed = action_time(leave_index, "completedAtUtc", f"Cycle {cycle_number} roster-section leave")
        started, completed = bounded_window(window_id, f"Cycle {cycle_number} suspension return")
        require(leave_completed <= checkpoint_time < started < completed
                < action_time(next_index, "issuedAtUtc", f"Cycle {cycle_number} next route action"),
                f"Cycle {cycle_number} return window falls outside the interval after its suspension checkpoint and before the next route action.")
        cycle_returns.append({"cycleNumber": cycle_number, "windowId": window_id})
    return {
        "coldBaselineWindowId": "matched-baseline",
        "coldReturnWindowId": "matched-after-suspension",
        "postPrimingBaselineWindowIds": list(baseline_ids),
        "cycleReturnWindowIds": [record["windowId"] for record in cycle_returns],
        "passed": True,
    }


_UTC_TICK_TIMESTAMP = re.compile(
    r"^(?P<date>\d{4}-\d{2}-\d{2})[Tt ]"
    r"(?P<time>\d{2}:\d{2}:\d{2})"
    r"(?:[.,](?P<fraction>\d+))?"
    r"(?P<zone>Z|z|[+-]\d{2}(?::?\d{2})?(?::?\d{2}(?:[.,]\d{1,6})?)?)$"
)


def parse_utc_timestamp_ticks(value: Any, label: str) -> int:
    """Return exact UTC 100 ns ticks for a timezone-aware ISO timestamp."""
    text = str(value or "")
    require(bool(text), f"{label} timestamp is missing.")
    match = _UTC_TICK_TIMESTAMP.fullmatch(text)
    require(match is not None, f"{label} timestamp is not valid ISO-8601 with 100 ns precision: {text}.")
    fraction = match.group("fraction") or ""
    require(len(fraction) <= 7,
            f"{label} timestamp exceeds the supported 100 ns precision: {text}.")
    zone = match.group("zone")
    if zone in ("Z", "z"):
        zone = "+00:00"
    try:
        local_whole_second = dt.datetime.fromisoformat(
            f"{match.group('date')}T{match.group('time')}{zone}"
        )
    except ValueError as exc:
        raise ValueError(f"{label} timestamp is not valid ISO-8601 with 100 ns precision: {text}.") from exc
    require(local_whole_second.tzinfo is not None and local_whole_second.utcoffset() is not None,
            f"{label} timestamp does not include a timezone.")
    utc_whole_second = local_whole_second.astimezone(dt.timezone.utc)
    epoch = dt.datetime(1, 1, 1, tzinfo=dt.timezone.utc)
    delta = utc_whole_second - epoch
    whole_second_ticks = (delta.days * 86_400 + delta.seconds) * 10_000_000 + delta.microseconds * 10
    fraction_ticks = int(fraction.ljust(7, "0")) if fraction else 0
    return whole_second_ticks + fraction_ticks


def validate_cycle_uia_contract(
    evidence_root: Path,
    snapshot_record: dict[str, Any],
    expected_name: str,
    process_id: int,
    main_window_handle: int,
    fixture_targets: dict[str, dict[str, Any]],
    expected_values: dict[str, str],
    expected_readbacks: list[dict[str, Any]],
) -> None:
    contract_path = verify_recorded_file(evidence_root, snapshot_record)
    contract = load_json(contract_path)
    require(contract.get("schema") == "classmngr-phase9-p9-17-roster-uia-contract-v1"
            and contract.get("name") == expected_name,
            f"Cycle UIA contract schema or name differs: {expected_name}.")
    require(int(contract.get("processId", -1)) == process_id
            and int(contract.get("mainWindowHandle", -1)) == main_window_handle,
            f"Cycle UIA contract {expected_name} is not bound to the measured PID/HWND.")
    grid = contract.get("grid") or {}
    require(int(grid.get("rowCount", -1)) == 100 and int(grid.get("columnCount", -1)) == 156,
            f"Cycle UIA contract {expected_name} does not record the 100x156 grid.")
    headers = contract.get("headers")
    expected_headers = expected_roster_headers()
    require(isinstance(headers, list) and len(headers) == 156
            and [int(row.get("column", -1)) for row in headers] == list(range(156))
            and [str(row.get("text", "")) for row in headers] == expected_headers,
            f"Cycle UIA contract {expected_name} does not record all 156 ordered headers.")
    observed_values = contract.get("observedValues")
    require(isinstance(observed_values, list) and len(observed_values) == 3
            and isinstance(expected_readbacks, list) and len(expected_readbacks) == 3,
            f"Cycle UIA contract {expected_name} omits the three target-cell readbacks.")
    def normalize_readbacks(readbacks: list[dict[str, Any]], label: str) -> list[tuple[Any, ...]]:
        require(isinstance(readbacks, list) and len(readbacks) == 3,
                f"{label} must contain exactly three target readbacks.")
        by_name: dict[str, tuple[Any, ...]] = {}
        for item in readbacks:
            name = str(item.get("name", ""))
            require(name in fixture_targets and name not in by_name,
                    f"{label} has a missing, unknown, or duplicated fixture target name: {name}.")
            target = fixture_targets[name]
            coordinates: list[int] = []
            for field in ("row", "modelColumn", "storedColumn"):
                coordinate = item.get(field)
                require(type(coordinate) is int and coordinate == int(target[field]),
                        f"{label} target {name} has a mutated fixture coordinate {field}={coordinate!r}; expected {target[field]}.")
                coordinates.append(coordinate)
            expected_value = str(expected_values.get(name, ""))
            require(bool(expected_value) and str(item.get("expected", "")) == expected_value
                    and str(item.get("actual", "")) == expected_value
                    and item.get("matched") is True,
                    f"{label} target {name} differs from its fixture-derived expected value or readback.")
            by_name[name] = (name, *coordinates, expected_value, expected_value, True)
        require(set(by_name) == set(fixture_targets) == set(expected_values),
                f"{label} does not cover the exact fixture target set.")
        return [by_name[name] for name in fixture_targets]

    observed = normalize_readbacks(observed_values, f"Cycle UIA contract {expected_name}")
    expected = normalize_readbacks(expected_readbacks, f"Cycle route record {expected_name}")
    require(observed == expected,
            f"Cycle UIA contract {expected_name} target coordinates or readbacks differ from its route record.")


def validate_final_cycle_uia_contract(
    evidence_root: Path,
    snapshots_by_path: dict[str, dict[str, Any]],
    recorded_relative_path: Any,
    process_id: int,
    main_window_handle: int,
    fixture_targets: dict[str, dict[str, Any]],
    expected_values: dict[str, str],
    final_readbacks: list[dict[str, Any]],
) -> None:
    expected_name = "cycle-final-reentry-contract"
    expected_path = f"uia/{expected_name}.json"
    require(recorded_relative_path == expected_path,
            "Final measured-cycle re-entry UIA contract path is missing or differs from the pinned path.")
    snapshot_record = snapshots_by_path.get(expected_path)
    require(snapshot_record is not None,
            "Final measured-cycle re-entry UIA contract is missing from the evidence inventory.")
    validate_cycle_uia_contract(
        evidence_root,
        snapshot_record,
        expected_name,
        process_id,
        main_window_handle,
        fixture_targets,
        expected_values,
        final_readbacks,
    )


def is_roster_content_control(node: Any) -> bool:
    if not isinstance(node, dict):
        return False
    automation_id = str(node.get("automationId", "")).casefold()
    return (
        automation_id in {"selectedclassrostertab", "selectedclassrosterscrollarea", "nextrostertable"}
        or "selectedclassrostertab" in automation_id
        or "selectedclassrosterscrollarea" in automation_id
        or automation_id.endswith(".nextrostertable")
    )


def validate_roster_absence_checkpoint(
    evidence_root: Path,
    route: dict[str, Any],
    checkpoint: Any,
    process_id: int,
    main_window_handle: int,
    expected_action_kind: str,
    checkpoint_label: str,
) -> dict[str, Any]:
    require(isinstance(checkpoint, dict), f"{checkpoint_label} roster-absence checkpoint is missing.")
    require(
        checkpoint.get("selectedSection") == "Class Details and Times"
        and checkpoint.get("selectedTabVerified") is True
        and checkpoint.get("selectedTabReadbackSource") == P917_ROSTER_SELECTION_READBACK_SOURCE,
        f"{checkpoint_label} does not contain a fresh SelectionItemPattern readback for Class Details and Times.",
    )
    settled = checkpoint.get("settledMilliseconds")
    require(type(settled) is int and settled >= 500,
            f"{checkpoint_label} did not settle for at least 500 ms after leaving Roster.")
    require(
        checkpoint.get("activeUiAutomationRosterTablePresent") is False
        and checkpoint.get("freshActiveTreeCaptured") is True
        and checkpoint.get("proofKind") == P917_ROSTER_ABSENCE_PROOF_KIND
        and type(checkpoint.get("activeTreeRosterControlCount")) is int
        and checkpoint.get("activeTreeRosterControlCount") == 0,
        f"{checkpoint_label} does not prove fresh active-tree absence of roster controls.",
    )
    require(
        type(checkpoint.get("processId")) is int
        and checkpoint.get("processId") == process_id
        and type(checkpoint.get("mainWindowHandle")) is int
        and checkpoint.get("mainWindowHandle") == main_window_handle,
        f"{checkpoint_label} roster-absence checkpoint is not bound to the measured process and main HWND.",
    )

    actions = route.get("actions")
    action_index = checkpoint.get("leaveActionIndex")
    require(isinstance(actions, list) and type(action_index) is int and 0 <= action_index < len(actions),
            f"{checkpoint_label} has no valid linked section-leave action.")
    leave_action = actions[action_index]
    require(
        isinstance(leave_action, dict)
        and checkpoint.get("leaveActionKind") == expected_action_kind
        and leave_action.get("actionKind") == expected_action_kind
        and leave_action.get("expectedSection") == "Class Details and Times"
        and leave_action.get("selectedTabName") == "Class Details and Times",
        f"{checkpoint_label} is not linked to the expected Class Details and Times tab-selection action.",
    )
    action_completed = parse_utc_timestamp(leave_action.get("completedAtUtc"), f"{checkpoint_label} leave action completion")
    selected_readback = parse_utc_timestamp(checkpoint.get("selectedTabReadbackAtUtc"), f"{checkpoint_label} selected-tab readback")
    minimum_settle = dt.timedelta(milliseconds=500)
    require(
        selected_readback - action_completed >= minimum_settle,
        f"{checkpoint_label} timestamps prove less than 500 ms from leave action to fresh Details-tab readback.",
    )

    snapshot_relative = str(checkpoint.get("uiaSnapshotRelativePath", ""))
    require(snapshot_relative.startswith("uia/"), f"{checkpoint_label} fresh active-tree UIA snapshot path is invalid.")
    uia_snapshots = route.get("uiaSnapshots")
    require(isinstance(uia_snapshots, list), f"{checkpoint_label} UIA snapshot inventory is missing.")
    matching_snapshots = [
        record for record in uia_snapshots
        if record.get("relativePath") == snapshot_relative
    ]
    require(len(matching_snapshots) == 1, f"{checkpoint_label} fresh active-tree UIA snapshot is missing or duplicated.")
    snapshot_record = matching_snapshots[0]
    require(
        type(snapshot_record.get("processId")) is int
        and snapshot_record.get("processId") == process_id
        and type(snapshot_record.get("windowHandle")) is int
        and snapshot_record.get("windowHandle") == main_window_handle,
        f"{checkpoint_label} UIA snapshot inventory entry is not bound to the measured process and main HWND.",
    )
    snapshot = load_json(verify_recorded_file(evidence_root, snapshot_record))
    snapshot_captured = parse_utc_timestamp(snapshot.get("capturedAtUtc"), f"{checkpoint_label} active-tree snapshot")
    recorded_snapshot_captured = parse_utc_timestamp(snapshot_record.get("capturedAtUtc"), f"{checkpoint_label} UIA snapshot inventory")
    checkpoint_snapshot_captured = parse_utc_timestamp(checkpoint.get("activeTreeCapturedAtUtc"), f"{checkpoint_label} checkpoint snapshot")
    require(
        snapshot.get("name") == Path(snapshot_relative).stem
        and snapshot_captured == recorded_snapshot_captured == checkpoint_snapshot_captured
        and snapshot_captured >= selected_readback,
        f"{checkpoint_label} does not link a fresh active-tree snapshot captured after the tab readback.",
    )
    actual_settled_milliseconds = int((snapshot_captured - action_completed) // dt.timedelta(milliseconds=1))
    require(
        snapshot_captured - action_completed >= minimum_settle
        and settled == actual_settled_milliseconds,
        f"{checkpoint_label} settledMilliseconds does not match at least 500 ms of leave-to-tree timestamp evidence.",
    )
    root = snapshot.get("root") or {}
    require(
        type(root.get("processId")) is int
        and root.get("processId") == process_id
        and type(root.get("ownerWindowHandle")) is int
        and root.get("ownerWindowHandle") == main_window_handle
        and type(root.get("ownerWindowProcessId")) is int
        and root.get("ownerWindowProcessId") == process_id
        and type(checkpoint.get("activeTreeRootProcessId")) is int
        and checkpoint.get("activeTreeRootProcessId") == process_id
        and type(checkpoint.get("activeTreeRootOwnerWindowHandle")) is int
        and checkpoint.get("activeTreeRootOwnerWindowHandle") == main_window_handle,
        f"{checkpoint_label} fresh active-tree root is not owned by the measured process and main HWND.",
    )
    descendants = snapshot.get("descendants")
    require(isinstance(descendants, list), f"{checkpoint_label} fresh active-tree snapshot has no descendant list.")
    require(
        all(type(node.get("processId")) is int and node.get("processId") == process_id for node in descendants if isinstance(node, dict))
        and all(isinstance(node, dict) for node in descendants),
        f"{checkpoint_label} fresh active-tree snapshot contains a missing or foreign-process descendant.",
    )
    visible_roster_controls = [node for node in descendants if is_roster_content_control(node)]
    require(
        not visible_roster_controls,
        f"{checkpoint_label} fresh active UIA tree still exposes roster controls, including a retained nextRosterTable.",
    )

    screenshot_relative = str(checkpoint.get("screenshotRelativePath", ""))
    screenshots = route.get("screenshots")
    require(isinstance(screenshots, list), f"{checkpoint_label} screenshot inventory is missing.")
    matching_screenshots = [
        record for record in screenshots
        if record.get("relativePath") == screenshot_relative
    ]
    require(screenshot_relative.startswith("screenshots/") and len(matching_screenshots) == 1,
            f"{checkpoint_label} measured-window screenshot is missing or duplicated.")
    screenshot_record = matching_screenshots[0]
    require(
        type(screenshot_record.get("processId")) is int
        and screenshot_record.get("processId") == process_id
        and type(screenshot_record.get("windowHandle")) is int
        and screenshot_record.get("windowHandle") == main_window_handle,
        f"{checkpoint_label} screenshot is not bound to the measured process and main HWND.",
    )
    verify_recorded_file(evidence_root, screenshot_record)
    screenshot_captured = parse_utc_timestamp(screenshot_record.get("capturedAtUtc"), f"{checkpoint_label} screenshot")
    checkpoint_screenshot_captured = parse_utc_timestamp(checkpoint.get("screenshotCapturedAtUtc"), f"{checkpoint_label} checkpoint screenshot")
    require(
        screenshot_captured == checkpoint_screenshot_captured
        and screenshot_captured >= snapshot_captured
        and screenshot_captured - action_completed >= minimum_settle,
        f"{checkpoint_label} screenshot is not linked after the fresh active-tree observation and 500 ms leave settle.",
    )

    return {
        "passed": True,
        "proofKind": P917_ROSTER_ABSENCE_PROOF_KIND,
        "selectedSection": "Class Details and Times",
        "settledMilliseconds": settled,
        "activeUiAutomationRosterTablePresent": False,
        "activeTreeRosterControlCount": 0,
        "modelDestructionClaimedByUia": False,
    }


def validate_section_suspension_checkpoint(
    evidence_root: Path, route: dict[str, Any], process_id: int, main_window_handle: int
) -> dict[str, Any]:
    model_counts = route.get("modelCounts") or {}
    require(
        model_counts.get("suspendedRosterRootRows") is None
        and model_counts.get("suspendedRosterColumnCount") is None,
        "Section-leave evidence must not claim a stale GridPattern/model row count after roster-widget destruction.",
    )
    checkpoint = route.get("sectionSuspensionCheckpoint")
    return validate_roster_absence_checkpoint(
        evidence_root, route, checkpoint, process_id, main_window_handle,
        "leave-roster-section", "Section-leave",
    )


def validate_snapshot_pair(
    receipt: dict[str, Any],
    before: dict[str, Any],
    after: dict[str, Any],
) -> dict[str, Any]:
    require(before.get("schema") == FIXTURE_SCHEMA, "Before snapshot has the wrong fixture schema.")
    require(after.get("schema") == FIXTURE_SCHEMA, "After snapshot has the wrong fixture schema.")
    require(before.get("phase") == "before", "Before snapshot phase is not before.")
    require(after.get("phase") == "after", "After snapshot phase is not after.")
    for snapshot in (before, after):
        require(snapshot.get("status") == "passed", "Workspace snapshot did not pass integrity inspection.")
        require(snapshot.get("integrityCheck") == "ok", "Workspace integrity_check did not return ok.")
        require(snapshot.get("foreignKeyErrors") == [], "Workspace foreign_key_check returned errors.")
    require(before.get("schemaObjects") == after.get("schemaObjects"), "Workspace SQLite schema objects changed.")
    require(before.get("workspaceTableCount") == after.get("workspaceTableCount"), "Workspace table count changed.")
    require(before.get("workspaceTables", {}).keys() == after.get("workspaceTables", {}).keys(), "Workspace table set changed.")
    targets = receipt.get("expectedEditTargets")
    require(isinstance(targets, list) and len(targets) == 3, "Preparation receipt must list exactly three cell targets.")
    allowed: dict[tuple[int, int, int], dict[str, Any]] = {}
    for target in targets:
        key = (int(target["classId"]), int(target["row"]), int(target["storedColumn"]))
        if key in allowed:
            raise ValueError(f"Duplicate expected edit target in preparation receipt: {key}")
        allowed[key] = target
    require(set(allowed) == {(1, 0, 6), (1, 49, 77), (1, 99, 155)}, "Edit targets differ from the pinned first/middle/last custom-column coordinates.")

    before_tables = before["workspaceTables"]
    after_tables = after["workspaceTables"]
    require("roster_data" in before_tables, "Workspace snapshots omit roster_data.")
    require("roster_columns" in before_tables, "Workspace snapshots omit roster_columns.")
    target_class_id = next(iter(allowed))[0]
    changed: set[tuple[int, int, int]] = set()
    for table_name in before_tables:
        left = before_tables[table_name]
        right = after_tables[table_name]
        require(left.get("columns") == right.get("columns"), f"Column names changed in table {table_name}.")
        require(left.get("declaration") == right.get("declaration"), f"Declared table schema changed in table {table_name}.")
        require(left.get("rowCount") == right.get("rowCount"), f"Row count changed in table {table_name}.")
        if table_name == "roster_columns":
            columns = left["columns"]
            required_columns = ("id", "class_id", "position", "name", "width")
            require(
                all(column in columns for column in required_columns),
                "Workspace roster_columns snapshot omits a required descriptor field.",
            )
            indices = {name: columns.index(name) for name in required_columns}

            def partition_roster_columns(rows: list[list[Any]]) -> tuple[list[list[Any]], list[list[Any]]]:
                target_rows: list[list[Any]] = []
                other_rows: list[list[Any]] = []
                for row in rows:
                    if int(row[indices["class_id"]]) == target_class_id:
                        target_rows.append(row)
                    else:
                        other_rows.append(row)
                return target_rows, other_rows

            before_target, before_other = partition_roster_columns(left["rows"])
            after_target, after_other = partition_roster_columns(right["rows"])
            require(before_other == after_other, "Non-target roster_columns rows changed.")

            def ordered_descriptors(rows: list[list[Any]]) -> list[tuple[Any, ...]]:
                ordered = sorted(
                    rows,
                    key=lambda row: (int(row[indices["position"]]), int(row[indices["id"]])),
                )
                return [
                    (
                        row[indices["class_id"]],
                        row[indices["position"]],
                        row[indices["name"]],
                        row[indices["width"]],
                    )
                    for row in ordered
                ]

            require(
                ordered_descriptors(before_target) == ordered_descriptors(after_target),
                "Target roster_columns descriptors changed.",
            )
            continue
        if table_name != "roster_data":
            require(left.get("rows") == right.get("rows"), f"Rows changed outside targeted roster data: {table_name}.")
            continue
        columns = left["columns"]
        indices = {name: columns.index(name) for name in ("class_id", "row_index", "col_index", "value")}
        left_rows = left["rows"]
        right_rows = right["rows"]
        require(len(left_rows) == len(right_rows), "Roster data row count changed.")
        right_by_key = {
            (int(row[indices["class_id"]]), int(row[indices["row_index"]]), int(row[indices["col_index"]])): row
            for row in right_rows
        }
        require(len(right_by_key) == len(right_rows), "After snapshot has duplicate roster_data primary keys.")
        for old_row in left_rows:
            key = (
                int(old_row[indices["class_id"]]),
                int(old_row[indices["row_index"]]),
                int(old_row[indices["col_index"]]),
            )
            require(key in right_by_key, f"Roster data row disappeared: {key}.")
            new_row = right_by_key[key]
            if old_row == new_row:
                continue
            require(key in allowed, f"Unexpected roster_data value changed at {key}.")
            for index, column_name in enumerate(columns):
                if column_name == "value":
                    continue
                require(old_row[index] == new_row[index], f"Non-value roster_data field changed at {key}, column {column_name}.")
            target = allowed[key]
            require(str(old_row[indices["value"]]) == str(target["beforeValue"]), f"Target baseline value changed from the pinned fixture at {key}.")
            require(str(new_row[indices["value"]]) == str(target["afterValue"]), f"Target persisted value differs from its deterministic edit at {key}.")
            changed.add(key)
        require(set(right_by_key) == {
            (int(row[indices["class_id"]]), int(row[indices["row_index"]]), int(row[indices["col_index"]]))
            for row in left_rows
        }, "Roster data primary key set changed.")
    require(changed == set(allowed), f"Changed target cell set differs from the required three edits: {sorted(changed)}.")
    return {
        "passed": True,
        "schemaUnchanged": True,
        "tableCount": int(before["workspaceTableCount"]),
        "unchangedTables": len(before_tables) - 1,
        "changedTable": "roster_data",
        "changedCellKeys": [list(key) for key in sorted(changed)],
        "onlyTargetCellValuesChanged": True,
        "targetRosterColumnDescriptorsUnchanged": True,
        "integrityChecksPassed": True,
        "foreignKeyChecksPassed": True,
    }


def validate_class_selection_evidence(
    evidence_root: Path,
    route: dict[str, Any],
    receipt: dict[str, Any],
    actions: list[dict[str, Any]],
    uia_snapshots: list[dict[str, Any]],
    process_id: int,
    main_window_handle: int,
) -> dict[str, Any]:
    identity = receipt.get("identity") or {}
    class_id = int(identity.get("targetClassId", -1))
    class_name = str(identity.get("targetClassName", ""))
    accessible_name = str(identity.get("targetClassListAccessibleName", ""))
    require(class_id == 1 and class_name == "Heavy Fixture Class 001", "Preparation receipt does not pin exact target class 1 identity.")
    require(bool(accessible_name), "Preparation receipt does not derive the target Classes ListItem.Name.")

    selection = route.get("targetClassSelection") or {}
    require(
        selection.get("selected") is True
        and int(selection.get("classId", -1)) == class_id
        and selection.get("className") == class_name
        and selection.get("listItemName") == accessible_name
        and selection.get("expectedAccessibleName") == accessible_name
        and "ListItem.Name" in str(selection.get("selectionProperty", "")),
        "Exact target class selection evidence is missing or does not match the fixture-derived accessible row.",
    )
    action_index = int(selection.get("actionIndex", -1))
    require(0 <= action_index < len(actions), "Target class selection action index is outside the UIA action list.")
    select_action = actions[action_index]
    select_target = select_action.get("target") or {}
    require(
        select_action.get("actionKind") == "select-target-class"
        and select_action.get("selectedClassId") == class_id
        and select_action.get("selectedClassName") == class_name
        and select_action.get("selectedListItemName") == accessible_name
        and select_action.get("selectionItemPatternIsSelected") is True
        and select_target.get("name") == accessible_name
        and select_target.get("controlType") == "ControlType.ListItem"
        and int(select_target.get("processId", -1)) == process_id
        and int(select_target.get("ownerWindowHandle", -1)) == main_window_handle,
        "Measured UIA click did not select the exact fixture-derived class ListItem.",
    )

    search = route.get("targetClassSearch") or {}
    pages = search.get("pages")
    require(
        search.get("className") == class_name
        and search.get("targetAccessibleName") == accessible_name
        and search.get("matched") is True
        and isinstance(pages, list)
        and 1 <= len(pages) <= int(search.get("maximumScrolls", -1)) + 1,
        "Bounded Classes ListItem search does not prove the target accessible name was found.",
    )
    matches = [page for page in pages if int(page.get("matchingAccessibleNameCount", 0)) == 1]
    require(len(matches) == 1, "Classes UIA search did not find exactly one visible target ListItem page.")
    matching_page = matches[0]
    page_number = int(matching_page.get("page", -1))
    require(page_number >= 0 and page_number < len(pages), "Target Classes UIA search page number is invalid.")
    snapshot_records = {str(item.get("relativePath", "")): item for item in uia_snapshots}
    relative = str(matching_page.get("snapshotRelativePath", ""))
    require(relative in snapshot_records, "Target Classes ListItem snapshot is not linked from the evidence inventory.")
    snapshot_path = verify_recorded_file(evidence_root, snapshot_records[relative])
    snapshot = load_json(snapshot_path)
    list_snapshot = snapshot.get("classList") or {}
    target_items = [item for item in snapshot.get("items", []) if item.get("name") == accessible_name]
    target_item = target_items[0] if len(target_items) == 1 else {}
    require(
        snapshot.get("itemCount", 0) >= 1
        and list_snapshot.get("controlType") == "ControlType.List"
        and int(list_snapshot.get("processId", -1)) == process_id
        and int(list_snapshot.get("ownerWindowHandle", -1)) == main_window_handle
        and len(target_items) == 1
        and target_items[0].get("controlType") == "ControlType.ListItem"
        and target_items[0].get("isEnabled") is True
        and target_items[0].get("isOffscreen") is False
        and int(target_items[0].get("processId", -1)) == process_id
        and str(target_items[0].get("automationId", "")).endswith(".classesNavigatorClassList"),
        "UIA class-list snapshot does not expose one visible, enabled, PID-bound target class row.",
    )

    def rectangle(value: Any, label: str) -> tuple[float, float, float, float]:
        require(isinstance(value, dict), f"{label} rectangle is missing.")
        left = float(value.get("x", -1))
        top = float(value.get("y", -1))
        width = float(value.get("width", -1))
        height = float(value.get("height", -1))
        require(width > 0 and height > 0, f"{label} rectangle is empty.")
        return left, top, width, height

    def same_rectangle(actual: Any, expected: tuple[float, float, float, float], label: str) -> None:
        observed = rectangle(actual, label)
        require(
            all(abs(left - right) <= 0.01 for left, right in zip(observed, expected)),
            f"{label} rectangle differs from the measured visible intersection.",
        )

    list_rectangle = rectangle(list_snapshot.get("rectangle"), "Classes List")
    item_rectangle = rectangle(target_item.get("rectangle"), "Target ListItem")
    item_left, item_top, item_width, item_height = item_rectangle
    list_left, list_top, list_width, list_height = list_rectangle
    intersection = (
        max(item_left, list_left),
        max(item_top, list_top),
        min(item_left + item_width, list_left + list_width) - max(item_left, list_left),
        min(item_top + item_height, list_top + list_height) - max(item_top, list_top),
    )
    require(intersection[2] > 0 and intersection[3] > 0, "Target class row does not intersect the visible Classes List bounds.")
    pointer = select_action.get("pointerAction") or {}
    click_region = pointer.get("clickRegion") or {}
    require(
        pointer.get("actionType") == "WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP"
        and int(pointer.get("windowHandle", -1)) == main_window_handle
        and int(pointer.get("windowProcessId", -1)) == process_id
        and int(pointer.get("targetProcessId", -1)) == process_id
        and click_region.get("strategy") == "intersection-of-target-and-visible-classes-list-bounds",
        "Target class click lacks a measured PID-bound click in the visible list-item intersection.",
    )
    require(
        click_region.get("clippingControl", {}).get("controlType") == "ControlType.List"
        and int(click_region.get("clippingControl", {}).get("processId", -1)) == process_id
        and int(click_region.get("clippingControl", {}).get("ownerWindowHandle", -1)) == main_window_handle,
        "Target class click clipping control is not the measured Classes List.",
    )
    same_rectangle(click_region.get("targetRectangle"), item_rectangle, "Target ListItem click")
    same_rectangle(click_region.get("clippingRectangle"), list_rectangle, "Classes List click clipping")
    same_rectangle(click_region.get("clickRectangle"), intersection, "Target visible click intersection")
    click_x = float(pointer.get("screenX", -1))
    click_y = float(pointer.get("screenY", -1))
    require(
        intersection[0] <= click_x < intersection[0] + intersection[2]
        and intersection[1] <= click_y < intersection[1] + intersection[3],
        "Measured target class click point is outside the visible ListItem and Classes List intersection.",
    )

    scroll_actions = [item for item in actions if item.get("actionKind") == "scroll-class-list-for-target"]
    require(len(scroll_actions) == page_number, "Classes ListItem search scroll count differs from its matched page.")
    for action in scroll_actions:
        target = action.get("target") or {}
        scroll = action.get("scrollInput") or {}
        require(
            target.get("controlType") == "ControlType.List"
            and str(target.get("automationId", "")).endswith(".classesNavigatorClassList")
            and int(target.get("processId", -1)) == process_id
            and int(target.get("ownerWindowHandle", -1)) == main_window_handle
            and int(scroll.get("windowHandle", -1)) == main_window_handle
            and int(scroll.get("expectedProcessId", -1)) == process_id
            and int(scroll.get("windowProcessId", -1)) == process_id
            and int(scroll.get("wheelDelta", 0)) == -120
            and bool(scroll.get("waitForInputIdleSucceeded", False))
            and bool(scroll.get("messageSent", False))
            and bool(scroll.get("visibleRowsChanged", False)),
            "Classes list scroll action lacks measured-process bounds, delivery, or visible-row-change evidence.",
        )
    return {
        "passed": True,
        "classId": class_id,
        "className": class_name,
        "accessibleName": accessible_name,
        "visibleMatchPage": page_number,
        "scrollActionCount": len(scroll_actions),
    }


def validate_roster_tab_selection_actions(actions: list[dict[str, Any]]) -> dict[str, Any]:
    by_kind = {str(action.get("actionKind", "")): action for action in actions}
    results: dict[str, Any] = {}
    for action_kind in ("open-roster-tab", "reenter-roster-tab"):
        action = by_kind.get(action_kind)
        require(action is not None, f"Required UIA tab action is missing: {action_kind}.")
        target = action.get("target") or {}
        require(
            target.get("controlType") == "ControlType.TabItem"
            and target.get("name") == "Roster"
            and action.get("selectedTabName") == "Roster"
            and action.get("expectedSection") == "Roster"
            and action.get("method") == "SelectionItemPattern.Select with IsSelected readback"
            and action.get("selectionItemPatternAvailable") is True
            and action.get("selectionItemPatternIsSelected") is True,
            f"UIA SelectionItemPattern does not prove the exact Roster tab selection: {action_kind}.",
        )
        results[action_kind] = {"passed": True, "selectedTabName": "Roster"}
    return results


def median(values: list[int]) -> float:
    return float(statistics.median(values))


def linear_fitted_change(values: list[float]) -> float:
    if len(values) < 2:
        return 0.0
    x_values = list(range(len(values)))
    x_mean = statistics.mean(x_values)
    y_mean = statistics.mean(values)
    denominator = sum((value - x_mean) ** 2 for value in x_values)
    slope = sum((x - x_mean) * (y - y_mean) for x, y in zip(x_values, values)) / denominator
    return slope * (len(values) - 1)


def validate_memory(memory: dict[str, Any], route: dict[str, Any], process_id: int, main_window_handle: int) -> dict[str, Any]:
    require(int(memory.get("workingSetTargetBytes", -1)) == WORKING_SET_TARGET, "Working Set target differs from the pinned 256 MiB goal.")
    require(bool(memory.get("workingSetTargetExclusive", False)), "Working Set target must be recorded as exclusive.")
    require(int(memory.get("workingSetCeilingBytes", -1)) == WORKING_SET_CEILING, "Working Set ceiling differs from the pinned 450 MiB acceptance ceiling.")
    require(memory.get("workingSetCeilingExclusive") is False, "Working Set ceiling must be inclusive.")
    windows = memory.get("windows")
    require(isinstance(windows, list) and windows, "Memory evidence contains no settled windows.")
    by_id: dict[str, dict[str, Any]] = {}
    for window in windows:
        window_id = str(window.get("windowId", ""))
        require(window_id and window_id not in by_id, f"Memory window identifier is missing or duplicated: {window_id}.")
        by_id[window_id] = window
        require(int(window.get("processId", -1)) == process_id, f"Memory window {window_id} has a different process ID.")
        require(int(window.get("mainWindowHandle", -1)) == main_window_handle, f"Memory window {window_id} has a different HWND.")
        samples = window.get("samples")
        require(isinstance(samples, list) and len(samples) >= 5, f"Memory window {window_id} contains fewer than five samples.")
        _memory_window_timestamp_bounds(window, f"Memory window {window_id}")
        interval = int(window.get("sampleIntervalMilliseconds", -1))
        require(interval > 0, f"Memory window {window_id} has no valid sample interval.")
        if window_id.startswith("plateau-baseline-") or window_id.startswith("cycle-"):
            require(len(samples) == 10 and interval == 100,
                    f"Measured P9-17 cycle window {window_id} must contain ten samples at 100 ms intervals.")
            require(int(window.get("sampleCount", -1)) == 10
                    and window.get("samplingCompleted") is True
                    and int(window.get("samplingErrorCount", -1)) == 0
                    and window.get("samplingErrors") == [],
                    f"Measured P9-17 cycle window {window_id} is incomplete or contains sampling errors.")
            require(int(window.get("settleDelaySeconds", -1)) == 2,
                    f"Measured P9-17 cycle window {window_id} lacks the required two-second settle delay.")
            require([int(sample.get("sampleIndex", -1)) for sample in samples] == list(range(10)),
                    f"Measured P9-17 cycle window {window_id} sample indexes are missing or reordered.")
            require(all(sample.get("processState") == "running" and sample.get("metricErrors") == [] for sample in samples),
                    f"Measured P9-17 cycle window {window_id} contains a non-running or incomplete process sample.")
            elapsed = [int(sample.get("elapsedMilliseconds", -1)) for sample in samples]
            require(all(value >= 0 for value in elapsed) and elapsed == sorted(elapsed)
                    and len(set(elapsed)) == 10,
                    f"Measured P9-17 cycle window {window_id} lacks strictly ordered sample timestamps.")
            require(750 <= elapsed[-1] - elapsed[0] <= 2_000
                    and all(35 <= current - previous <= 350 for previous, current in zip(elapsed, elapsed[1:])),
                    f"Measured P9-17 cycle window {window_id} samples do not span the bounded 100 ms schedule.")
        target_exceeded: list[int] = []
        ceiling_exceeded: list[int] = []
        for sample_index, sample in enumerate(samples):
            working_set = int(sample.get("workingSetBytes", -1))
            require(working_set >= 0, f"Memory window {window_id} contains a missing Working Set sample.")
            require(working_set <= WORKING_SET_CEILING, f"Settled Working Set exceeded the 450 MiB ceiling in {window_id}: {working_set}.")
            if working_set >= WORKING_SET_TARGET:
                target_exceeded.append(sample_index)
            if working_set > WORKING_SET_CEILING:
                ceiling_exceeded.append(sample_index)
            for diagnostic in ("privateBytes", "peakWorkingSetBytes", "handles", "threadCount"):
                require(diagnostic in sample and int(sample[diagnostic]) >= 0, f"Memory window {window_id} lacks diagnostic {diagnostic} evidence.")
        require(int(window.get("workingSetTargetBytes", -1)) == WORKING_SET_TARGET and bool(window.get("workingSetTargetExclusive", False)), f"Memory window {window_id} has different target metadata.")
        require(int(window.get("workingSetCeilingBytes", -1)) == WORKING_SET_CEILING and window.get("workingSetCeilingExclusive") is False, f"Memory window {window_id} has different ceiling metadata.")
        require(window.get("workingSetTargetExceededSampleIndexes") == target_exceeded, f"Memory window {window_id} target-exceedance indexes differ from its samples.")
        require(window.get("workingSetCeilingExceededSampleIndexes") == ceiling_exceeded, f"Memory window {window_id} ceiling-exceedance indexes differ from its samples.")
        require(bool(window.get("workingSetTargetMet", False)) == (not target_exceeded), f"Memory window {window_id} target status differs from its samples.")
        require(bool(window.get("workingSetCeilingPassed", False)) == (not ceiling_exceeded), f"Memory window {window_id} ceiling status differs from its samples.")
    required = {
        "startup-ready", "workspace-entry", "class-page-entry", "roster-entry",
        "post-edit", "post-save", "matched-baseline", "matched-after-suspension",
        "page-suspended", "re-entry",
    }
    require(required.issubset(by_id), f"Required normal-use memory checkpoints are missing: {sorted(required - set(by_id))}.")
    baseline = by_id["matched-baseline"]
    returned = by_id["matched-after-suspension"]
    reentry = by_id["re-entry"]
    require(baseline.get("matchedState") == returned.get("matchedState"), "Baseline and post-suspension sample windows are not matched to the same UI state.")
    base_samples = baseline["samples"]
    return_samples = returned["samples"]
    require(len(base_samples) >= 10 and len(return_samples) >= 10, "Matched baseline/return windows require at least ten samples each.")
    require(len(reentry.get("samples", [])) >= 10, "Re-entry memory window requires at least ten samples.")
    require(len(base_samples) == len(return_samples), "Matched baseline and return windows have different sample counts.")
    require(int(baseline["sampleIntervalMilliseconds"]) == int(returned["sampleIntervalMilliseconds"]), "Matched baseline and return windows have different sample intervals.")
    baseline_values = [int(sample["workingSetBytes"]) for sample in base_samples]
    return_values = [int(sample["workingSetBytes"]) for sample in return_samples]
    baseline_range = max(baseline_values) - min(baseline_values)
    baseline_median = median(baseline_values)
    return_median = median(return_values)
    initial_allowance_ceiling = baseline_median + baseline_range
    initial_allowance = memory.get("initialPrimingReturnAllowance")
    require(isinstance(initial_allowance, dict), "Memory evidence omits the separately reported cold-baseline priming uplift comparison.")
    require(initial_allowance.get("gating") is True,
            "Cold-baseline-to-first-return cleanup comparison must be recorded as a required gate.")
    require(int(initial_allowance.get("observedBaselineRangeBytes", -1)) == baseline_range, "Reported priming comparison differs from the cold baseline window range.")
    require(float(initial_allowance.get("baselineMedianWorkingSetBytes", -1)) == baseline_median, "Reported priming comparison baseline median differs from its samples.")
    require(float(initial_allowance.get("postSuspensionMedianWorkingSetBytes", -1)) == return_median, "Reported priming comparison return median differs from its samples.")
    require(float(initial_allowance.get("maximumAllowedPostSuspensionMedianBytes", -1)) == initial_allowance_ceiling, "Reported priming comparison ceiling differs from its baseline-derived value.")
    initial_allowance_passed = return_median <= initial_allowance_ceiling
    require(bool(initial_allowance.get("passed", False)) == initial_allowance_passed, "Reported initial priming comparison status differs from the samples.")

    measured_cycle_count = int(route.get("measuredCycleCount", 0))
    baseline_window_count = int(route.get("baselineWindowsPerState", 0))
    priming_cycle_count = int(route.get("primingCycleCount", 0))
    require(priming_cycle_count == 1 and measured_cycle_count == 10 and baseline_window_count == 5,
            "P9-17 memory plan must declare one priming cycle, five post-priming baseline windows, and ten measured cycles.")
    cycle_schema = "classmngr-phase9-p9-17-repeated-roster-cycle-v1"
    require(route.get("cycleEvidenceSchema") == cycle_schema
            and bool(route.get("repeatedCycleProtocol")),
            "P9-17 repeated-cycle schema and protocol description are missing.")
    baseline_ids = memory.get("plateauBaselineWindowIds")
    cycle_window_ids = memory.get("plateauCycleWindowIds")
    require(isinstance(baseline_ids, list) and len(baseline_ids) == 5,
            "P9-17 memory evidence must identify five post-priming baseline windows.")
    expected_baseline_ids = [f"plateau-baseline-{index:02d}" for index in range(1, 6)]
    require(baseline_ids == expected_baseline_ids, "Post-priming baseline window IDs are missing, duplicated, or out of order.")
    expected_cycle_ids = []
    for cycle_number in range(1, measured_cycle_count + 1):
        tag = f"{cycle_number:02d}"
        expected_cycle_ids.extend((f"cycle-{tag}-roster-loaded", f"cycle-{tag}-post-save", f"cycle-{tag}-return"))
    expected_cycle_ids.append("cycle-final-reentry")
    require(cycle_window_ids == expected_cycle_ids, "P9-17 measured cycle window IDs are missing, duplicated, or out of order.")
    require(set(baseline_ids + cycle_window_ids).issubset(by_id), "P9-17 cycle memory windows are absent from the sample record.")
    chronology_summary = validate_cleanup_memory_window_chronology(memory, route, by_id)

    plateau_state = str(baseline.get("matchedState"))
    require(plateau_state == "classes-details-selected-class", "Cold baseline does not identify the pinned Classes detail state.")
    plateau_baselines = [by_id[window_id] for window_id in baseline_ids]
    require(all(window.get("matchedState") == plateau_state for window in plateau_baselines),
            "Post-priming no-action baselines do not match the cold baseline UI state.")
    require(all(window.get("phase") == "baseline" and len(window.get("samples", [])) == 10 for window in plateau_baselines),
            "Every post-priming no-action baseline must have ten settled samples.")
    require(all(int(window.get("sampleIntervalMilliseconds", -1)) == 100 for window in plateau_baselines),
            "Post-priming baseline windows must use 100 ms sample intervals.")
    plateau_medians = [median([int(sample["workingSetBytes"]) for sample in window["samples"]]) for window in plateau_baselines]
    plateau_center = median([int(value) for value in plateau_medians])
    plateau_envelope = max(abs(value - plateau_center) for value in plateau_medians)
    plateau_drift = linear_fitted_change(plateau_medians)
    plateau_baseline_stable = plateau_center > 0 and plateau_envelope < plateau_center
    plateau_drift_within_envelope = abs(plateau_drift) <= plateau_envelope
    plateau_upper_bound = plateau_center + plateau_envelope

    cycle_records = route.get("cycleRecords")
    require(isinstance(cycle_records, list) and len(cycle_records) == measured_cycle_count,
            "Route evidence must contain exactly ten complete measured roster cycle records.")
    cycle_return_medians: list[float] = []
    cycle_stage_summaries: list[dict[str, Any]] = []
    for cycle_index, record in enumerate(cycle_records, start=1):
        tag = f"{cycle_index:02d}"
        require(record.get("schema") == cycle_schema and int(record.get("cycleNumber", -1)) == cycle_index,
                "Measured roster cycle schema, sequence, or count is invalid.")
        require(record.get("rosterLoadedWindowId") == f"cycle-{tag}-roster-loaded"
                and record.get("postSaveWindowId") == f"cycle-{tag}-post-save"
                and record.get("postSuspensionWindowId") == f"cycle-{tag}-return",
                f"Cycle {cycle_index} memory checkpoints do not link to its UIA cycle record.")
        require(int(record.get("gridRows", -1)) == 100 and int(record.get("gridColumns", -1)) == 156
                and int(record.get("verifiedHeaderCount", -1)) == 156,
                f"Cycle {cycle_index} does not verify the full 100x156 roster grid and all 156 headers.")
        checkpoint = record.get("suspensionCheckpoint") or {}
        require(checkpoint.get("selectedSection") == "Class Details and Times"
                and checkpoint.get("selectedTabVerified") is True
                and checkpoint.get("activeUiAutomationRosterTablePresent") is False
                and checkpoint.get("freshActiveTreeCaptured") is True
                and checkpoint.get("proofKind") == P917_ROSTER_ABSENCE_PROOF_KIND
                and type(checkpoint.get("activeTreeRosterControlCount")) is int
                and checkpoint.get("activeTreeRosterControlCount") == 0,
                f"Cycle {cycle_index} does not verify fresh active-tree absence of roster controls after the Details section is selected.")
        for phase_key in ("initialReadbacks", "editedReadbacks", "savedReadbacks"):
            readbacks = record.get(phase_key)
            require(isinstance(readbacks, list) and len(readbacks) == 3
                    and all(bool(item.get("matched", False)) for item in readbacks),
                    f"Cycle {cycle_index} target UIA readbacks are incomplete at {phase_key}.")
        for window_id in (record["rosterLoadedWindowId"], record["postSaveWindowId"], record["postSuspensionWindowId"]):
            window = by_id[str(window_id)]
            require(len(window.get("samples", [])) == 10 and int(window.get("sampleIntervalMilliseconds", -1)) == 100,
                    f"Cycle {cycle_index} settled memory window {window_id} must contain ten 100 ms samples.")
            require(int(window.get("cycleNumber", -1)) == cycle_index,
                    f"Cycle {cycle_index} settled memory window {window_id} has a different cycle number.")
        require(by_id[str(record["rosterLoadedWindowId"])].get("phase") == "cycle-loaded"
                and by_id[str(record["postSaveWindowId"])].get("phase") == "cycle-post-save",
                f"Cycle {cycle_index} roster-loaded or post-save memory window has the wrong phase.")
        return_window = by_id[str(record["postSuspensionWindowId"])]
        require(return_window.get("phase") == "cycle"
                and int(return_window.get("cycleNumber", -1)) == cycle_index
                and return_window.get("matchedState") == plateau_state,
                f"Cycle {cycle_index} return window is not bound to its same-state cycle and PID/HWND.")
        cycle_return_medians.append(median([int(sample["workingSetBytes"]) for sample in return_window["samples"]]))
        stages = {}
        for stage_key, window_id in (("rosterLoaded", record["rosterLoadedWindowId"]),
                                     ("postSave", record["postSaveWindowId"]),
                                     ("postSuspension", record["postSuspensionWindowId"])):
            stage_window = by_id[str(window_id)]
            stages[stage_key] = {
                "workingSetMedianBytes": median([int(sample["workingSetBytes"]) for sample in stage_window["samples"]]),
                "privateBytesMedian": median([int(sample["privateBytes"]) for sample in stage_window["samples"]]),
            }
        cycle_stage_summaries.append({"cycleNumber": cycle_index, **stages})

    returns_above_envelope = [
        {"cycleNumber": index, "medianWorkingSetBytes": value, "upperBoundBytes": plateau_upper_bound}
        for index, value in enumerate(cycle_return_medians, start=1)
        if value > plateau_upper_bound
    ]
    cycle_trend_change = linear_fitted_change(cycle_return_medians)
    cycle_returns_within_envelope = not returns_above_envelope
    cycle_trend_within_envelope = cycle_trend_change <= plateau_envelope
    plateau_failure_reasons = []
    if not plateau_baseline_stable:
        plateau_failure_reasons.append(
            "Post-priming Working Set baseline is unstable/inconclusive because its noise envelope is at least its median."
        )
    if not plateau_drift_within_envelope:
        plateau_failure_reasons.append(
            "Post-priming Working Set baseline is unstable/inconclusive because it has directional drift beyond its envelope."
        )
    if not cycle_returns_within_envelope:
        plateau_failure_reasons.append(
            f"Post-suspension returns exceed the stable post-priming baseline envelope: {returns_above_envelope}."
        )
    if not cycle_trend_within_envelope:
        plateau_failure_reasons.append(
            f"Measured post-suspension Working Set trend {cycle_trend_change} exceeds the baseline noise envelope {plateau_envelope}."
        )
    plateau_passed = not plateau_failure_reasons

    final_cycle_readbacks = route.get("finalCycleReadback")
    require(isinstance(final_cycle_readbacks, list) and len(final_cycle_readbacks) == 3
            and all(bool(item.get("matched", False)) for item in final_cycle_readbacks),
            "Final re-entry after the measured cycles did not verify all three persisted after-values.")
    final_window = by_id["cycle-final-reentry"]
    require(len(final_window.get("samples", [])) == 10 and all(
        int(sample.get("workingSetBytes", -1)) >= 0 for sample in final_window["samples"]
    ), "Final post-cycle re-entry lacks ten valid settled samples.")
    require(int(final_window.get("settleDelaySeconds", -1)) == 2,
            "Final post-cycle re-entry window lacks the required two-second settle delay.")

    private_baseline_medians = [median([int(sample["privateBytes"]) for sample in window["samples"]]) for window in plateau_baselines]
    private_center = median([int(value) for value in private_baseline_medians])
    private_envelope = max(abs(value - private_center) for value in private_baseline_medians)
    private_return_medians = [median([int(sample["privateBytes"]) for sample in by_id[f"cycle-{index:02d}-return"]["samples"]]) for index in range(1, measured_cycle_count + 1)]
    private_trend = linear_fitted_change(private_return_medians)
    operation_peak = memory.get("operationPeakWorkingSetBytes")
    require(operation_peak is not None and int(operation_peak) >= 0, "Operation peak Working Set diagnostic is missing.")
    require(int(memory.get("operationPeakAdvisoryLimitBytes", -1)) == OPERATION_PEAK_ADVISORY_LIMIT, "Operation peak advisory threshold differs from the pinned value.")
    cleanup_gate_results = {
        "coldBaselineToFirstReturn": {
            "passed": initial_allowance_passed,
            "failureReasons": [] if initial_allowance_passed else [
                f"Cold-baseline-to-first-return cleanup median {return_median} exceeds its empirical allowance {initial_allowance_ceiling}."
            ],
        },
        "postPrimingPlateauAndCycleReturns": {
            "passed": plateau_passed,
            "failureReasons": plateau_failure_reasons,
        },
    }
    return {
        "passed": initial_allowance_passed and plateau_passed,
        "cleanupGateResults": cleanup_gate_results,
        "cleanupWindowChronology": chronology_summary,
        "workingSetTargetBytes": WORKING_SET_TARGET,
        "workingSetTargetExclusive": True,
        "workingSetCeilingBytes": WORKING_SET_CEILING,
        "workingSetCeilingExclusive": False,
        "allWindowsMetTarget": all(bool(window.get("workingSetTargetMet", False)) for window in windows),
        "windowsAboveTarget": [str(window["windowId"]) for window in windows if not bool(window.get("workingSetTargetMet", False))],
        "windowCount": len(windows),
        "samplesPerWindow": {str(window["windowId"]): len(window["samples"]) for window in windows},
        "initialPrimingReturnAllowance": {
            "gating": True,
            "recordedGating": initial_allowance["gating"],
            "passed": initial_allowance_passed,
            "failureReasons": cleanup_gate_results["coldBaselineToFirstReturn"]["failureReasons"],
            "coldBaselineMedianWorkingSetBytes": baseline_median,
            "initialPostSuspensionMedianWorkingSetBytes": return_median,
            "initialUpliftBytes": return_median - baseline_median,
            "coldBaselineRangeBytes": baseline_range,
            "maximumAllowedColdReturnBytes": initial_allowance_ceiling,
        },
        "plateauCycleAnalysis": {
            "passed": plateau_passed,
            "failureReasons": plateau_failure_reasons,
            "baselineWindowIds": baseline_ids,
            "baselineWindowMediansWorkingSetBytes": plateau_medians,
            "baselineMedianWorkingSetBytes": plateau_center,
            "baselineNoiseEnvelopeBytes": plateau_envelope,
            "baselineFittedDriftBytes": plateau_drift,
            "maximumAllowedReturnMedianBytes": plateau_upper_bound,
            "cycleReturnMediansWorkingSetBytes": cycle_return_medians,
            "cycleReturnExceedances": returns_above_envelope,
            "cycleFittedChangeBytes": cycle_trend_change,
            "cycleTrendWithinBaselineEnvelope": cycle_trend_within_envelope,
            "cycleStageMedians": cycle_stage_summaries,
            "privateBytesDiagnostic": {
                "baselineMedian": private_center,
                "baselineNoiseEnvelope": private_envelope,
                "cycleReturnMedians": private_return_medians,
                "fittedChangeDiagnosticOnly": private_trend,
            },
        },
        "matchedBaselineSampleCount": len(base_samples),
        "matchedInitialReturnSampleCount": len(return_samples),
        "postPrimingBaselineWindowCount": len(plateau_baselines),
        "measuredCycleCount": measured_cycle_count,
        "plateauBaselineMedianWorkingSetBytes": plateau_center,
        "plateauBaselineNoiseEnvelopeBytes": plateau_envelope,
        "maximumAllowedCycleReturnMedianBytes": plateau_upper_bound,
        "measuredCycleReturnMediansWorkingSetBytes": cycle_return_medians,
        "initialPostSuspensionMedianWorkingSetBytes": return_median,
        "initialMaximumAllowedReturnBytes": initial_allowance_ceiling,
        "operationPeakWorkingSetBytes": int(operation_peak),
        "operationPeakAdvisoryLimitBytes": OPERATION_PEAK_ADVISORY_LIMIT,
        "operationPeakAdvisoryExceeded": int(operation_peak) >= OPERATION_PEAK_ADVISORY_LIMIT,
        "privateBytesAndOtherMetricsDiagnosticOnly": True,
    }


def require_memory_cleanup_gates_passed(memory_summary: dict[str, Any]) -> None:
    gates = memory_summary.get("cleanupGateResults") or {}
    cold = gates.get("coldBaselineToFirstReturn") or {}
    plateau = gates.get("postPrimingPlateauAndCycleReturns") or {}
    cold_passed = cold.get("passed") is True
    plateau_passed = plateau.get("passed") is True
    if cold_passed and plateau_passed:
        return
    cold_reasons = "; ".join(str(reason) for reason in cold.get("failureReasons", []))
    plateau_reasons = "; ".join(str(reason) for reason in plateau.get("failureReasons", []))
    raise ValueError(
        "Required memory cleanup gates failed: "
        f"coldBaselineToFirstReturn.passed={cold_passed} ({cold_reasons}); "
        f"postPrimingPlateauAndCycleReturns.passed={plateau_passed} ({plateau_reasons})."
    )


def verify_source_inventory(evidence_root: Path, record: dict[str, Any], repository_root: Path) -> dict[str, Any]:
    path = verify_recorded_file(evidence_root, record)
    inventory = load_json(path)
    files = inventory.get("files")
    require(isinstance(files, list) and int(inventory.get("fileCount", -1)) == len(files), f"Source inventory file count differs: {path.name}.")
    require(canonical_inventory_fingerprint(files) == str(inventory.get("sha256", "")), f"Source inventory fingerprint is invalid: {path.name}.")
    require(str(inventory.get("repositoryRoot", "")).casefold() == str(repository_root).casefold(), f"Source inventory repository root differs: {path.name}.")
    for row in files:
        source_path = (repository_root / str(row["path"])).resolve(strict=True)
        require(repository_root.resolve() in source_path.parents or source_path == repository_root.resolve(), f"Source inventory path escapes the repository: {row['path']}.")
        require(source_path.stat().st_size == int(row["sizeBytes"]), f"Source size changed after capture: {row['path']}.")
        require(sha256_file(source_path) == str(row["sha256"]), f"Source changed after inventory capture: {row['path']}.")
    return {"path": record["relativePath"], "sha256": inventory["sha256"], "fileCount": len(files)}


def _directory_inventory(root: Path) -> tuple[str, list[dict[str, Any]]]:
    root = root.resolve(strict=True)
    rows = [
        {
            "path": path.relative_to(root).as_posix(),
            "sizeBytes": path.stat().st_size,
            "sha256": sha256_file(path),
        }
        for path in root.rglob("*")
        if path.is_file() and not (SOURCE_CACHE_DIRECTORIES.intersection(path.parts) or path.suffix.lower() in {".pyc", ".pyo"})
    ]
    rows.sort(key=lambda row: str(row["path"]).casefold())
    return canonical_inventory_fingerprint(rows), rows


def validate_exact_build_exit_code(build: Any) -> int:
    require(isinstance(build, dict), "Release receipt build result is missing.")
    exit_code = build.get("exitCode")
    require(type(exit_code) is int and exit_code == 0,
            "Release receipt build exitCode must be the exact JSON integer 0.")
    return exit_code


def validate_receipt_source_file_count(receipt: dict[str, Any], build_source: dict[str, Any]) -> int:
    build_rows = build_source.get("files")
    require(isinstance(build_rows, list), "Build source inventory file rows are missing.")
    inventory_count = build_source.get("fileCount")
    require(type(inventory_count) is int and inventory_count == len(build_rows),
            "Build source inventory fileCount must be the exact integer row count.")
    source_file_count = receipt.get("sourceFileCount")
    require(type(source_file_count) is int and source_file_count == len(build_rows),
            "Release receipt sourceFileCount must be the exact integer build inventory row count.")
    return source_file_count


def _normalized_windows_path(value: Any) -> str:
    normalized = str(value or "").replace("/", "\\")
    while "\\\\" in normalized:
        normalized = normalized.replace("\\\\", "\\")
    return normalized.rstrip("\\").casefold()


def read_cmake_release_toolchain(cache_path: Path) -> dict[str, str]:
    expected_keys = (
        "CMAKE_GENERATOR",
        "CMAKE_GENERATOR_PLATFORM",
        "CMAKE_GENERATOR_TOOLSET",
        "CMAKE_GENERATOR_INSTANCE",
    )
    lines = cache_path.read_text(encoding="utf-8", errors="replace").splitlines()
    values: dict[str, str] = {}
    for key in expected_keys:
        matches = [line.split("=", 1)[1] for line in lines if line.startswith(key + ":") and "=" in line]
        require(len(matches) == 1, f"Current CMake cache must contain exactly one {key} entry.")
        values[key] = matches[0]
    return values


def validate_exact_release_build_toolchain(
    receipt: dict[str, Any], cache_values: dict[str, str]
) -> dict[str, Any]:
    require(receipt.get("schema") == P917_BUILD_RECEIPT_SCHEMA,
            "Release build receipt is not the acceptance-grade P9-17 version-3 schema.")
    require(receipt.get("generator") == P917_RELEASE_GENERATOR
            and receipt.get("generatorPlatform") == P917_RELEASE_PLATFORM
            and receipt.get("generatorToolset") == P917_RELEASE_TOOLSET,
            "Release build receipt does not prove the required Visual Studio 17 2022 x64 v143 toolchain.")
    require(_normalized_windows_path(receipt.get("generatorInstance"))
            == _normalized_windows_path(P917_RELEASE_GENERATOR_INSTANCE),
            "Release build receipt does not identify the installed VS2022 Build Tools instance.")
    require(receipt.get("windowsSdkVersion") == P917_RELEASE_WINDOWS_SDK_VERSION,
            "Release build receipt does not prove the required Windows 10.0.26100.0 SDK.")

    compiler = receipt.get("compiler")
    require(isinstance(compiler, dict), "Release build receipt omits its compiler identity.")
    require(_normalized_windows_path(compiler.get("path"))
            == _normalized_windows_path(P917_RELEASE_COMPILER_PATH)
            and compiler.get("fileVersion") == P917_RELEASE_COMPILER_VERSION
            and compiler.get("toolsetVersion") == P917_RELEASE_TOOLSET_VERSION
            and compiler.get("windowsSdkVersion") == P917_RELEASE_WINDOWS_SDK_VERSION,
            "Release build receipt compiler identity or version does not match VS2022/v143.")
    require(isinstance(compiler.get("productVersion"), str) and bool(compiler["productVersion"].strip()),
            "Release build receipt omits the compiler product version.")
    compiler_sha = compiler.get("sha256")
    require(isinstance(compiler_sha, str) and re.fullmatch(r"[0-9a-fA-F]{64}", compiler_sha) is not None,
            "Release build receipt compiler SHA-256 is missing or malformed.")
    require(_normalized_windows_path(compiler.get("windowsSdkIncludeRoot"))
            == _normalized_windows_path(P917_RELEASE_WINDOWS_SDK_INCLUDE_ROOT),
            "Release build receipt Windows SDK installation path differs from the required SDK.")

    generated = receipt.get("generatedProjectToolchain")
    require(isinstance(generated, dict)
            and generated.get("platformToolset") == P917_RELEASE_TOOLSET
            and generated.get("windowsSdkVersion") == P917_RELEASE_WINDOWS_SDK_VERSION
            and type(generated.get("projectFileCount")) is int
            and generated["projectFileCount"] > 0,
            "Release build receipt does not prove generated projects selected v143 and the required Windows SDK.")
    build = receipt.get("build")
    require(isinstance(build, dict)
            and _normalized_windows_path(build.get("actualCompilerPath"))
            == _normalized_windows_path(P917_RELEASE_COMPILER_PATH)
            and build.get("compilerObservedInLog") is True,
            "Release build receipt does not prove the verbose build invoked the expected v143 x64 compiler.")

    expected_cache = {
        "CMAKE_GENERATOR": P917_RELEASE_GENERATOR,
        "CMAKE_GENERATOR_PLATFORM": P917_RELEASE_PLATFORM,
        "CMAKE_GENERATOR_TOOLSET": P917_RELEASE_TOOLSET,
        "CMAKE_GENERATOR_INSTANCE": P917_RELEASE_GENERATOR_INSTANCE,
    }
    for key, expected in expected_cache.items():
        actual = cache_values.get(key)
        if key == "CMAKE_GENERATOR_INSTANCE":
            matches = _normalized_windows_path(actual) == _normalized_windows_path(expected)
        else:
            matches = actual == expected
        require(matches, f"Current CMake cache {key} does not select the required VS2022/v143 toolchain.")
    for key, receipt_key in (
        ("CMAKE_GENERATOR", "generator"),
        ("CMAKE_GENERATOR_PLATFORM", "generatorPlatform"),
        ("CMAKE_GENERATOR_TOOLSET", "generatorToolset"),
        ("CMAKE_GENERATOR_INSTANCE", "generatorInstance"),
    ):
        if key == "CMAKE_GENERATOR_INSTANCE":
            matches = _normalized_windows_path(cache_values[key]) == _normalized_windows_path(receipt[receipt_key])
        else:
            matches = cache_values[key] == receipt[receipt_key]
        require(matches, f"Current CMake cache {key} differs from the exact-build receipt.")
    return {
        "generator": P917_RELEASE_GENERATOR,
        "platform": P917_RELEASE_PLATFORM,
        "toolset": P917_RELEASE_TOOLSET,
        "generatorInstance": receipt["generatorInstance"],
        "compilerPath": compiler["path"],
        "compilerFileVersion": compiler["fileVersion"],
        "compilerToolsetVersion": compiler["toolsetVersion"],
        "windowsSdkVersion": P917_RELEASE_WINDOWS_SDK_VERSION,
    }


def validate_windows_execution_context(context: Any, label: str) -> dict[str, Any]:
    require(isinstance(context, dict), f"{label} Windows execution-context record is missing.")
    identity = context.get("identity")
    user_sid = context.get("userSid")
    require(isinstance(identity, str) and bool(identity.strip()), f"{label} execution context omits the observed Windows identity.")
    sid_parts = user_sid.split("-") if isinstance(user_sid, str) else []
    valid_user_sid = (
        4 <= len(sid_parts) <= 18
        and sid_parts[:3] == ["S", "1", "5"]
        and all(1 <= len(part) <= 10 and part.isascii() and part.isdecimal()
                and (part == "0" or not part.startswith("0")) for part in sid_parts[3:])
        and all(int(part) <= 0xFFFFFFFF for part in sid_parts[3:])
    )
    require(valid_user_sid, f"{label} execution context omits a structurally valid Windows user SID.")
    for field in ("isElevated", "isAdministratorRole", "userInteractive"):
        require(type(context.get(field)) is bool, f"{label} execution context {field} must be an observed JSON Boolean.")
    require(context["isElevated"] is False, f"{label} execution context was elevated.")
    require(context["isAdministratorRole"] is False, f"{label} execution context has the Administrator role.")
    require(context.get("integrityLevel") == "Medium", f"{label} execution context is not Medium integrity.")
    require(type(context.get("integrityLevelRid")) is int and context["integrityLevelRid"] == WINDOWS_MEDIUM_INTEGRITY_RID,
            f"{label} execution context does not contain the actual Medium integrity RID.")
    require(context["userInteractive"] is True, f"{label} execution context is not UserInteractive.")
    for field in ("processSessionId", "activeConsoleSessionId"):
        require(type(context.get(field)) is int and context[field] > 0, f"{label} execution context {field} must be a positive observed integer.")
    require(context["processSessionId"] == context["activeConsoleSessionId"],
            f"{label} process session differs from the active local console session.")
    observed_at = context.get("observedAtUtc")
    require(isinstance(observed_at, str) and bool(observed_at.strip()),
            f"{label} execution context omits its observation timestamp.")
    try:
        parsed_observation = dt.datetime.fromisoformat(observed_at.replace("Z", "+00:00"))
    except ValueError:
        parsed_observation = None
    require(parsed_observation is not None and parsed_observation.tzinfo is not None
            and parsed_observation.utcoffset() is not None,
            f"{label} execution context observation timestamp is not timezone-aware ISO-8601.")
    return context


def validate_build_execution_context(context: Any) -> dict[str, Any]:
    require(isinstance(context, dict), "Version-2 Release receipt omits build execution-context evidence.")
    start = validate_windows_execution_context(context.get("start"), "Build-start")
    completion = validate_windows_execution_context(context.get("completion"), "Build-completion")
    require(type(context.get("sameIdentityAndConsoleSession")) is bool and context["sameIdentityAndConsoleSession"] is True,
            "Release receipt does not verify stable build identity and console session.")
    stable_fields = (
        "identity", "userSid", "isElevated", "isAdministratorRole", "integrityLevelRid",
        "integrityLevel", "userInteractive", "processSessionId", "activeConsoleSessionId",
    )
    require(all(start[field] == completion[field] for field in stable_fields),
            "Release build execution context changed between its start and completion observations.")
    return {"start": start, "completion": completion, "sameIdentityAndConsoleSession": True}


def build_execution_contexts_match(left: Any, right: Any) -> bool:
    left_context = validate_build_execution_context(left)
    right_context = validate_build_execution_context(right)
    if left_context.keys() != right_context.keys():
        return False
    for field in left_context:
        left_value = left_context[field]
        right_value = right_context[field]
        if field not in ("start", "completion"):
            if left_value != right_value:
                return False
            continue
        if left_value.keys() != right_value.keys():
            return False
        for observation_field in left_value:
            left_observation = left_value[observation_field]
            right_observation = right_value[observation_field]
            if observation_field == "observedAtUtc":
                left_instant = parse_utc_timestamp_ticks(left_observation, f"Build {field} observation")
                right_instant = parse_utc_timestamp_ticks(right_observation, f"Receipt {field} observation")
                if left_instant != right_instant:
                    return False
            elif left_observation != right_observation:
                return False
    return True


def validate_package_execution_context(
    package: dict[str, Any], process: dict[str, Any], receipt_context: dict[str, Any] | None = None
) -> dict[str, Any]:
    context = package.get("executionContext")
    require(isinstance(context, dict), "Package provenance omits build and observer execution-context evidence.")
    build = validate_build_execution_context(context.get("build"))
    if receipt_context is not None:
        require(build_execution_contexts_match(context.get("build"), receipt_context),
                "Package build execution context differs from the source-bound receipt.")
    observer = validate_windows_execution_context(context.get("observer"), "Observer")
    completion = build["completion"]
    require(observer["userSid"] == completion["userSid"], "Build and observer Windows user SIDs differ.")
    require(observer["processSessionId"] == completion["processSessionId"]
            and observer["activeConsoleSessionId"] == completion["activeConsoleSessionId"],
            "Build and observer are not in the same active console session.")

    app_context = process.get("executionContext")
    require(isinstance(app_context, dict), "Measured app process omits build/observer session evidence.")
    require(type(app_context.get("sharedSessionVerified")) is bool and app_context["sharedSessionVerified"] is True,
            "Measured app process session was not verified against build and observer sessions.")
    require(type(process.get("sessionId")) is int and process["sessionId"] > 0,
            "Measured app process omits its observed Windows session ID.")
    session_id = process["sessionId"]
    for label, observed in (
        ("build", completion["processSessionId"]),
        ("observer", observer["processSessionId"]),
        ("app context", app_context.get("measuredProcessSessionId")),
    ):
        require(type(observed) is int and observed == session_id,
                f"Measured app process session differs from the {label} session.")
    for field in ("buildProcessSessionId", "observerProcessSessionId", "activeConsoleSessionId"):
        require(type(app_context.get(field)) is int, f"Measured app execution context {field} must be an observed integer.")
    require(app_context["buildProcessSessionId"] == completion["processSessionId"]
            and app_context["observerProcessSessionId"] == observer["processSessionId"],
            "Measured app session record differs from the build or observer session IDs.")
    require(app_context.get("buildUserSid") == completion["userSid"]
            and app_context.get("observerUserSid") == observer["userSid"],
            "Measured app session record differs from the build or observer user SIDs.")
    require(app_context.get("buildIdentity") == completion["identity"]
            and app_context.get("observerIdentity") == observer["identity"],
            "Measured app session record differs from the build or observer identities.")
    require(app_context["activeConsoleSessionId"] == observer["activeConsoleSessionId"] == session_id,
            "Measured app was not verified in the active local console session.")
    return {
        "buildIdentity": completion["identity"],
        "buildUserSid": completion["userSid"],
        "observerIdentity": observer["identity"],
        "observerUserSid": observer["userSid"],
        "buildSessionId": completion["processSessionId"],
        "observerSessionId": observer["processSessionId"],
        "measuredAppSessionId": session_id,
        "activeConsoleSessionId": observer["activeConsoleSessionId"],
        "passed": True,
    }


def verify_exact_current_source_build(
    package: dict[str, Any], evidence_root: Path, repository_root: Path
) -> dict[str, Any]:
    receipt_path = Path(str(package.get("releaseBuildReceiptPath", ""))).resolve(strict=True)
    allowed_root = (repository_root / "artifacts/phase9/p9-17-exact-release-builds").resolve(strict=True)
    require(allowed_root in receipt_path.parents, "Release build receipt is outside the P9-17 exact-build evidence root.")
    receipt_sha = sha256_file(receipt_path)
    require(receipt_sha == str(package.get("releaseBuildReceiptSha256", "")), "Release build receipt hash differs from package provenance.")
    require(Path(str(package.get("acceptedReleaseReceiptPath", ""))).resolve(strict=True) == receipt_path, "Package references a different Release build receipt.")
    require(str(package.get("acceptedReleaseReceiptSha256", "")) == receipt_sha, "Accepted Release receipt hash differs from current build receipt.")
    receipt = load_json(receipt_path)
    require(receipt.get("schema") == P917_BUILD_RECEIPT_SCHEMA and receipt.get("status") == "succeeded", "Release build receipt schema or status differs.")
    require(Path(str(receipt.get("repositoryRoot", ""))).resolve() == repository_root.resolve(), "Release build receipt references a different repository root.")
    build_directory = (repository_root / "build/windows-x64-release").resolve(strict=True)
    require(Path(str(receipt.get("buildDirectory", ""))).resolve(strict=True) == build_directory, "Release build receipt does not use the standard Windows x64 Release build directory.")
    require(Path(str(package.get("cachePath", ""))).resolve(strict=True) == build_directory / "CMakeCache.txt", "Package CMake cache path differs from the standard build cache.")
    cache_path = build_directory / "CMakeCache.txt"
    cache_sha = sha256_file(cache_path)
    require(cache_sha == str(receipt.get("cacheSha256", "")) == str(package.get("cacheSha256", "")), "Current Release cache hash differs from the receipt.")
    cache_toolchain = read_cmake_release_toolchain(cache_path)
    build_toolchain = validate_exact_release_build_toolchain(receipt, cache_toolchain)
    for package_field, receipt_field in (
        ("generator", "generator"),
        ("generatorPlatform", "generatorPlatform"),
        ("generatorToolset", "generatorToolset"),
        ("generatorInstance", "generatorInstance"),
        ("windowsSdkVersion", "windowsSdkVersion"),
        ("compiler", "compiler"),
    ):
        if package_field == "generatorInstance":
            matches = _normalized_windows_path(package.get(package_field)) == _normalized_windows_path(receipt.get(receipt_field))
        else:
            matches = package.get(package_field) == receipt.get(receipt_field)
        require(matches, f"Current-source package {package_field} differs from the VS2022/v143 exact-build receipt.")
    compiler_path = Path(str((receipt.get("compiler") or {}).get("path", ""))).resolve(strict=True)
    require(sha256_file(compiler_path) == str((receipt.get("compiler") or {}).get("sha256", "")),
            "Current VS2022/v143 compiler binary differs from the exact-build receipt.")
    require(receipt.get("configuration") == "Release" and receipt.get("platform") == "x64", "Release build receipt is not Windows x64 Release.")
    require(receipt.get("packageOption") == "CLASSMNGR_NEXT_RELEASE_PACKAGE=ON", "Release build receipt does not enable the Next package configuration.")
    require(receipt.get("incrementalBuild") is True and package.get("incrementalBuild") is True, "Release build receipt does not identify the authorized incremental build.")
    build = receipt.get("build") or {}
    validate_exact_build_exit_code(build)
    require(type(package.get("buildExitCode")) is int and package["buildExitCode"] == 0,
            "Current-source package buildExitCode must be the exact JSON integer 0.")
    targets = receipt.get("targets") or []
    require({"ClassMngrNext", "ClassMngrNextResourcePackage"}.issubset(set(targets)), "Release receipt omits the app or current resource package target.")

    preflight_path = Path(str(receipt.get("preflightPath", ""))).resolve(strict=True)
    require(allowed_root in preflight_path.parents
            and sha256_file(preflight_path) == str(receipt.get("preflightSha256", "")),
            "VS2022/v143 build preflight is missing or changed.")
    preflight = load_json(preflight_path)
    require(preflight.get("schema") == "classmngr-phase9-p9-17-standard-token-build-preflight-v2"
            and preflight.get("status") == "passed"
            and preflight.get("cacheSha256") == cache_sha,
            "VS2022/v143 build preflight schema, status, or cache fingerprint differs.")
    for field in ("generator", "generatorPlatform", "generatorToolset", "generatorInstance",
                  "windowsSdkVersion", "generatedProjectToolchain", "compiler"):
        require(preflight.get(field) == receipt.get(field),
                f"VS2022/v143 build preflight {field} differs from the exact-build receipt.")

    source_inventory_path = Path(str(receipt.get("sourceInventoryPath", ""))).resolve(strict=True)
    require(allowed_root in source_inventory_path.parents, "Build source inventory is outside the P9-17 exact-build evidence root.")
    require(sha256_file(source_inventory_path) == str(receipt.get("sourceInventoryFileSha256", "")), "Build source inventory file hash differs from the receipt.")
    build_source = load_json(source_inventory_path)
    build_rows = build_source.get("files")
    require(isinstance(build_rows, list), "Build source inventory is incomplete.")
    verified_source_file_count = validate_receipt_source_file_count(receipt, build_source)
    require(canonical_inventory_fingerprint(build_rows) == str(receipt.get("sourceSha256", "")) == str(build_source.get("sha256", "")), "Build source inventory fingerprint differs from its receipt.")
    current_record = package.get("currentSourceInventory") or {}
    current_path = verify_recorded_file(evidence_root, current_record)
    current_source = load_json(current_path)
    require(current_source.get("includedRoots") == build_source.get("includedRoots"), "Build and route source inventories have different roots.")
    require(str(current_source.get("sha256", "")) == str(package.get("currentSourceSha256", "")) == str(receipt.get("sourceSha256", "")), "Current complete source tree differs from the Release build source fingerprint.")
    require(_source_row_map(build_rows, "Build") == _source_row_map(current_source.get("files", []), "Route"), "Build source inventory differs from the current complete source tree.")
    source_delta_summary = verify_p917_source_delta_contract(package, evidence_root, repository_root, current_source)

    build_helper_path = Path(str(receipt.get("buildHelperPath", ""))).resolve(strict=True)
    expected_helper_path = (repository_root / P917_BUILD_HELPER_RELATIVE).resolve(strict=True)
    require(build_helper_path == expected_helper_path, "Release receipt was not produced by the tracked P9-17 current-source build helper.")
    require(sha256_file(build_helper_path) == str(receipt.get("buildHelperSha256", "")), "Tracked P9-17 build helper hash differs from the receipt.")
    build_execution_context = validate_build_execution_context(receipt.get("executionContext"))
    observer_execution_context = validate_windows_execution_context(
        (package.get("executionContext") or {}).get("observer"), "Observer"
    )
    require(build_execution_contexts_match(
                (package.get("executionContext") or {}).get("build"), receipt.get("executionContext")
            ),
            "Package build execution-context record differs from the source-bound receipt.")
    require(observer_execution_context["userSid"] == build_execution_context["completion"]["userSid"],
            "Build and observer Windows user SIDs differ.")
    require(observer_execution_context["processSessionId"] == build_execution_context["completion"]["processSessionId"]
            and observer_execution_context["activeConsoleSessionId"] == build_execution_context["completion"]["activeConsoleSessionId"],
            "Build and observer are not in the same active console session.")

    log_path = Path(str(build.get("logPath", ""))).resolve(strict=True)
    require(allowed_root in log_path.parents and sha256_file(log_path) == str(build.get("logSha256", "")), "Current-source Release build log is missing or changed.")
    build_log_text = log_path.read_text(encoding="utf-8", errors="replace").casefold()
    require(str((receipt.get("compiler") or {}).get("path", "")).casefold() in build_log_text,
            "Current-source Release build log does not show the expected VS2022/v143 x64 compiler path.")
    release_executable = Path(str(receipt.get("releaseExecutablePath", ""))).resolve(strict=True)
    expected_executable = build_directory / "next-package-build/Release/ClassMngr.exe"
    require(release_executable == expected_executable, "Release receipt executable does not come from the standard build tree.")
    release_sha = sha256_file(release_executable)
    require(release_sha == str(receipt.get("releaseExecutableSha256", "")) == str(package.get("releaseExecutableSha256", "")) == str(package.get("applicationSha256", "")), "Current Release executable hash differs from the package and receipt.")
    require(_pe_machine(release_executable) == 0x8664, "Current Release executable is not Windows x64.")

    generated = receipt.get("generatedResources") or {}
    assets_root = Path(str(generated.get("bakedAssetsRoot", ""))).resolve(strict=True)
    translations_root = Path(str(generated.get("translationsRoot", ""))).resolve(strict=True)
    catalog_path = Path(str(generated.get("catalogPath", ""))).resolve(strict=True)
    expected_generated_root = (build_directory / "generated/ClassMngrNext").resolve()
    for path in (assets_root, translations_root, catalog_path):
        require(expected_generated_root in path.parents, f"Generated current-source resource output escapes the standard build tree: {path}.")
    asset_sha, asset_rows = _directory_inventory(assets_root)
    translations_sha, translations_rows = _directory_inventory(translations_root)
    require(asset_sha == str(generated.get("bakedAssetsSha256", "")), "Release baked-resource inventory hash differs from the receipt.")
    require(len(asset_rows) == int(generated.get("bakedAssetsFileCount", -1)), "Release baked-resource inventory count differs from the receipt.")
    require(translations_sha == str(generated.get("translationsSha256", "")), "Release translation inventory hash differs from the receipt.")
    require(len(translations_rows) == int(generated.get("translationsFileCount", -1)), "Release translation inventory count differs from the receipt.")
    require(sha256_file(catalog_path) == str(generated.get("catalogSha256", "")), "Release resource catalog hash differs from the receipt.")

    resource_root = Path(str(package.get("resourceRoot", ""))).resolve(strict=True)
    resource_record = package.get("resourceInventory") or {}
    resource_inventory_path = verify_recorded_file(evidence_root, resource_record)
    resource_inventory = load_json(resource_inventory_path)
    require(Path(str(resource_inventory.get("root", ""))).resolve() == resource_root, "Staged resource inventory root differs from package provenance.")
    require(canonical_inventory_fingerprint(resource_inventory.get("files", [])) == str(package.get("resourceInventorySha256", "")) == str(resource_inventory.get("sha256", "")), "Staged resource tree fingerprint differs from package provenance.")
    resource_rows = _source_row_map(resource_inventory.get("files", []), "Staged resource")
    expected_resource_rows: dict[str, tuple[int, str]] = {}
    for row in asset_rows:
        expected_resource_rows[("assets/" + str(row["path"])).casefold()] = (int(row["sizeBytes"]), str(row["sha256"]))
    for row in translations_rows:
        expected_resource_rows[("assets/translations/" + str(row["path"])).casefold()] = (int(row["sizeBytes"]), str(row["sha256"]))
    expected_resource_rows["resource-catalog.json"] = (catalog_path.stat().st_size, str(generated["catalogSha256"]))
    actual_resource_rows = {path: (int(row["sizeBytes"]), str(row["sha256"])) for path, row in resource_rows.items()}
    require(actual_resource_rows == expected_resource_rows, "Packaged resource files differ from the exact current-source build outputs.")

    return {
        "passed": True,
        "receiptPath": str(receipt_path),
        "receiptSha256": receipt_sha,
        "sourceSha256": str(receipt["sourceSha256"]),
        "sourceFileCount": verified_source_file_count,
        "buildExitCode": validate_exact_build_exit_code(build),
        "configuration": "Release",
        "platform": "x64",
        "incrementalBuild": True,
        "toolchain": build_toolchain,
        "releaseExecutableSha256": release_sha,
        "generatedResourcesSha256": str(package.get("resourceInventorySha256", "")),
        "sourceDelta": source_delta_summary,
        "executionContext": {
            "build": build_execution_context,
            "observer": observer_execution_context,
        },
    }


def _source_row_map(files: list[dict[str, Any]], label: str) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    for row in files:
        path = str(row.get("path", ""))
        key = path.casefold()
        require(path and key not in result, f"{label} source inventory has a missing or duplicate path: {path}.")
        result[key] = row
    return result


def validate_source_inventory_delta(
    prior_files: list[dict[str, Any]],
    current_files: list[dict[str, Any]],
    expected_additions: tuple[str, ...] | list[str],
    allowed_changes: dict[str, tuple[tuple[int, str], tuple[int, str]]] | None = None,
) -> list[str]:
    """Require only the exact harness additions and explicit source-change set."""
    prior = _source_row_map(prior_files, "Prior")
    current = _source_row_map(current_files, "Current")
    permitted = {
        str(path).casefold(): (str(path), before, after)
        for path, (before, after) in (allowed_changes or {}).items()
    }
    observed_changes: set[str] = set()
    for key, old_row in prior.items():
        require(key in current, f"Previously inventoried source disappeared: {old_row['path']}.")
        new_row = current[key]
        old_value = (int(old_row["sizeBytes"]), str(old_row["sha256"]).lower())
        new_value = (int(new_row["sizeBytes"]), str(new_row["sha256"]).lower())
        if old_value == new_value:
            continue
        require(key in permitted, f"Previously inventoried source changed: {old_row['path']}.")
        expected_path, expected_before, expected_after = permitted[key]
        require(
            str(old_row["path"]) == expected_path
            and str(new_row["path"]) == expected_path
            and old_value == (int(expected_before[0]), str(expected_before[1]).lower())
            and new_value == (int(expected_after[0]), str(expected_after[1]).lower()),
            f"Authorized source change does not match its pinned before/after hashes: {old_row['path']}.",
        )
        observed_changes.add(key)
    require(observed_changes == set(permitted), "The exact authorized source-change set was not observed.")
    added = sorted(
        (row["path"] for key, row in current.items() if key not in prior),
        key=str.casefold,
    )
    expected = sorted(expected_additions, key=str.casefold)
    require([path.casefold() for path in added] == [path.casefold() for path in expected], f"Unexpected source additions: {added}.")
    return added


def p917_source_delta_allowlist() -> dict[str, tuple[tuple[int, str], tuple[int, str]]]:
    changes = (
        *P917_AUTHORIZED_PRODUCT_SOURCE_CHANGES,
        *P917_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES,
        *P917_AUTHORIZED_HARNESS_SOURCE_CHANGES,
    )
    return {
        str(change["path"]): (
            (int(change["beforeSizeBytes"]), str(change["beforeSha256"])),
            (int(change["afterSizeBytes"]), str(change["afterSha256"])),
        )
        for change in changes
    }


def verify_p917_source_delta_contract(
    package: dict[str, Any],
    evidence_root: Path,
    repository_root: Path,
    current_source: dict[str, Any],
) -> dict[str, Any]:
    """Prove the exact P9-17 source delta and retain product changes in the current fingerprint."""
    accepted_manifest_path = (repository_root / P916_MANIFEST_RELATIVE).resolve(strict=True)
    accepted_manifest = load_json(accepted_manifest_path)
    require(
        accepted_manifest.get("taskId") == "P9_16_PACKAGED_NEXT_WORKSPACE_REPEATED_CYCLES"
        and accepted_manifest.get("status") == "passed",
        "Source-delta ancestry is not the passed accepted P9-16 run.",
    )
    accepted = accepted_manifest.get("packageProvenance") or {}
    accepted_run_root = accepted_manifest_path.parent
    accepted_source_path = (accepted_run_root / str(accepted.get("sourceInventoryPath", ""))).resolve(strict=True)
    require(accepted_run_root in accepted_source_path.parents, "Accepted P9-16 source inventory path escapes its run evidence.")
    require(sha256_file(accepted_source_path) == str(accepted.get("sourceInventoryFileSha256", "")), "Accepted P9-16 source inventory file hash differs.")
    accepted_source = load_json(accepted_source_path)
    accepted_files = accepted_source.get("files")
    current_files = current_source.get("files")
    included_roots = current_source.get("includedRoots")
    require(isinstance(accepted_files, list) and isinstance(current_files, list), "Accepted or current source inventory rows are missing.")
    require(isinstance(included_roots, list) and included_roots == accepted_source.get("includedRoots"), "Accepted and current source inventory roots differ.")
    require(int(current_source.get("fileCount", -1)) == len(current_files), "Current source inventory count differs from its rows.")
    require(canonical_inventory_fingerprint(current_files) == str(current_source.get("sha256", "")), "Current source inventory fingerprint is invalid.")
    require(canonical_inventory_fingerprint(accepted_files) == str(accepted_source.get("sha256", "")), "Accepted P9-16 source inventory fingerprint is invalid.")
    require(str(accepted_source.get("sha256", "")) == str(accepted.get("currentSourceSha256", "")), "Accepted P9-16 source tree differs from its manifest.")

    added = validate_source_inventory_delta(
        accepted_files,
        current_files,
        P917_OWNED_SOURCES,
        p917_source_delta_allowlist(),
    )
    accepted_map = _source_row_map(accepted_files, "Accepted P9-16")
    current_map = _source_row_map(current_files, "Current P9-17")

    p913_manifest_path = Path(str(accepted.get("priorP913ManifestPath", ""))).resolve(strict=True)
    require(sha256_file(p913_manifest_path) == str(accepted.get("priorP913ManifestSha256", "")), "P9-13 source ancestry manifest hash differs.")
    require(Path(str(package.get("priorP913ManifestPath", ""))).resolve(strict=True) == p913_manifest_path, "P9-17 source proof references a different P9-13 ancestry manifest.")
    require(str(package.get("priorP913ManifestSha256", "")) == str(accepted.get("priorP913ManifestSha256", "")), "P9-17 source proof P9-13 manifest hash differs.")
    p913_manifest = load_json(p913_manifest_path)
    require(p913_manifest.get("taskId") == "P9_13_NEXT_SPEAKING_REPORT_PACKAGED_LIFECYCLE" and p913_manifest.get("status") == "passed", "P9-13 source ancestry is not a passed Release run.")
    p913_source = p913_manifest.get("source") or {}
    derived_path = (p913_manifest_path.parent / str(p913_source.get("derivedReceiptPath", ""))).resolve(strict=True)
    require(p913_manifest_path.parent in derived_path.parents, "P9-13 derivation receipt path escapes its run evidence.")
    require(sha256_file(derived_path) == str(p913_source.get("derivedReceiptSha256", "")), "P9-13 derivation receipt hash differs.")
    derived = load_json(derived_path)
    derived_source = derived.get("source") or {}
    require(derived.get("status") == "verified-script-only-derivation", "P9-13 source derivation is not verified.")
    accepted_product_sha = str(accepted.get("productSourceSha256", ""))
    require(
        accepted_product_sha == str(p913_source.get("productSourceSha256", "")) == str(derived_source.get("productSourceSha256", "")),
        "P9-13 and P9-16 product-source fingerprints differ.",
    )

    legacy_excluded = {str(path).casefold() for path in derived_source.get("allowedPaths", [])}
    legacy_excluded.update(path.casefold() for path in P914_HARNESS_SOURCES + P915_HARNESS_SOURCES + P916_HARNESS_SOURCES)
    accepted_legacy_product = filter_product_source_rows(accepted_files, legacy_excluded)
    require(
        canonical_inventory_fingerprint(accepted_legacy_product) == accepted_product_sha,
        "Accepted P9-16 product-source fingerprint does not match its source inventory.",
    )

    test_paths = {str(change["path"]).casefold() for change in P917_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES}
    product_paths = {str(change["path"]).casefold() for change in P917_AUTHORIZED_PRODUCT_SOURCE_CHANGES}
    harness_change_paths = {str(change["path"]).casefold() for change in P917_AUTHORIZED_HARNESS_SOURCE_CHANGES}
    require(not product_paths.intersection(test_paths), "P9-17 product and test-only source allowlists overlap.")
    require(not product_paths.intersection(harness_change_paths), "P9-17 product and harness source allowlists overlap.")
    require(not test_paths.intersection(harness_change_paths), "P9-17 test-only and harness source allowlists overlap.")
    excluded_paths = set(legacy_excluded)
    excluded_paths.update(path.casefold() for path in P917_HARNESS_SOURCES)
    excluded_paths.update(test_paths)
    require(excluded_paths.issubset(current_map), "A declared harness/test-only exclusion is absent from the current full source inventory.")
    require(not product_paths.intersection(excluded_paths), "An authorized production source delta was excluded from the current product fingerprint.")

    product_files = filter_product_source_rows(current_files, excluded_paths)
    product_map = _source_row_map(product_files, "Current P9-17 product")
    product_sha = canonical_inventory_fingerprint(product_files)
    require(str(package.get("productSourceSha256", "")) == product_sha, "Current P9-17 product-source fingerprint differs from package provenance.")
    require(product_paths.issubset(product_map), "An authorized production source delta is missing from the current product-source inventory.")

    common_exclusions = excluded_paths | product_paths
    accepted_common = filter_product_source_rows(accepted_files, common_exclusions)
    current_common = filter_product_source_rows(current_files, common_exclusions)
    accepted_common_map = _source_row_map(accepted_common, "Accepted unchanged common product")
    current_common_map = _source_row_map(current_common, "Current unchanged common product")
    require(accepted_common_map == current_common_map, "A product/test source changed outside the exact P9-17 allowlist.")
    unchanged_common_sha = canonical_inventory_fingerprint(current_common)
    require(unchanged_common_sha == canonical_inventory_fingerprint(accepted_common), "Unchanged common product-source fingerprints differ.")

    source_equivalence = package.get("sourceEquivalence") or {}
    require(source_equivalence.get("schema") == "classmngr-phase9-p9-17-source-delta-proof-v1", "P9-17 source-delta proof schema differs.")
    require(source_equivalence.get("classification") == "current-p9-17-product-source-with-pinned-deltas", "P9-17 source proof does not identify the current P9-17 product source.")
    require(source_equivalence.get("acceptedSourceInventoryPath") == accepted.get("sourceInventoryPath"), "Source proof references a different accepted P9-16 source inventory.")
    require(source_equivalence.get("acceptedSourceInventorySha256") == accepted.get("sourceInventoryFileSha256"), "Source proof accepted-inventory file hash differs.")
    require(source_equivalence.get("acceptedSourceTreeSha256") == accepted.get("currentSourceSha256"), "Source proof accepted source-tree fingerprint differs.")
    require(Path(str(source_equivalence.get("acceptedSourceManifestPath", ""))).resolve(strict=True) == accepted_manifest_path, "Source proof references a different accepted P9-16 manifest.")
    require(str(source_equivalence.get("acceptedSourceManifestSha256", "")) == sha256_file(accepted_manifest_path), "Source proof accepted P9-16 manifest hash differs.")
    require(source_equivalence.get("acceptedP916ProductSourceSha256") == accepted_product_sha, "Source proof accepted P9-16 product fingerprint differs.")
    require(str(package.get("acceptedP916ProductSourceSha256", "")) == accepted_product_sha, "Package accepted P9-16 product fingerprint differs.")
    require(Path(str(package.get("acceptedRunManifestPath", ""))).resolve(strict=True) == accepted_manifest_path, "Package source proof references a different accepted P9-16 manifest.")
    require(str(package.get("acceptedRunManifestSha256", "")) == sha256_file(accepted_manifest_path), "Package accepted P9-16 manifest hash differs.")
    require(source_equivalence.get("currentP917ProductSourceSha256") == product_sha, "Source proof current P9-17 product fingerprint differs.")
    require(source_equivalence.get("unchangedCommonPreviousProductSourceSha256") == unchanged_common_sha, "Unchanged common prior product fingerprint differs.")
    require(source_equivalence.get("unchangedCommonCurrentProductSourceSha256") == unchanged_common_sha, "Unchanged common current product fingerprint differs.")
    require(int(source_equivalence.get("unchangedCommonProductFileCount", -1)) == len(current_common), "Unchanged common product file count differs.")
    require([str(path).casefold() for path in source_equivalence.get("addedHarnessPaths", [])] == [path.casefold() for path in added], "Source proof harness additions differ from the source delta.")

    expected_product_changes: list[dict[str, Any]] = []
    for change in P917_AUTHORIZED_PRODUCT_SOURCE_CHANGES:
        path = str(change["path"])
        before = accepted_map.get(path.casefold())
        after = current_map.get(path.casefold())
        require(before is not None and after is not None, f"Authorized P9-17 product source is missing: {path}.")
        require(
            int(before["sizeBytes"]) == int(change["beforeSizeBytes"])
            and str(before["sha256"]).lower() == str(change["beforeSha256"])
            and int(after["sizeBytes"]) == int(change["afterSizeBytes"])
            and str(after["sha256"]).lower() == str(change["afterSha256"]),
            f"Full source inventories do not retain the pinned product before/after hashes for {path}.",
        )
        expected_product_changes.append(
            {
                "path": path,
                "beforeSizeBytes": int(change["beforeSizeBytes"]),
                "beforeSha256": str(change["beforeSha256"]),
                "afterSizeBytes": int(change["afterSizeBytes"]),
                "afterSha256": str(change["afterSha256"]),
                "classification": "authorized-p9-17-production-source-change-included-in-current-product-fingerprint",
                "productFingerprintIncluded": True,
            }
        )
    require_exact_path_records(
        source_equivalence.get("authorizedProductSourceChanges"),
        expected_product_changes,
        "Recorded P9-17 product source deltas differ from the pinned before/after proof.",
    )

    expected_test_changes: list[dict[str, Any]] = []
    for change in P917_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES:
        path = str(change["path"])
        before = accepted_map.get(path.casefold())
        after = current_map.get(path.casefold())
        require(before is not None and after is not None, f"Authorized P9-17 test-only source is missing: {path}.")
        require(
            int(before["sizeBytes"]) == int(change["beforeSizeBytes"])
            and str(before["sha256"]).lower() == str(change["beforeSha256"])
            and int(after["sizeBytes"]) == int(change["afterSizeBytes"])
            and str(after["sha256"]).lower() == str(change["afterSha256"]),
            f"Full source inventories do not retain the pinned test-only before/after hashes for {path}.",
        )
        expected_test_changes.append(
            {
                "path": path,
                "beforeSizeBytes": int(change["beforeSizeBytes"]),
                "beforeSha256": str(change["beforeSha256"]),
                "afterSizeBytes": int(change["afterSizeBytes"]),
                "afterSha256": str(change["afterSha256"]),
                "classification": "test-only-source-change-excluded-from-product-fingerprint",
                "productFingerprintExcluded": True,
                "cmakeBoundaryEvidence": list(change["cmakeBoundaryEvidence"]),
            }
        )
    require_exact_path_records(
        source_equivalence.get("authorizedTestOnlySourceChanges"),
        expected_test_changes,
        "Recorded P9-17 test-only deltas or CMake boundaries differ from the pinned proof.",
    )

    expected_harness_changes: list[dict[str, Any]] = []
    for change in P917_AUTHORIZED_HARNESS_SOURCE_CHANGES:
        path = str(change["path"])
        before = accepted_map.get(path.casefold())
        after = current_map.get(path.casefold())
        require(before is not None and after is not None, f"Authorized P9-17 harness dependency is missing: {path}.")
        require(
            int(before["sizeBytes"]) == int(change["beforeSizeBytes"])
            and str(before["sha256"]).lower() == str(change["beforeSha256"])
            and int(after["sizeBytes"]) == int(change["afterSizeBytes"])
            and str(after["sha256"]).lower() == str(change["afterSha256"]),
            f"Full source inventories do not retain the pinned harness before/after hashes for {path}.",
        )
        expected_harness_changes.append(
            {
                "path": path,
                "beforeSizeBytes": int(change["beforeSizeBytes"]),
                "beforeSha256": str(change["beforeSha256"]),
                "afterSizeBytes": int(change["afterSizeBytes"]),
                "afterSha256": str(change["afterSha256"]),
                "classification": "shared-p9-17-harness-dependency-change-excluded-from-product-fingerprint",
                "productFingerprintExcluded": True,
            }
        )
    require_exact_path_records(
        source_equivalence.get("authorizedHarnessSourceChanges"),
        expected_harness_changes,
        "Recorded P9-17 harness dependency deltas differ from the pinned before/after proof.",
    )

    expected_product_paths = [str(change["path"]) for change in P917_AUTHORIZED_PRODUCT_SOURCE_CHANGES]
    expected_test_paths = [str(change["path"]) for change in P917_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES]
    expected_harness_paths = [str(change["path"]) for change in P917_AUTHORIZED_HARNESS_SOURCE_CHANGES]
    require_exact_paths(source_equivalence.get("changedPriorProductSourcePaths"), expected_product_paths, "Source proof production delta path list differs.")
    require_exact_paths(source_equivalence.get("changedPriorTestOnlySourcePaths"), expected_test_paths, "Source proof test-only delta path list differs.")
    require_exact_paths(source_equivalence.get("changedPriorHarnessSourcePaths"), expected_harness_paths, "Source proof harness dependency delta path list differs.")
    require_exact_paths(
        source_equivalence.get("productFingerprintExcludedPaths"),
        sorted(excluded_paths),
        "Source proof product-fingerprint exclusion paths differ from the pinned harness/test-only paths.",
    )

    product_record = package.get("productSourceInventory") or {}
    product_inventory_path = verify_recorded_file(evidence_root, product_record)
    product_inventory = load_json(product_inventory_path)
    recorded_product_files = product_inventory.get("files")
    require(isinstance(recorded_product_files, list) and int(product_inventory.get("fileCount", -1)) == len(recorded_product_files), "P9-17 product-source inventory is incomplete.")
    require(str(product_inventory.get("repositoryRoot", "")).casefold() == str(repository_root).casefold(), "P9-17 product-source inventory repository root differs.")
    require(product_inventory.get("includedRoots") == included_roots, "P9-17 product-source inventory roots differ from the full source inventory.")
    require(canonical_inventory_fingerprint(recorded_product_files) == product_sha == str(product_inventory.get("sha256", "")), "P9-17 current product-source inventory fingerprint differs.")
    require(_source_row_map(recorded_product_files, "Recorded current P9-17 product") == product_map, "P9-17 product-source inventory does not match the complete current product source.")
    require(str(product_record.get("treeSha256", "")) == product_sha and int(product_record.get("fileCount", -1)) == len(product_files), "P9-17 product-source manifest record differs.")
    for change in P917_AUTHORIZED_PRODUCT_SOURCE_CHANGES:
        path = str(change["path"]).casefold()
        require(path in _source_row_map(recorded_product_files, "Recorded current P9-17 product"), f"Authorized production source is absent from the product-source inventory: {path}.")

    harness_record = package.get("harnessSourceInventory") or {}
    harness_inventory_path = verify_recorded_file(evidence_root, harness_record)
    harness_inventory = load_json(harness_inventory_path)
    harness_files = harness_inventory.get("files")
    require(isinstance(harness_files, list) and int(harness_inventory.get("fileCount", -1)) == len(harness_files), "P9-17 harness-source inventory is incomplete.")
    require(harness_inventory.get("includedRoots") == list(P917_HARNESS_SOURCES), "P9-17 harness-source inventory roots differ from the explicit harness dependency list.")
    expected_harness = [current_map[path.casefold()] for path in P917_HARNESS_SOURCES]
    require(_source_row_map(harness_files, "Recorded P9-17 harness") == _source_row_map(expected_harness, "Expected P9-17 harness"), "P9-17 harness inventory differs from the exact owned source list.")
    harness_sha = canonical_inventory_fingerprint(harness_files)
    require(str(harness_inventory.get("sha256", "")) == harness_sha == str(harness_record.get("treeSha256", "")), "P9-17 harness-source inventory fingerprint differs.")
    require(int(harness_record.get("fileCount", -1)) == len(P917_HARNESS_SOURCES), "P9-17 harness-source manifest count differs.")

    return {
        "passed": True,
        "classification": "current-p9-17-product-source-with-pinned-deltas",
        "currentProductSourceSha256": product_sha,
        "unchangedCommonProductSourceSha256": unchanged_common_sha,
        "unchangedCommonProductFileCount": len(current_common),
        "authorizedProductSourcePaths": expected_product_paths,
        "authorizedTestOnlySourcePaths": expected_test_paths,
        "authorizedHarnessSourcePaths": expected_harness_paths,
        "addedHarnessPaths": added,
    }


def filter_product_source_rows(
    files: list[dict[str, Any]], excluded_paths: set[str] | list[str] | tuple[str, ...]
) -> list[dict[str, Any]]:
    excluded = {str(path).casefold() for path in excluded_paths}
    return [row for row in files if str(row.get("path", "")).casefold() not in excluded]


def _scan_source_inventory(repository_root: Path, included_roots: list[str]) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for relative_root in included_roots:
        root_path = (repository_root / str(relative_root)).resolve(strict=True)
        require(repository_root.resolve() in root_path.parents or root_path == repository_root.resolve(), f"Source root escapes the repository: {relative_root}.")
        paths = [root_path] if root_path.is_file() else [path for path in root_path.rglob("*") if path.is_file()]
        for source_path in paths:
            if SOURCE_CACHE_DIRECTORIES.intersection(source_path.parts) or source_path.suffix.lower() in {".pyc", ".pyo"}:
                continue
            resolved = source_path.resolve(strict=True)
            require(repository_root.resolve() in resolved.parents, f"Source path escapes the repository: {source_path}.")
            relative = resolved.relative_to(repository_root.resolve()).as_posix()
            rows.append({"path": relative, "sizeBytes": resolved.stat().st_size, "sha256": sha256_file(resolved)})
    rows.sort(key=lambda row: str(row["path"]).casefold())
    return rows


def _verify_external_inventory(
    inventory_path: Path,
    expected_file_sha256: str,
    expected_root: Path,
    expected_tree_sha256: str,
    expected_file_count: int,
) -> tuple[dict[str, Any], dict[str, tuple[int, str]]]:
    inventory_path = inventory_path.resolve(strict=True)
    require(sha256_file(inventory_path) == expected_file_sha256, f"Recorded inventory file changed: {inventory_path}.")
    inventory = load_json(inventory_path)
    files = inventory.get("files")
    require(isinstance(files, list) and int(inventory.get("fileCount", -1)) == len(files), f"Inventory file count differs: {inventory_path}.")
    require(canonical_inventory_fingerprint(files) == str(inventory.get("sha256", "")), f"Inventory fingerprint is invalid: {inventory_path}.")
    require(str(inventory.get("root", "")).casefold() == str(expected_root.resolve()).casefold(), f"Inventory root differs: {inventory_path}.")
    require(str(inventory["sha256"]) == expected_tree_sha256 and len(files) == expected_file_count, f"Inventory contents differ from provenance: {inventory_path}.")
    recorded = {
        str(row["path"]): (int(row["sizeBytes"]), str(row["sha256"]).lower())
        for row in files
    }
    actual: dict[str, tuple[int, str]] = {}
    for file_path in expected_root.rglob("*"):
        if file_path.is_file():
            actual[file_path.relative_to(expected_root).as_posix()] = (file_path.stat().st_size, sha256_file(file_path))
    require(actual == recorded, f"Inventory does not describe the complete current tree: {inventory_path}.")
    return inventory, recorded


def _pe_machine(path: Path) -> int:
    data = path.read_bytes()
    require(len(data) >= 0x40, f"Executable is too short to contain a PE header: {path}.")
    offset = int.from_bytes(data[0x3C:0x40], "little")
    require(offset >= 0 and offset + 6 <= len(data) and data[offset : offset + 4] == b"PE\0\0", f"Executable has an invalid PE header: {path}.")
    return int.from_bytes(data[offset + 4 : offset + 6], "little")


def verify_p916_package_fallback(
    package: dict[str, Any], evidence_root: Path, repository_root: Path
) -> dict[str, Any]:
    require(package.get("buildMode") == "verified-accepted-p916-package-fallback", "Package fallback mode is not the authorized P9-16 accepted-package route.")
    require(package.get("freshExactTreeBuild") is False and package.get("freshBuildAttemptedInCurrentRun") is False, "Fallback provenance must explicitly avoid claiming a fresh exact-tree build.")
    require(package.get("buildExitCode") is None, "Fallback provenance must not report a successful fresh build exit code.")
    require(package.get("configuration") == "Release" and package.get("platform") == "x64" and package.get("architecture") == "x64", "Fallback package is not Windows x64 Release.")

    accepted_manifest_path = (repository_root / P916_MANIFEST_RELATIVE).resolve(strict=True)
    require(Path(str(package.get("acceptedRunManifestPath", ""))).resolve(strict=True) == accepted_manifest_path, "Fallback does not reference the pinned accepted P9-16 manifest.")
    require(sha256_file(accepted_manifest_path) == str(package.get("acceptedRunManifestSha256", "")), "Accepted P9-16 manifest hash differs from fallback provenance.")
    accepted_manifest = load_json(accepted_manifest_path)
    require(accepted_manifest.get("taskId") == "P9_16_PACKAGED_NEXT_WORKSPACE_REPEATED_CYCLES" and accepted_manifest.get("status") == "passed", "Fallback ancestry is not the passed P9-16 accepted run.")
    accepted_validation = accepted_manifest.get("validation") or {}
    require(accepted_validation.get("status") == "passed" and int(accepted_validation.get("exitCode", -1)) == 0, "P9-16 accepted run lacks passed evidence validation.")
    accepted = accepted_manifest.get("packageProvenance") or {}
    require(bool(accepted.get("reusedPackage")) and accepted.get("freshExactTreeRebuild") is False, "P9-16 accepted provenance is not a verified reused Release package.")
    require(accepted.get("configuration") == "Release" and accepted.get("platform") == "x64" and accepted.get("architecture") == "x64", "P9-16 accepted package is not Windows x64 Release.")

    blocked = package.get("buildBlockEvidence") or {}
    blocked_manifest_path = (repository_root / P917_BLOCKED_MANIFEST_RELATIVE).resolve(strict=True)
    require(str(blocked.get("manifestPath", "")).replace("\\", "/") == P917_BLOCKED_MANIFEST_RELATIVE, "Fallback does not reference the captured P9-17 build blocker.")
    require(sha256_file(blocked_manifest_path) == str(blocked.get("manifestSha256", "")), "Captured P9-17 blocker manifest hash differs.")
    blocked_manifest = load_json(blocked_manifest_path)
    require(blocked_manifest.get("taskId") == "P9_17_PACKAGED_NEXT_LARGE_ROSTER_EDITING" and blocked_manifest.get("status") == "failed", "Captured P9-17 build blocker is not a failed P9-17 attempt.")
    require("Fresh exact-tree Release build failed with exit code 1" in str((blocked_manifest.get("failure") or {}).get("message", "")), "Captured P9-17 attempt does not prove a failed exact-tree Release build.")
    blocked_log_path = (repository_root / P917_BLOCKED_BUILD_LOG_RELATIVE).resolve(strict=True)
    require(str(blocked.get("buildLogPath", "")).replace("\\", "/") == P917_BLOCKED_BUILD_LOG_RELATIVE, "Fallback does not reference the captured P9-17 build log.")
    require(sha256_file(blocked_log_path) == str(blocked.get("buildLogSha256", "")), "Captured P9-17 build log hash differs.")
    blocked_log = blocked_log_path.read_text(encoding="utf-8", errors="replace")
    require("UnauthorizedAccessException" in blocked_log and "ZERO_CHECK.vcxproj" in blocked_log and "error MSB4018" in blocked_log and "exitCode=1" in blocked_log, "Captured P9-17 build log does not prove the ZERO_CHECK access failure.")
    require(int(blocked.get("exitCode", -1)) == 1, "Captured P9-17 build blocker did not exit with code 1.")

    accepted_run_root = accepted_manifest_path.parent
    accepted_source_path = (accepted_run_root / str(accepted.get("sourceInventoryPath", ""))).resolve(strict=True)
    require(accepted_run_root in accepted_source_path.parents, "Accepted P9-16 source inventory path escapes its run evidence.")
    require(sha256_file(accepted_source_path) == str(accepted.get("sourceInventoryFileSha256", "")), "Accepted P9-16 source inventory file hash differs.")
    accepted_source = load_json(accepted_source_path)
    accepted_source_files = accepted_source.get("files")
    require(isinstance(accepted_source_files, list) and int(accepted_source.get("fileCount", -1)) == len(accepted_source_files), "Accepted P9-16 source inventory file count differs.")
    require(canonical_inventory_fingerprint(accepted_source_files) == str(accepted_source.get("sha256", "")), "Accepted P9-16 source inventory fingerprint is invalid.")
    require(str(accepted_source.get("sha256")) == str(accepted.get("currentSourceSha256")), "Accepted P9-16 source inventory differs from its recorded tree fingerprint.")

    current_source_record = package.get("currentSourceInventory") or {}
    current_source_path = verify_recorded_file(evidence_root, current_source_record)
    current_source = load_json(current_source_path)
    current_files = current_source.get("files")
    included_roots = current_source.get("includedRoots")
    require(isinstance(current_files, list) and isinstance(included_roots, list), "Current source inventory is incomplete.")
    require(str(current_source.get("repositoryRoot", "")).casefold() == str(repository_root).casefold(), "Current source inventory repository root differs.")
    require(int(current_source.get("fileCount", -1)) == len(current_files), "Current source inventory file count differs.")
    require(canonical_inventory_fingerprint(current_files) == str(current_source.get("sha256", "")), "Current source inventory fingerprint is invalid.")
    actual_source_files = _scan_source_inventory(repository_root, [str(path) for path in included_roots])
    require(_source_row_map(actual_source_files, "Actual") == _source_row_map(current_files, "Current"), "Current source inventory does not cover the complete accepted source roots.")
    require(str(current_source.get("sha256")) == str(package.get("currentSourceSha256", "")), "Current source inventory differs from package provenance.")
    require(str(current_source.get("sha256")) == str(current_source_record.get("treeSha256", "")), "Current source inventory tree hash differs from its manifest record.")
    require(int(current_source.get("fileCount", -1)) == int(current_source_record.get("fileCount", -2)), "Current source inventory count differs from its manifest record.")
    require(included_roots == accepted_source.get("includedRoots"), "P9-16 and current source inventory roots differ.")
    source_delta_summary = verify_p917_source_delta_contract(package, evidence_root, repository_root, current_source)
    added = source_delta_summary["addedHarnessPaths"]
    product_sha = source_delta_summary["currentProductSourceSha256"]

    package_root = Path(str(package.get("packageRoot", ""))).resolve(strict=True)
    require(package_root == Path(str(accepted.get("packageRoot", ""))).resolve(strict=True), "Fallback package root differs from accepted P9-16 package provenance.")
    require(str(package.get("packageSha256", "")) == str(accepted.get("packageSha256", "")), "Fallback package tree fingerprint differs from accepted P9-16 provenance.")
    require(int(package.get("packageFileCount", -1)) == int(accepted.get("packageFileCount", -2)), "Fallback package file count differs from accepted P9-16 provenance.")
    accepted_package_path = (accepted_run_root / str(accepted.get("packageInventoryPath", ""))).resolve(strict=True)
    require(accepted_run_root in accepted_package_path.parents, "Accepted P9-16 package inventory path escapes its run evidence.")
    accepted_package, accepted_package_rows = _verify_external_inventory(
        accepted_package_path,
        str(accepted.get("packageInventoryFileSha256", "")),
        package_root,
        str(accepted.get("packageSha256", "")),
        int(accepted.get("packageFileCount", -1)),
    )
    current_package_record = package.get("packageInventory") or {}
    current_package_path = verify_recorded_file(evidence_root, current_package_record)
    current_package = load_json(current_package_path)
    current_package_rows = {
        str(row["path"]): (int(row["sizeBytes"]), str(row["sha256"]).lower())
        for row in current_package.get("files", [])
    }
    require(current_package_rows == accepted_package_rows, "Current-run package inventory differs from the accepted P9-16 full inventory.")
    require(str(current_package.get("sha256", "")) == str(accepted_package.get("sha256", "")), "Current-run package tree fingerprint differs from accepted P9-16.")
    require(package.get("packageSha256") == current_package.get("sha256") and int(current_package_record.get("fileCount", -1)) == len(current_package_rows), "Current-run package manifest record differs from its inventory.")

    application_path = Path(str(package.get("applicationPath", ""))).resolve(strict=True)
    require(application_path == Path(str(accepted.get("applicationPath", ""))).resolve(strict=True), "Fallback application path differs from accepted P9-16 provenance.")
    application_sha = sha256_file(application_path)
    require(application_sha == str(accepted.get("applicationSha256", "")) == str(package.get("applicationSha256", "")), "Fallback application hash differs from accepted P9-16 provenance.")
    require(_pe_machine(application_path) == 0x8664 and int(package.get("machineType", 0)) == 0x8664, "Fallback application is not Windows x64.")

    release_path = Path(str(accepted.get("acceptedReleaseReceiptPath", ""))).resolve(strict=True)
    require(release_path == Path(str(package.get("acceptedReleaseReceiptPath", ""))).resolve(strict=True), "Fallback references a different Release receipt.")
    require(sha256_file(release_path) == str(accepted.get("acceptedReleaseReceiptSha256", "")) == str(package.get("acceptedReleaseReceiptSha256", "")), "Accepted Release receipt hash differs.")
    release = load_json(release_path)
    require(release.get("status") == "succeeded" and release.get("configuration") == "Release" and release.get("platform") == "x64", "Accepted Release receipt does not describe a successful Windows x64 Release build.")
    require(int((release.get("build") or {}).get("exitCode", -1)) == 0 and release.get("releaseExecutableSha256") == application_sha, "Accepted Release receipt does not prove the packaged executable.")

    resource_root = Path(str(package.get("resourceRoot", ""))).resolve(strict=True)
    require(resource_root == Path(str(accepted.get("resourceRoot", ""))).resolve(strict=True), "Fallback staged resource root differs from accepted P9-16 provenance.")
    require(str(package.get("resourceInventorySha256", "")) == str(accepted.get("resourceInventorySha256", "")), "Fallback resource fingerprint differs from accepted P9-16 provenance.")
    accepted_resource_path = (accepted_run_root / str(accepted.get("resourceInventoryPath", ""))).resolve(strict=True)
    require(accepted_run_root in accepted_resource_path.parents, "Accepted P9-16 resource inventory path escapes its run evidence.")
    current_resource_record = package.get("resourceInventory") or {}
    accepted_resource, accepted_resource_rows = _verify_external_inventory(
        accepted_resource_path,
        str(accepted.get("resourceInventoryFileSha256", "")),
        resource_root,
        str(accepted.get("resourceInventorySha256", "")),
        int(current_resource_record.get("fileCount", -1)),
    )
    current_resource_path = verify_recorded_file(evidence_root, current_resource_record)
    current_resource = load_json(current_resource_path)
    current_resource_rows = {
        str(row["path"]): (int(row["sizeBytes"]), str(row["sha256"]).lower())
        for row in current_resource.get("files", [])
    }
    require(current_resource_rows == accepted_resource_rows and current_resource.get("sha256") == accepted_resource.get("sha256"), "Current staged resource inventory differs from the accepted P9-16 resource inventory.")
    require(release.get("generatedResources", {}).get("stagedResourceInputs", {}).get("sha256") == accepted.get("resourceInventorySha256"), "Staged Release resource inputs differ from the accepted Release receipt.")
    require(str(current_resource_record.get("treeSha256", "")) == str(accepted.get("resourceInventorySha256", "")) and int(current_resource_record.get("fileCount", -1)) == len(current_resource_rows), "Current staged resource manifest record differs.")

    cache_path = Path(str(package.get("configuredCachePath", ""))).resolve(strict=True)
    require(cache_path == Path(str(accepted.get("configuredCachePath", ""))).resolve(strict=True), "Fallback configured cache path differs from accepted P9-16 provenance.")
    cache_sha = sha256_file(cache_path)
    require(cache_sha == str(accepted.get("configuredCacheSha256", "")) == str(package.get("configuredCacheSha256", "")) == str(release.get("cacheSha256After", "")), "Configured Release cache hash differs from the accepted build receipt.")
    cache_values: dict[str, str] = {}
    for line in cache_path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line and not line.startswith("#") and ":" in line and "=" in line:
            key = line.split(":", 1)[0]
            cache_values[key] = line.split("=", 1)[1]
    require(Path(cache_values.get("CMAKE_HOME_DIRECTORY", "")).resolve() == repository_root.resolve(), "Accepted Release cache points to a different source tree.")
    require(cache_values.get("CMAKE_GENERATOR", "").startswith("Visual Studio") and cache_values.get("CMAKE_GENERATOR_PLATFORM", "") in {"", "x64"}, "Accepted Release cache is not configured for Windows x64.")
    require(cache_values.get("CLASSMNGR_NEXT_RELEASE_PACKAGE") == "ON", "Accepted Release cache does not enable the Next release package.")
    require(str(package.get("cmakeHomeDirectory", "")).casefold() == str(repository_root).casefold(), "Fallback CMake home directory differs from this repository.")
    require(str(package.get("generator", "")) == cache_values.get("CMAKE_GENERATOR"), "Fallback generator differs from the configured Release cache.")

    return {
        "acceptedRunManifestPath": str(accepted_manifest_path),
        "acceptedRunManifestSha256": str(package["acceptedRunManifestSha256"]),
        "productSourceSha256": product_sha,
        "currentSourceSha256": str(current_source["sha256"]),
        "addedHarnessPaths": added,
        "packageInventory": {"sha256": str(current_package["sha256"]), "fileCount": len(current_package_rows)},
        "applicationSha256": application_sha,
        "releaseReceiptSha256": str(package["acceptedReleaseReceiptSha256"]),
        "resourceInventorySha256": str(accepted["resourceInventorySha256"]),
        "configuration": "Release",
        "platform": "x64",
        "freshExactTreeBuild": False,
    }


def verify_package_inventory(evidence_root: Path, record: dict[str, Any], package_root: Path) -> dict[str, Any]:
    path = verify_recorded_file(evidence_root, record)
    inventory = load_json(path)
    files = inventory.get("files")
    require(isinstance(files, list) and int(inventory.get("fileCount", -1)) == len(files), "Package inventory file count differs.")
    require(canonical_inventory_fingerprint(files) == str(inventory.get("sha256", "")), "Package inventory fingerprint is invalid.")
    require(str(inventory.get("root", "")).casefold() == str(package_root).casefold(), "Package inventory root differs from package provenance.")
    actual_files: dict[str, tuple[int, str]] = {}
    for file in package_root.rglob("*"):
        if not file.is_file():
            continue
        actual_files[file.relative_to(package_root).as_posix()] = (file.stat().st_size, sha256_file(file))
    recorded_files = {
        str(row["path"]): (int(row["sizeBytes"]), str(row["sha256"]))
        for row in files
    }
    require(actual_files == recorded_files, "Packaged file tree differs from its exact inventory.")
    return {"sha256": inventory["sha256"], "fileCount": len(files)}


def validate_evidence_inventory(evidence_root: Path, record: dict[str, Any]) -> dict[str, Any]:
    path = verify_recorded_file(evidence_root, record)
    inventory = load_json(path)
    files = inventory.get("files")
    require(isinstance(files, list), "Evidence inventory omits its file list.")
    seen: set[str] = set()
    for item in files:
        relative = str(item.get("path", ""))
        require(relative and relative not in seen, f"Evidence inventory path is missing or duplicated: {relative}.")
        seen.add(relative)
        actual = resolve_evidence_path(evidence_root, relative)
        require(actual.stat().st_size == int(item["sizeBytes"]), f"Evidence inventory size mismatch: {relative}.")
        require(sha256_file(actual) == str(item["sha256"]), f"Evidence inventory hash mismatch: {relative}.")
    require("manifest.json" not in seen and str(record["relativePath"]) not in seen, "Evidence inventory must not recursively include itself or the manifest.")
    return {"passed": True, "fileCount": len(files), "inventorySha256": inventory["sha256"]}


def p917_acceptance_eligible(build_mode: str) -> bool:
    """Only the strict exact-current-source receipt can support P9-17 acceptance."""
    return build_mode == "exact-current-source-release"


def validate_manifest(manifest_path: Path) -> dict[str, Any]:
    manifest_path = manifest_path.resolve(strict=True)
    evidence_root = manifest_path.parent
    manifest = load_json(manifest_path)
    require(manifest.get("schema") == SCHEMA, "Manifest schema is not P9-17 packaged large-roster evidence.")
    require(manifest.get("taskId") == "P9_17_PACKAGED_NEXT_LARGE_ROSTER_EDITING", "Manifest task ID differs from P9-17.")
    require(manifest.get("diagnosticOnly") is not True, "P9-17 UIA observer diagnostic-only manifests cannot be accepted by the normal validator.")
    require(manifest.get("status") in ("completed", "passed"), "Manifest does not describe a completed route attempt.")

    repository_root = Path(str(manifest.get("repositoryRoot", ""))).resolve(strict=True)
    package = manifest.get("packageProvenance") or {}
    require(package.get("configuration") == "Release" and package.get("platform") == "x64" and package.get("architecture") == "x64", "Packaged application is not Windows x64 Release.")
    require(str(Path(str(package.get("cmakeHomeDirectory", ""))).resolve()).casefold() == str(repository_root).casefold(), "Release CMake cache points to a different source tree.")
    build_mode = str(package.get("buildMode", ""))
    fallback_summary = None
    build_receipt_summary = None
    if build_mode == "exact-current-source-release":
        require(int(package.get("buildExitCode", -1)) == 0, "Exact-current-source Release build did not exit successfully.")
        require(bool(package.get("exactCurrentSourceBuild", False)), "Package provenance does not identify the exact current-source Release build.")
        current_source_inventory = verify_source_inventory(evidence_root, package["currentSourceInventory"], repository_root)
        product_inventory = verify_source_inventory(evidence_root, package["productSourceInventory"], repository_root)
        harness_inventory = verify_source_inventory(evidence_root, package["harnessSourceInventory"], repository_root)
        build_receipt_summary = verify_exact_current_source_build(package, evidence_root, repository_root)
    elif build_mode == "fresh-exact-tree-release":
        require(int(package.get("buildExitCode", -1)) == 0, "Fresh Release build did not exit successfully.")
        require(bool(package.get("freshExactTreeBuild", False)), "Package provenance does not claim the successful fresh exact-tree build.")
        product_inventory = verify_source_inventory(evidence_root, package["productSourceInventory"], repository_root)
        harness_inventory = verify_source_inventory(evidence_root, package["harnessSourceInventory"], repository_root)
    elif build_mode == "verified-accepted-p916-package-fallback":
        fallback_summary = verify_p916_package_fallback(package, evidence_root, repository_root)
        product_inventory = {
            "path": package["productSourceInventory"]["relativePath"],
            "sha256": package["productSourceInventory"].get("treeSha256"),
            "fileCount": package["productSourceInventory"].get("fileCount"),
        }
        harness_inventory = {
            "path": package["harnessSourceInventory"]["relativePath"],
            "sha256": package["harnessSourceInventory"].get("treeSha256"),
            "fileCount": package["harnessSourceInventory"].get("fileCount"),
        }
    else:
        raise ValueError(f"Unsupported P9-17 package build mode: {build_mode}.")
    package_root = Path(str(package.get("packageRoot", ""))).resolve(strict=True)
    if build_mode == "fresh-exact-tree-release":
        require(evidence_root.resolve() in package_root.parents, "Fresh package root is outside this run evidence directory.")
    package_inventory = verify_package_inventory(evidence_root, package["packageInventory"], package_root)
    app_path = Path(str(package.get("applicationPath", ""))).resolve(strict=True)
    require(package_root in app_path.parents, "Packaged executable is outside the package root.")
    require(sha256_file(app_path) == str(package.get("applicationSha256", "")), "Packaged executable hash differs from provenance.")
    require(str(app_path).casefold().endswith("classmngrnext.exe"), "Application path is not the packaged ClassMngr Next executable.")
    require(int(package.get("machineType", 0)) == 0x8664, "Packaged executable is not AMD64 x64.")

    fixture = manifest.get("fixture") or {}
    preparation_path = verify_recorded_file(evidence_root, fixture["preparationReceipt"])
    receipt = load_json(preparation_path)
    require(receipt.get("schema") == FIXTURE_SCHEMA and receipt.get("status") == "passed", "Preparation receipt is not a passed P9-17 fixture.")
    require(receipt.get("baseWorkspaceSha256") == BASE_SHA256, "Preparation receipt base-workspace fingerprint differs from the pinned source.")
    require(receipt.get("seedSqlSha256") == SEED_SHA256, "Preparation receipt seed SQL fingerprint differs from the pinned source.")
    require(receipt.get("preparerSha256") == fixture.get("fixturePreparerSha256"), "Preparation receipt preparer source hash differs from its recorded fixture source hash.")
    require(receipt.get("workspaceSha256") == fixture["preparationReceipt"].get("workspaceSha256"), "Prepared fixture hash differs between receipt and manifest.")
    before_path = verify_recorded_file(evidence_root, fixture["beforeSnapshot"])
    after_path = verify_recorded_file(evidence_root, fixture["afterSnapshot"])
    before = load_json(before_path)
    after = load_json(after_path)
    snapshot_comparison = validate_snapshot_pair(receipt, before, after)
    require(before.get("workspaceSha256") == receipt.get("workspaceSha256"), "Before snapshot does not match the prepared fixture bytes.")
    require(before.get("identity") == receipt.get("identity"), "Before snapshot identity differs from the preparation receipt.")

    process = manifest.get("process") or {}
    process_id = int(process.get("processId", -1))
    main_handle = int(process.get("mainWindowHandle", -1))
    require(process_id > 0 and main_handle > 0, "Measured process PID or main HWND is missing.")
    require(int(process.get("windowProcessId", -1)) == process_id, "Main HWND is not owned by the measured PID.")
    receipt_execution_context = None
    if isinstance(build_receipt_summary, dict):
        receipt_execution_context = (build_receipt_summary.get("executionContext") or {}).get("build")
    execution_context_summary = validate_package_execution_context(package, process, receipt_execution_context)
    allowed_window_handles = {main_handle}
    allowed_window_handles.update(int(handle) for handle in process.get("dialogWindowHandles", []))
    require(str(Path(str(process.get("processImagePath", ""))).resolve()).casefold() == str(app_path).casefold(), "Live process image path differs from the packaged executable.")
    require(process.get("processImageSha256") == package.get("applicationSha256"), "Live process image hash differs from the packaged executable.")
    exit_record = manifest.get("processExit") or {}
    require(bool(exit_record.get("observed", False)) and bool(exit_record.get("normalExit", False)), "Natural process exit was not observed.")
    require(int(exit_record.get("exitCode", -1)) == 0, "Packaged process exit code is not zero.")
    require(exit_record.get("closeMethod") == "WindowPattern.Close" and not bool(exit_record.get("forcedTermination", True)), "Process did not exit through a natural UIA window close.")

    route = manifest.get("route") or {}
    natural_close_summary = validate_natural_process_close_action(route, exit_record)
    workspace_open = route.get("workspaceOpen") or {}
    require(Path(str(workspace_open.get("selectedPath", ""))).resolve() == Path(str(fixture.get("workspacePath", ""))).resolve(), "Fixture was not opened through the measured UIA file dialog.")
    require(int(workspace_open.get("dialogWindowHandle", -1)) in allowed_window_handles - {main_handle}, "Workspace file dialog HWND is not included in measured-process dialog evidence.")
    actions = route.get("actions")
    require(isinstance(actions, list), "Route action evidence is missing.")
    action_kinds = [str(action.get("actionKind", "")) for action in actions]
    required_actions = {
        "open-teacher-profile", "navigate-classes", "select-target-class", "open-roster-tab",
        "edit-target-start", "edit-target-middle", "edit-target-end", "save-roster",
        "leave-roster-section", "navigate-my_workspace", "reenter-roster-tab",
    }


    require(required_actions.issubset(set(action_kinds)), f"Required UIA actions are missing: {sorted(required_actions - set(action_kinds))}.")
    roster_tab_selection = validate_roster_tab_selection_actions(actions)
    for action in actions:
        target = action.get("target")
        if isinstance(target, dict) and target.get("processId") is not None:
            require(int(target["processId"]) == process_id, f"UIA action target PID differs from measured process: {action.get('actionKind')}.")
        if isinstance(target, dict) and target.get("ownerWindowHandle") is not None:
            require(int(target["ownerWindowHandle"]) in allowed_window_handles, f"UIA action target HWND is not owned by the measured process: {action.get('actionKind')}.")
    route_events = route.get("routeEvents")
    require(isinstance(route_events, list), "Page-route events are missing.")
    targets = [str(event.get("targetPageId", "")) for event in route_events]
    require("my_workspace" in targets and "classes" in targets, "Route evidence does not prove Workspace entry and Classes navigation.")
    require(targets.count("classes") >= 2, "Route evidence does not prove Classes page re-entry.")
    require(action_kinds.count("navigate-classes") >= 2, "UIA route actions do not prove Classes page re-entry.")
    require(route.get("targetClassSelection", {}).get("selected") is True and int(route.get("targetClassSelection", {}).get("classId", -1)) == 1 and route.get("targetClassSelection", {}).get("className") == "Heavy Fixture Class 001", "Exact target class selection evidence is missing.")
    expected_accessible_name = str(receipt["identity"]["targetClassListAccessibleName"])
    reentry_search = route.get("reentryTargetClassSearch") or {}
    require(reentry_search.get("targetAccessibleName") == expected_accessible_name and reentry_search.get("matched") is True, "Re-entry Classes list search did not match the exact fixture-derived target accessible name.")
    reentry_pages = reentry_search.get("pages")
    require(isinstance(reentry_pages, list) and len(reentry_pages) >= 1 and any(int(page.get("matchingAccessibleNameCount", 0)) == 1 for page in reentry_pages), "Re-entry Classes list search does not contain one exact accessible-name match.")
    reentry_selection = route.get("reentryTargetClassSelection") or {}
    require(int(reentry_selection.get("classId", -1)) == 1 and reentry_selection.get("className") == "Heavy Fixture Class 001" and reentry_selection.get("listItemName") == expected_accessible_name and reentry_selection.get("expectedAccessibleName") == expected_accessible_name and reentry_selection.get("selected") is True, "Exact target class was not selected through UIA during re-entry.")
    lifecycle = route.get("lifecycleEvents")
    require(isinstance(lifecycle, list), "Page lifecycle events are missing.")
    class_activations = [event for event in lifecycle if event.get("pageKey") == "classes" and event.get("state") == "activated"]
    class_suspensions = [event for event in lifecycle if event.get("pageKey") == "classes" and event.get("state") == "suspended"]
    require(len(class_activations) >= 2 and len(class_suspensions) >= 1, "Lifecycle evidence does not prove Classes activate/suspend/re-entry.")
    model_counts = route.get("modelCounts") or {}
    require(int(model_counts.get("rosterRows", -1)) == 100 and int(model_counts.get("rosterColumns", -1)) == 156, "UIA GridPattern dimensions differ from the 100x156 fixture profile.")
    require(int(model_counts.get("verifiedHeaderCount", -1)) == 156, "UIA header evidence does not cover all 156 columns.")
    require(int(model_counts.get("preSuspensionRootModelRows", -1)) >= 100, "Lifecycle model evidence does not retain the loaded roster row count.")
    section_suspension_summary = validate_section_suspension_checkpoint(
        evidence_root, route, process_id, main_handle
    )
    post_priming_absence_summary = validate_roster_absence_checkpoint(
        evidence_root, route, route.get("postPrimingClearCheckpoint"), process_id,
        main_handle, "cycle-baseline-select-class-details", "Post-priming",
    )
    cycle_absence_summaries = []
    for cycle_number, record in enumerate(route.get("cycleRecords", []), start=1):
        cycle_tag = f"{cycle_number:02d}"
        cycle_absence_summaries.append(validate_roster_absence_checkpoint(
            evidence_root, route, record.get("suspensionCheckpoint"), process_id,
            main_handle, f"cycle-{cycle_tag}-leave-roster", f"Measured cycle {cycle_number}",
        ))
    screenshots = route.get("screenshots")
    require(isinstance(screenshots, list) and len(screenshots) >= 3, "Action-bound roster screenshots are incomplete.")
    for screenshot in screenshots:
        verify_recorded_file(evidence_root, screenshot)
        require(int(screenshot.get("processId", -1)) == process_id and int(screenshot.get("windowHandle", -1)) in allowed_window_handles, "Screenshot is not bound to the measured process and one of its verified HWNDs.")
    uia_snapshots = route.get("uiaSnapshots")
    require(isinstance(uia_snapshots, list) and len(uia_snapshots) >= 3, "Action-bound UIA snapshots are incomplete.")
    uia_snapshots_by_path: dict[str, dict[str, Any]] = {}
    for snapshot_record in uia_snapshots:
        verify_recorded_file(evidence_root, snapshot_record)
        require(int(snapshot_record.get("processId", -1)) == process_id and int(snapshot_record.get("windowHandle", -1)) in allowed_window_handles, "UIA snapshot is not bound to the measured process and one of its verified HWNDs.")
        relative_path = str(snapshot_record.get("relativePath", ""))
        require(relative_path and relative_path not in uia_snapshots_by_path,
                f"UIA snapshot paths are missing or duplicated: {relative_path}.")
        uia_snapshots_by_path[relative_path] = snapshot_record
    class_selection_summary = validate_class_selection_evidence(
        evidence_root, route, receipt, actions, uia_snapshots, process_id, main_handle
    )
    checks = route.get("readBackChecks") or {}
    expected_by_name = {str(target["name"]): str(target["afterValue"]) for target in receipt["expectedEditTargets"]}
    for phase in ("afterEdit", "afterSave", "afterReentry"):
        records = checks.get(phase)
        require(isinstance(records, list) and len(records) == 3, f"UIA persisted value readbacks are incomplete at {phase}.")
        observed = {str(row.get("name")): str(row.get("actual", "")) for row in records if bool(row.get("matched", False))}
        require(observed == expected_by_name, f"UIA cell readbacks do not match all three deterministic edits at {phase}.")
    cycle_records = route.get("cycleRecords")
    require(isinstance(cycle_records, list) and len(cycle_records) == 10,
            "The repeated large-roster route must record exactly ten measured UIA cycles.")
    memory_windows_by_id = {
        str(window.get("windowId", "")): window
        for window in (manifest.get("memory") or {}).get("windows", [])
        if isinstance(window, dict)
    }
    original_targets = {str(target["name"]): target for target in receipt["expectedEditTargets"]}
    for cycle_number, record in enumerate(cycle_records, start=1):
        require(int(record.get("cycleNumber", -1)) == cycle_number, "Repeated roster cycle records are missing or reordered.")
        require(int(record.get("targetClassId", -1)) == 1
                and record.get("targetClassName") == receipt["identity"]["targetClassName"]
                and record.get("selectedClassAccessibleName") == expected_accessible_name
                and record.get("classSelectionVerified") is True,
                f"Cycle {cycle_number} does not remain bound to the UIA-selected fixture class.")
        expected_before = {
            name: str(target["afterValue"] if cycle_number % 2 else target["beforeValue"])
            for name, target in original_targets.items()
        }
        expected_after = {
            name: str(target["beforeValue"] if cycle_number % 2 else target["afterValue"])
            for name, target in original_targets.items()
        }
        for phase_key, phase_expected_values, path_key, suffix in (
            ("initialReadbacks", expected_before, "loadedContractRelativePath", "roster-loaded-contract"),
            ("editedReadbacks", expected_after, "editedContractRelativePath", "edited-contract"),
            ("savedReadbacks", expected_after, "savedContractRelativePath", "post-save-contract"),
        ):
            values = record.get(phase_key)
            require(isinstance(values, list) and len(values) == 3,
                    f"Cycle {cycle_number} UIA readbacks are incomplete at {phase_key}.")
            actual = {str(item.get("name")): str(item.get("actual", "")) for item in values if bool(item.get("matched", False))}
            require(actual == phase_expected_values, f"Cycle {cycle_number} values differ from the required toggle parity at {phase_key}.")
            contract_name = f"cycle-{cycle_number:02d}-{suffix}"
            relative_path = f"uia/{contract_name}.json"
            require(record.get(path_key) == relative_path and relative_path in uia_snapshots_by_path,
                    f"Cycle {cycle_number} {phase_key} UIA contract is not linked to its exact evidence artifact.")
            validate_cycle_uia_contract(
                evidence_root,
                uia_snapshots_by_path[relative_path],
                contract_name,
                process_id,
                main_handle,
                original_targets,
                phase_expected_values,
                values,
            )
        require(record.get("parity") == ("after-to-before" if cycle_number % 2 else "before-to-after"),
                f"Cycle {cycle_number} parity metadata differs from the alternating expected values.")
        require(int(record.get("saveActionIndex", -1)) >= 0
                and int(record.get("saveActionIndex", -1)) < len(actions)
                and str(actions[int(record["saveActionIndex"])].get("actionKind", "")) == "save-roster",
                f"Cycle {cycle_number} save action index does not identify the UIA save action.")
        save_action = actions[int(record["saveActionIndex"])]
        require(int(save_action.get("cycleNumber", -1)) == cycle_number
                and save_action.get("measurementRole") == "measured-cycle-save",
                f"Cycle {cycle_number} save action index is not tagged to that measured cycle.")
        require(bool(save_action.get("savedStateObservedAtUtc")) and bool(save_action.get("completedAtUtc")),
                f"Cycle {cycle_number} save action lacks completion and saved-state readback evidence.")
        save_issued = parse_utc_timestamp(save_action.get("issuedAtUtc"), f"Cycle {cycle_number} save issued")
        save_observed = parse_utc_timestamp(save_action.get("savedStateObservedAtUtc"), f"Cycle {cycle_number} saved-state observed")
        require(save_issued <= save_observed,
                f"Cycle {cycle_number} saved-state timestamp precedes the save action.")
        loaded_window = memory_windows_by_id.get(str(record.get("rosterLoadedWindowId", "")))
        post_save_window = memory_windows_by_id.get(str(record.get("postSaveWindowId", "")))
        require(isinstance(loaded_window, dict) and isinstance(post_save_window, dict),
                f"Cycle {cycle_number} save action cannot be bounded by its loaded and post-save windows.")
        loaded_completed = parse_utc_timestamp(loaded_window.get("completedAtUtc"), f"Cycle {cycle_number} loaded window completed")
        post_save_started = parse_utc_timestamp(post_save_window.get("startedAtUtc"), f"Cycle {cycle_number} post-save window started")
        require(loaded_completed <= save_issued <= save_observed <= post_save_started,
                f"Cycle {cycle_number} save action timestamps do not fall between its loaded and post-save windows.")
    final_cycle_readbacks = route.get("finalCycleReadback")
    require(isinstance(final_cycle_readbacks, list) and len(final_cycle_readbacks) == 3,
            "Final measured-cycle re-entry readbacks are incomplete.")
    final_cycle_actual = {str(item.get("name")): str(item.get("actual", "")) for item in final_cycle_readbacks if bool(item.get("matched", False))}
    require(final_cycle_actual == expected_by_name,
            "Final measured-cycle re-entry does not restore all three original after-values.")
    validate_final_cycle_uia_contract(
        evidence_root,
        uia_snapshots_by_path,
        route.get("finalCycleRosterContractRelativePath"),
        process_id,
        main_handle,
        original_targets,
        expected_by_name,
        final_cycle_readbacks,
    )
    memory_summary = validate_memory(manifest.get("memory") or {}, route, process_id, main_handle)
    require_memory_cleanup_gates_passed(memory_summary)

    evidence_inventory = manifest.get("evidenceInventory")
    inventory_summary = None
    if evidence_inventory:
        inventory_summary = validate_evidence_inventory(evidence_root, evidence_inventory)
    acceptance_eligible = p917_acceptance_eligible(build_mode)
    return {
        "schema": SCHEMA,
        "status": "passed" if acceptance_eligible else "diagnostic-only",
        "acceptanceEligible": acceptance_eligible,
        "diagnosticReason": None if acceptance_eligible else (
            "Historical/supplemental build modes do not prove the exact-current-source VS2022/v143 receipt contract."
        ),
        "taskId": manifest["taskId"],
        "runId": manifest.get("runId"),
        "package": {
            "buildMode": package["buildMode"],
            "configuration": package["configuration"],
            "platform": package["platform"],
            "freshExactTreeBuild": bool(package.get("freshExactTreeBuild", False)),
            "exactCurrentSourceBuild": bool(package.get("exactCurrentSourceBuild", False)),
            "incrementalBuild": bool(package.get("incrementalBuild", False)),
            "releaseBuildReceipt": build_receipt_summary,
            "productSource": product_inventory,
            "harnessSource": harness_inventory,
            "packageInventory": package_inventory,
            "applicationSha256": package["applicationSha256"],
            "fallbackProvenance": fallback_summary,
        },
        "fixture": {
            "workspaceSha256Before": receipt["workspaceSha256"],
            "targetClassId": receipt["identity"]["targetClassId"],
            "targetClassName": receipt["identity"]["targetClassName"],
            "targetClassListAccessibleName": receipt["identity"]["targetClassListAccessibleName"],
            "rosterRows": receipt["identity"]["targetRosterRowCount"],
            "displayedRosterColumns": receipt["identity"]["targetDisplayedRosterColumnCount"],
            "tableCount": receipt["identity"].get("tableCount", before["workspaceTableCount"]),
            "snapshotComparison": snapshot_comparison,
        },
        "process": {
            "processId": process_id,
            "mainWindowHandle": main_handle,
            "sessionId": int(process["sessionId"]),
            "executionContext": execution_context_summary,
            "naturalExitCode": 0,
            "forcedTermination": False,
            "naturalClose": natural_close_summary,
        },
        "classSelection": class_selection_summary,
        "rosterTabSelection": roster_tab_selection,
        "sectionSuspension": section_suspension_summary,
        "postPrimingRosterAbsence": post_priming_absence_summary,
        "measuredCycleRosterAbsenceCount": len(cycle_absence_summaries),
        "memory": memory_summary,
        "evidenceInventory": inventory_summary,
    }


PAYLOAD_DIAGNOSTIC_SCHEMA = "classmngr-phase9-p9-17-payload-diagnostic-v1"
PAYLOAD_FIXTURE_SCHEMA = "classmngr-phase9-p9-17-payload-fixture-diagnostic-v1"
PAYLOAD_PROBE_COORDINATES = {(0, 6), (49, 77), (99, 155)}
PAYLOAD_SPARSE_SENTINEL = "x"
PAYLOAD_SPARSE_SENTINEL_COUNT = 15000 - len(PAYLOAD_PROBE_COORDINATES)


def _payload_require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(f"P9-17 payload diagnostic comparison failed: {message}")


def _payload_readbacks(rows: Any, label: str) -> list[dict[str, Any]]:
    _payload_require(isinstance(rows, list) and len(rows) == 3, f"{label} must contain three probe readbacks.")
    signature = []
    for row in rows:
        _payload_require(isinstance(row, dict), f"{label} contains a malformed readback.")
        signature.append({
            key: row.get(key)
            for key in ("name", "row", "modelColumn", "storedColumn", "expected", "actual", "matched")
        })
        _payload_require(row.get("matched") is True and row.get("expected") == row.get("actual"),
                         f"{label} contains an unmatched probe readback.")
    _payload_require(len({str(row["name"]) for row in signature}) == 3,
                     f"{label} contains duplicated probe names.")
    return signature


def _payload_arm_action_signature(route: dict[str, Any]) -> list[dict[str, Any]]:
    actions = route.get("actions")
    _payload_require(isinstance(actions, list) and actions, "UIA action history is missing.")
    fields = (
        "actionKind", "method", "cycleNumber", "measurementRole", "row", "column",
        "modelColumn", "storedColumn", "expectedBeforeValue", "valueAfter",
        "selectedTabName", "expectedSection", "horizontalPercent", "verticalPercent",
        "valuePatternSetValueSucceeded", "immediateSetValueReadback",
        "immediateSetValueReadbackSource",
    )
    signature = []
    for action in actions:
        _payload_require(isinstance(action, dict), "UIA action history contains a malformed row.")
        target = action.get("target") if isinstance(action.get("target"), dict) else {}
        signature.append({
            **{key: action.get(key) for key in fields},
            "target": {
                key: target.get(key)
                for key in ("name", "controlType", "automationId")
            },
        })
    return signature


def _payload_arm_readback_signature(route: dict[str, Any]) -> dict[str, Any]:
    readbacks = route.get("readBackChecks")
    _payload_require(isinstance(readbacks, dict), "Route-level readback checks are missing.")
    route_signature = {
        phase: _payload_readbacks(readbacks.get(phase), f"route.readBackChecks.{phase}")
        for phase in ("beforeEdit", "afterEdit", "afterSave", "afterReentry", "afterCycles")
    }
    cycles = route.get("cycleRecords")
    _payload_require(isinstance(cycles, list) and len(cycles) == 10,
                     "Route does not contain all ten measured cycle records, including cycle 1.")
    cycle_signature = []
    for expected_number, cycle in enumerate(cycles, start=1):
        _payload_require(isinstance(cycle, dict) and int(cycle.get("cycleNumber", -1)) == expected_number,
                         f"Measured cycle {expected_number} is missing or reordered.")
        cycle_signature.append({
            "cycleNumber": expected_number,
            "parity": cycle.get("parity"),
            "initialReadbacks": _payload_readbacks(cycle.get("initialReadbacks"), f"cycle {expected_number} initial readbacks"),
            "editedReadbacks": _payload_readbacks(cycle.get("editedReadbacks"), f"cycle {expected_number} edited readbacks"),
            "savedReadbacks": _payload_readbacks(cycle.get("savedReadbacks"), f"cycle {expected_number} saved readbacks"),
        })
    return {"routeReadbacks": route_signature, "measuredCycles": cycle_signature}


def _payload_compare_table_snapshots(
    left_tables: Any,
    right_tables: Any,
    label: str,
    left_mode: str,
    right_mode: str,
    probe_values: dict[tuple[int, int], str] | None = None,
) -> dict[str, int]:
    _payload_require(isinstance(left_tables, dict) and isinstance(right_tables, dict),
                     f"{label} database table snapshots are missing.")
    _payload_require(set(left_tables) == set(right_tables), f"{label} database table names differ between arms.")
    table_row_counts: dict[str, int] = {}
    observed_custom: dict[str, int] = {
        "filledValues": 0,
        "sparseValues": 0,
        "sparseSentinelValues": 0,
        "sparseBlankValues": 0,
    }
    for table_name in sorted(left_tables):
        left_table = left_tables[table_name]
        right_table = right_tables[table_name]
        _payload_require(isinstance(left_table, dict) and isinstance(right_table, dict),
                         f"{label} table {table_name} snapshot is malformed.")
        for field in ("columns", "declaration", "rowCount"):
            _payload_require(left_table.get(field) == right_table.get(field),
                             f"{label} table {table_name} differs in {field}.")
        left_rows = left_table.get("rows")
        right_rows = right_table.get("rows")
        _payload_require(isinstance(left_rows, list) and isinstance(right_rows, list)
                         and len(left_rows) == len(right_rows) == int(left_table.get("rowCount", -1)),
                         f"{label} table {table_name} row count differs from its snapshot.")
        table_row_counts[table_name] = len(left_rows)
        if table_name != "roster_data":
            _payload_require(left_rows == right_rows, f"{label} table {table_name} rows differ between arms.")
            continue

        columns = [str(column) for column in left_table["columns"]]
        required_columns = {"class_id", "row_index", "col_index", "value"}
        _payload_require(required_columns <= set(columns), "roster_data snapshot lacks its row identity or value columns.")
        indexes = {column: columns.index(column) for column in required_columns}
        left_by_key: dict[tuple[int, int, int], list[Any]] = {}
        right_by_key: dict[tuple[int, int, int], list[Any]] = {}
        for target, rows in ((left_by_key, left_rows), (right_by_key, right_rows)):
            for row in rows:
                _payload_require(isinstance(row, list) and len(row) == len(columns),
                                 f"{label} roster_data contains a malformed row.")
                key = tuple(int(row[indexes[field]]) for field in ("class_id", "row_index", "col_index"))
                _payload_require(key not in target, f"{label} roster_data contains a duplicate cell key.")
                target[key] = row
        _payload_require(set(left_by_key) == set(right_by_key), f"{label} roster_data record keys differ between arms.")
        custom_record_count = 0
        value_index = indexes["value"]
        for key in sorted(left_by_key):
            left_row = left_by_key[key]
            right_row = right_by_key[key]
            class_id, row_index, column_index = key
            is_custom = class_id == 1 and 6 <= column_index <= 155
            if not is_custom:
                _payload_require(left_row == right_row,
                                 f"{label} non-custom roster record differs at {key}.")
                continue
            custom_record_count += 1
            for index, (left_value, right_value) in enumerate(zip(left_row, right_row)):
                if index != value_index:
                    _payload_require(left_value == right_value,
                                     f"{label} custom roster record identity differs at {key}.")
            left_value = left_row[value_index]
            right_value = right_row[value_index]
            observed_custom["filledValues"] += int(left_mode == "filled" and left_value not in (None, ""))
            observed_custom["sparseValues"] += int(right_mode == "sparse" and right_value not in (None, ""))
            observed_custom["sparseSentinelValues"] += int(
                right_mode == "sparse" and right_value == PAYLOAD_SPARSE_SENTINEL
            )
            observed_custom["sparseBlankValues"] += int(right_value in (None, ""))
            if (row_index, column_index) in PAYLOAD_PROBE_COORDINATES:
                _payload_require(left_value == right_value and left_value not in (None, ""),
                                 f"{label} preserved probe value differs between arms at {(row_index, column_index)}.")
                if probe_values is not None:
                    _payload_require(left_value == probe_values[(row_index, column_index)],
                                     f"{label} preserved probe value differs from its measured readback at {(row_index, column_index)}.")
            else:
                _payload_require(left_mode == "filled" and left_value not in (None, ""),
                                 f"{label} filled-arm custom payload is unexpectedly blank at {(row_index, column_index)}.")
                _payload_require(right_mode == "sparse" and right_value == PAYLOAD_SPARSE_SENTINEL,
                                 f"{label} sparse-arm custom payload is not the compact sentinel at {(row_index, column_index)}.")
        _payload_require(custom_record_count == 15000,
                         f"{label} custom-cell record count is {custom_record_count}, expected 15000.")
        _payload_require(observed_custom == {
            "filledValues": 15000,
            "sparseValues": 15000,
            "sparseSentinelValues": PAYLOAD_SPARSE_SENTINEL_COUNT,
            "sparseBlankValues": 0,
        }, f"{label} custom-cell payload counts differ from the filled/sparse sentinel contract: {observed_custom}.")
        if left_mode == "filled" and right_mode == "sparse":
            name_rows = {0: [], 1: []}
            for row in left_rows:
                if int(row[indexes["class_id"]]) == 1 and int(row[indexes["col_index"]]) in name_rows:
                    name_rows[int(row[indexes["col_index"]])].append(
                        (int(row[indexes["row_index"]]), row[value_index])
                    )
            for column in (0, 1):
                values = name_rows[column]
                _payload_require(len(values) == 100
                                 and {row_index for row_index, _ in values} == set(range(100))
                                 and all(value not in (None, "") for _, value in values)
                                 and len({value for _, value in values}) == 100,
                                 f"{label} does not preserve 100 unique nonempty name cells in column {column}.")
    return table_row_counts


def _payload_validate_evidence_inventory(evidence_root: Path, manifest: dict[str, Any]) -> dict[str, Any]:
    record = manifest.get("evidenceInventory")
    _payload_require(isinstance(record, dict), "Arm evidence inventory record is missing.")
    inventory_path = verify_recorded_file(evidence_root, record)
    inventory = load_json(inventory_path)
    _payload_require(inventory.get("schema") == "classmngr-phase9-p9-17-evidence-files-v1"
                     and Path(str(inventory.get("root", ""))).resolve() == evidence_root.resolve(),
                     "Arm evidence inventory schema or root is invalid.")
    files = inventory.get("files")
    _payload_require(isinstance(files, list)
                     and type(inventory.get("fileCount")) is int
                     and inventory.get("fileCount") == len(files)
                     and type(record.get("fileCount")) is int
                     and record.get("fileCount") == len(files),
                     "Arm evidence inventory file count differs from its rows or manifest record.")
    inventory_sha = canonical_inventory_fingerprint(files)
    _payload_require(inventory.get("sha256") == inventory_sha
                     and record.get("treeSha256") == inventory_sha,
                     "Arm evidence inventory tree hash is invalid.")
    validate_evidence_inventory(evidence_root, record)
    recorded_paths = {str(row.get("path", "")) for row in files}
    actual_paths = {
        path.relative_to(evidence_root).as_posix()
        for path in evidence_root.rglob("*")
        if path.is_file() and path.relative_to(evidence_root).as_posix() not in {"manifest.json", "evidence-files.json"}
    }
    _payload_require(recorded_paths == actual_paths,
                     "Arm evidence inventory omits or adds evidence files.")
    return {"path": str(inventory_path), "sha256": inventory_sha, "fileCount": len(files)}


def _payload_verify_source_rows(files: Any, repository_root: Path, label: str) -> dict[str, dict[str, Any]]:
    _payload_require(isinstance(files, list), f"{label} source inventory rows are missing.")
    rows = _source_row_map(files, label)
    for row in files:
        source_path = (repository_root / str(row["path"])).resolve(strict=True)
        _payload_require(repository_root.resolve() in source_path.parents,
                         f"{label} source path escapes the repository: {row['path']}.")
        _payload_require(source_path.stat().st_size == int(row["sizeBytes"])
                         and sha256_file(source_path) == str(row["sha256"]),
                         f"{label} source file differs from its inventory: {row['path']}.")
    return rows


def _payload_validate_v3_release_contract(
    evidence_root: Path,
    repository_root: Path,
    package: dict[str, Any],
    manifest: dict[str, Any],
    receipt: dict[str, Any],
    allowed_receipt_root: Path,
    receipt_inventory_path: Path,
) -> dict[str, Any]:
    build_directory = (repository_root / "build/windows-x64-release").resolve(strict=True)
    build_directory_value = str(receipt.get("buildDirectory", ""))
    package_build_directory_value = str(package.get("buildDirectory", ""))
    _payload_require(bool(build_directory_value) and Path(build_directory_value).is_absolute()
                     and Path(build_directory_value).resolve(strict=True) == build_directory
                     and bool(package_build_directory_value)
                     and Path(package_build_directory_value).is_absolute()
                     and Path(package_build_directory_value).resolve(strict=True) == build_directory,
                     "Arm Release receipt/package build directory is missing or not the standard x64 Release tree.")
    cmake_home_value = str(package.get("cmakeHomeDirectory", ""))
    _payload_require(bool(cmake_home_value) and Path(cmake_home_value).is_absolute()
                     and Path(cmake_home_value).resolve() == repository_root,
                     "Arm package CMake source root differs from its repository root.")

    cache_path_value = str(receipt.get("cachePath", ""))
    _payload_require(bool(cache_path_value) and Path(cache_path_value).is_absolute(),
                     "Arm Release receipt CMake cache path is missing or not absolute.")
    cache_path = Path(cache_path_value).resolve(strict=True)
    expected_cache_path = build_directory / "CMakeCache.txt"
    package_cache_value = str(package.get("cachePath", ""))
    _payload_require(cache_path == expected_cache_path
                     and bool(package_cache_value)
                     and Path(package_cache_value).is_absolute()
                     and Path(package_cache_value).resolve(strict=True) == cache_path,
                     "Arm Release receipt/package CMake cache path differs from the standard build cache.")
    cache_sha = sha256_file(cache_path)
    _payload_require(cache_sha == str(receipt.get("cacheSha256", ""))
                     == str(package.get("cacheSha256", "")),
                     "Arm Release CMake cache hash differs from the receipt or package provenance.")
    cache_lines = cache_path.read_text(encoding="utf-8", errors="replace").splitlines()
    for key, expected in (
        ("CMAKE_HOME_DIRECTORY", str(repository_root)),
        ("CLASSMNGR_NEXT_RELEASE_PACKAGE", "ON"),
    ):
        values = [line.split("=", 1)[1] for line in cache_lines if line.startswith(key + ":") and "=" in line]
        _payload_require(len(values) == 1 and (Path(values[0]).resolve() == repository_root if key == "CMAKE_HOME_DIRECTORY" else values[0] == expected),
                         f"Arm Release CMake cache does not contain the expected {key} value.")
    cache_toolchain = read_cmake_release_toolchain(cache_path)
    toolchain_summary = validate_exact_release_build_toolchain(receipt, cache_toolchain)
    for package_field, receipt_field in (
        ("generator", "generator"),
        ("generatorPlatform", "generatorPlatform"),
        ("generatorToolset", "generatorToolset"),
        ("generatorInstance", "generatorInstance"),
        ("windowsSdkVersion", "windowsSdkVersion"),
        ("compiler", "compiler"),
    ):
        if package_field == "generatorInstance":
            matches = _normalized_windows_path(package.get(package_field)) == _normalized_windows_path(receipt.get(receipt_field))
        else:
            matches = package.get(package_field) == receipt.get(receipt_field)
        _payload_require(matches, f"Arm package {package_field} differs from the v3 Release receipt.")
    _payload_require(receipt.get("packageOption") == "CLASSMNGR_NEXT_RELEASE_PACKAGE=ON",
                     "Arm Release receipt does not prove the Next Release package option.")
    _payload_require(receipt.get("incrementalBuild") is True and package.get("incrementalBuild") is True,
                     "Arm Release receipt/package does not identify the authorized incremental build.")
    _payload_require(isinstance(receipt.get("targets"), list)
                     and {"ClassMngrNext", "ClassMngrNextResourcePackage"}.issubset(set(receipt["targets"])),
                     "Arm Release receipt omits the application or resource package build target.")
    compiler = receipt.get("compiler")
    _payload_require(isinstance(compiler, dict), "Arm v3 Release receipt omits compiler identity evidence.")
    compiler_path = Path(str(compiler.get("path", ""))).resolve(strict=True)
    _payload_require(_normalized_windows_path(compiler_path) == _normalized_windows_path(P917_RELEASE_COMPILER_PATH)
                     and sha256_file(compiler_path) == str(compiler.get("sha256", "")),
                     "Arm Release receipt compiler path/hash differs from the installed VS2022 compiler.")
    _payload_require(Path(P917_RELEASE_WINDOWS_SDK_INCLUDE_ROOT).is_dir(),
                     "Required Windows SDK include tree is missing.")

    preflight_value = str(receipt.get("preflightPath", ""))
    _payload_require(bool(preflight_value) and Path(preflight_value).is_absolute(),
                     "Arm Release receipt preflight path is missing or not absolute.")
    preflight_path = Path(preflight_value).resolve(strict=True)
    _payload_require(allowed_receipt_root in preflight_path.parents
                     and sha256_file(preflight_path) == str(receipt.get("preflightSha256", "")),
                     "Arm Release preflight is outside the exact-build evidence root or its hash changed.")
    preflight = load_json(preflight_path)
    _payload_require(preflight.get("schema") == "classmngr-phase9-p9-17-standard-token-build-preflight-v2"
                     and preflight.get("status") == "passed"
                     and preflight.get("cacheSha256") == cache_sha
                     and Path(str(preflight.get("cachePath", ""))).resolve(strict=True) == cache_path
                     and Path(str(preflight.get("sourceInventoryPath", ""))).resolve(strict=True) == receipt_inventory_path
                     and preflight.get("sourceInventoryFileSha256") == receipt.get("sourceInventoryFileSha256")
                     and preflight.get("sourceSha256") == receipt.get("sourceSha256")
                     and type(preflight.get("sourceFileCount")) is int
                     and preflight.get("sourceFileCount") == receipt.get("sourceFileCount")
                     and preflight.get("packageOption") == receipt.get("packageOption")
                     and preflight.get("executionContext") == (receipt.get("executionContext") or {}).get("start"),
                     "Arm Release preflight does not match the cache, source inventory, package option, or build context.")
    for field in (
        "generator", "generatorPlatform", "generatorToolset", "generatorInstance",
        "windowsSdkVersion", "generatedProjectToolchain", "compiler",
    ):
        _payload_require(preflight.get(field) == receipt.get(field),
                         f"Arm Release preflight {field} differs from the v3 receipt.")
    _payload_require(preflight.get("resourceOutputsGeneratedByBuild") is True
                     and type(preflight.get("expectedBakedAssetFileCount")) is int
                     and preflight.get("expectedBakedAssetFileCount") == 192,
                     "Arm Release preflight omits generated-resource build evidence.")

    helper_path = Path(str(receipt.get("buildHelperPath", ""))).resolve(strict=True)
    expected_helper_path = (repository_root / P917_BUILD_HELPER_RELATIVE).resolve(strict=True)
    _payload_require(helper_path == expected_helper_path
                     and sha256_file(helper_path) == str(receipt.get("buildHelperSha256", "")),
                     "Arm Release receipt build-helper path/hash differs from the current tracked helper.")

    build = receipt.get("build") or {}
    build_log_value = str(build.get("logPath", ""))
    _payload_require(bool(build_log_value) and Path(build_log_value).is_absolute(),
                     "Arm Release build log path is missing or not absolute.")
    build_log_path = Path(build_log_value).resolve(strict=True)
    _payload_require(allowed_receipt_root in build_log_path.parents
                     and sha256_file(build_log_path) == str(build.get("logSha256", ""))
                     and package.get("buildLogSha256") == build.get("logSha256"),
                     "Arm Release build log is outside the exact-build evidence root or its hash changed.")
    build_log_text = build_log_path.read_text(encoding="utf-8", errors="replace").casefold()
    _payload_require(str(compiler.get("path", "")).casefold() in build_log_text
                     and _normalized_windows_path(build.get("actualCompilerPath")) == _normalized_windows_path(compiler.get("path"))
                     and build.get("compilerObservedInLog") is True,
                     "Arm Release build log does not prove invocation of the receipt compiler.")
    _payload_require(package.get("buildLogPath") == build_log_value,
                     "Arm package build-log path differs from its Release receipt.")

    execution_context_summary = validate_package_execution_context(
        package, manifest.get("process") or {}, receipt.get("executionContext")
    )

    release_executable_value = str(receipt.get("releaseExecutablePath", ""))
    _payload_require(bool(release_executable_value) and Path(release_executable_value).is_absolute(),
                     "Arm Release executable path is missing or not absolute.")
    release_executable = Path(release_executable_value).resolve(strict=True)
    expected_executable = build_directory / "next-package-build/Release/ClassMngr.exe"
    _payload_require(release_executable == expected_executable
                     and package.get("releaseExecutablePath") == release_executable_value,
                     "Arm Release receipt executable does not come from the standard build output.")
    release_sha = sha256_file(release_executable)
    _payload_require(release_sha == str(receipt.get("releaseExecutableSha256", ""))
                     == str(package.get("releaseExecutableSha256", ""))
                     == str(package.get("applicationSha256", ""))
                     and _pe_machine(release_executable) == 0x8664,
                     "Arm Release executable hash or x64 machine identity differs from package provenance.")

    generated = receipt.get("generatedResources") or {}
    _payload_require(isinstance(generated, dict), "Arm v3 Release receipt omits generated-resource evidence.")
    generated_root = (build_directory / "generated/ClassMngrNext").resolve(strict=True)
    assets_root = Path(str(generated.get("bakedAssetsRoot", ""))).resolve(strict=True)
    translations_root = Path(str(generated.get("translationsRoot", ""))).resolve(strict=True)
    catalog_path = Path(str(generated.get("catalogPath", ""))).resolve(strict=True)
    _payload_require(Path(str(preflight.get("resourceRoot", ""))).resolve(strict=True) == assets_root,
                     "Arm Release preflight resource root differs from the baked asset output.")
    for path in (assets_root, translations_root, catalog_path):
        _payload_require(generated_root in path.parents,
                         f"Arm Release generated resource output escapes the build tree: {path}.")
    assets_sha, assets_files = _directory_inventory(assets_root)
    translations_sha, translations_files = _directory_inventory(translations_root)
    _payload_require(assets_sha == str(generated.get("bakedAssetsSha256", ""))
                     and type(generated.get("bakedAssetsFileCount")) is int
                     and len(assets_files) == generated.get("bakedAssetsFileCount")
                     and translations_sha == str(generated.get("translationsSha256", ""))
                     and type(generated.get("translationsFileCount")) is int
                     and len(translations_files) == generated.get("translationsFileCount")
                     and sha256_file(catalog_path) == str(generated.get("catalogSha256", "")),
                     "Arm Release generated-resource files differ from their v3 receipt fingerprints.")
    resource_root = Path(str(package.get("resourceRoot", ""))).resolve(strict=True)
    resource_files = _source_row_map(_directory_inventory(resource_root)[1], "Staged resource")
    expected_resource_rows: dict[str, tuple[int, str]] = {}
    for row in assets_files:
        expected_resource_rows[("assets/" + str(row["path"])).casefold()] = (int(row["sizeBytes"]), str(row["sha256"]))
    for row in translations_files:
        expected_resource_rows[("assets/translations/" + str(row["path"])).casefold()] = (int(row["sizeBytes"]), str(row["sha256"]))
    expected_resource_rows["resource-catalog.json"] = (catalog_path.stat().st_size, str(generated["catalogSha256"]))
    actual_resource_rows = {
        path: (int(row["sizeBytes"]), str(row["sha256"])) for path, row in resource_files.items()
    }
    _payload_require(actual_resource_rows == expected_resource_rows,
                     "Arm staged resources differ from the Release receipt generated-resource outputs.")
    return {
        "buildDirectory": str(build_directory),
        "cachePath": str(cache_path),
        "cacheSha256": cache_sha,
        "toolchain": toolchain_summary,
        "preflightPath": str(preflight_path),
        "buildHelperPath": str(helper_path),
        "buildLogPath": str(build_log_path),
        "releaseExecutableSha256": release_sha,
        "executionContext": execution_context_summary,
        "generatedResourcesSha256": canonical_inventory_fingerprint(
            _directory_inventory(resource_root)[1]
        ),
    }


def _payload_validate_release_and_package_provenance(
    evidence_root: Path,
    manifest: dict[str, Any],
) -> dict[str, Any]:
    package = manifest.get("packageProvenance") or {}
    _payload_require(isinstance(package, dict), "Arm package provenance is missing or malformed.")
    repository_value = manifest.get("repositoryRoot")
    _payload_require(isinstance(repository_value, str) and bool(repository_value),
                     "Arm repository root is missing.")
    repository_root = Path(repository_value).resolve(strict=True)
    _payload_require(package.get("buildMode") == "exact-current-source-release"
                     and package.get("exactCurrentSourceBuild") is True,
                     "Arm is not bound to an exact-current-source Release package.")
    _payload_require(package.get("configuration") == "Release"
                     and package.get("platform") == "x64"
                     and package.get("architecture") == "x64",
                     "Arm package provenance is not Windows x64 Release.")

    allowed_receipt_root = (repository_root / "artifacts/phase9/p9-17-exact-release-builds").resolve(strict=True)
    receipt_value = str(package.get("releaseBuildReceiptPath", ""))
    _payload_require(bool(receipt_value) and Path(receipt_value).is_absolute(),
                     "Arm Release build receipt path is missing or not absolute.")
    receipt_path = Path(receipt_value).resolve(strict=True)
    _payload_require(allowed_receipt_root in receipt_path.parents,
                     "Arm Release build receipt is outside the P9-17 exact-build evidence root.")
    _payload_require(Path(str(package.get("acceptedReleaseReceiptPath", ""))).resolve(strict=True) == receipt_path,
                     "Arm package references a different accepted Release receipt.")
    receipt_sha = sha256_file(receipt_path)
    _payload_require(receipt_sha == str(package.get("releaseBuildReceiptSha256", ""))
                     == str(package.get("acceptedReleaseReceiptSha256", "")),
                     "Arm Release build receipt hash differs from package provenance.")
    receipt = load_json(receipt_path)
    _payload_require(receipt.get("schema") == P917_BUILD_RECEIPT_SCHEMA
                     and receipt.get("status") == "succeeded"
                     and receipt.get("configuration") == "Release"
                     and receipt.get("platform") == "x64",
                     "Arm Release receipt schema, status, or target configuration is invalid.")
    _payload_require(Path(str(receipt.get("repositoryRoot", ""))).resolve() == repository_root,
                     "Arm Release receipt references a different repository root.")
    build = receipt.get("build")
    validate_exact_build_exit_code(build)
    _payload_require(type(package.get("buildExitCode")) is int and package.get("buildExitCode") == 0,
                     "Arm package build exit code is not the exact JSON integer zero.")

    receipt_inventory_value = str(receipt.get("sourceInventoryPath", ""))
    _payload_require(bool(receipt_inventory_value) and Path(receipt_inventory_value).is_absolute(),
                     "Arm Release receipt source inventory path is missing or not absolute.")
    receipt_inventory_path = Path(receipt_inventory_value).resolve(strict=True)
    _payload_require(allowed_receipt_root in receipt_inventory_path.parents,
                     "Arm Release receipt source inventory is outside its exact-build evidence root.")
    _payload_require(Path(str(package.get("buildReceiptSourceInventoryPath", ""))).resolve(strict=True)
                     == receipt_inventory_path
                     and package.get("buildReceiptSourceInventoryFileSha256")
                     == receipt.get("sourceInventoryFileSha256"),
                     "Arm package source-inventory reference differs from its Release receipt.")
    _payload_require(sha256_file(receipt_inventory_path) == str(receipt.get("sourceInventoryFileSha256", "")),
                     "Arm Release receipt source inventory file hash differs from the receipt.")
    release_contract_summary = _payload_validate_v3_release_contract(
        evidence_root,
        repository_root,
        package,
        manifest,
        receipt,
        allowed_receipt_root,
        receipt_inventory_path,
    )
    build_inventory = load_json(receipt_inventory_path)
    build_files = build_inventory.get("files")
    _payload_require(isinstance(build_files, list)
                     and type(build_inventory.get("fileCount")) is int
                     and build_inventory.get("fileCount") == len(build_files)
                     and type(receipt.get("sourceFileCount")) is int
                     and receipt.get("sourceFileCount") == len(build_files),
                     "Arm Release receipt source inventory count differs from its entries.")
    build_sha = canonical_inventory_fingerprint(build_files)
    _payload_require(build_sha == str(receipt.get("sourceSha256", ""))
                     == str(build_inventory.get("sha256", "")),
                     "Arm Release receipt source inventory tree hash is invalid.")
    _payload_require(Path(str(build_inventory.get("repositoryRoot", ""))).resolve() == repository_root,
                     "Arm build source inventory references a different repository root.")
    build_rows = _payload_verify_source_rows(build_files, repository_root, "Build")

    current_record = package.get("currentSourceInventory")
    _payload_require(isinstance(current_record, dict), "Arm current-source inventory record is missing.")
    verify_source_inventory(evidence_root, current_record, repository_root)
    current_path = verify_recorded_file(evidence_root, current_record)
    current_inventory = load_json(current_path)
    current_files = current_inventory.get("files")
    _payload_require(current_inventory.get("includedRoots") == build_inventory.get("includedRoots")
                     and current_record.get("treeSha256") == build_sha
                     and type(current_record.get("fileCount")) is int
                     and isinstance(current_files, list)
                     and current_record.get("fileCount") == len(current_files)
                     and package.get("currentSourceSha256") == build_sha,
                     "Arm current-source inventory identity differs from the Release receipt.")
    current_rows = _payload_verify_source_rows(current_files, repository_root, "Current")
    scanned_rows = _scan_source_inventory(repository_root, list(current_inventory.get("includedRoots", [])))
    _payload_require(_source_row_map(scanned_rows, "Scanned current") == current_rows,
                     "Arm current-source inventory omits or adds files from its included roots.")
    _payload_require(build_rows == current_rows,
                     "Arm Release source inventory differs from the current-source inventory entries.")

    product_record = package.get("productSourceInventory")
    _payload_require(isinstance(product_record, dict), "Arm product-source inventory record is missing.")
    verify_source_inventory(evidence_root, product_record, repository_root)
    product_inventory = load_json(verify_recorded_file(evidence_root, product_record))
    product_files = product_inventory.get("files")
    product_rows = _payload_verify_source_rows(product_files, repository_root, "Product")
    _payload_require(product_inventory.get("includedRoots") == current_inventory.get("includedRoots")
                     and set(product_rows) <= set(current_rows),
                     "Arm product-source inventory is not a subset of its complete current-source inventory.")
    product_sha = canonical_inventory_fingerprint(product_files)
    _payload_require(product_sha == str(package.get("productSourceSha256", ""))
                     == str(product_inventory.get("sha256", ""))
                     == str(product_record.get("treeSha256", "")),
                     "Arm product-source fingerprint differs from its actual source inventory.")
    _payload_require(type(product_record.get("fileCount")) is int
                     and product_record.get("fileCount") == len(product_files),
                     "Arm product-source inventory count differs from its entries.")

    harness_record = package.get("harnessSourceInventory")
    _payload_require(isinstance(harness_record, dict), "Arm harness-source inventory record is missing.")
    verify_source_inventory(evidence_root, harness_record, repository_root)
    harness_inventory = load_json(verify_recorded_file(evidence_root, harness_record))
    harness_files = harness_inventory.get("files")
    harness_rows = _payload_verify_source_rows(harness_files, repository_root, "Harness")
    _payload_require(harness_inventory.get("includedRoots") == list(P917_HARNESS_SOURCES)
                     and set(harness_rows) == {path.casefold() for path in P917_HARNESS_SOURCES}
                     and set(harness_rows) <= set(current_rows)
                     and type(harness_record.get("fileCount")) is int
                     and harness_record.get("fileCount") == len(harness_files)
                     and canonical_inventory_fingerprint(harness_files) == str(harness_record.get("treeSha256", "")),
                     "Arm harness-source inventory differs from the current source or owned harness list.")

    app_value = str(package.get("applicationPath", ""))
    package_root_value = str(package.get("packageRoot", ""))
    _payload_require(bool(app_value) and Path(app_value).is_absolute()
                     and bool(package_root_value) and Path(package_root_value).is_absolute(),
                     "Arm application or package root path is missing or not absolute.")
    app_path = Path(app_value).resolve(strict=True)
    package_root = Path(package_root_value).resolve(strict=True)
    _payload_require(evidence_root.resolve() in package_root.parents
                     and package_root in app_path.parents
                     and app_path.is_file(),
                     "Arm application executable is outside the inventoried package root.")
    app_sha = sha256_file(app_path)
    _payload_require(app_sha == str(package.get("applicationSha256", ""))
                     == str(package.get("releaseExecutableSha256", ""))
                     == str(receipt.get("releaseExecutableSha256", "")),
                     "Arm packaged executable hash differs from the actual executable or Release receipt.")
    release_executable_value = str(receipt.get("releaseExecutablePath", ""))
    _payload_require(bool(release_executable_value) and Path(release_executable_value).is_absolute(),
                     "Arm Release receipt executable path is missing or not absolute.")
    release_executable = Path(release_executable_value).resolve(strict=True)
    _payload_require(sha256_file(release_executable) == app_sha,
                     "Arm packaged executable differs from the executable bound by its Release receipt.")
    process = manifest.get("process") or {}
    _payload_require(process.get("processImageSha256") == app_sha
                     and Path(str(process.get("processImagePath", ""))).resolve() == app_path,
                     "Arm process image path or hash differs from the packaged executable.")

    package_record = package.get("packageInventory")
    _payload_require(isinstance(package_record, dict), "Arm package inventory record is missing.")
    package_inventory_summary = verify_package_inventory(evidence_root, package_record, package_root)
    _payload_require(package_inventory_summary["sha256"] == str(package.get("packageSha256", ""))
                     == str(package_record.get("treeSha256", "")),
                     "Arm package inventory fingerprint differs from package provenance.")

    resource_root_value = str(package.get("resourceRoot", ""))
    _payload_require(bool(resource_root_value) and Path(resource_root_value).is_absolute(),
                     "Arm staged-resource root path is missing or not absolute.")
    resource_root = Path(resource_root_value).resolve(strict=True)
    _payload_require(package_root in resource_root.parents,
                     "Arm staged-resource root is outside the inventoried package root.")
    resource_record = package.get("resourceInventory")
    _payload_require(isinstance(resource_record, dict), "Arm staged-resource inventory record is missing.")
    resource_inventory_summary = verify_package_inventory(evidence_root, resource_record, resource_root)
    _payload_require(resource_inventory_summary["sha256"] == str(package.get("resourceInventorySha256", ""))
                     == str(resource_record.get("treeSha256", "")),
                     "Arm staged-resource inventory fingerprint differs from package provenance.")
    return {
        "receiptPath": str(receipt_path),
        "receiptSha256": receipt_sha,
        "sourceSha256": build_sha,
        "productSourceSha256": product_sha,
        "applicationSha256": app_sha,
        "packageSha256": package_inventory_summary["sha256"],
        "resourceInventorySha256": resource_inventory_summary["sha256"],
        "releaseContract": release_contract_summary,
    }


def _payload_validate_arm(manifest_path: Path, expected_mode: str) -> dict[str, Any]:
    manifest_path = manifest_path.resolve(strict=True)
    evidence_root = manifest_path.parent
    manifest = load_json(manifest_path)
    evidence_inventory_summary = _payload_validate_evidence_inventory(evidence_root, manifest)
    _payload_require(manifest.get("schema") == SCHEMA, f"{expected_mode} arm has the wrong P9-17 manifest schema.")
    _payload_require(manifest.get("taskId") == "P9_17_PACKAGED_NEXT_LARGE_ROSTER_EDITING",
                     f"{expected_mode} arm has the wrong task ID.")
    _payload_require(manifest.get("status") == "diagnostic-only"
                     and manifest.get("diagnosticOnly") is True
                     and manifest.get("accepted") is False,
                     f"{expected_mode} arm is not explicitly diagnostic-only and unaccepted.")
    diagnostic = manifest.get("payloadDiagnostic")
    _payload_require(isinstance(diagnostic, dict)
                     and diagnostic.get("schema") == PAYLOAD_DIAGNOSTIC_SCHEMA
                     and diagnostic.get("diagnosticOnly") is True
                     and diagnostic.get("accepted") is False
                     and diagnostic.get("payloadMode") == expected_mode
                     and diagnostic.get("customCellRecordCount") == 15000
                     and diagnostic.get("customCellValueCount") == 15000
                     and diagnostic.get("blankCustomCellValueCount") == 0
                     and diagnostic.get("compactSentinelValue") == (
                         PAYLOAD_SPARSE_SENTINEL if expected_mode == "sparse" else None
                     )
                     and diagnostic.get("compactSentinelValueCount") == (
                         PAYLOAD_SPARSE_SENTINEL_COUNT if expected_mode == "sparse" else 0
                     )
                     and diagnostic.get("preservedProbeCellCount") == 3,
                     f"{expected_mode} arm payload diagnostic metadata is missing or inconsistent.")

    fixture = manifest.get("fixture") or {}
    receipt_path = verify_recorded_file(evidence_root, fixture.get("preparationReceipt"))
    receipt = load_json(receipt_path)
    payload = receipt.get("payloadDiagnostic")
    _payload_require(receipt.get("payloadMode") == expected_mode
                     and isinstance(payload, dict)
                     and payload.get("schema") == PAYLOAD_FIXTURE_SCHEMA
                     and payload.get("payloadMode") == expected_mode
                     and payload.get("diagnosticOnly") is True
                     and payload.get("accepted") is False,
                     f"{expected_mode} arm preparation receipt does not describe a diagnostic payload fixture.")
    _payload_require(payload.get("customCellRecordCount") == 15000
                     and payload.get("preservedProbeCellCount") == 3,
                     f"{expected_mode} arm custom-cell record or probe count is invalid.")
    expected_values = 15000
    expected_blanks = 0
    expected_sentinel_count = PAYLOAD_SPARSE_SENTINEL_COUNT if expected_mode == "sparse" else 0
    expected_sentinel = PAYLOAD_SPARSE_SENTINEL if expected_mode == "sparse" else None
    _payload_require(payload.get("customCellValueCount") == expected_values
                     and payload.get("blankCustomCellValueCount") == expected_blanks
                     and payload.get("compactSentinelValue") == expected_sentinel
                     and payload.get("compactSentinelValueCount") == expected_sentinel_count,
                     f"{expected_mode} arm custom-cell value counts do not match the declared mode.")
    _payload_require(payload.get("fixtureSource") == {
        "baseWorkspaceSha256": receipt.get("baseWorkspaceSha256"),
        "seedSqlSha256": receipt.get("seedSqlSha256"),
        "preparerSha256": receipt.get("preparerSha256"),
    }, f"{expected_mode} arm fixture source hashes do not match its preparation receipt.")
    _payload_require(fixture.get("fixturePreparerSha256") == receipt.get("preparerSha256"),
                     f"{expected_mode} arm fixture preparer hash differs from the preparation receipt.")
    _payload_require(fixture.get("payloadMode") == expected_mode
                     and fixture.get("payloadDiagnostic") == payload,
                     f"{expected_mode} arm manifest fixture metadata differs from its preparation receipt.")
    _payload_require(diagnostic.get("preparationReceiptSha256") == fixture["preparationReceipt"].get("sha256"),
                     f"{expected_mode} arm manifest does not bind its preparation receipt hash.")
    _payload_require(diagnostic.get("beforeSnapshotSha256") == fixture.get("beforeSnapshot", {}).get("sha256"),
                     f"{expected_mode} arm manifest does not bind its before-snapshot hash.")
    before_path = verify_recorded_file(evidence_root, fixture.get("beforeSnapshot"))
    after_path = verify_recorded_file(evidence_root, fixture.get("afterSnapshot"))
    before = load_json(before_path)
    after = load_json(after_path)
    _payload_require(before.get("phase") == "before" and before.get("status") == "passed"
                     and before.get("workspaceSha256") == receipt.get("workspaceSha256")
                     and before.get("identity") == receipt.get("identity")
                     and before.get("workspaceTables") == receipt.get("baselineTables"),
                     f"{expected_mode} arm before snapshot differs from its preparation baseline.")
    _payload_require(after.get("phase") == "after" and after.get("status") == "passed"
                     and after.get("integrityCheck") == "ok"
                     and after.get("foreignKeyErrors") == []
                     and after.get("identity") == receipt.get("identity"),
                     f"{expected_mode} arm after snapshot is incomplete or has invalid database identity.")
    _payload_require(before.get("payloadMode") == expected_mode
                     and after.get("payloadMode") == expected_mode,
                     f"{expected_mode} arm preparation/baseline snapshots omit payloadMode.")
    for snapshot_label, snapshot_value in (("before", before), ("after", after)):
        snapshot_payload = snapshot_value.get("payloadDiagnostic") or {}
        counts = snapshot_payload.get("snapshotCounts") or {}
        _payload_require(counts.get("customCellRecordCount") == 15000
                         and counts.get("customCellValueCount") == expected_values
                         and counts.get("blankCustomCellValueCount") == expected_blanks
                         and counts.get("compactSentinelValueCount") == expected_sentinel_count
                         and len(snapshot_payload.get("snapshotProbeCells", [])) == 3,
                         f"{expected_mode} {snapshot_label} snapshot payload counts or probe records are incomplete.")
        expected_snapshot_values = {
            (int(target["row"]), int(target["storedColumn"])): str(
                target["beforeValue"] if snapshot_label == "before" else target["afterValue"]
            )
            for target in receipt.get("expectedEditTargets", [])
        }
        observed_snapshot_values = {
            (int(row["row"]), int(row["storedColumn"])): str(row.get("snapshotValue", ""))
            for row in snapshot_payload.get("snapshotProbeCells", [])
        }
        _payload_require(observed_snapshot_values == expected_snapshot_values,
                         f"{expected_mode} {snapshot_label} snapshot probe values differ from their declared targets.")

    identity = receipt.get("identity") or {}
    _payload_require(identity.get("classCount") == 96
                     and identity.get("classInfoCount") == 96
                     and identity.get("scheduleEntryCount") == 768
                     and identity.get("targetRosterRowCount") == 100
                     and identity.get("targetStoredRosterColumnCount") == 156
                     and identity.get("targetEnglishNameCellCount") == 100
                     and identity.get("targetKoreanNameCellCount") == 100
                     and identity.get("targetRosterCellCount") == 15200
                     and identity.get("databaseRosterCellCount") == 22325,
                     f"{expected_mode} arm schedule/database row counts or 100x156 fixture geometry are invalid.")
    expected_probe = {
        (int(target["row"]), int(target["storedColumn"])): str(target["beforeValue"])
        for target in receipt.get("expectedEditTargets", [])
    }
    _payload_require(set(expected_probe) == PAYLOAD_PROBE_COORDINATES,
                     f"{expected_mode} arm preparation receipt does not define the exact three probes.")
    _payload_require(len(payload.get("probeCells", [])) == 3
                     and {(int(cell["row"]), int(cell["storedColumn"])): str(cell["beforeValue"])
                          for cell in payload.get("probeCells", [])} == expected_probe,
                     f"{expected_mode} arm payload preparation values differ from the three edit targets.")

    package = manifest.get("packageProvenance") or {}
    _payload_require(package.get("buildMode") == "exact-current-source-release"
                     and package.get("exactCurrentSourceBuild") is True,
                     f"{expected_mode} arm is not bound to an exact-current-source Release package.")
    for field in ("applicationSha256", "productSourceSha256", "currentSourceSha256"):
        _payload_require(isinstance(package.get(field), str) and bool(package[field]),
                         f"{expected_mode} arm package is missing {field}.")
    process = manifest.get("process") or {}
    _payload_require(int(process.get("processId", 0)) > 0
                     and process.get("processImageSha256") == package.get("applicationSha256"),
                     f"{expected_mode} arm process image does not match the packaged executable.")
    provenance_summary = _payload_validate_release_and_package_provenance(evidence_root, manifest)
    process_exit = manifest.get("processExit") or {}
    _payload_require(process_exit.get("observed") is True
                     and process_exit.get("normalExit") is True
                     and int(process_exit.get("exitCode", -1)) == 0
                     and process_exit.get("forcedTermination") is False,
                     f"{expected_mode} arm did not record a natural process exit.")

    route = manifest.get("route") or {}
    _payload_require(route.get("observerMode") == "full-observer"
                     and route.get("observerDiagnosticOnly") is False
                     and route.get("measuredCycleCount") == 10
                     and route.get("primingCycleCount") == 1
                     and route.get("baselineWindowsPerState") == 5,
                     f"{expected_mode} arm is not the full-observer ten-cycle route with its primer and five baselines.")
    counts = route.get("modelCounts") or {}
    _payload_require(counts.get("rosterRows") == 100
                     and counts.get("rosterColumns") == 156
                     and counts.get("verifiedHeaderCount") == 156,
                     f"{expected_mode} arm did not verify the 100x156 grid and all headers.")
    cycle_records = route.get("cycleRecords")
    _payload_require(isinstance(cycle_records, list) and len(cycle_records) == 10,
                     f"{expected_mode} arm omitted measured cycle 1 or another measured cycle.")
    for cycle_number, cycle in enumerate(cycle_records, start=1):
        _payload_require(cycle.get("cycleNumber") == cycle_number
                         and cycle.get("gridRows") == 100
                         and cycle.get("gridColumns") == 156
                         and cycle.get("verifiedHeaderCount") == 156,
                         f"{expected_mode} arm cycle {cycle_number} has invalid ordering or grid geometry.")
    return {
        "manifestPath": str(manifest_path),
        "manifest": manifest,
        "evidenceRoot": evidence_root,
        "fixture": fixture,
        "receipt": receipt,
        "before": before,
        "after": after,
        "package": package,
        "provenance": provenance_summary,
        "evidenceInventory": evidence_inventory_summary,
        "process": process,
        "processExit": process_exit,
        "route": route,
        "mode": expected_mode,
        "expectedProbeValues": expected_probe,
    }


def _payload_memory_window_contract(arm: dict[str, Any]) -> dict[str, Any]:
    route = arm["route"]
    manifest = arm["manifest"]
    memory = manifest.get("memory") or {}
    _payload_require(route.get("memoryWindowSampleCount") == 10
                     and route.get("memoryWindowIntervalMilliseconds") == 100,
                     f"{arm['mode']} arm route sample-window contract is not ten samples at 100 ms.")
    baseline_ids = memory.get("plateauBaselineWindowIds")
    cycle_ids = memory.get("plateauCycleWindowIds")
    expected_baseline_ids = [f"plateau-baseline-{index:02d}" for index in range(1, 6)]
    expected_measured_cycle_ids = [
        f"cycle-{cycle:02d}-{stage}"
        for cycle in range(1, 11)
        for stage in ("roster-loaded", "post-save", "return")
    ]
    expected_cycle_window_ids = [*expected_measured_cycle_ids, "cycle-final-reentry"]
    _payload_require(baseline_ids == expected_baseline_ids and cycle_ids == expected_cycle_window_ids,
                     f"{arm['mode']} arm sample-window IDs omit or reorder a baseline or measured cycle stage.")
    _payload_require(memory.get("workingSetTargetBytes") == WORKING_SET_TARGET
                     and memory.get("workingSetTargetExclusive") is True
                     and memory.get("workingSetCeilingBytes") == WORKING_SET_CEILING
                     and memory.get("workingSetCeilingExclusive") is False
                     and memory.get("operationPeakAdvisoryLimitBytes") == OPERATION_PEAK_ADVISORY_LIMIT,
                     f"{arm['mode']} arm memory thresholds differ from unchanged P9-17 values.")
    windows = memory.get("windows")
    _payload_require(isinstance(windows, list) and windows, f"{arm['mode']} arm memory windows are missing.")
    by_id = {}
    contract = []
    for window in windows:
        _payload_require(isinstance(window, dict), f"{arm['mode']} arm contains a malformed memory window.")
        window_id = str(window.get("windowId", ""))
        _payload_require(window_id and window_id not in by_id, f"{arm['mode']} arm has a duplicate or unnamed memory window.")
        samples = window.get("samples")
        _payload_require(isinstance(samples, list) and len(samples) == 10
                         and window.get("sampleCount") == 10
                         and window.get("sampleIntervalMilliseconds") == 100
                         and window.get("samplingCompleted") is True
                         and window.get("samplingErrorCount") == 0
                         and window.get("samplingErrors") == [],
                         f"{arm['mode']} arm window {window_id} violates the ten-by-100-ms sample contract.")
        sample_indexes = [sample.get("sampleIndex") for sample in samples]
        _payload_require(sample_indexes == list(range(10)),
                         f"{arm['mode']} arm window {window_id} samples are missing or reordered.")
        for sample in samples:
            for metric in ("workingSetBytes", "privateBytes"):
                _payload_require(isinstance(sample.get(metric), int) and sample[metric] >= 0,
                                 f"{arm['mode']} arm window {window_id} has a missing {metric} sample.")
        by_id[window_id] = window
        contract.append({
            "windowId": window_id,
            "state": window.get("state"),
            "phase": window.get("phase"),
            "matchedState": window.get("matchedState"),
            "sampleCount": window.get("sampleCount"),
            "sampleIntervalMilliseconds": window.get("sampleIntervalMilliseconds"),
            "samplingCompleted": window.get("samplingCompleted"),
            "samplingErrorCount": window.get("samplingErrorCount"),
            "sampleIndexes": sample_indexes,
        })
    _payload_require(set(expected_baseline_ids + expected_cycle_window_ids) <= set(by_id),
                     f"{arm['mode']} arm is missing a required plateau or cycle sample window.")
    return {"windows": contract, "byId": by_id}


def _payload_window_median(window: dict[str, Any], metric: str) -> float:
    return median([int(sample[metric]) for sample in window["samples"]])


def _payload_memory_deltas(left: dict[str, Any], right: dict[str, Any]) -> dict[str, Any]:
    left_windows = left["byId"]
    right_windows = right["byId"]
    _payload_require(set(left_windows) == set(right_windows), "Memory window identifiers differ between arms.")
    stage_deltas = []
    for window_id in left_windows:
        left_window = left_windows[window_id]
        right_window = right_windows[window_id]
        metrics = {}
        for metric in ("workingSetBytes", "privateBytes"):
            filled_median = _payload_window_median(left_window, metric)
            sparse_median = _payload_window_median(right_window, metric)
            metrics[metric] = {
                "filledMedianBytes": filled_median,
                "sparseMedianBytes": sparse_median,
                "sparseMinusFilledBytes": sparse_median - filled_median,
            }
        stage_deltas.append({
            "windowId": window_id,
            "stage": "final-reentry" if window_id == "cycle-final-reentry" else None,
            **metrics,
        })

    stage_series = {
        "cyclePostSuspensionReturn": [f"cycle-{cycle:02d}-return" for cycle in range(1, 11)],
    }
    slopes = []
    for stage_name, ids in stage_series.items():
        for metric in ("workingSetBytes", "privateBytes"):
            filled_values = [_payload_window_median(left_windows[window_id], metric) for window_id in ids]
            sparse_values = [_payload_window_median(right_windows[window_id], metric) for window_id in ids]
            filled_change = linear_fitted_change(filled_values)
            sparse_change = linear_fitted_change(sparse_values)
            divisor = max(len(ids) - 1, 1)
            slopes.append({
                "stage": stage_name,
                "metric": metric,
                "sampleCount": len(ids),
                "windowIds": list(ids),
                "filledFittedChangeBytes": filled_change,
                "filledSlopeBytesPerStage": filled_change / divisor,
                "sparseFittedChangeBytes": sparse_change,
                "sparseSlopeBytesPerStage": sparse_change / divisor,
                "sparseMinusFilledSlopeBytesPerStage": (sparse_change - filled_change) / divisor,
            })
    return {
        "perWindowStageMedianDeltas": stage_deltas,
        "fittedStageSlopes": slopes,
        "interpretation": (
            "These are descriptive paired Working Set and PrivateUsage measurements. "
            "Neither metric alone establishes that custom-cell text caused a change."
        ),
    }


def compare_p917_payload_diagnostic(filled_manifest_path: Path, sparse_manifest_path: Path) -> dict[str, Any]:
    filled = _payload_validate_arm(filled_manifest_path, "filled")
    sparse = _payload_validate_arm(sparse_manifest_path, "sparse")
    filled_manifest = filled["manifest"]
    sparse_manifest = sparse["manifest"]
    filled_route = filled["route"]
    sparse_route = sparse["route"]

    _payload_require(filled["receipt"].get("identity") == sparse["receipt"].get("identity"),
                     "Fixture identity, grid geometry, schedule counts, or database row counts differ.")
    for snapshot_label, filled_snapshot, sparse_snapshot in (
        ("preparation baseline", filled["receipt"].get("baselineSchemaObjects"), sparse["receipt"].get("baselineSchemaObjects")),
        ("before snapshot", filled["before"].get("schemaObjects"), sparse["before"].get("schemaObjects")),
        ("after snapshot", filled["after"].get("schemaObjects"), sparse["after"].get("schemaObjects")),
    ):
        _payload_require(filled_snapshot == sparse_snapshot,
                         f"{snapshot_label} SQL schema objects differ between arms.")
    _payload_require(filled["receipt"].get("expectedEditTargets") == sparse["receipt"].get("expectedEditTargets"),
                     "The initial values or edit values for the three probes differ.")

    filled_probe_values = filled["expectedProbeValues"]
    sparse_probe_values = sparse["expectedProbeValues"]
    _payload_require(filled_probe_values == sparse_probe_values,
                     "Initial probe values differ between the filled and sparse fixtures.")
    baseline_row_counts = _payload_compare_table_snapshots(
        filled["receipt"].get("baselineTables"), sparse["receipt"].get("baselineTables"),
        "Preparation baseline", "filled", "sparse", filled_probe_values,
    )
    before_row_counts = _payload_compare_table_snapshots(
        filled["before"].get("workspaceTables"), sparse["before"].get("workspaceTables"),
        "Before snapshot", "filled", "sparse", filled_probe_values,
    )

    filled_final_readbacks = _payload_readbacks(
        filled_route.get("readBackChecks", {}).get("afterCycles"), "filled final cycle readbacks"
    )
    sparse_final_readbacks = _payload_readbacks(
        sparse_route.get("readBackChecks", {}).get("afterCycles"), "sparse final cycle readbacks"
    )
    final_probe_values = {
        (int(row["row"]), int(row["storedColumn"])): str(row["actual"])
        for row in filled_final_readbacks
    }


    _payload_require(final_probe_values == {
        (int(row["row"]), int(row["storedColumn"])): str(row["actual"])
        for row in sparse_final_readbacks
    }, "Final probe readbacks differ between arms.")
    for arm in (filled, sparse):
        snapshot_probe_values = {
            (int(row["row"]), int(row["storedColumn"])): str(row["snapshotValue"])
            for row in (arm["after"].get("payloadDiagnostic") or {}).get("snapshotProbeCells", [])
        }
        expected_values = {
            (int(row["row"]), int(row["storedColumn"])): str(row["actual"])
            for row in _payload_readbacks(
                arm["route"].get("readBackChecks", {}).get("afterCycles"),
                f"{arm['mode']} final readbacks",
            )
        }
        _payload_require(snapshot_probe_values == expected_values,
                         f"{arm['mode']} after snapshot probe values differ from final UIA readbacks.")
    after_row_counts = _payload_compare_table_snapshots(
        filled["after"].get("workspaceTables"), sparse["after"].get("workspaceTables"),
        "After snapshot", "filled", "sparse", final_probe_values,
    )

    filled_actions = _payload_arm_action_signature(filled_route)
    sparse_actions = _payload_arm_action_signature(sparse_route)
    filled_readbacks = _payload_arm_readback_signature(filled_route)
    sparse_readbacks = _payload_arm_readback_signature(sparse_route)
    filled_windows = _payload_memory_window_contract(filled)
    sparse_windows = _payload_memory_window_contract(sparse)

    filled_package = filled["package"]
    sparse_package = sparse["package"]
    filled_process = filled["process"]
    sparse_process = sparse["process"]
    source_identity_fields = (
        "applicationSha256", "productSourceSha256", "currentSourceSha256",
        "packageSha256", "resourceInventorySha256",
    )
    checks = {
        "bothArmsDiagnosticOnlyAndUnaccepted": (
            filled_manifest.get("diagnosticOnly") is True
            and sparse_manifest.get("diagnosticOnly") is True
            and filled_manifest.get("accepted") is False
            and sparse_manifest.get("accepted") is False
            and filled_manifest.get("status") == "diagnostic-only"
            and sparse_manifest.get("status") == "diagnostic-only"
        ),
        "filledThenSparsePayloadModes": (
            filled["receipt"].get("payloadMode") == "filled"
            and sparse["receipt"].get("payloadMode") == "sparse"
        ),
        "sameExecutableAndProductSourceIdentity": all(
            filled_package.get(field) == sparse_package.get(field)
            and bool(filled_package.get(field))
            for field in source_identity_fields
        ),
        "sameFixtureSourceAndPreparerHashes": (
            filled["receipt"].get("baseWorkspaceSha256") == sparse["receipt"].get("baseWorkspaceSha256")
            and filled["receipt"].get("seedSqlSha256") == sparse["receipt"].get("seedSqlSha256")
            and filled["receipt"].get("preparerSha256") == sparse["receipt"].get("preparerSha256")
        ),
        "distinctNaturalExitProcesses": (
            int(filled_process.get("processId", 0)) > 0
            and int(sparse_process.get("processId", 0)) > 0
            and int(filled_process.get("processId", 0)) != int(sparse_process.get("processId", 0))
            and filled["processExit"].get("normalExit") is True
            and sparse["processExit"].get("normalExit") is True
        ),
        "sameFullObserverContract": (
            filled_route.get("observerMode") == sparse_route.get("observerMode") == "full-observer"
            and filled_route.get("observerDiagnosticOnly") is False
            and sparse_route.get("observerDiagnosticOnly") is False
        ),
        "sameTenCycleActionSignature": filled_actions == sparse_actions,
        "sameInitialEditAndReadbackSignature": filled_readbacks == sparse_readbacks,
        "sameSampleWindowContract": filled_windows["windows"] == sparse_windows["windows"],
        "sameScheduleAndDatabaseRowCounts": baseline_row_counts == before_row_counts == after_row_counts,
    }
    comparison_passed = all(checks.values())
    return {
        "schema": PAYLOAD_DIAGNOSTIC_SCHEMA,
        "status": "diagnostic-only" if comparison_passed else "failed",
        "diagnosticOnly": True,
        "accepted": False,
        "armModes": ["filled", "sparse"],
        "measuredCyclesPerArm": 10,
        "primingCyclesPerArm": 1,
        "matchedBaselineWindowsPerArm": 5,
        "sampleWindowContract": {
            "samplesPerWindow": 10,
            "sampleIntervalMilliseconds": 100,
            "workingSetTargetBytesExclusive": WORKING_SET_TARGET,
            "workingSetCeilingBytesInclusive": WORKING_SET_CEILING,
            "operationPeakAdvisoryLimitBytes": OPERATION_PEAK_ADVISORY_LIMIT,
        },
        "fixture": {
            "filledManifestPath": str(filled["manifestPath"]),
            "sparseManifestPath": str(sparse["manifestPath"]),
            "filledPreparationReceiptSha256": filled["fixture"]["preparationReceipt"].get("sha256"),
            "sparsePreparationReceiptSha256": sparse["fixture"]["preparationReceipt"].get("sha256"),
            "filledBeforeSnapshotSha256": filled["fixture"]["beforeSnapshot"].get("sha256"),
            "sparseBeforeSnapshotSha256": sparse["fixture"]["beforeSnapshot"].get("sha256"),
            "fixtureSourceHashes": {
                "baseWorkspaceSha256": filled["receipt"].get("baseWorkspaceSha256"),
                "seedSqlSha256": filled["receipt"].get("seedSqlSha256"),
                "preparerSha256": filled["receipt"].get("preparerSha256"),
            },
            "filledPayloadMode": filled["receipt"].get("payloadMode"),
            "sparsePayloadMode": sparse["receipt"].get("payloadMode"),
            "customCellRecordCount": 15000,
            "filledCustomCellValueCount": 15000,
            "sparseCustomCellValueCount": 15000,
            "sparseBlankCustomCellValueCount": 0,
            "sparseCompactSentinelValue": PAYLOAD_SPARSE_SENTINEL,
            "sparseCompactSentinelValueCount": PAYLOAD_SPARSE_SENTINEL_COUNT,
            "preservedProbeCoordinates": [list(value) for value in sorted(PAYLOAD_PROBE_COORDINATES)],
            "scheduleEntryCount": filled["receipt"]["identity"]["scheduleEntryCount"],
            "databaseRosterCellCount": filled["receipt"]["identity"]["databaseRosterCellCount"],
            "tableRowCounts": baseline_row_counts,
        },
        "comparisonChecks": checks,
        "memoryComparison": _payload_memory_deltas(filled_windows, sparse_windows),
        "failure": None if comparison_passed else {
            "failedChecks": [name for name, passed in checks.items() if not passed],
            "remediation": (
                "If executable, package, or source identity differs, rerun both arms with the same "
                "refreshed exact-current-source Release executable before comparing them."
                if not checks["sameExecutableAndProductSourceIdentity"] else None
            ),
        },
    }

RECORD_COUNT_DIAGNOSTIC_SCHEMA = "classmngr-phase9-p9-17-roster-record-count-diagnostic-v1"
RECORD_COUNT_FIXTURE_SCHEMA = "classmngr-phase9-p9-17-roster-record-count-fixture-v1"
RECORD_COUNT_PROBE_COORDINATES = {(0, 6), (49, 77), (99, 155)}
RECORD_COUNT_FULL_ROWS = 15000
RECORD_COUNT_REDUCED_ROWS = len(RECORD_COUNT_PROBE_COORDINATES)
RECORD_COUNT_REMOVED_ROWS = RECORD_COUNT_FULL_ROWS - RECORD_COUNT_REDUCED_ROWS


def _record_count_require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(f"P9-17 roster record-count comparison failed: {message}")


def _record_count_table_rows(table: Any, label: str) -> tuple[list[str], dict[tuple[int, int, int], list[Any]]]:
    _record_count_require(isinstance(table, dict), f"{label} roster_data snapshot is missing or malformed.")
    columns = [str(column) for column in table.get("columns", [])]
    required_columns = {"class_id", "row_index", "col_index", "value"}
    _record_count_require(required_columns <= set(columns), f"{label} roster_data lacks key/value columns.")
    indexes = {name: columns.index(name) for name in required_columns}
    rows = table.get("rows")
    _record_count_require(isinstance(rows, list) and len(rows) == table.get("rowCount"),
                          f"{label} roster_data row count differs from its rows.")
    mapped: dict[tuple[int, int, int], list[Any]] = {}
    for row in rows:
        _record_count_require(isinstance(row, list) and len(row) == len(columns),
                              f"{label} contains a malformed roster_data row.")
        key = tuple(int(row[indexes[name]]) for name in ("class_id", "row_index", "col_index"))
        _record_count_require(key not in mapped, f"{label} contains duplicate roster_data key {key}.")
        mapped[key] = row
    return columns, mapped


def _record_count_validate_arm_snapshots(
    receipt: dict[str, Any],
    before: dict[str, Any],
    after: dict[str, Any],
    mode: str,
) -> dict[str, int]:
    baseline_tables = receipt.get("baselineTables")
    before_tables = before.get("workspaceTables")
    after_tables = after.get("workspaceTables")
    _record_count_require(isinstance(baseline_tables, dict) and isinstance(before_tables, dict)
                          and isinstance(after_tables, dict), f"{mode} arm table snapshots are missing.")
    _record_count_require(baseline_tables == before_tables,
                          f"{mode} arm before snapshot differs from its preparation baseline.")
    _record_count_require(set(baseline_tables) == set(after_tables),
                          f"{mode} arm database table names changed after the route.")
    for name in sorted(baseline_tables):
        baseline = baseline_tables[name]
        current = after_tables[name]
        _record_count_require(isinstance(baseline, dict) and isinstance(current, dict),
                              f"{mode} arm table {name} snapshot is malformed.")
        _record_count_require(baseline.get("columns") == current.get("columns")
                              and baseline.get("declaration") == current.get("declaration"),
                              f"{mode} arm table {name} schema changed after the route.")
        if name != "roster_data":
            _record_count_require(baseline == current,
                                  f"{mode} arm non-roster table {name} changed after the route.")
            continue
        _record_count_require(baseline.get("rowCount") == current.get("rowCount"),
                              f"{mode} arm roster_data row count changed after the route.")
        columns, before_rows = _record_count_table_rows(baseline, f"{mode} preparation")
        after_columns, after_rows = _record_count_table_rows(current, f"{mode} after")
        _record_count_require(columns == after_columns and set(before_rows) == set(after_rows),
                              f"{mode} arm added or deleted roster_data rows during the route.")
        value_index = columns.index("value")
        targets = {
            (1, int(target["row"]), int(target["storedColumn"])): str(target["afterValue"])
            for target in receipt.get("expectedEditTargets", [])
        }
        for key, original in before_rows.items():
            expected = targets.get(key)
            if expected is None:
                _record_count_require(original == after_rows[key],
                                      f"{mode} arm changed non-probe roster_data row {key}.")
            else:
                _record_count_require(original[:value_index] + original[value_index + 1:]
                                      == after_rows[key][:value_index] + after_rows[key][value_index + 1:]
                                      and after_rows[key][value_index] == expected,
                                      f"{mode} arm changed or failed to save probe cell {key}.")
    custom_coordinates = {
        (row_index, column_index)
        for class_id, row_index, column_index in _record_count_table_rows(
            after_tables["roster_data"], f"{mode} after"
        )[1]
        if class_id == 1 and 6 <= column_index <= 155
    }
    expected_coordinates = (
        {(row, column) for row in range(100) for column in range(6, 156)}
        if mode == "filled"
        else set(RECORD_COUNT_PROBE_COORDINATES)
    )
    _record_count_require(custom_coordinates == expected_coordinates,
                          f"{mode} arm after snapshot has unexpected custom roster coordinates.")
    return {name: int(table.get("rowCount", -1)) for name, table in baseline_tables.items()}


def _record_count_validate_arm(manifest_path: Path, expected_mode: str) -> dict[str, Any]:
    manifest_path = manifest_path.resolve(strict=True)
    evidence_root = manifest_path.parent
    manifest = load_json(manifest_path)
    evidence_inventory = _payload_validate_evidence_inventory(evidence_root, manifest)
    _record_count_require(manifest.get("schema") == SCHEMA
                          and manifest.get("taskId") == "P9_17_PACKAGED_NEXT_LARGE_ROSTER_EDITING",
                          f"{expected_mode} arm has the wrong P9-17 manifest identity.")
    _record_count_require(manifest.get("status") == "diagnostic-only"
                          and manifest.get("diagnosticOnly") is True
                          and manifest.get("accepted") is False,
                          f"{expected_mode} arm is not diagnostic-only and unaccepted.")
    diagnostic = manifest.get("recordCountDiagnostic")
    expected_count = RECORD_COUNT_FULL_ROWS if expected_mode == "filled" else RECORD_COUNT_REDUCED_ROWS
    expected_removed = RECORD_COUNT_REMOVED_ROWS if expected_mode == "reduced-rows" else 0
    _record_count_require(isinstance(diagnostic, dict)
                          and diagnostic.get("schema") == RECORD_COUNT_DIAGNOSTIC_SCHEMA
                          and diagnostic.get("diagnosticOnly") is True
                          and diagnostic.get("accepted") is False
                          and diagnostic.get("recordCountMode") == expected_mode
                          and diagnostic.get("customCellRecordCount") == expected_count
                          and diagnostic.get("customCellValueCount") == expected_count
                          and diagnostic.get("blankCustomCellValueCount") == 0
                          and diagnostic.get("removedCustomCellRecordCount") == expected_removed
                          and diagnostic.get("preservedProbeCellCount") == 3,
                          f"{expected_mode} arm record-count diagnostic metadata is inconsistent.")

    fixture = manifest.get("fixture") or {}
    receipt_path = verify_recorded_file(evidence_root, fixture.get("preparationReceipt"))
    receipt = load_json(receipt_path)
    record = receipt.get("recordCountDiagnostic")
    _record_count_require(receipt.get("recordCountMode") == expected_mode
                          and isinstance(record, dict)
                          and record.get("schema") == RECORD_COUNT_FIXTURE_SCHEMA
                          and record.get("recordCountMode") == expected_mode
                          and record.get("diagnosticOnly") is True
                          and record.get("accepted") is False
                          and record.get("customCellRecordCount") == expected_count
                          and record.get("customCellValueCount") == expected_count
                          and record.get("blankCustomCellValueCount") == 0
                          and record.get("removedCustomCellRecordCount") == expected_removed
                          and record.get("preservedProbeCellCount") == 3,
                          f"{expected_mode} arm preparation receipt has invalid record-count metadata.")
    _record_count_require(record.get("fixtureSource") == {
        "baseWorkspaceSha256": receipt.get("baseWorkspaceSha256"),
        "seedSqlSha256": receipt.get("seedSqlSha256"),
        "preparerSha256": receipt.get("preparerSha256"),
    } and fixture.get("fixturePreparerSha256") == receipt.get("preparerSha256")
      and fixture.get("recordCountMode") == expected_mode
      and fixture.get("recordCountDiagnostic") == record
      and diagnostic.get("preparationReceiptSha256") == fixture.get("preparationReceipt", {}).get("sha256")
      and diagnostic.get("beforeSnapshotSha256") == fixture.get("beforeSnapshot", {}).get("sha256"),
      f"{expected_mode} arm record-count fixture/source evidence is not hash-bound.")
    probe_coordinates = {
        (int(cell.get("row", -1)), int(cell.get("storedColumn", -1)))
        for cell in record.get("probeCells", [])
    }
    _record_count_require(probe_coordinates == RECORD_COUNT_PROBE_COORDINATES
                          and record.get("retainedCustomRecordCoordinates")
                          == [list(value) for value in sorted(RECORD_COUNT_PROBE_COORDINATES)],
                          f"{expected_mode} arm does not preserve exactly the three probe records.")
    expected_probe = {
        (int(target["row"]), int(target["storedColumn"])): str(target["beforeValue"])
        for target in receipt.get("expectedEditTargets", [])
    }
    _record_count_require(set(expected_probe) == RECORD_COUNT_PROBE_COORDINATES
                          and len({target.get("name") for target in receipt["expectedEditTargets"]}) == 3
                          and {
                              (int(cell["row"]), int(cell["storedColumn"])): str(cell["beforeValue"])
                              for cell in record.get("probeCells", [])
                          } == expected_probe,
                          f"{expected_mode} arm probe values or identities differ from the three edit targets.")

    identity = receipt.get("identity") or {}
    expected_identity = {
        "classCount": 96, "classInfoCount": 96, "scheduleEntryCount": 768,
        "targetRosterRowCount": 100, "targetStoredRosterColumnCount": 156,
        "targetDisplayedRosterColumnCount": 156, "targetEnglishNameCellCount": 100,
        "targetKoreanNameCellCount": 100,
        "targetRosterCellCount": 15200 if expected_mode == "filled" else 203,
        "targetCustomRosterCellCount": expected_count,
        "databaseRosterCellCount": 22325 if expected_mode == "filled" else 7328,
    }
    _record_count_require(all(identity.get(key) == value for key, value in expected_identity.items()),
                          f"{expected_mode} arm schedule, row cardinality, names, or 100x156 geometry is invalid.")
    before_path = verify_recorded_file(evidence_root, fixture.get("beforeSnapshot"))
    after_path = verify_recorded_file(evidence_root, fixture.get("afterSnapshot"))
    before = load_json(before_path)
    after = load_json(after_path)
    _record_count_require(before.get("phase") == "before" and before.get("status") == "passed"
                          and before.get("workspaceSha256") == receipt.get("workspaceSha256")
                          and before.get("identity") == identity
                          and before.get("workspaceTables") == receipt.get("baselineTables")
                          and before.get("recordCountMode") == expected_mode,
                          f"{expected_mode} arm before snapshot differs from its receipt baseline.")
    _record_count_require(after.get("phase") == "after" and after.get("status") == "passed"
                          and after.get("integrityCheck") == "ok"
                          and after.get("foreignKeyErrors") == []
                          and after.get("identity") == identity
                          and after.get("recordCountMode") == expected_mode,
                          f"{expected_mode} arm after snapshot is incomplete or has different database identity.")
    for label, snapshot in (("before", before), ("after", after)):
        snapshot_record = snapshot.get("recordCountDiagnostic") or {}
        snapshot_counts = snapshot_record.get("snapshotCounts") or {}
        expected_probe_values = expected_probe if label == "before" else {
            (int(target["row"]), int(target["storedColumn"])): str(target["afterValue"])
            for target in receipt.get("expectedEditTargets", [])
        }
        observed_probe_values = {
            (int(cell.get("row", -1)), int(cell.get("storedColumn", -1))): str(cell.get("snapshotValue", ""))
            for cell in snapshot_record.get("snapshotProbeCells", [])
        }
        _record_count_require(snapshot_counts.get("customCellRecordCount") == expected_count
                              and snapshot_counts.get("customCellValueCount") == expected_count
                              and snapshot_counts.get("blankCustomCellValueCount") == 0
                              and snapshot_counts.get("removedCustomCellRecordCount") == expected_removed
                              and observed_probe_values == expected_probe_values,
                              f"{expected_mode} arm {label} snapshot row counts or probe values are invalid.")
    own_row_counts = _record_count_validate_arm_snapshots(receipt, before, after, expected_mode)

    package = manifest.get("packageProvenance") or {}
    process = manifest.get("process") or {}
    provenance = _payload_validate_release_and_package_provenance(evidence_root, manifest)
    _record_count_require(int(process.get("processId", 0)) > 0
                          and process.get("processImageSha256") == package.get("applicationSha256")
                          and manifest.get("processExit", {}).get("observed") is True
                          and manifest.get("processExit", {}).get("normalExit") is True
                          and manifest.get("processExit", {}).get("exitCode") == 0
                          and manifest.get("processExit", {}).get("forcedTermination") is False,
                          f"{expected_mode} arm executable binding or natural process exit is invalid.")
    route = manifest.get("route") or {}
    _record_count_require(route.get("observerMode") == "full-observer"
                          and route.get("observerDiagnosticOnly") is False
                          and route.get("measuredCycleCount") == 10
                          and route.get("primingCycleCount") == 1
                          and route.get("baselineWindowsPerState") == 5,
                          f"{expected_mode} arm is not the full-observer one-primer/five-baseline/ten-cycle route.")
    cycle_records = route.get("cycleRecords")
    _record_count_require(isinstance(cycle_records, list) and len(cycle_records) == 10
                          and [cycle.get("cycleNumber") for cycle in cycle_records] == list(range(1, 11))
                          and all(cycle.get("gridRows") == 100 and cycle.get("gridColumns") == 156
                                  and cycle.get("verifiedHeaderCount") == 156 for cycle in cycle_records),
                          f"{expected_mode} arm omitted cycle 1 or changed the 100x156 route geometry.")
    counts = route.get("modelCounts") or {}
    _record_count_require(counts.get("rosterRows") == 100
                          and counts.get("rosterColumns") == 156
                          and counts.get("verifiedHeaderCount") == 156,
                          f"{expected_mode} arm did not verify 100x156 rows, columns, and headers.")
    _record_count_require(route.get("memoryWindowSampleCount") == 10
                          and route.get("memoryWindowIntervalMilliseconds") == 100,
                          f"{expected_mode} arm memory sampling contract is invalid.")
    return {
        "manifestPath": str(manifest_path), "manifest": manifest, "evidenceRoot": evidence_root,
        "fixture": fixture, "receipt": receipt, "before": before, "after": after,
        "package": package, "provenance": provenance, "evidenceInventory": evidence_inventory,
        "process": process, "processExit": manifest["processExit"], "route": route,
        "mode": expected_mode, "expectedProbeValues": expected_probe, "rowCounts": own_row_counts,
    }


def _record_count_compare_table_snapshots(
    left_tables: Any,
    right_tables: Any,
    label: str,
    probe_values: dict[tuple[int, int], str],
) -> dict[str, dict[str, int]]:
    _record_count_require(isinstance(left_tables, dict) and isinstance(right_tables, dict)
                          and set(left_tables) == set(right_tables),
                          f"{label} table names differ between filled and reduced-rows arms.")
    row_counts: dict[str, dict[str, int]] = {}
    for name in sorted(left_tables):
        left = left_tables[name]
        right = right_tables[name]
        _record_count_require(isinstance(left, dict) and isinstance(right, dict)
                              and left.get("columns") == right.get("columns")
                              and left.get("declaration") == right.get("declaration"),
                              f"{label} table {name} schemas differ between arms.")
        row_counts[name] = {
            "filled": int(left.get("rowCount", -1)),
            "reducedRows": int(right.get("rowCount", -1)),
        }
        if name != "roster_data":
            _record_count_require(left.get("rowCount") == right.get("rowCount")
                                  and left.get("rows") == right.get("rows"),
                                  f"{label} non-target table {name} differs between arms.")
            continue
        left_columns, left_rows = _record_count_table_rows(left, f"{label} filled")
        right_columns, right_rows = _record_count_table_rows(right, f"{label} reduced")
        _record_count_require(left_columns == right_columns,
                              f"{label} roster_data columns differ between arms.")
        expected_removed = {
            (1, row, column)
            for row in range(100)
            for column in range(6, 156)
            if (row, column) not in RECORD_COUNT_PROBE_COORDINATES
        }
        _record_count_require(set(left_rows) - set(right_rows) == expected_removed
                              and set(right_rows) - set(left_rows) == set(),
                              f"{label} roster_data differs by rows outside the declared 14997 custom-cell deletion set.")
        value_index = left_columns.index("value")
        for key in right_rows:
            left_row = left_rows[key]
            right_row = right_rows[key]
            if key[0] == 1 and 6 <= key[2] <= 155:
                _record_count_require(
                    left_row[:value_index] + left_row[value_index + 1:]
                    == right_row[:value_index] + right_row[value_index + 1:],
                    f"{label} custom roster row identity differs at {key}.",
                )
                coordinate = (key[1], key[2])
                _record_count_require(left_row[value_index] == right_row[value_index],
                                      f"{label} shared custom roster value differs at {coordinate}.")
                if coordinate in RECORD_COUNT_PROBE_COORDINATES:
                    _record_count_require(left_row[value_index] == probe_values.get(coordinate),
                                          f"{label} probe value differs from the matched UIA readback at {coordinate}.")
            else:
                _record_count_require(left_row == right_row,
                                      f"{label} non-target roster_data row differs at {key}.")
        _record_count_require(left.get("rowCount") == 22325 and right.get("rowCount") == 7328,
                              f"{label} roster_data row counts are not 22325/7328.")
        _record_count_require(len(expected_removed) == RECORD_COUNT_REMOVED_ROWS,
                              "Declared reduced-row coordinate set is not 14997 rows.")
        for column in (0, 1):
            names = [
                row[left_columns.index("value")]
                for key, row in right_rows.items()
                if key[0] == 1 and key[2] == column
            ]
            _record_count_require(len(names) == 100 and len(set(names)) == 100
                                  and all(value not in (None, "") for value in names),
                                  f"{label} does not preserve 100 unique names in column {column}.")
    return row_counts


def _record_count_memory_deltas(left: dict[str, Any], right: dict[str, Any]) -> dict[str, Any]:
    left_windows = left["byId"]
    right_windows = right["byId"]
    _record_count_require(set(left_windows) == set(right_windows),
                          "Memory window identifiers differ between record-count arms.")
    per_stage = []
    for window_id in left_windows:
        left_window = left_windows[window_id]
        right_window = right_windows[window_id]
        metrics = {}
        for metric in ("workingSetBytes", "privateBytes"):
            filled_median = _payload_window_median(left_window, metric)
            reduced_median = _payload_window_median(right_window, metric)
            metrics[metric] = {
                "filledMedianBytes": filled_median,
                "reducedRowsMedianBytes": reduced_median,
                "reducedRowsMinusFilledBytes": reduced_median - filled_median,
            }
        per_stage.append({
            "windowId": window_id,
            "stage": "final-reentry" if window_id == "cycle-final-reentry" else None,
            **metrics,
        })
    cycle_return_ids = [f"cycle-{cycle:02d}-return" for cycle in range(1, 11)]
    fitted_slopes = []
    for metric in ("workingSetBytes", "privateBytes"):
        filled_values = [_payload_window_median(left_windows[window_id], metric) for window_id in cycle_return_ids]
        reduced_values = [_payload_window_median(right_windows[window_id], metric) for window_id in cycle_return_ids]
        filled_change = linear_fitted_change(filled_values)
        reduced_change = linear_fitted_change(reduced_values)
        divisor = len(cycle_return_ids) - 1
        fitted_slopes.append({
            "stage": "cyclePostSuspensionReturn", "metric": metric,
            "sampleCount": len(cycle_return_ids), "windowIds": cycle_return_ids,
            "filledFittedChangeBytes": filled_change,
            "filledSlopeBytesPerStage": filled_change / divisor,
            "reducedRowsFittedChangeBytes": reduced_change,
            "reducedRowsSlopeBytesPerStage": reduced_change / divisor,
            "reducedRowsMinusFilledSlopeBytesPerStage": (reduced_change - filled_change) / divisor,
        })
    return {
        "perWindowStageMedianDeltas": per_stage,
        "fittedStageSlopes": fitted_slopes,
        "trendWindowIds": cycle_return_ids,
        "finalReentryIsSeparateStage": True,
        "interpretation": (
            "These are descriptive paired Working Set and PrivateUsage measurements. "
            "They do not establish that custom-record cardinality caused a change."
        ),
    }


def compare_p917_roster_record_count_diagnostic(
    filled_manifest_path: Path,
    reduced_manifest_path: Path,
) -> dict[str, Any]:
    filled = _record_count_validate_arm(filled_manifest_path, "filled")
    reduced = _record_count_validate_arm(reduced_manifest_path, "reduced-rows")
    _record_count_require(filled["receipt"].get("identity", {}).get("classCount")
                          == reduced["receipt"].get("identity", {}).get("classCount")
                          and filled["receipt"].get("identity", {}).get("classInfoCount")
                          == reduced["receipt"].get("identity", {}).get("classInfoCount")
                          and filled["receipt"].get("identity", {}).get("scheduleEntryCount")
                          == reduced["receipt"].get("identity", {}).get("scheduleEntryCount") == 768,
                          "Class, class-info, or schedule row counts differ between arms.")
    _record_count_require(filled["expectedProbeValues"] == reduced["expectedProbeValues"],
                          "Initial probe values differ between the filled and reduced-row fixtures.")
    final_readbacks = []
    for arm in (filled, reduced):
        readbacks = _payload_readbacks(
            arm["route"].get("readBackChecks", {}).get("afterCycles"),
            f"{arm['mode']} final cycle readbacks",
        )
        final_readbacks.append({
            (int(row["row"]), int(row["storedColumn"])): str(row["actual"])
            for row in readbacks
        })
    _record_count_require(final_readbacks[0] == final_readbacks[1]
                          and set(final_readbacks[0]) == RECORD_COUNT_PROBE_COORDINATES,
                          "Final probe readbacks differ between record-count arms.")
    table_counts: dict[str, dict[str, dict[str, int]]] = {}
    for label, left_tables, right_tables, values in (
        ("preparation baseline", filled["receipt"].get("baselineTables"), reduced["receipt"].get("baselineTables"), filled["expectedProbeValues"]),
        ("before snapshot", filled["before"].get("workspaceTables"), reduced["before"].get("workspaceTables"), filled["expectedProbeValues"]),
        ("after snapshot", filled["after"].get("workspaceTables"), reduced["after"].get("workspaceTables"), final_readbacks[0]),
    ):
        table_counts[label] = _record_count_compare_table_snapshots(left_tables, right_tables, label, values)

    filled_route = filled["route"]
    reduced_route = reduced["route"]
    filled_actions = _payload_arm_action_signature(filled_route)
    reduced_actions = _payload_arm_action_signature(reduced_route)
    filled_readbacks_signature = _payload_arm_readback_signature(filled_route)
    reduced_readbacks_signature = _payload_arm_readback_signature(reduced_route)
    filled_windows = _payload_memory_window_contract(filled)
    reduced_windows = _payload_memory_window_contract(reduced)
    package_fields = (
        "applicationSha256", "productSourceSha256", "currentSourceSha256",
        "packageSha256", "resourceInventorySha256", "releaseBuildReceiptSha256",
    )
    identity_fields = (
        "receiptSha256", "sourceSha256", "productSourceSha256", "applicationSha256",
        "packageSha256", "resourceInventorySha256",
    )
    same_provenance = all(
        filled["provenance"].get(key) == reduced["provenance"].get(key)
        and bool(filled["provenance"].get(key))
        for key in identity_fields
    )
    checks = {
        "bothArmsDiagnosticOnlyAndUnaccepted": all(
            arm["manifest"].get("status") == "diagnostic-only"
            and arm["manifest"].get("diagnosticOnly") is True
            and arm["manifest"].get("accepted") is False
            and arm["manifest"].get("recordCountDiagnostic", {}).get("diagnosticOnly") is True
            and arm["manifest"].get("recordCountDiagnostic", {}).get("accepted") is False
            for arm in (filled, reduced)
        ),
        "filledThenReducedRowsModes": (
            filled["receipt"].get("recordCountMode") == "filled"
            and reduced["receipt"].get("recordCountMode") == "reduced-rows"
        ),
        "sameReleaseReceiptSourceAppAndPackageIdentity": same_provenance and all(
            filled["package"].get(field) == reduced["package"].get(field)
            and bool(filled["package"].get(field)) for field in package_fields
        ),
        "sameFixtureSourceHashes": (
            filled["receipt"].get("baseWorkspaceSha256") == reduced["receipt"].get("baseWorkspaceSha256")
            and filled["receipt"].get("seedSqlSha256") == reduced["receipt"].get("seedSqlSha256")
            and filled["receipt"].get("preparerSha256") == reduced["receipt"].get("preparerSha256")
        ),
        "distinctNaturalExitProcesses": (
            int(filled["process"].get("processId", 0)) > 0
            and int(reduced["process"].get("processId", 0)) > 0
            and int(filled["process"].get("processId", 0)) != int(reduced["process"].get("processId", 0))
            and filled["processExit"].get("normalExit") is True
            and reduced["processExit"].get("normalExit") is True
        ),
        "sameFullObserverOnePrimerFiveBaselineTenCycleContract": (
            filled_route.get("observerMode") == reduced_route.get("observerMode") == "full-observer"
            and filled_route.get("primingCycleCount") == reduced_route.get("primingCycleCount") == 1
            and filled_route.get("baselineWindowsPerState") == reduced_route.get("baselineWindowsPerState") == 5
            and filled_route.get("measuredCycleCount") == reduced_route.get("measuredCycleCount") == 10
            and filled_route.get("observerDiagnosticOnly") is False
            and reduced_route.get("observerDiagnosticOnly") is False
        ),
        "sameTenCycleActionSignature": filled_actions == reduced_actions,
        "sameProbeEditAndReadbackSignature": filled_readbacks_signature == reduced_readbacks_signature,
        "same31WindowContract": filled_windows["windows"] == reduced_windows["windows"],
        "onlyDeclared14997CustomRowsDiffer": (
            all(counts["roster_data"]["filled"] == 22325
                and counts["roster_data"]["reducedRows"] == 7328
                for counts in table_counts.values())
            and filled["receipt"]["identity"]["databaseRosterCellCount"] == 22325
            and reduced["receipt"]["identity"]["databaseRosterCellCount"] == 7328
        ),
    }
    # Each paired stage's reduced row count is independently checked by the table comparator;
    # this explicit predicate keeps the expected per-arm counts visible in the report.
    checks["exactTargetAndDatabaseRecordCounts"] = (
        filled["receipt"]["identity"].get("targetRosterCellCount") == 15200
        and filled["receipt"]["identity"].get("targetCustomRosterCellCount") == 15000
        and filled["receipt"]["identity"].get("databaseRosterCellCount") == 22325
        and reduced["receipt"]["identity"].get("targetRosterCellCount") == 203
        and reduced["receipt"]["identity"].get("targetCustomRosterCellCount") == 3
        and reduced["receipt"]["identity"].get("databaseRosterCellCount") == 7328
    )
    checks["same100NamesAnd768Schedules"] = (
        filled["receipt"]["identity"].get("targetEnglishNameCellCount")
        == reduced["receipt"]["identity"].get("targetEnglishNameCellCount") == 100
        and filled["receipt"]["identity"].get("targetKoreanNameCellCount")
        == reduced["receipt"]["identity"].get("targetKoreanNameCellCount") == 100
        and filled["receipt"]["identity"].get("scheduleEntryCount")
        == reduced["receipt"]["identity"].get("scheduleEntryCount") == 768
    )
    passed = all(checks.values())
    return {
        "schema": RECORD_COUNT_DIAGNOSTIC_SCHEMA,
        "status": "diagnostic-only" if passed else "failed",
        "diagnosticOnly": True,
        "accepted": False,
        "armModes": ["filled", "reduced-rows"],
        "measuredCyclesPerArm": 10,
        "primingCyclesPerArm": 1,
        "matchedBaselineWindowsPerArm": 5,
        "sampleWindowContract": {
            "samplesPerWindow": 10, "sampleIntervalMilliseconds": 100,
            "workingSetTargetBytesExclusive": WORKING_SET_TARGET,
            "workingSetCeilingBytesInclusive": WORKING_SET_CEILING,
            "operationPeakAdvisoryLimitBytes": OPERATION_PEAK_ADVISORY_LIMIT,
        },
        "fixture": {
            "filledManifestPath": str(filled["manifestPath"]),
            "reducedRowsManifestPath": str(reduced["manifestPath"]),
            "filledPreparationReceiptSha256": filled["fixture"]["preparationReceipt"].get("sha256"),
            "reducedRowsPreparationReceiptSha256": reduced["fixture"]["preparationReceipt"].get("sha256"),
            "fixtureSourceHashes": {
                "baseWorkspaceSha256": filled["receipt"].get("baseWorkspaceSha256"),
                "seedSqlSha256": filled["receipt"].get("seedSqlSha256"),
                "preparerSha256": filled["receipt"].get("preparerSha256"),
            },
            "filledCustomCellRecordCount": RECORD_COUNT_FULL_ROWS,
            "reducedRowsCustomCellRecordCount": RECORD_COUNT_REDUCED_ROWS,
            "removedCustomCellRecordCount": RECORD_COUNT_REMOVED_ROWS,
            "preservedProbeCoordinates": [list(value) for value in sorted(RECORD_COUNT_PROBE_COORDINATES)],
            "scheduleEntryCount": 768,
            "filledDatabaseRosterCellCount": 22325,
            "reducedRowsDatabaseRosterCellCount": 7328,
            "tableRowCounts": table_counts,
        },
        "comparisonChecks": checks,
        "memoryComparison": _record_count_memory_deltas(filled_windows, reduced_windows),
        "failure": None if passed else {"failedChecks": [name for name, value in checks.items() if not value]},
    }


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--compare-payload-diagnostic", action="store_true")
    parser.add_argument("--compare-roster-record-count-diagnostic", action="store_true")
    parser.add_argument("--filled-manifest", type=Path)
    parser.add_argument("--sparse-manifest", type=Path)
    parser.add_argument("--reduced-rows-manifest", type=Path)
    args = parser.parse_args()
    try:
        if args.compare_payload_diagnostic:
            if args.manifest is not None or args.filled_manifest is None or args.sparse_manifest is None:
                raise ValueError("Payload comparison requires --filled-manifest and --sparse-manifest only.")
            if args.compare_roster_record_count_diagnostic or args.reduced_rows_manifest is not None:
                raise ValueError("Payload and roster record-count comparisons must be requested separately.")
            result = compare_p917_payload_diagnostic(args.filled_manifest, args.sparse_manifest)
        elif args.compare_roster_record_count_diagnostic:
            if (args.manifest is not None or args.filled_manifest is None
                    or args.sparse_manifest is not None or args.reduced_rows_manifest is None):
                raise ValueError("Roster record-count comparison requires --filled-manifest and --reduced-rows-manifest only.")
            result = compare_p917_roster_record_count_diagnostic(
                args.filled_manifest,
                args.reduced_rows_manifest,
            )
        else:
            if (args.manifest is None or args.filled_manifest is not None
                    or args.sparse_manifest is not None or args.reduced_rows_manifest is not None):
                raise ValueError("Normal P9-17 validation requires --manifest only.")
            result = validate_manifest(args.manifest)
        print(json.dumps(result, ensure_ascii=False, separators=(",", ":")))
        return 0
    except Exception as error:
        operation = (
            "payload diagnostic comparison" if args.compare_payload_diagnostic
            else "roster record-count diagnostic comparison" if args.compare_roster_record_count_diagnostic
            else "packaged large-roster validation"
        )
        print(f"P9-17 {operation} failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
