# Phase 2 device handoff

Prepared 2026-09-04 on branch `NativeWindowsPort`.

This handoff records the current state of the portable-engine follow-up work.
The source and test changes are committed, but the cross-platform Phase 2
exit gate is intentionally not claimed as complete. Final validation can
continue on another device without redoing the implementation work.

## What this commit contains

- Speaking-evaluation dashboard orchestration and headless analytics coverage.
- Engine-backed interactive roster and speaking-evaluation validation parity,
  including normalized names and duplicate-pair diagnostics.
- Teacher-import candidate duplicate and stored-record matching policy tests.
- Session-only production feature-service composition and removal of runtime
  `DataService` fallback operations. `DataService::save()` and
  `ApplicationServices::saveDatabase()` are compatibility no-ops.
- Path-backed retained repositories. `DatabaseSession::compatibilityDatabase()`
  is now the explicitly named Qt SQL compatibility boundary.
- Injectable clock usage at filesystem and ZIP archive engine boundaries,
  including deterministic tests and controlled platform-service failures.
- Phase 2 exit-gate evidence classification, complete inventory checks,
  workflow lane validation, and the updated runbook.
- Quarantining of Qt-era shared validation helpers to the legacy shared-policy
  test target rather than the production domain object library.

The main references are:

- [Phase 2 plan](../../../plans/windows-winui3-port-plan/phase-2-portable-engine-extraction.md)
- [Phase 2 exit-gate runbook](phase2-exit-gate-runbook.md)
- [Exit-gate runner](../../../scripts/phase2_exit_gate.py)

## Validation performed before handoff

Completed:

- `git diff --check` passed.
- Qt-free engine configuration succeeded in
  `build/phase2-speaking-engine-x64`.
- The changed `ClassMngrEngine` library compiled successfully.
- Engine tests for speaking analytics, roster validation, teacher import,
  database fixture round-trip, ZIP output, and filesystem behavior passed in
  the Qt-free engine build tree.
- Calendar import, class transfer, and platform-service engine tests passed in
  the Qt-enabled build tree.
- The modified Qt integration targets for data-service lifecycle, teacher
  import, roster model, speaking-evaluation model, and speaking-evaluation
  service compiled successfully.
- The `ClassMngrQtLegacySharedValidation` library compiled successfully.

Not yet completed:

- The newly compiled Qt integration executables were not all run before the
  handoff. In particular, relink/run `ClassMngrSharedPolicyTests` after the
  legacy validation library is available, and run the calendar cache and
  calendar repository targets if they are part of the selected fixture.
- The nine engine tests were split across two local build trees because the
  first tree already contained three binaries. The three available in the Qt
  tree passed; the remaining six passed in the engine-only tree. Re-run the
  complete set from one fresh tree for a single-tree record.
- The full seven-lane cross-platform exit gate was not run. Existing evidence
  still has the documented limitations: missing registered binaries, a host
  Qt 6.11.1 versus required Qt 6.12.0 mismatch, and a red Linux baseline.

Do not convert a compile-only, missing-binary, host-blocked, or failed result
into a passing runtime result. The exit-gate runner preserves those
distinctions.

## Moving the work to another device

If the branch is available from the remote, push this local commit from the
current device before switching devices:

```powershell
git status --short
git log -1 --oneline --decorate
git push origin NativeWindowsPort
```

On the next device:

```powershell
git fetch origin
git switch NativeWindowsPort
git pull --ff-only origin NativeWindowsPort
git log -1 --oneline --decorate
```

If pushing is not possible, transfer the one committed change as a patch:

```powershell
git format-patch -1 --stdout HEAD > phase2-device-handoff.patch
```

Apply it on the next device from the matching branch/base:

```powershell
git switch NativeWindowsPort
git am phase2-device-handoff.patch
git log -1 --oneline --decorate
```

Do not copy generated `build/`, `dist/`, or `artifacts/` directories as
source state. They are machine- and toolchain-specific. Start fresh build
directories on the next device.

## Required toolchain setup

Install or verify:

- Visual Studio 18 2026 Build Tools with the v145 C++ workload and a Windows
  SDK compatible with the WinUI presets.
- CMake 3.25 or newer.
- Python 3.
- Qt 6.12.0 for every retained-Qt lane. The exact prefix is required for an
  exit-gate runtime claim; another Qt version is useful for development but is
  `host-blocked` for the retained-Qt gate.

