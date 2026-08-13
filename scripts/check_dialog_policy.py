#!/usr/bin/env python3
"""Reject direct Qt dialog dependencies outside the shared policy boundary."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


APPROVED_DEPENDENCIES = {
    Path("src/ui/shared/dialogs/about_dialog.cpp"): {"QMessageBox"},
    Path("src/ui/shared/dialogs/file_dialog_service.cpp"): {"QFileDialog"},
    Path("src/ui/shared/dialogs/user_prompt_service.cpp"): {"QMessageBox"},
    Path("src/ui/shared/styles/file_dialog_icon_style.cpp"): {"QFileDialog"},
}
QT_DIALOG_TYPES = ("QMessageBox", "QFileDialog")
SOURCE_SUFFIXES = {".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Ensure production code uses UserPromptService and "
            "FileDialogService instead of direct Qt dialog APIs."
        )
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="repository root (defaults to the parent of scripts/)",
    )
    return parser.parse_args()


def source_lines(path: Path):
    """Yield source lines with comments removed for stable token checks."""
    in_block_comment = False
    for line_number, original_line in enumerate(
        path.read_text(encoding="utf-8").splitlines(), start=1
    ):
        line = original_line
        cleaned = []
        index = 0
        while index < len(line):
            if in_block_comment:
                end = line.find("*/", index)
                if end == -1:
                    index = len(line)
                    continue
                in_block_comment = False
                index = end + 2
                continue

            block_start = line.find("/*", index)
            line_start = line.find("//", index)
            if line_start != -1 and (
                block_start == -1 or line_start < block_start
            ):
                cleaned.append(line[index:line_start])
                break
            if block_start == -1:
                cleaned.append(line[index:])
                break

            cleaned.append(line[index:block_start])
            in_block_comment = True
            index = block_start + 2

        yield line_number, "".join(cleaned), original_line.strip()


def find_violations(root: Path) -> list[str]:
    violations: list[str] = []
    source_root = root / "src"
    for path in sorted(source_root.rglob("*")):
        if not path.is_file() or path.suffix.lower() not in SOURCE_SUFFIXES:
            continue

        relative_path = path.relative_to(root)
        approved = APPROVED_DEPENDENCIES.get(relative_path, set())
        for line_number, source, display_line in source_lines(path):
            for dialog_type in QT_DIALOG_TYPES:
                if dialog_type in approved:
                    continue
                if re.search(rf"\b{dialog_type}\b", source):
                    violations.append(
                        f"{relative_path.as_posix()}:{line_number}: "
                        f"direct {dialog_type} dependency: {display_line}"
                    )

    return violations


def main() -> int:
    root = parse_args().root.resolve()
    violations = find_violations(root)
    if violations:
        print("Dialog policy violations found:")
        for violation in violations:
            print(f"  {violation}")
        print(
            "Use DialogServices::prompts() or DialogServices::fileDialogs(); "
            "update the narrow allowlist only for a reviewed policy boundary."
        )
        return 1

    print("Dialog policy check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
