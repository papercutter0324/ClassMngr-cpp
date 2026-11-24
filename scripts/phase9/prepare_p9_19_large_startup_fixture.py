#!/usr/bin/env python3
"""Prepare a fresh P9-19 workspace from the strictly pinned large fixture."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import sys
from pathlib import Path
from typing import Any


SCHEMA = "classmngr-phase9-p9-19-large-startup-fixture-v1"
BASE_SHA256 = "85c23437621fe67361b38157bb53acdb19e78573ebaa0297fad769b71ad0c9e9"
SEED_CANONICAL_SHA256 = "1b2f09be03cce809e239112b3f7b4f2af405bad83a420db0c6ffe5ef0eb597e3"
CLASS_COUNT = 96
SCHEDULE_ENTRY_COUNT = 768
P916_MODULE_PATH = Path(__file__).with_name("prepare_p9_16_workspace.py")
P916_SPEC = importlib.util.spec_from_file_location("p916_workspace_for_p919", P916_MODULE_PATH)
assert P916_SPEC is not None and P916_SPEC.loader is not None
p916 = importlib.util.module_from_spec(P916_SPEC)
P916_SPEC.loader.exec_module(p916)


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def canonical_seed_bytes(seed: Path) -> tuple[bytes, dict[str, Any]]:
    raw = seed.read_bytes()
    canonical = raw.replace(b"\r\n", b"\n")
    if b"\r" in canonical:
        raise ValueError("large_startup.sql contains a bare carriage return.")
    raw_sha256 = sha256_bytes(raw)
    canonical_sha256 = sha256_bytes(canonical)
    if canonical_sha256 != SEED_CANONICAL_SHA256:
        raise ValueError(
            "large_startup.sql canonical-LF content differs from its strict pinned fingerprint."
        )
    # Decode here so malformed UTF-8 fails before any workspace is written.
    canonical.decode("utf-8", errors="strict")
    return canonical, {
        "rawSha256": raw_sha256,
        "rawBytes": len(raw),
        "canonicalLfSha256": canonical_sha256,
        "canonicalLfBytes": len(canonical),
        "lineEndingNormalization": "CRLF-to-LF" if raw != canonical else "none",
    }


def prepare(
    base: Path,
    seed: Path,
    output: Path,
    receipt_path: Path,
    p916_receipt_path: Path,
    canonical_seed_path: Path,
) -> dict[str, Any]:
    base = base.resolve(strict=True)
    seed = seed.resolve(strict=True)
    output = output.resolve()
    receipt_path = receipt_path.resolve()
    p916_receipt_path = p916_receipt_path.resolve()
    canonical_seed_path = canonical_seed_path.resolve()

    if sha256_file(base) != BASE_SHA256:
        raise ValueError("Master blank workspace differs from its strict pinned fingerprint.")
    canonical, seed_fingerprints = canonical_seed_bytes(seed)
    for path in (output, receipt_path, p916_receipt_path, canonical_seed_path):
        if path.exists():
            raise ValueError(f"Refusing to overwrite P9-19 fixture artifact {path}")
    if len({output, receipt_path, p916_receipt_path, canonical_seed_path}) != 4:
        raise ValueError("P9-19 fixture output paths must be distinct.")

    canonical_seed_path.parent.mkdir(parents=True, exist_ok=True)
    canonical_seed_path.write_bytes(canonical)
    p916_receipt = p916.prepare(base, canonical_seed_path, output, "large")
    p916_receipt_path.parent.mkdir(parents=True, exist_ok=True)
    p916.write_new_json(p916_receipt_path, p916_receipt)

    receipt = {
        "schema": SCHEMA,
        "status": "passed",
        "kind": "fresh-p9-19-large-startup-workspace",
        "workspace": str(output),
        "workspaceSha256": p916_receipt["workspaceSha256"],
        "workspaceBytes": p916_receipt["workspaceBytes"],
        "baseWorkspace": str(base),
        "baseWorkspaceSha256": sha256_file(base),
        "baseWorkspaceSha256Pinned": BASE_SHA256,
        "seedSql": str(seed),
        "seedSqlRawSha256": seed_fingerprints["rawSha256"],
        "seedSqlRawBytes": seed_fingerprints["rawBytes"],
        "seedSqlCanonicalLfSha256": seed_fingerprints["canonicalLfSha256"],
        "seedSqlCanonicalLfBytes": seed_fingerprints["canonicalLfBytes"],
        "seedSqlLineEndingNormalization": seed_fingerprints[
            "lineEndingNormalization"
        ],
        "seedSqlCanonicalLfPinnedSha256": SEED_CANONICAL_SHA256,
        "canonicalSeedSql": str(canonical_seed_path),
        "canonicalSeedSqlSha256": sha256_file(canonical_seed_path),
        "p916PreparationReceipt": str(p916_receipt_path),
        "p916PreparationReceiptSha256": sha256_file(p916_receipt_path),
        "identity": p916_receipt["identity"],
        "identityRule": p916_receipt["identityRule"],
    }
    receipt_path.parent.mkdir(parents=True, exist_ok=True)
    write_new_json(receipt_path, receipt)
    return receipt


def read_receipt(path: Path, workspace: Path) -> dict[str, Any]:
    receipt = json.loads(path.read_text(encoding="utf-8"))
    if receipt.get("schema") != SCHEMA or receipt.get("status") != "passed":
        raise ValueError("Preparation receipt does not identify the P9-19 fixture.")
    if Path(str(receipt.get("workspace", ""))).resolve() != workspace.resolve():
        raise ValueError("Requested workspace path differs from the P9-19 receipt.")
    if receipt.get("baseWorkspaceSha256") != BASE_SHA256:
        raise ValueError("Preparation receipt does not retain the strict base workspace pin.")
    if receipt.get("seedSqlCanonicalLfSha256") != SEED_CANONICAL_SHA256:
        raise ValueError("Preparation receipt does not retain the canonical SQL pin.")
    if receipt.get("identity") != {
        "classCount": CLASS_COUNT,
        "classInfoCount": CLASS_COUNT,
        "scheduleEntryCount": SCHEDULE_ENTRY_COUNT,
    }:
        raise ValueError("Preparation receipt has an unexpected large fixture identity.")
    return receipt


def snapshot(phase: str, workspace: Path, receipt_path: Path) -> dict[str, Any]:
    workspace = workspace.resolve(strict=True)
    receipt_path = receipt_path.resolve(strict=True)
    receipt = read_receipt(receipt_path, workspace)
    p916_receipt_path = Path(str(receipt["p916PreparationReceipt"])).resolve(strict=True)
    if sha256_file(p916_receipt_path) != receipt["p916PreparationReceiptSha256"]:
        raise ValueError("P9-16 preparation receipt changed after P9-19 fixture creation.")
    result = p916.snapshot(phase, workspace, p916_receipt_path)
    result.update(
        {
            "schema": SCHEMA,
            "phase": phase,
            "workspace": str(workspace),
            "workspaceSha256": sha256_file(workspace),
            "preparationReceiptSha256": sha256_file(receipt_path),
            "baseWorkspaceSha256": receipt["baseWorkspaceSha256"],
            "seedSqlRawSha256": receipt["seedSqlRawSha256"],
            "seedSqlCanonicalLfSha256": receipt["seedSqlCanonicalLfSha256"],
            "readOnlyConnection": True,
            "readOnlyConnectionMode": "SQLite URI mode=ro with PRAGMA query_only=ON",
        }
    )
    return result


def write_new_json(path: Path, value: dict[str, Any]) -> None:
    path = path.resolve()
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        raise ValueError(f"Refusing to overwrite evidence file {path}")
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(dest="command", required=True)
    prepare_parser = commands.add_parser("prepare")
    prepare_parser.add_argument("--base-workspace", type=Path, required=True)
    prepare_parser.add_argument("--seed-sql", type=Path, required=True)
    prepare_parser.add_argument("--output", type=Path, required=True)
    prepare_parser.add_argument("--receipt", type=Path, required=True)
    prepare_parser.add_argument("--p916-receipt", type=Path, required=True)
    prepare_parser.add_argument("--canonical-seed-sql", type=Path, required=True)
    snapshot_parser = commands.add_parser("snapshot")
    snapshot_parser.add_argument("--phase", choices=("before", "after"), required=True)
    snapshot_parser.add_argument("--workspace", type=Path, required=True)
    snapshot_parser.add_argument("--preparation-receipt", type=Path, required=True)
    snapshot_parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "prepare":
            receipt = prepare(
                args.base_workspace,
                args.seed_sql,
                args.output,
                args.receipt,
                args.p916_receipt,
                args.canonical_seed_sql,
            )
            result = {
                "schema": SCHEMA,
                "status": "passed",
                "workspace": receipt["workspace"],
                "workspaceSha256": receipt["workspaceSha256"],
                "identity": receipt["identity"],
                "seedSqlRawSha256": receipt["seedSqlRawSha256"],
                "seedSqlCanonicalLfSha256": receipt[
                    "seedSqlCanonicalLfSha256"
                ],
                "receipt": str(args.receipt.resolve()),
                "receiptSha256": sha256_file(args.receipt.resolve()),
            }
        else:
            result = snapshot(args.phase, args.workspace, args.preparation_receipt)
            write_new_json(args.output, result)
            result = {
                "schema": SCHEMA,
                "status": "passed",
                "phase": args.phase,
                "snapshot": str(args.output.resolve()),
                "snapshotSha256": sha256_file(args.output.resolve()),
                "identity": result["identity"],
                "integrityCheck": result["integrityCheck"],
            }
        print(json.dumps(result, ensure_ascii=False, separators=(",", ":")))
        return 0
    except Exception as error:
        print(f"P9-19 fixture preparation/inspection failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
