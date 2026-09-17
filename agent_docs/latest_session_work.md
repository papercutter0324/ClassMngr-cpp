# Latest Session Work

Qt Rewrite Phase 0 remains in progress on branch `Qt-Rewrite`. The latest
completed slice is the macOS universal packaged Release route matrix: 24/24
routes passed and independent validation exited 0. Windows x64 also has a
24/24 run, but the combined Phase 0 gate still needs its evidence consolidated
with the macOS result and the remaining visual/output review. Current Heavy
deployment: `qt0_macos_route_matrix_20260917`. Phase 1 must not begin until the
combined gate and review are complete.

## Current Deployment Handoff

- The macOS universal runner and its focused tests were committed as
  `84b45902` (`Phase 0: add macOS universal evidence runner`); all 10 focused
  runner tests passed. The final packaged Release root is
  `/private/tmp/ClassMngr-Phase0-macos-universal-20260917T033003Z-QT0-EXEC-02-final`;
  all 24 route IDs completed and independent validation with
  `--expected-platform macos-universal` exited 0. The full root remains local
  and unmodified.
- The earlier 22/24 root is
  `/private/tmp/ClassMngr-Phase0-macos-universal-20260917021448372-nihtwpxo7`.
  Its only failed routes were `lifecycle-calendar-import` and
  `lifecycle-calendar-import-error`, both failing at
  `QTcpServer::listen(QHostAddress::LocalHost)` in the test harness
  (`tests/startup_performance_tests.cpp:8831`) before the packaged app launched.
  The error was `Unknown error`; focused and final reruns passed with unchanged
  source and binary hashes. This supports a host/loopback-bind limitation, but
  does not prove a specific sandbox denial or a product defect.
- The Windows x64 24/24 run remains recorded in its compact audit bundle, but
  the combined exit gate is still pending Windows evidence consolidation and
  human visual/output review. Windows ARM64 and Linux remain deferred ports.
- The prior merge reconciliation is recorded in local commit `7a9cb3c`
  (parents `875159da` and `e7d05f4c`); its 17 intended paths were checked for
  unresolved paths and conflict markers. The macOS 14.4 runtime was not
  directly exercised on that merge host.
- Product support scope: packaged Windows x64 and macOS universal remain
  supported Phase 0 targets. Per the user's decision, Windows ARM64 and Linux
  are unofficial ports deferred to later and are not Phase 0 blockers.
- Added `scripts/phase0/run_phase0_evidence.ps1`,
  `scripts/phase0/validate_phase0_evidence.py`, and their usage guide. The
  runner defines 24 opt-in packaged routes for visual captures, generated
  outputs, memory/lifecycle traces, and fixtures; planning is no-write and
  requires no build or run. Execution requires an explicit evidence parent and
  creates a new timestamped child directory.
- Independent validation passed the all-route plan and checked that all 24
  route IDs, paths, test slots, and environment-variable mappings match the
  existing harness. Repository/build paths and non-empty evidence-root
  collisions were rejected without writes.
- Validator corrections are in `6f08877b` and generated-output references are
  in `227664b4`. The validator now rejects incomplete Windows process records
  and checks the full startup-only completion sequence; its 13 self-tests pass.
- A fresh full run used `-Routes all`, with no build/run/validation skips. All
  24 routes (1 fixture, 8 visual, 8 memory, 1 workflow, 6 output) plus the six
  build/package/orchestration/validation commands exited 0 without timing out.
  The independent audit confirmed the run validator passed with zero failures
  and 151/151 required files present. The only warning is legacy memory-trend
  data.
- The original evidence root is
  `C:\Users\wfelt\AppData\Local\Temp\ClassMngr-QT0-Windows-x64-final-20260916T201256Z\qt0-windows-x64-final-20260916T201256Z-msvc`.
  It contains 331 files / 147,732,123 bytes and remains local. The checked-in
  compact bundle retains byte-identical run and validation manifests plus a
  per-file SHA-256 inventory; it is not a substitute for the complete evidence
  root. The resource trace records 189 entries and normal completion without
  timeout.
- The validator's combined `exitGate` remains incomplete pending Windows x64
  evidence consolidation and the remaining human visual/output review. The
  macOS universal matrix is now complete at 24/24. Phase 1 and v2 memory
  remediation remain blocked until the Phase 0 exit gate and review are closed.
- The opt-in output-reference commit retains 11 PDFs, 12 PNGs, and five
  manifests. In noninteractive/offscreen Windows testing, two Speaking
  Evaluation clipboard UI cases fail with `0x800401d0`; capture-specific PDF
  checks pass. PowerPoint COM export fails before PDF creation with
  `0x80070520`, so native Office output is not accepted evidence.

## Prior Session Detail (superseded by the current handoff above)

- Use the Heavy route for every slice, commit each completed slice, and do not
  begin Phase 1 until the Phase 0 exit gate is actually satisfied.