Set the CMake preset variables appropriate for the machine. Windows desktop
uses `QT_MSVC_X64_PREFIX` (and `QT_MSVC_ARM64_PREFIX` for ARM64); the
retained Linux and macOS lanes use `QT_LINUX_PREFIX` and
`QT_MACOS_PREFIX`. The preset definitions and cache options are in
`CMakePresets.json`.

## Recommended continuation order

### 1. Run the focused Qt tests

Use a fresh Windows Qt x64 debug tree. The `--fresh` configure avoids stale
generated targets:

```powershell
$env:QT_MSVC_X64_PREFIX = 'C:/Qt/6.12.0/msvc2022_64'
cmake --fresh --preset windows-x64-debug
cmake --build build/windows-x64-debug --config Debug --target `
  ClassMngrDataServiceLifecycleTests `
  ClassMngrTeacherImportTests `
  ClassMngrRosterModelTests `
  ClassMngrSpeakingEvalModelTests `
  ClassMngrSpeakingEvaluationServiceTests `
  ClassMngrSharedPolicyTests `
  ClassMngrCalendarEventCacheTests `
  ClassMngrCalendarEventRepositoryTests `
  --parallel 1 -- /p:TrackFileAccess=false
ctest --test-dir build/windows-x64-debug -C Debug -R `
  'ClassMngr(DataServiceLifecycle|TeacherImport|RosterModel|SpeakingEvalModel|SpeakingEvaluationService|SharedPolicy|CalendarEventCache|CalendarEventRepository)Tests' `
  --output-on-failure
```

If Visual Studio reports missing generated `.moc` files while using a reduced
target build, build the corresponding `<target>_autogen` target first, or run
the normal target build without disabling project references. This is a build
system detail, not a reason to remove MOC sources or change production code.

### 2. Re-run one complete Qt-free engine tree

Configure and build each required Windows engine lane from a fresh directory.
The matching build preset targets the WinUI executable, so the exit-gate
matrix must use an unfiltered build followed by a filtered CTest command:

```powershell
cmake --fresh --preset windows-x64-winui-debug
cmake --build build/windows-x64-winui-debug --config Debug --clean-first --parallel 2 -- /p:TrackFileAccess=false
ctest --test-dir build/windows-x64-winui-debug -C Debug -R '^ClassMngrEngine' --output-on-failure
```

Repeat for:

- `windows-x64-winui-release` / `Release`
- `windows-x86-winui-debug` / `Debug`
- `windows-x86-winui-release` / `Release`

Every registered `ClassMngrEngine*` executable must exist and have a
matching runtime entry. Missing or unexecuted binaries are failures.

### 3. Collect the seven exit-gate lane reports

From the repository root, use the dependency-free runner. For example:

```powershell
$env:TrackFileAccess = 'false'
python scripts/phase2_exit_gate.py run `
  --lane-id windows-x64-winui-debug `
  --lane-type engine `
  --configure-preset windows-x64-winui-debug `
  --build-dir build/windows-x64-winui-debug `
  --configuration Debug `
  --output artifacts/phase2/windows-x64-winui-debug/windows-x64-winui-debug.json `
  --logs-dir artifacts/phase2/windows-x64-winui-debug/logs
```

Run the analogous three other Windows engine lanes, then the retained Qt 6.12
Windows, Linux, and macOS lanes. The exact retained-Qt command shape is in
[the exit-gate runbook](phase2-exit-gate-runbook.md).

After all lane reports are present:

```powershell
python scripts/phase2_exit_gate.py validate `
  --reports-dir artifacts/phase2/reports `
  --output artifacts/phase2/aggregate-report.json
```

`--compile-only` is useful for recording a build limitation but cannot
produce an aggregate pass. The aggregate requires runtime-tested evidence for
all four Qt-free Windows lanes and exact-Qt runtime-tested evidence for all
three retained-Qt lanes.

### 4. Update the plan only after evidence is complete

Record exact commands, toolchain versions, lane reports, and any human review
of fixture semantics in the Phase 2 plan and local validation record. Keep
P2-R07 deferred until the seven-lane report is complete and non-red. Do not
mark the overall Phase 2 exit gate complete merely because the Windows source
build succeeds.

## Useful final audits

```powershell
rg -n "\\.database\\(\\)|DatabaseSession::database|m_legacyDataService" src tests cmake
rg -n "m_database\\.(isValid|isOpen|databaseName)" src/data/repositories
git diff --check
git status --short
```

The expected production boundary is `compatibilityDatabase()` for retained
Qt SQL tests/adapters; production feature services should use the shared
engine-backed `DatabaseSession` and must not regain `DataService`
operation fallbacks.
