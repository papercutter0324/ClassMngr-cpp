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

The required lane set is intentionally split as follows:

| lane group | required lanes | evidence allowed for aggregate |
| --- | --- | --- |
| Qt-free WinUI engine | x64 Debug, x64 Release, x86 Debug, x86 Release | runtime-tested only |
| retained Qt adapter | Windows Qt 6.12 x64, Linux Qt 6.12 x64, macOS Qt 6.12 universal | exact Qt 6.12.0 runtime-tested only |

The JSON report records `evidence_class` as `runtime-tested`, `compile-only`,
`host-blocked`, or `failed`. A compile-only report is an honest build result,
but it is not an exit-gate pass. A retained-Qt report is `host-blocked` when
the exact Qt prefix/version cannot be established; it must include the
blocking reasons. The four engine lanes remain separate from the three
retained-Qt lanes, including their x64/x86 results.

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
registered engine tests. The required unfiltered CTest inventory is collected
after the build and before the filtered runtime command. Every registered
`ClassMngrEngine*` test must have a resolved executable and a matching JUnit
runtime entry; missing or unexecuted binaries are failures, not omitted tests.
The current explicit category evidence test is
`ClassMngrEngineDatabaseFixtureRoundTripTests`, whose fixture suite exercises
all five categories. The report only marks a category covered when that test
is registered and actually executed.

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
To record a build without making a runtime claim, add `--compile-only` to the
`run` command. This remains a failed aggregate lane until runtime evidence is
collected.

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

This repository does not claim a completed cross-platform exit gate. The
current local/plan limitations are concrete: existing build trees are missing
nine registered engine binaries, the available host Qt is 6.11.1 while the
gate requires exactly 6.12.0, and the Linux baseline is red. Those conditions
must remain visible as missing-artifact, `host-blocked`, or failed evidence;
they must not be converted into passing runtime results. Because
`NativeWindowsPort` is unofficial, the outcome remains deferred rather than a
release claim.

- Fresh artifacts are required for every Windows matrix lane; focused tests
  are not a complete matrix.
- Missing registered binaries, the Qt 6.11.1 versus 6.12.0 host mismatch,
  MSBuild FileTracker failures, and any Linux/macOS failures must be recorded
  in the lane report or repaired before the gate is closed.
- The three retained-Qt lanes must publish current fixture evidence for every
  migrated persistence slice, including invalid input, rollback, migration,
  busy/locked database, and partial-failure outcomes where the lane owns that
  fixture.
