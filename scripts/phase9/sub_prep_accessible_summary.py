#!/usr/bin/env python3
"""Derive the exact accessible row label for a Sub Prep class from its fixture."""

from __future__ import annotations

import argparse
import collections
import datetime as dt
import json
import sqlite3
import sys
from pathlib import Path
from typing import Any


DEFAULT_SELECTED_DAYS = ("Monday", "Tuesday", "Wednesday", "Thursday", "Friday")
DAY_CODES = {
    "Monday": "M",
    "Tuesday": "T",
    "Wednesday": "W",
    "Thursday": "Th",
    "Friday": "F",
    "Saturday": "Sat",
    "Sunday": "Sun",
}
DAY_ORDER = {day: index for index, day in enumerate(DAY_CODES)}
BULLET = "\u2022"


class FixtureSummaryError(ValueError):
    """Raised when the fixture cannot identify one accessible class row."""


def _compact_time(value: str) -> str:
    text = value.strip()
    if not text:
        return ""
    uses_meridiem = "AM" in text.upper() or "PM" in text.upper()
    for pattern in ("%I:%M %p", "%I:%M%p", "%H:%M"):
        try:
            parsed = dt.datetime.strptime(text.upper(), pattern)
            if uses_meridiem:
                return f"{parsed.hour % 12 or 12}:{parsed.minute:02d}"
            return f"{parsed.hour}:{parsed.minute:02d}"
        except ValueError:
            pass
    # Match the product's documented fallback after removing AM/PM markers.
    return text.replace(" AM", "").replace(" PM", "").replace(" am", "").replace(" pm", "").strip()


def _day_code(day: str) -> str:
    return DAY_CODES.get(day, day.strip())


def _compressed_days(day_names: list[str]) -> str:
    ordered = sorted(set(day_names), key=lambda day: DAY_ORDER.get(day, 1_000))
    codes = [_day_code(day) for day in ordered if day.strip()]
    if codes in (
        ["M", "W"],
        ["M", "F"],
        ["W", "F"],
        ["M", "W", "F"],
        ["T", "Th"],
    ):
        return "/".join(codes)
    return "/".join(codes)


def derive_accessible_name(
    fixture_path: Path,
    class_id: int,
    grade: str = "M2",
    selected_days: tuple[str, ...] = DEFAULT_SELECTED_DAYS,
) -> dict[str, Any]:
    if class_id < 1 or not grade.strip() or not selected_days:
        raise FixtureSummaryError("class id, grade, and selected weekdays must be valid")
    missing_days = [day for day in selected_days if day not in DAY_CODES]
    if missing_days:
        raise FixtureSummaryError(f"unknown selected weekdays: {missing_days}")
    if not fixture_path.is_file():
        raise FixtureSummaryError(f"fixture file does not exist: {fixture_path}")

    connection = sqlite3.connect(fixture_path.resolve().as_uri() + "?mode=ro", uri=True)
    try:
        rows = connection.execute(
            """SELECT c.id, ci.class_grade, ci.class_level, t.teacher_kr, t.teacher_en
               FROM classes c
               INNER JOIN class_info ci ON ci.class_id = c.id
               INNER JOIN teachers t ON t.id = ci.teacher_id
               LEFT JOIN testing_classes tc ON tc.class_id = c.id
               WHERE ci.class_grade = ? AND ci.teacher_id > 0 AND tc.class_id IS NULL
               ORDER BY c.id""",
            (grade,),
        ).fetchall()
        if not rows:
            raise FixtureSummaryError(f"fixture has no eligible classes for grade {grade}")

        placeholders = ",".join("?" for _ in selected_days)
        ids = [int(row[0]) for row in rows]
        id_placeholders = ",".join("?" for _ in ids)
        meetings = connection.execute(
            f"""SELECT class_id, day, start_time FROM class_times
                WHERE class_id IN ({id_placeholders}) AND day IN ({placeholders})
                ORDER BY class_id, id""",
            (*ids, *selected_days),
        ).fetchall()
    except sqlite3.Error as error:
        raise FixtureSummaryError(f"fixture class summary query failed: {error}") from error
    finally:
        connection.close()

    meetings_by_class: dict[int, list[tuple[str, str]]] = collections.defaultdict(list)
    for meeting_class_id, day, start_time in meetings:
        meetings_by_class[int(meeting_class_id)].append((str(day), str(start_time or "")))

    summaries: list[dict[str, Any]] = []
    for raw_id, raw_grade, raw_level, raw_teacher_kr, raw_teacher_en in rows:
        current_id = int(raw_id)
        current_meetings = meetings_by_class.get(current_id, [])
        if not current_meetings:
            continue
        groups: list[list[Any]] = []
        for day, raw_time in current_meetings:
            time = _compact_time(raw_time)
            if not time:
                continue
            for group in groups:
                if group[0] == time:
                    group[1].append(day)
                    break
            else:
                groups.append([time, [day]])
        if not groups:
            continue
        schedule = "; ".join(
            f"{_compressed_days(group_days)} {time}" for time, group_days in groups
        )
        level = str(raw_level or "").strip()
        class_grade = str(raw_grade or "").strip()
        base = f"{level or class_grade} {BULLET} {schedule}"
        teacher = str(raw_teacher_en or "").strip() or str(raw_teacher_kr or "").strip()
        summaries.append({"classId": current_id, "baseLabel": base, "teacherLabel": teacher})

    targets = [row for row in summaries if row["classId"] == class_id]
    if len(targets) != 1:
        raise FixtureSummaryError(
            f"class {class_id} is not exactly once in the visible {grade} summary rows"
        )

    base_counts = collections.Counter(row["baseLabel"] for row in summaries)
    labels: dict[int, str] = {}
    for row in summaries:
        label = row["baseLabel"]
        if base_counts[label] > 1:
            teacher = row["teacherLabel"]
            label = f"{label} {BULLET} {teacher}" if teacher else label
            same_expanded = sum(
                1
                for other in summaries
                if other["baseLabel"] == row["baseLabel"]
                and (
                    f"{other['baseLabel']} {BULLET} {other['teacherLabel']}"
                    if other["teacherLabel"]
                    else other["baseLabel"]
                )
                == label
            )
            if same_expanded > 1:
                label = f"{label} #{row['classId']}"
        labels[row["classId"]] = label

    target = targets[0]
    return {
        "classId": str(class_id),
        "grade": grade,
        "accessibleName": labels[class_id],
        "selectedDays": list(selected_days),
        "visibleClassCount": len(summaries),
        "baseLabelDuplicateCount": base_counts[target["baseLabel"]],
        "fixturePath": str(fixture_path.resolve()),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixture", required=True, type=Path)
    parser.add_argument("--class-id", required=True, type=int)
    parser.add_argument("--grade", default="M2")
    parser.add_argument("--selected-days", nargs="+", choices=tuple(DAY_CODES),
                        default=list(DEFAULT_SELECTED_DAYS))
    arguments = parser.parse_args()
    try:
        result = derive_accessible_name(
            arguments.fixture, arguments.class_id, arguments.grade,
            tuple(arguments.selected_days),
        )
    except FixtureSummaryError as error:
        print(str(error), file=sys.stderr)
        return 2
    print(json.dumps(result, ensure_ascii=True, sort_keys=True, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