- The `<250 MiB` RAM value is the target for the completed rewrite, not a
  Phase 0 acceptance gate for the legacy widget graph. Phase 0 retains current
  packaged Windows x64 Release measurements as baseline/trend evidence. Later
  phases must avoid regressions and should lower the affected heavy-route
  working set. The temporary `<512 MiB` ceiling is diagnostic only.
- Phase 0 is not complete and Phase 1 must not begin yet. Remaining gaps
  include remaining feature loading/error states, generated-output references,
  Windows ARM64 and Linux Release evidence, and the eventual v2 memory/ownership
  remediation. Populated Classes visual states are retained for both
  language/theme axes.
- The CMake configuration cleanup deployment is complete and independent of the
  accepted Calendar Import slice. Multi-config generators are limited to
  `Debug;Release`; Debug/Release test intent is explicit; non-QML test targets
  skip unnecessary import scanning; active Qt references are all 6.12.0; and
  only the empty root-generated `CMakeFiles/` residue was removed. The tracked
  `cmake/` modules, `build/`, and `dist/` were preserved.

## Previous Completed Slice: macOS Universal Release Baseline (2026-09-17)

- The macOS deployment minimum is now 14.4 in the configure preset and build
  documentation, matching the selected universal Qt 6.12.0 kit.
- Release packaging no longer moves SQL drivers out of the external Qt kit;
  it removes unwanted SQL plugins only from the app bundle and re-signs after
  deployment. The release validator checks both architectures and minimum OS
  versions for every embedded Mach-O.
- A clean universal Release build produced
  `dist/ClassMngr-0.19.0-macos-universal.dmg`. The staged app reports 14.4 in
  both `Info.plist` and the main executable; all 115 bundled Mach-O files are
  arm64/x86_64 and target no later than 14.4. Signature and disk-image
  checksums passed; `libqsqlite.dylib` is the only bundled SQL driver.
- The packaged app ran on macOS 26.6.2 through empty startup and a 96-class,
  12-route workflow including two PDF render/release cycles. Fresh universal
  Debug startup-performance and dialog-services CTest targets passed with a
  14.4 deployment target.
- Evidence, profiles, trace, screenshots, DMG hash, and host/toolchain details
  are in `docs/qt-rewrite/visual-baseline/macos-universal/release/`. The empty
  startup profile is retained without its screenshot because that frame
  exposes seeded campus Wi-Fi credentials. The minimum macOS 14.4 runtime was
  not directly exercised on this macOS 26.6.2 host.
- The large `.tps` workflow can write workspace state on exit. Its final run
  used a byte-identical temporary copy; the tracked source fixture was restored
  to its baseline and remains unchanged.
- At the completion of this earlier slice, both 24-route matrices were open.
  The Windows x64 matrix is now complete; macOS universal route coverage and
  remaining human visual/output review remain open. Windows ARM64 and Linux
  remain deferred unofficial ports.

## Earlier Session Changes

- Accepted Calendar Import parser-failure reference: the implementation is
  based on `ded8b5dc`, and the evidence-only follow-up scrolls Calendar
  Preferences to the Import section before capturing the error and restores
  the prior position. The route is selected by
  `CLASSMNGR_STARTUP_CALENDAR_IMPORT_EXPECTED_FAILURE=1` and writes to
  `CLASSMNGR_LARGE_CALENDAR_IMPORT_ERROR_BOUNDARY_REFERENCE_DIR`.
- Earlier automation attempt — deployment `qt0_calendar_verify_20260916` —
  was superseded by the fresh Ninja Release verification described below.
- Fresh Ninja Release app/test targets compile and link. The expected-failure
  route passed with a 69-byte malformed local HTTP response, the real missing
  `xl/workbook.xml` error, re-enabled Import Events, unchanged events,
  forbidden success checkpoints absent, complete evidence, and normal exit.
  Its peak was `410,251,264` working-set bytes and `452,120,576` private-usage
  bytes; this is legacy before-state evidence only. The unchanged success
  route and opt-in-cleared full startup suite also passed. The error screenshot
  was manually inspected and visibly shows the Calendar Import section, the
  enabled Import Events control, and the real parser error. Retained failure
  evidence is under
  `docs/qt-rewrite/visual-baseline/release/large-calendar-import-error-boundary/`.

- `b5c1a8a1` — `Phase 0: retain PowerPoint renderer reference`
  - Added a real Speaking Evaluation export-dialog PowerPoint renderer
    selection capture and checkpoint to the packaged Heavy route.
- `e3fcde2d` — `Phase 0: retain Sub Prep validation reference`
  - Retained the real Sub Prep generation-dialog validation-error state with
    its disabled OK action.
- `17b3336b` — `Phase 0: restore Sub Prep PDF evidence`
  - Restored/tracked the two generated PDFs that were temporarily staged as
    deletions because the app-created output folder had a sandbox-only ACL.
  - The exact generated evidence folder was granted read access to the normal
    Git identity; no source behavior changed.
