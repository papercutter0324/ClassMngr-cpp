# Phase 2 exit-gate runbook

Status: deferred (2026-09-04). This is an unofficial port, so the
cross-platform evidence gate is intentionally deferred. The automated path
remains available for the point when the port is promoted for official use.

## Automated entry point

Dispatch [`.github/workflows/phase2-exit-gate.yml`](../../../.github/workflows/phase2-exit-gate.yml)
from GitHub Actions. The workflow runs the four Qt-free Windows engine lanes,
the retained Qt 6.12.0 Windows x64 fixture, and the retained Qt Linux and
macOS fixtures. Each lane uploads its JSON report, CTest JUnit result, CTest
inventory, and separate configure/build/CTest logs. The aggregate job requires
the exact seven-lane set and rejects missing registered engine executables,
failed tests, missing artifacts, incorrect Qt metadata, compile-only or
blocked fixtures, and incomplete category evidence.

For a local lane, use the dependency-free runner from the repository root. A
Windows engine invocation is:

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

The retained Windows Qt fixture additionally requires the exact Qt prefix:

```powershell
$env:TrackFileAccess = 'false'
$env:QT_MSVC_X64_PREFIX = 'C:\Qt\6.12.0\msvc2022_64'
python scripts/phase2_exit_gate.py run `
  --lane-id windows-qt-6.12-x64 --lane-type retained-qt `
  --configure-preset windows-x64-debug --build-dir build/windows-x64-debug `
  --configuration Debug --qt-version 6.12.0 --qt-architecture x64 `
  --qt-prefix-env QT_MSVC_X64_PREFIX `
  --output artifacts/phase2/windows-qt-6.12-x64/windows-qt-6.12-x64.json `
  --logs-dir artifacts/phase2/windows-qt-6.12-x64/logs
```

After collecting lane directories, validate the complete matrix with:

```powershell
python scripts/phase2_exit_gate.py validate `
  --reports-dir artifacts/phase2/reports `
  --output artifacts/phase2/aggregate-report.json
```

## Windows headless engine matrix

Configure each test-enabled preset from a fresh build directory:

```powershell
cmake --fresh --preset windows-x64-winui-debug
cmake --fresh --preset windows-x64-winui-release
cmake --fresh --preset windows-x86-winui-debug
cmake --fresh --preset windows-x86-winui-release
```

Build every registered target rather than using the matching build preset,
which builds only `ClassMngrWindowsWinUI`:

```powershell
cmake --build build/windows-x64-winui-debug --config Debug --clean-first --parallel 2 -- /p:TrackFileAccess=false
ctest --test-dir build/windows-x64-winui-debug -C Debug -R "^ClassMngrEngine" --output-on-failure
```

Repeat the build and CTest command with the matching build directory and
configuration for x64 Release, x86 Debug, and x86 Release. Archive the CTest
output with the build logs. The matrix must include invalid-input, rollback,
migration, busy/locked database, and partial-failure coverage from the
registered engine tests.

## Retained Qt adapter fixture

Use the Qt 6.12 x64 desktop route (via `QT_MSVC_X64_PREFIX`):

```powershell
cmake --fresh --preset windows-x64-debug
cmake --build --preset windows-x64-debug --clean-first --parallel 1 --target ClassMngrDatabasePortFixtureGenerator -- /p:TrackFileAccess=false
ctest --test-dir build/windows-x64-debug -C Debug -R "^ClassMngrDatabasePortFixtureTests$" --output-on-failure
```

The CTest entry is named `ClassMngrDatabasePortFixtureTests`, but intentionally
executes the `ClassMngrDatabasePortFixtureGenerator` target. The automated
validator treats those as the test name and executable name respectively.

Keep this result separate from the Qt-free engine matrix. Record the exact Qt
version and whether the test was runtime-tested, compile-only, or blocked.

## macOS and Linux fixture ownership

The automated `phase2-exit-gate.yml` workflow now runs and archives these
retained-Qt directions as part of its final gate. The manually dispatched
`refactoring-baseline.yml` workflow remains the historical owner of the same
fixture directions:

- `baseline` / `Linux x64 Debug` (`linux-gcc-debug`)
- `baseline` / `macOS universal Debug` (`macos-clang-debug`)

That workflow archives JSON, JUnit, and CTest logs. Release workflows are not
substitutes because they set `BUILD_TESTING=OFF`.

Automation proves the declared build, inventory, test, artifact, and metadata
contracts. Human review remains required for interpreting fixture semantics,
confirming that the required migration slices and failure categories are still
meaningful, reviewing platform-specific failures or environment issues, and
deciding whether the documented Phase 2 intent is satisfied.

## Deferred closure conditions for P2-R07

The latest hosted rerun passed the four Qt-free Windows engine lanes, but the
full cross-platform baseline still has a failing Linux test phase. Because
`NativeWindowsPort` is unofficial, this result is recorded as deferred rather
than treated as a release blocker. Reopen the gate when the port becomes an
official supported target.

- Fresh artifacts are required for every Windows matrix lane; focused tests
  are not a complete matrix.
- Historical missing registered binaries, the Qt 6.11.1 versus 6.12.0 host
  mismatch, and MSBuild FileTracker failures must be recorded as failures or
  repaired before the gate is closed.
- The two cross-platform CI lanes must publish current fixture evidence for
  every migrated persistence slice.
