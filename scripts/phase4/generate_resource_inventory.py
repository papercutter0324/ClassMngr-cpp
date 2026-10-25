#!/usr/bin/env python3
"""Generate the Phase 4 effective-resource inventory from tracked inputs."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Iterable


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
OUTPUT_PATH = Path("docs/qt-rewrite/phase-4-resource-inventory.json")
CALENDAR_QML_ROOT = "src/features/calendar/ui/qml/"


METHODS = {
    "installed_direct": (
        "Not measured: the Phase 4 v2 installed resource tree has not been "
        "built. Once available, stat each canonical installed file after the "
        "offline-bake/package step; report container or executable overhead "
        "separately. The source_file byte count is exact and is not a claim "
        "about a compressed RCC or future package size."
    ),
    "installed_generated": (
        "Not measured: the build-generated runtime form has not been produced "
        "for the v2 package. Stat the generated .qm, compiled QML module, or "
        "other installed output in the staged package; keep its size separate "
        "from the exact tracked source_file bytes."
    ),
    "installed_none": (
        "Not applicable: this input is excluded from the v2 runtime resource "
        "tree or is a build-only input. No installed resource file is expected "
        "from this source record."
    ),
    "resident_json": (
        "Unknown until the selected JSON is decoded by the v2 implementation. "
        "Measure the owned decoded model/container capacity and its lifetime "
        "in the resource trace; source bytes are not a decoded-memory estimate."
    ),
    "resident_image": (
        "Unknown until the real decoder and target size are selected. Load "
        "with the production Qt image path and record QImage::sizeInBytes() "
        "(or the equivalent QPixmap backing allocation) at the actual device "
        "pixel ratio and requested size. Do not sum these values as concurrent "
        "residency unless the resources are simultaneously retained."
    ),
    "resident_vector_image": (
        "Unknown until the actual SVG target dimensions and device pixel ratio "
        "are known. Render through the production Qt SVG/icon path and record "
        "the resulting raster allocation; vector source bytes are not decoded "
        "resident bytes."
    ),
    "resident_font": (
        "Unknown: source font bytes do not represent native font residency. "
        "Measure process resident-memory change around registration/removal "
        "for the specific face on each supported platform, and record the "
        "loaded family and registration lifetime."
    ),
    "resident_pdf": (
        "Unknown and content-dependent. Measure the active QPdfDocument load "
        "and page-render path with the intended file-backed input; record the "
        "rendered page QImage::sizeInBytes(), active document count, and "
        "process working-set/RSS deltas. Close/replacement measurements must "
        "show release. No PDF body is counted as startup residency."
    ),
    "resident_office": (
        "Unknown and consumer-dependent. Measure the actual Office import, "
        "export, or external-process path for a requested file, including "
        "temporary buffers and process working-set deltas. Do not infer a "
        "decoded size from ZIP/archive source bytes."
    ),
    "resident_stylesheet": (
        "Unknown until the theme loader is implemented. Measure the decoded "
        "stylesheet string and retained style object after loading one active "
        "theme; measure release or replacement when the theme changes."
    ),
    "resident_translation": (
        "Unknown until the selected generated .qm is loaded. Measure the "
        "QTranslator load and process resident-memory delta for one locale; "
        "verify that replacing a locale releases the prior translator."
    ),
    "resident_qml": (
        "Unknown until the compiled QML module and Calendar object tree are "
        "measured. Record the installed module bytes separately from the "
        "Calendar page's object and image residency, then measure page release."
    ),
    "resident_build_only": (
        "Not applicable to the v2 runtime loader: this record is a build input "
        "or is excluded. If its generated output is installed, measure that "
        "output under its own runtime resource record."
    ),
    "stream_file_document": (
        "Candidate: open a requested installed file through a file-backed "
        "QIODevice/consumer interface, without first copying the full body to "
        "a catalog buffer. Confirm the chosen adapter's actual read and cache "
        "behavior in its implementation and resource trace."
    ),
    "stream_consumer_dependent": (
        "Consumer-dependent: pass the requested file path or file-backed "
        "device to the owning Office/import/export adapter. Confirm whether "
        "that adapter streams or materializes the complete document before "
        "assigning a final streamability guarantee."
    ),
    "stream_no": (
        "No streaming contract is planned for this small metadata, style, "
        "font, icon, or image resource. Load/decode only at the owning tier "
        "and retain it only for its declared lifetime."
    ),
    "stream_build_only": (
        "Not applicable to a runtime resource: this is a build-time source "
        "input or an excluded file."
    ),
}


def _tracked_paths(root: Path, pathspec: str) -> list[str]:
    result = subprocess.run(
        ["git", "ls-files", "--", pathspec],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
    )
    return sorted(line for line in result.stdout.splitlines() if line)


def _git_tree(root: Path, pathspec: str) -> str | None:
    result = subprocess.run(
        ["git", "rev-parse", f"HEAD:{pathspec}"],
        cwd=root,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        return None
    return result.stdout.strip()


def _resource_status(root: Path, paths: Iterable[str]) -> list[str]:
    """Return tracked input paths whose working-tree content differs from HEAD."""
    paths = sorted(paths)
    if not paths:
        return []
    result = subprocess.run(
        ["git", "status", "--short", "--", *paths],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
    )
    changed: list[str] = []
    for line in result.stdout.splitlines():
        # `git status --short` has two status characters, a space, then path.
        path = line[3:] if len(line) > 3 else ""
        if path:
            changed.append(path)
    return sorted(changed)


def _record_policy(path: str) -> dict[str, str]:
    if path == "resources/assets/documents/documents.json":
        return {
            "collection": "document_catalog_metadata",
            "kind": "document_catalog_metadata",
            "category": "documents",
            "owner": "Document Catalog metadata service",
            "disposition": "runtime_packaged",
            "reason": (
                "Catalog metadata is retained independently from document "
                "bodies; catalog initialization must not load PDF/PPTX bodies."
            ),
            "startup": "required_metadata_only",
            "tier": "core_catalog_metadata",
            "load": (
                "Read and decode this JSON metadata file into the document "
                "catalog. Resolve body paths without opening the referenced "
                "PDF/PPTX/Office files."
            ),
            "lifetime": "Document catalog/application-service lifetime.",
            "release": (
                "Keep catalog metadata while the catalog service is active; "
                "release it with that service. Never retain document bodies "
                "as a side effect of catalog initialization."
            ),
            "stream": "no",
            "stream_ref": "stream_no",
            "resident_ref": "resident_json",
            "installed_ref": "installed_direct",
        }

    if path.startswith("resources/assets/documents/"):
        if "/~$" in path or Path(path).name.startswith("~$"):
            return {
                "collection": "excluded_resources",
                "kind": "office_lock_file",
                "category": "documents",
                "owner": "Office transient lock artifact",
                "disposition": "excluded_v2",
                "reason": (
                    "The legacy documents RCC glob includes this tracked "
                    "165-byte Office lock/temporary payload, as recorded by "
                    "the Phase 0 resource trace. The v2 canonical resource "
                    "tree excludes it as a transient lock artifact; it is not "
                    "a document catalog body."
                ),
                "startup": "not_required",
                "tier": "excluded",
                "load": "Do not load or install this transient lock artifact.",
                "lifetime": "No v2 runtime lifetime.",
                "release": "Excluded from the v2 runtime package.",
                "stream": "not_applicable",
                "stream_ref": "stream_build_only",
                "resident_ref": "resident_build_only",
                "installed_ref": "installed_none",
            }
        return {
            "collection": "document_bodies",
            "kind": "document_body",
            "category": "documents",
            "owner": "Document viewer and document operation workflows",
            "disposition": "runtime_packaged",
            "reason": (
                "Canonical document body available in the installed resource "
                "tree; open only when the selected document or operation "
                "requests it."
            ),
            "startup": "not_required_until_document_request",
            "tier": "viewer_session_or_operation",
            "load": _document_load_method(path),
            "lifetime": (
                "Selected viewer session or owning print/export/import "
                "operation only."
            ),
            "release": (
                "Close the active document on viewer close, replacement, "
                "feature release, or operation completion; release rendered "
                "page buffers with their owner."
            ),
            "stream": _document_streamability(path),
            "stream_ref": _document_stream_method(path),
            "resident_ref": _document_resident_method(path),
            "installed_ref": "installed_direct",
        }

    if path.startswith("resources/assets/templates/speaking-eval/sources/"):
        return {
            "collection": "build_inputs",
            "kind": "speaking_report_art_source",
            "category": "templates",
            "owner": "Speaking Evaluation report-art build inputs",
            "disposition": "build_only_excluded",
            "reason": (
                "Source art/specification inputs used to produce the checked-in "
                "speaking-evaluation runtime artwork; this sources/ subtree is "
                "excluded from the runtime resource pack."
            ),
            "startup": "not_required",
            "tier": "build_only",
            "load": "Consumed only by the report-art preparation/build process.",
            "lifetime": "Build input only; no runtime lifetime.",
            "release": "Not copied into the v2 runtime resource tree.",
            "stream": "not_applicable",
            "stream_ref": "stream_build_only",
            "resident_ref": "resident_build_only",
            "installed_ref": "installed_none",
        }

    if path.startswith("resources/assets/splash/"):
        return {
            "collection": "excluded_resources",
            "kind": "legacy_splash_asset",
            "category": "splash",
            "owner": "Legacy startup splash screen",
            "disposition": "excluded_v2",
            "reason": (
                "The Phase 4 packaging-removal decision explicitly removes "
                "splash resources and splash paths from the v2 runtime. The "
                "tracked legacy asset is retained in this inventory for audit."
            ),
            "startup": "excluded_by_v2_decision",
            "tier": "excluded",
            "load": "No v2 loader request; retained only as a legacy source record.",
            "lifetime": "No v2 runtime lifetime.",
            "release": "Excluded from the v2 runtime package.",
            "stream": "not_applicable",
            "stream_ref": "stream_build_only",
            "resident_ref": "resident_build_only",
            "installed_ref": "installed_none",
        }

    if path.startswith("resources/windows/"):
        return {
            "collection": "build_inputs",
            "kind": "windows_packaging_template",
            "category": "platform_packaging",
            "owner": "Windows build/deployment configuration",
            "disposition": "build_only_excluded",
            "reason": (
                "Template/configuration input consumed during Windows "
                "configure or deployment; it is not a v2 ResourceLoader item."
            ),
            "startup": "not_a_runtime_resource",
            "tier": "build_only",
            "load": "Read by the relevant CMake configure/deployment step.",
            "lifetime": "Build or deployment process only.",
            "release": "No source template is installed as a resource.",
            "stream": "not_applicable",
            "stream_ref": "stream_build_only",
            "resident_ref": "resident_build_only",
            "installed_ref": "installed_none",
        }

    return _asset_policy(path)


def _document_load_method(path: str) -> str:
    suffix = Path(path).suffix.lower()
    if suffix == ".pdf":
        return (
            "Open only after an explicit viewer/operation request through the "
            "file-backed PDF adapter; the document catalog reads metadata only."
        )
    return (
        f"Open the {suffix.lstrip('.').upper()} body only through the owning "
        "import/export or document operation adapter; never during catalog "
        "initialization."
    )


def _document_streamability(path: str) -> str:
    return "candidate_file_backed" if Path(path).suffix.lower() == ".pdf" else "consumer_dependent"


def _document_stream_method(path: str) -> str:
    return "stream_file_document" if Path(path).suffix.lower() == ".pdf" else "stream_consumer_dependent"


def _document_resident_method(path: str) -> str:
    return "resident_pdf" if Path(path).suffix.lower() == ".pdf" else "resident_office"


def _asset_policy(path: str) -> dict[str, str]:
    if path.startswith("resources/assets/campuses/"):
        if Path(path).suffix.lower() == ".json":
            method = "Read the requested campus JSON on campus-feature entry; decode only the selected campus record."
            resident = "resident_json"
        else:
            method = "Decode the requested campus map/building image when the campus/map view needs it."
            resident = "resident_image"
        return _runtime_policy(
            path,
            "campus_data" if Path(path).suffix.lower() == ".json" else "campus_map_image",
            "campuses",
            "Campus pages and campus map views",
            "feature_on_demand",
            "not_required_until_campus_feature_use",
            method,
            "Campus feature/view lifetime; retain only images needed by the active view.",
            "Release decoded maps when the owning campus view/cache is discarded; budget any retained map cache.",
            "no",
            "stream_no",
            resident,
        )

    if path.startswith("resources/assets/files/"):
        suffix = Path(path).suffix.lower()
        resident = "resident_office" if suffix in {".ods", ".odt", ".xlsx"} else "resident_build_only"
        return _runtime_policy(
            path,
            "roster_workflow_file",
            "files",
            "Roster import/export and roster-template workflows",
            "operation_on_demand",
            "not_required_until_roster_operation",
            "Open the selected roster template/file only for the owning import or export operation.",
            "Owning roster import/export operation only.",
            "Release file handles and temporary model/buffer state when the operation completes.",
            "consumer_dependent",
            "stream_consumer_dependent",
            resident,
        )

    if path.startswith("resources/assets/fonts/"):
        filename = Path(path).name
        core_font = filename in {"Inter.ttc", "PretendardVariable.ttf"}
        return _runtime_policy(
            path,
            "required_ui_font" if core_font else "optional_decorative_font",
            "fonts",
            "Shared UI typography" if core_font else "Optional typed-signature and report typography",
            "core_startup" if core_font else "feature_on_demand",
            "required_for_primary_ui" if core_font else "not_required_until_signature_or_report_use",
            "Register the required family at UI startup." if core_font else "Register only when the owning signature or speaking-report feature requests this family.",
            "Process lifetime for required UI families." if core_font else "Owning signature feature or report operation; avoid eager process-wide registration.",
            "Keep required UI families for process lifetime; remove optional registrations when the owning feature/operation releases them.",
            "no",
            "stream_no",
            "resident_font",
        )

    if path.startswith("resources/assets/icons/"):
        if "/file_dialog/" in path:
            tier = "feature_on_demand"
            startup = "not_required_until_file_dialog_use"
            lifetime = "Active file-dialog style/widget lifetime; Qt may retain a decoded icon cache."
            release = "Drop icon/style references with the dialog and bound any cross-dialog icon cache."
        elif Path(path).name in {"app_icon.ico", "app_icon.icns"}:
            tier = "platform_package"
            startup = "platform_shell_metadata"
            lifetime = "Installed application shell lifetime."
            release = "Owned by the platform application package; not a ResourceLoader cache entry."
        elif Path(path).name == "icon_256x256.png":
            tier = "core_startup"
            startup = "application_icon_on_startup"
            lifetime = "Application shell/icon lifetime; decode only if an in-app icon request needs it."
            release = "Release decoded image cache when application icon references are discarded."
        else:
            tier = "core_ui_on_demand"
            startup = "not_required_until_widget_or_style_use"
            lifetime = "Owning widget/style lifetime; Qt may keep a decoded icon cache."
            release = "Drop icon references with the owning UI and bound shared decoded-icon caching."
        return _runtime_policy(
            path,
            "icon",
            "icons",
            "Shared application UI and platform shell",
            tier,
            startup,
            "Load or decode only when the owning widget, active stylesheet, file dialog, or platform shell requests this icon.",
            lifetime,
            release,
            "no",
            "stream_no",
            "resident_vector_image" if Path(path).suffix.lower() == ".svg" else "resident_image",
        )

    if path.startswith("resources/assets/images/"):
        if path == "resources/assets/images/dyb.png":
            return _runtime_policy(
                path,
                "optional_dynamic_image",
                "images",
                "Shared legacy helper only: ResourcePaths::DynamicImages; no production feature caller found",
                "feature_or_operation_on_demand_if_claimed",
                "not_required_no_legacy_feature_callsite",
                (
                    "The v2 destination is an optional typed resource descriptor "
                    "in ClassMngrNext::Resources. Decode only after a migrated "
                    "feature or operation explicitly requests its ResourceId; "
                    "the current source tree has no production request call site."
                ),
                "No current feature consumer. If claimed during migration, retain the decoded image only for that feature/page or operation.",
                "Do not load at startup. The requesting feature/operation releases decoded pixels on leave/completion; add no shared cache without an explicit budget.",
                "no",
                "stream_no",
                "resident_image",
                reason=(
                    "The tracked image is part of the legacy optional images RCC "
                    "and is retained as an optional v2 resource-catalog entry. "
                    "No feature-level production caller currently requests it; "
                    "the v2 catalog must not cause startup decoding."
                ),
                migration_destination=(
                    "ClassMngrNext::Resources optional typed catalog descriptor; "
                    "a future owning feature/operation must request it explicitly. "
                    "Do not recreate ResourcePaths::DynamicImages as a global "
                    "compatibility helper."
                ),
            )
        return _runtime_policy(
            path,
            "feature_or_operation_image",
            "images",
            "DynamicImages resource path; specific feature caller to confirm",
            "operation_on_demand",
            "not_required_until_output_or_branding_use",
            "Decode only when an owning report/output operation requests the image.",
            "Owning report/output operation.",
            "Release decoded image data and temporary output buffers when the operation completes.",
            "no",
            "stream_no",
            "resident_image",
        )

    if path.startswith("resources/assets/styles/"):
        return _runtime_policy(
            path,
            "theme_stylesheet",
            "styles",
            "Shared theme service",
            "core_startup_selected_theme",
            "active_theme_only",
            "Read and apply only the selected theme stylesheet; replace it on a theme change.",
            "Application UI lifetime for the active theme.",
            "Release the prior stylesheet/string when the theme changes; retain one active theme.",
            "no",
            "stream_no",
            "resident_stylesheet",
        )

    if path.startswith("resources/assets/translations/"):
        return _runtime_policy(
            path,
            "translation_source_for_generated_qm",
            "translations",
            "Language service",
            "core_startup_selected_locale",
            "selected_locale_only",
            "Compile this tracked TS source into a QM runtime translation, then load only the selected locale with QTranslator.",
            "Current application language session until replaced.",
            "Replace the active translator atomically and release the prior translator after replacement.",
            "no",
            "stream_no",
            "resident_translation",
            disposition="runtime_packaged_generated",
            reason="Qt Linguist produces the runtime .qm from this tracked TS source; only the selected locale is loaded.",
            installed_ref="installed_generated",
        )

    if path.startswith("resources/assets/templates/"):
        if Path(path).suffix.lower() == ".json":
            resident = "resident_json"
            kind = "speaking_report_template_metadata"
            method = "Read the selected speaking-report template metadata only when that report feature is requested."
        else:
            resident = "resident_image"
            kind = "speaking_report_artwork"
            method = "Decode only the selected report artwork when generating or previewing a speaking-evaluation report."
        return _runtime_policy(
            path,
            kind,
            "templates",
            "Speaking Evaluation report generation and preview",
            "operation_on_demand",
            "not_required_until_speaking_report_use",
            method,
            "Speaking-report preview/export operation only.",
            "Release decoded artwork and report-font registrations on operation completion; any reusable cache needs an explicit budget.",
            "no",
            "stream_no",
            resident,
        )

    raise ValueError(f"No resource policy for tracked path: {path}")


def _runtime_policy(
    path: str,
    kind: str,
    category: str,
    owner: str,
    tier: str,
    startup: str,
    load: str,
    lifetime: str,
    release: str,
    stream: str,
    stream_ref: str,
    resident_ref: str,
    *,
    disposition: str = "runtime_packaged",
    reason: str = "Canonical source payload is part of the v2 installed resource tree.",
    installed_ref: str = "installed_direct",
    migration_destination: str | None = None,
) -> dict[str, str]:
    policy = {
        "collection": "resources",
        "kind": kind,
        "category": category,
        "owner": owner,
        "disposition": disposition,
        "reason": reason,
        "startup": startup,
        "tier": tier,
        "load": load,
        "lifetime": lifetime,
        "release": release,
        "stream": stream,
        "stream_ref": stream_ref,
        "resident_ref": resident_ref,
        "installed_ref": installed_ref,
    }
    if migration_destination is not None:
        policy["migration_destination"] = migration_destination
    return policy


def _calendar_qml_record(root: Path, path: str) -> dict[str, object]:
    source = root / path
    if not source.is_file():
        raise FileNotFoundError(f"Tracked calendar QML source is missing: {path}")
    return {
        "logical_id": "calendar.qml:" + Path(path).name,
        "source_path": path,
        "resource_kind": "calendar_qml_module_source",
        "category": "calendar_qml",
        "owner_feature": "Calendar page and Qt QML module",
        "package_disposition": "runtime_packaged_generated",
        "disposition_reason": (
            "Tracked QML source outside resources/; CMake compiles it into the "
            "ClassMngr.Calendar module under the Qt QML resource namespace."
        ),
        "source_file": _source_file_record(source),
        "installed_size": {
            "bytes": None,
            "status": "not_measured_generated_module",
            "method_ref": "installed_generated",
        },
        "startup_necessity": "not_required_until_calendar_feature_use",
        "loading_tier": "calendar_feature_on_demand",
        "loading_method": (
            "Load the compiled QML component when the Calendar UI requests it; "
            "do not parse the source file as application startup metadata."
        ),
        "expected_lifetime": "Calendar page/component lifetime; compiled module resource remains available with the application package.",
        "release_policy": "Destroy Calendar QML objects when the owning page is released; the compiled module remains package-owned.",
        "streamability": {
            "status": "no",
            "method_ref": "stream_no",
        },
        "expected_resident_size": {
            "bytes": None,
            "status": "unknown_until_compiled_module_and_page_are_measured",
            "method_ref": "resident_qml",
        },
    }


def _source_file_record(source: Path) -> dict[str, object]:
    data = source.read_bytes()
    return {
        "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
    }


def _make_resource_record(root: Path, path: str) -> tuple[str, dict[str, object]]:
    source = root / path
    if not source.is_file():
        raise FileNotFoundError(f"Tracked resource is missing: {path}")
    policy = _record_policy(path)
    logical_id = (
        "document.catalog-metadata"
        if policy["collection"] == "document_catalog_metadata"
        else "document.body:" + path.removeprefix("resources/assets/documents/")
        if policy["collection"] == "document_bodies"
        else "resource:" + path.removeprefix("resources/")
    )
    record: dict[str, object] = {
        "logical_id": logical_id,
        "source_path": path,
        "resource_kind": policy["kind"],
        "category": policy["category"],
        "owner_feature": policy["owner"],
        "package_disposition": policy["disposition"],
        "disposition_reason": policy["reason"],
        "source_file": _source_file_record(source),
        "installed_size": {
            "bytes": None,
            "status": "not_applicable" if policy["installed_ref"] == "installed_none" else "not_measured",
            "method_ref": policy["installed_ref"],
        },
        "startup_necessity": policy["startup"],
        "loading_tier": policy["tier"],
        "loading_method": policy["load"],
        "expected_lifetime": policy["lifetime"],
        "release_policy": policy["release"],
        "streamability": {
            "status": policy["stream"],
            "method_ref": policy["stream_ref"],
        },
        "expected_resident_size": {
            "bytes": None,
            "status": "unknown" if policy["resident_ref"] != "resident_build_only" else "not_applicable",
            "method_ref": policy["resident_ref"],
        },
    }
    if "migration_destination" in policy:
        record["v2_migration_destination"] = policy["migration_destination"]
    return policy["collection"], record


def _category_summary(records: Iterable[dict[str, object]]) -> dict[str, dict[str, int]]:
    counts: dict[str, Counter[str]] = defaultdict(Counter)
    for record in records:
        category = str(record["category"])
        source = record["source_file"]
        assert isinstance(source, dict)
        counts[category]["files"] += 1
        counts[category]["source_bytes"] += int(source["bytes"])
    return {
        category: dict(counts[category])
        for category in sorted(counts)
    }


def build_inventory(
    root: Path,
    resource_paths: Iterable[str],
    calendar_qml_paths: Iterable[str] = (),
    *,
    source_tree_ids: dict[str, str | None] | None = None,
    changed_input_paths: Iterable[str] = (),
) -> dict[str, object]:
    collections: dict[str, list[dict[str, object]]] = {
        "resources": [],
        "document_catalog_metadata": [],
        "document_bodies": [],
        "excluded_resources": [],
        "build_inputs": [],
    }
    seen_paths: set[str] = set()
    for path in sorted(resource_paths):
        if path in seen_paths:
            raise ValueError(f"Duplicate tracked resource path: {path}")
        seen_paths.add(path)
        collection, record = _make_resource_record(root, path)
        collections[collection].append(record)

    calendar_records = [
        _calendar_qml_record(root, path)
        for path in sorted(calendar_qml_paths)
    ]
    all_records = [record for group in collections.values() for record in group]
    all_records.extend(calendar_records)
    all_ids = [str(record["logical_id"]) for record in all_records]
    if len(all_ids) != len(set(all_ids)):
        raise ValueError("Resource logical IDs must be unique")

    asset_records = [
        record
        for group in collections.values()
        for record in group
        if str(record["source_path"]).startswith("resources/assets/")
    ]
    asset_source_bytes = sum(
        int(record["source_file"]["bytes"])
        for record in asset_records
    )
    asset_directory_summary: dict[str, dict[str, int]] = {}
    for record in asset_records:
        path = str(record["source_path"])
        asset_root = path.removeprefix("resources/assets/").split("/", 1)[0]
        category = asset_directory_summary.setdefault(asset_root, {"files": 0, "source_bytes": 0})
        category["files"] += 1
        category["source_bytes"] += int(record["source_file"]["bytes"])

    runtime_records = [
        record
        for record in all_records
        if str(record["package_disposition"]).startswith("runtime_packaged")
    ]
    disposition_counts = Counter(str(record["package_disposition"]) for record in all_records)

    return {
        "schema_version": 1,
        "title": "Phase 4 effective resource inventory",
        "source_snapshot": {
            "tracked_source_tree_ids": source_tree_ids or {},
            "tracked_input_worktree_changes": sorted(set(changed_input_paths)),
            "effective_baseline": (
                "Tracked source content at the recorded Git tree IDs plus the exact working-tree "
                "bytes recorded below. No installed resource-pack overrides "
                "were available in the checked application-owned resource-pack directory."
            ),
        },
        "scope": {
            "tracked_files_under_resources": len(seen_paths),
            "tracked_files_under_resources_assets": len(asset_records),
            "tracked_files_under_resources_windows": sum(
                str(record["source_path"]).startswith("resources/windows/")
                for record in all_records
            ),
            "tracked_calendar_qml_sources_outside_resources": len(calendar_records),
            "resource_asset_source_bytes_exact": asset_source_bytes,
            "all_tracked_resources_source_bytes_exact": sum(
                int(record["source_file"]["bytes"])
                for record in all_records
                if str(record["source_path"]).startswith("resources/")
            ),
            "calendar_qml_source_bytes_exact": sum(
                int(record["source_file"]["bytes"])
                for record in calendar_records
            ),
            "disposition_record_counts": dict(sorted(disposition_counts.items())),
            "resource_assets_by_directory": {
                key: asset_directory_summary[key]
                for key in sorted(asset_directory_summary)
            },
            "resource_categories": _category_summary(all_records),
        },
        "size_methodology": {
            "source_file_bytes": (
                "Exact current working-tree file length in bytes; sha256 is "
                "computed over the same exact bytes. Tracked inputs only."
            ),
            "installed_size": (
                "The v2 installed resource tree does not yet exist. Each record "
                "therefore reports installed_size.bytes=null with a direct or "
                "generated output measurement method. No RCC-compressed or "
                "application-container size is inferred from source bytes."
            ),
            "decoded_or_resident_size": (
                "Unknown unless produced by the future runtime measurement "
                "method referenced on that record. Source file bytes are not "
                "used as decoded/resident estimates. Image, font, PDF, Office, "
                "stylesheet, translation, JSON, and QML methods are explicit "
                "in measurement_methods."
            ),
        },
        "measurement_methods": METHODS,
        "absence_observations": {
            "roster_designs": {
                "state": "absent_from_source_and_effective_override_snapshot",
                "source_path": "resources/assets/roster-designs/",
                "tracked_file_count": 0,
                "installed_override_directory_checked": "~/Library/Application Support/ClassMngr/resource-packs",
                "installed_override_state": "empty",
                "note": (
                    "The legacy project declares roster-designs as optional, "
                    "but this checkout has no source payload and the checked "
                    "application-owned resource-pack directory is empty. "
                    "Do not synthesize or estimate entries."
                ),
            },
            "calendar_qml": {
                "state": "tracked_outside_resources_and_listed_separately",
                "source_paths": [str(record["source_path"]) for record in calendar_records],
                "note": (
                    "These build/package resource inputs live under src/, not "
                    "resources/, so they are reported in calendar_qml_sources "
                    "without changing the 226-file resources/assets baseline."
                ),
            },
        },
        "document_catalog_metadata": collections["document_catalog_metadata"],
        "document_bodies": collections["document_bodies"],
        "resources": collections["resources"],
        "excluded_resources": collections["excluded_resources"],
        "build_inputs": collections["build_inputs"],
        "calendar_qml_sources": calendar_records,
    }


def render_json(inventory: dict[str, object]) -> str:
    return json.dumps(inventory, indent=2, ensure_ascii=False, sort_keys=False) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=REPOSITORY_ROOT)
    parser.add_argument("--output", type=Path, default=REPOSITORY_ROOT / OUTPUT_PATH)
    parser.add_argument("--check", action="store_true", help="Fail when the checked-in inventory is stale.")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    output = args.output if args.output.is_absolute() else root / args.output
    resource_paths = _tracked_paths(root, "resources")
    calendar_qml_paths = [
        path
        for path in _tracked_paths(root, CALENDAR_QML_ROOT)
        if path.endswith(".qml")
    ]
    changed = _resource_status(root, [*resource_paths, *calendar_qml_paths])
    inventory = build_inventory(
        root,
        resource_paths,
        calendar_qml_paths,
        source_tree_ids={
            "resources": _git_tree(root, "resources"),
            "calendar_qml": _git_tree(root, "src/features/calendar/ui/qml"),
        },
        changed_input_paths=changed,
    )
    rendered = render_json(inventory)
    if args.check:
        if not output.is_file() or output.read_text(encoding="utf-8") != rendered:
            print(f"stale inventory: {output}", file=sys.stderr)
            return 1
        print(f"inventory is current: {output}")
        return 0

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(rendered, encoding="utf-8", newline="\n")
    print(f"wrote {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