- `9ba4e462` — `Phase 0: retain Sub Prep editing state`
  - Retained the packaged large-route Sub Prep mixed editing/read-only state
    and asserted four editable text editors plus six read-only line edits.
- `630f61b` — `Phase 0: retain Classes visual states`
  - Added opt-in in-process packaged Classes visual captures for populated
    entry, selected class 96, and selected class 1 after re-entry. The route
    retains all four English/Korean light/dark variants and reports the capture
    checkpoints in its metrics.
- `71e44c8` — `Phase 0: retain Schedule Import loading state`
  - Retained the real packaged large-workbook Schedule Import loading state for
    both cancel and apply boundary routes. The evidence records the
    `Loading workbook...` status, indeterminate progress, disabled source/load
    controls, and a non-empty screenshot. The focused routes and the full
    startup-performance suite passed; measured peaks were `418,488,320` bytes
    working set for cancel and `361,316,352` bytes for apply.
- `f0fb4b3` — `Phase 0: retain Calendar Import loading state`
  - Retained the real packaged Calendar Import Preferences loading state. The
  Calendar tab is scrolled to the Import section only for the opt-in capture,
  then restored; the evidence records `Importing events...`, the disabled
  import button, and a non-empty screenshot. The focused route and full
  startup-performance suite passed with a `416,694,272`-byte working-set and
  `457,310,208`-byte private-usage peak.
- Previously completed slice — `Phase 0: retain Schedule Import conflict
  warning` (commit created after the cutoff): the packaged large Schedule
  Import cancel route now retains the real conflict-warning modal, including
  visible warning text, disabled Import, and a validated screenshot. The
  cancel fixture intentionally overlaps projected meetings; the separate
  apply fixture remains conflict-free and commits normally. The current
  source/test/evidence/docs changes are included in the final post-cutoff
  commit for this session.

## Prior Session Verification

- CMake cleanup verification passed: JSON parsing, configure/build/workflow
  preset listing, `git diff --check`, and a fresh isolated Windows configure
  all succeeded. The configure cache contained only `Debug;Release`; it
  generated 195 tests and zero `qmlimportscan` files. Linux and macOS
  configurations were not executed on this Windows host, and application tests
  were intentionally not run for this configuration task.

- The Classes visual-state slice rebuilt the Release application and Debug
  startup test target successfully inside the Visual Studio developer
  environment.
- Its focused Heavy route passed across all four variants with peaks from
  `409,907,200` to `412,454,912` working-set bytes; a retained Classes frame
  was visually inspected.
- The full `ClassMngrStartupPerformanceTests.exe` suite passed after the
  Classes visual changes with the opt-in capture variables cleared.
- The full `ClassMngrStartupPerformanceTests.exe` suite passed with the opt-in
  capture variables cleared.
- The Schedule Import loading slice rebuilt the packaged Release application
  and Debug startup test target successfully inside the Visual Studio
  developer environment. Both focused boundary routes and the full
  `ClassMngrStartupPerformanceTests.exe` suite passed.
- The Calendar Import loading slice rebuilt the packaged Release application
  and Debug startup test target successfully inside the Visual Studio
  developer environment. Its focused packaged Heavy route, visual inspection,
  and the full `ClassMngrStartupPerformanceTests.exe` suite passed with all
  opt-in capture variables cleared for the full run.
- The Schedule Import conflict-warning slice rebuilt the packaged Release
  application and Debug startup test target successfully. Both focused
  boundary routes and the full `ClassMngrStartupPerformanceTests.exe` suite
  passed. The cancel warning capture was visually inspected and the profiler
  recorded peak working-set/private-usage values of
  `418,058,240`/`461,225,984` bytes; the conflict-free apply route recorded
  `360,960,000`/`402,731,008` bytes.
- `git diff --check` passed before the slice commit.

- Current accepted point: the Calendar Import expected-failure harness guards
  the normal-path screenshot assertions, captures the error after scrolling the
  Import section into view, and restores the prior position. Fresh Release
  evidence is complete and the retained error screenshot was manually
  inspected. The focused success route and opt-in-cleared full suite passed.

## Prior Pending Work and Blockers

- Phase 0 remains active for remaining feature loading/error states,
  generated-output coverage, Windows ARM64 and Linux Release evidence, and the
  eventual v2 ownership/memory work. The Calendar Import failure boundary is
  no longer a blocker.
- `.codex_workflow_staging_1.1.17/` is ignored workflow state. The durable
  `agent_docs/` context is tracked; preserve it for reboot/resume context.
- Do not run native Office automation unless a suitable platform/environment
  is available; the current in-process/offscreen boundary is intentional.

## Prior Next Entry Point

On the next session, start by reading `QT-REWRITE-HANDOFF.md`, this file, and checking:

```powershell
git status --short
git log -6 --oneline
```

Then inspect the Phase 0 baseline/plan tail and continue with the next Phase 0
evidence gap. Do not start Phase 1 or make v2 memory claims.
