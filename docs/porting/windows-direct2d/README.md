# Windows Direct2D/DirectComposition port — Phase 0

This directory freezes the Qt Windows application's externally observable
contracts before the native Windows target is introduced. It is the working
evidence for Phase 0 of
[`plans/windows-direct2d-directcomposition-port-plan.md`](../../../plans/windows-direct2d-directcomposition-port-plan.md),
not a claim that the Phase 0 exit gate has passed.

The source inventory was taken from revision `48fc5c5`, the branch state before
the Phase 0 additions. Update
the affected contract and its evidence location whenever a user-visible Qt
behavior changes while the port is in progress.

## Phase 0 deliverables

- [Feature and workflow inventory](phase-0/feature-inventory.md) — pages,
  commands, dialogs, output, platform behavior, source evidence, and capture
  status.
- [Windows parity matrix](phase-0/parity-matrix.csv) — the x64/ARM64 completion
  gate for each feature surface.
- [Parity matrix guide](phase-0/parity-matrix-guide.md) — evidence vocabulary,
  baseline versus native status, and the completion rule.
- [Database fixture contract](phase-0/database-fixture-contract.md) — required
  portable `.tps`/legacy `.db` fixtures, semantic digests, and rollback cases.
- [Visual, input, and accessibility capture protocol](phase-0/reference-capture.md).
- [Source-backed capture ledger](phase-0/capture-ledger.csv) — each page,
  dialog, command, and high-risk editor has a stable artifact prefix and state.
- [Performance budget and measurement protocol](phase-0/performance-baseline.md).
- [Initial Windows x64 baseline result](phase-0/baseline-results.md).
- [Current cross-device status and handoff](current-status.md) — validated
  revision, evidence provenance, remaining Phase 0 work, and reproduction
  commands.
- [Port foundation ADR](../adr/0001-windows-native-port-foundations.md) — the
  frozen architectural choices that constrain all later phases.

## Current status

| Area | State | Gate to advance |
| --- | --- | --- |
| Source and test inventory | initial pass complete | keep it synchronized with Qt behavior changes |
| Parity matrix | seeded | every cell must have evidence before cutover |
| Database fixtures | generated, SHA-pinned corpus; Linux Qt verifier passed all 9 fixtures | record Linux Qt/native-engine semantic result digests when the cross-platform harness and native engine are available |
| Screenshots, IME, UIA, and output samples | ledger, metadata-sidecar tooling, opt-in native Windows capture target, and 16 validated current-HEAD Qt captures | capture the remaining ledger states and manually review input/accessibility/output evidence |
| Performance | historical x64 Release budget plus three current x64 Debug GUI samples; ARM64 runtime pending | approve a current Release baseline, establish ARM64 equivalents, and capture page/scroll/output samples |
| Build preservation | Linux Qt Debug configured and rebuilt; 65/67 tests pass, with two diagnostics failures recorded in [current-status](current-status.md) | resolve the Linux diagnostics failures, then complete macOS and Windows x64/ARM64 validation unchanged |

The capture target is opt-in through
`CLASSMNGR_ENABLE_WINDOWS_QT_VISUAL_CAPTURE_TESTS`; it requires an interactive
Windows display and never treats the offscreen Qt platform as authoritative.
The Phase 0 additions do not change product behavior or database schema.

## Linux validation

The retained Linux Qt product was configured and rebuilt from the
`linux-gcc-debug` preset with Qt 6.11.1. The database-port verifier passed all
nine checked-in fixtures. The full Qt test suite passed 65 of 67 tests when run
with `QT_QPA_PLATFORM=offscreen` and normal loopback access; the two remaining
failures are the Linux `/proc` memory snapshot assertion and the dependent
startup-memory assertion. Details and the exact current-HEAD result are in
the [cross-device status record](current-status.md).

The validation commands are:

```bash
cmake --preset linux-gcc-debug
cmake --build build/linux-gcc-debug -j2
build/linux-gcc-debug/ClassMngrDatabasePortFixtureGenerator \
  --verify-directory tests/fixtures/database-port
env QT_QPA_PLATFORM=offscreen \
  ctest --test-dir build/linux-gcc-debug --output-on-failure
```

Validate the checked-in Phase 0 ledgers and fixture hashes with:

```powershell
.\scripts\porting\windows\validate_phase0_contracts.ps1
```
