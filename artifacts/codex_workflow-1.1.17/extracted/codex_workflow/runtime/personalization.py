"""Validation and materialization of project personalization."""

from __future__ import annotations

from .errors import ValidationError


def materialize_personalization(text: str) -> str:
    expected = [
        "Frontend Project Profile",
        "Design Principles",
        "Additional Workflow Decisions",
    ]
    sections: dict[str, tuple[str, str]] = {}
    current: str | None = None
    status: str | None = None
    decision: list[str] = []
    for raw_line in text.splitlines():
        if raw_line.startswith("## "):
            if current is not None:
                sections[current] = (status or "", "\n".join(decision).strip())
            current = raw_line[3:].strip()
            status = None
            decision = []
        elif current is not None and raw_line.startswith("Status:"):
            status = raw_line.split(":", 1)[1].strip()
        elif current is not None and raw_line.startswith("Decision:"):
            decision = [raw_line.split(":", 1)[1].strip()]
        elif current is not None and decision and raw_line.strip():
            decision.append(raw_line.strip())
    if current is not None:
        sections[current] = (status or "", "\n".join(decision).strip())
    if set(sections) != set(expected):
        raise ValidationError("personalization resource must contain exactly three sections")
    instructions = []
    for name in expected:
        status_value, decision_value = sections[name]
        if status_value not in {"default", "customized", "skipped"}:
            raise ValidationError(f"invalid personalization status for {name}")
        if not decision_value:
            raise ValidationError(f"missing personalization decision for {name}")
        if status_value == "customized":
            instructions.append(decision_value)
    return "\n\n".join(instructions)
