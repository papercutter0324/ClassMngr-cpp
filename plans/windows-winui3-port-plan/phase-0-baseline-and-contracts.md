# Phase 0 — Baseline and Port Contracts

> Progress is tracked in [00-START-HERE.md](00-START-HERE.md).

## Goal

Freeze the externally observable Windows Qt behavior and portable data
contracts before replacing the Windows presentation.

The baseline describes content, state, workflows, input behavior, output, and
performance. It does not require WinUI 3 to reproduce Qt pixels or typography.

## Work

1. Inventory pages, dialogs, commands, imports, exports, printing, updates,
   resource packs, platform integrations, and failure paths.
2. Maintain a parity matrix covering data read/write, input, visual evidence,
   errors, printing/output, and performance for every feature surface.
3. Maintain deterministic `.tps` and legacy `.db` fixtures for empty, typical,
   large, migration, corrupt, and rollback cases, including Korean text.
4. Capture representative Qt states at 100%, 150%, and 200% display scale,
   light/dark themes, English/Korean locales, and empty/populated/error states.
5. Record keyboard, Korean IME, focus restoration, dirty-state, and
   unsaved-change evidence where screenshots are insufficient.
6. Record startup, memory, first-paint, resize, scrolling, and output budgets
   on the release architecture.
7. Validate the retained Windows, macOS, and Linux Qt products without
   weakening platform-specific behavior.

## Evidence Locations

- [Phase 0 evidence index](../../docs/porting/windows-direct2d/README.md)
- [Feature inventory](../../docs/porting/windows-direct2d/phase-0/feature-inventory.md)
- [Parity matrix](../../docs/porting/windows-direct2d/phase-0/parity-matrix.csv)
- [Database fixture contract](../../docs/porting/windows-direct2d/phase-0/database-fixture-contract.md)
- [Reference capture protocol](../../docs/porting/windows-direct2d/phase-0/reference-capture.md)
- [Historical handoff](../../docs/porting/windows-direct2d/current-status.md)

The `windows-direct2d` directory name is historical. Its evidence remains
valid for the WinUI 3 port because it captures the source product rather than
prescribing the replacement UI framework.

## Validation

- Contract validator accepts every parity row, capture row, and fixture hash.
- Visual runs prove show/settle/capture/close cleanup.
- Fixture verification passes on retained platforms.
- Owner review accepts the representative baseline.

## Exit Gate

The behavior inventory, parity matrix, fixture corpus, capture evidence, and
retained-platform validation are accepted and reproducible. Any deferred native
validation is explicitly assigned to the phase that implements that surface.
