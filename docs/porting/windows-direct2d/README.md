# Windows Native Port — Phase 0 Historical Evidence

This directory freezes the Qt Windows application's externally observable
contracts before the native Windows target is introduced. It is retained as
the accepted Phase 0 evidence for the
[WinUI 3 port plan](../../../plans/windows-winui3-port-plan/00-START-HERE.md).
The directory name records the original Direct2D/DirectComposition direction;
it does not prescribe the replacement presentation framework.

The source inventory was taken from revision `48fc5c5`, the branch state before
the Phase 0 additions. Update
the affected contract and its evidence location whenever a user-visible Qt
behavior changes while the port is in progress.

The current Windows release gate is x64 only. ARM64 build compatibility is
unofficial and informational; official ARM64 runtime validation, performance,
and release support are deferred. UI Automation, Narrator, and high-contrast
support are also deferred from the current roadmap.

## Phase 0 deliverables

- [Feature and workflow inventory](phase-0/feature-inventory.md) — pages,
  commands, dialogs, output, platform behavior, source evidence, and capture
  status.
- [Windows parity matrix](phase-0/parity-matrix.csv) — the x64 completion gate
  for each feature surface; ARM64 columns are informational only.
- [Parity matrix guide](phase-0/parity-matrix-guide.md) — evidence vocabulary,
  baseline versus native status, and the completion rule.
- [Database fixture contract](phase-0/database-fixture-contract.md) — required
  portable `.tps`/legacy `.db` fixtures, semantic digests, and rollback cases.
- [Visual and input capture protocol](phase-0/reference-capture.md).
- [Source-backed capture ledger](phase-0/capture-ledger.csv) — each page,
  dialog, command, and high-risk editor has a stable artifact prefix and state.
- [Performance budget and measurement protocol](phase-0/performance-baseline.md).
- [Initial Windows x64 baseline result](phase-0/baseline-results.md).
- [Current cross-device status and handoff](current-status.md) — validated
  revision, evidence provenance, remaining Phase 0 work, and reproduction
  commands.
- [Original port foundation ADR](../adr/0001-windows-native-port-foundations.md)
  — retained engine, data-contract, and isolation foundations plus the
  superseded Direct2D presentation decision.
- [Current WinUI 3 presentation ADR](../adr/0002-winui3-windows-presentation.md)
  — the Windows UI, support-floor, and deployment choices that govern the
  current plan.
- [Current WinUI 3 Phase 1 plan](../../../plans/windows-winui3-port-plan/phase-1-winui-bootstrap.md)
  — target boundaries, Windows App SDK bootstrap, validation, and release
  isolation for the active implementation phase.

## Current status

| Area | State | Gate to advance |
| --- | --- | --- |
| Source and test inventory | initial pass complete | keep it synchronized with Qt behavior changes |
| Parity matrix | seeded | every applicable x64 cell must have evidence before cutover; ARM64 is informational |
| Database fixtures | generated, SHA-pinned corpus; Linux Qt verifier passed all 11 fixtures | record Linux Qt/native-engine semantic result digests when the cross-platform harness and native engine are available |
| Screenshots, keyboard/IME, and output samples | ledger, metadata-sidecar tooling, opt-in native Windows capture target, 16 validated baseline Qt captures per DPI run, and a 28-scenario expanded run covering populated, empty, large, dirty, validation, and error editor states at 150% | capture the remaining ledger states and manually review keyboard, IME, and output evidence; UIA/Narrator/high contrast are deferred |
| Performance | historical x64 Release budget plus three current x64 Debug GUI samples | approve a current x64 Release baseline and capture page/scroll/output samples; ARM64 is deferred |
| Build preservation | macOS universal Qt Debug/Release validation passed; Linux Qt Debug and Release configured, rebuilt, and tested 67/67 | no Linux-specific gate remains; retain green regression validation as the port advances |

The capture target is opt-in through
`CLASSMNGR_ENABLE_WINDOWS_QT_VISUAL_CAPTURE_TESTS`; it requires an interactive
Windows display and never treats the offscreen Qt platform as authoritative.
The Phase 0 additions do not change product behavior or database schema.

## Linux validation — complete

The Linux-specific Phase 0 retained-platform gate is complete. The retained
Linux Qt product was configured and rebuilt from the
`linux-gcc-debug` preset with Qt 6.11.1. The database-port verifier passed all
eleven checked-in fixtures, and the full Qt test suite passed 67/67 tests when
run with `QT_QPA_PLATFORM=offscreen` and normal loopback access. The Linux
`/proc` memory reader was corrected to read procfs content until
`QFile::readLine()` reaches its actual EOF; the memory snapshot and dependent
startup-memory tests now pass. The Linux Release preset also builds and tests
67/67; its installed XCB bundle launches successfully, and its archive
checksum verifies. Details and the recorded result are in the
[cross-device status record](current-status.md).

The validation commands are:

```bash
cmake --preset linux-gcc-debug
cmake --build build/linux-gcc-debug -j2
build/linux-gcc-debug/ClassMngrDatabasePortFixtureGenerator \
  --verify-directory tests/fixtures/database-port
env QT_QPA_PLATFORM=offscreen \
  ctest --test-dir build/linux-gcc-debug --output-on-failure
```

## macOS validation

The retained macOS Qt product was configured and rebuilt from the universal
`macos-clang-debug` preset with Qt 6.11.1. The database-port verifier passed
all eleven checked-in fixtures, and the complete retained Qt suite passed
68/68 on an interactive macOS 26.6.2 arm64 host. The universal Release app
also built successfully with `arm64;x86_64` slices. Details and the recorded
environment are in the [cross-device status record](current-status.md).

The validation commands are:

```bash
export QT_MACOS_PREFIX=/Users/papercutter0324/Qt/6.11.1/macos
cmake --preset macos-clang-debug
cmake --build build/macos-clang-debug --parallel 2
build/macos-clang-debug/ClassMngrDatabasePortFixtureGenerator \
  --verify-directory tests/fixtures/database-port
env QT_QPA_PLATFORM=offscreen \
  ctest --test-dir build/macos-clang-debug --output-on-failure
```

The macOS Initial Setup wizard test is assigned the Cocoa platform by CMake;
the updater cases require local loopback binding.

Validate the checked-in Phase 0 ledgers and fixture hashes with:

```powershell
.\scripts\porting\windows\validate_phase0_contracts.ps1
```
