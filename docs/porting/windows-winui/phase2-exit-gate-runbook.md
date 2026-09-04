# Phase 2 exit-gate runbook

Status: open. This runbook defines the required evidence path; it does not
represent a passing cross-platform matrix.

## Windows headless engine matrix

Configure each test-enabled preset from a fresh build directory:

```powershell
cmake --preset windows-x64-winui-debug
cmake --preset windows-x64-winui-release
cmake --preset windows-x86-winui-debug
cmake --preset windows-x86-winui-release
```

Build every registered target rather than using the matching build preset,
which builds only `ClassMngrWindowsWinUI`:

```powershell
cmake --build build/windows-x64-winui-debug --config Debug --parallel 2
ctest --test-dir build/windows-x64-winui-debug -R "^ClassMngrEngine" --output-on-failure
```

Repeat the build and CTest command with the matching build directory and
configuration for x64 Release, x86 Debug, and x86 Release. Archive the CTest
output with the build logs. The matrix must include invalid-input, rollback,
migration, busy/locked database, and partial-failure coverage from the
registered engine tests.

## Retained Qt adapter fixture

Use the Qt 6.12 x64 desktop route (via `QT_MSVC_X64_PREFIX`):

```powershell
cmake --preset windows-x64-debug
cmake --build --preset windows-x64-debug --target ClassMngrDatabasePortFixtureGenerator
ctest --test-dir build/windows-x64-debug -R "^ClassMngrDatabasePortFixtureTests$" --output-on-failure
```

Keep this result separate from the Qt-free engine matrix. Record the exact Qt
version and whether the test was runtime-tested, compile-only, or blocked.

## macOS and Linux fixture ownership

The manually dispatched `refactoring-baseline.yml` workflow owns the retained
Qt fixture directions:

- `baseline` / `Linux x64 Debug` (`linux-gcc-debug`)
- `baseline` / `macOS universal Debug` (`macos-clang-debug`)

That workflow archives JSON, JUnit, and CTest logs. Release workflows are not
substitutes because they set `BUILD_TESTING=OFF`.

## Current blockers to closing P2-R07

- Fresh artifacts are required for every Windows matrix lane; focused tests
  are not a complete matrix.
- Historical missing registered binaries, the Qt 6.11.1 versus 6.12.0 host
  mismatch, and MSBuild FileTracker failures must be recorded as failures or
  repaired before the gate is closed.
- The two cross-platform CI lanes must publish current fixture evidence for
  every migrated persistence slice.
