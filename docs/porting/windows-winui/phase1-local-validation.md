# Phase 1 local validation record

Date: 2026-08-29 (Asia/Seoul)

Scope: working tree based on `eaeb62f`, using the installed Visual Studio 2026
Community toolchain.

## Toolchain

- Visual Studio generator: `Visual Studio 18 2026`
- MSVC platform toolset: `v145` (`14.51.36231`)
- Windows SDK: `10.0.26100.0`
- CMake: `4.4.2`
- C++/WinRT: `3.0.260818.1`
- Windows App SDK: `2.4.0` meta-package with pinned component packages

The VS2026/v145 preflight passed and selected:
`C:\Program Files\Microsoft Visual Studio\18\Community`.

## Build and test matrix

Each lane was freshly configured, built, staged, and verified with its
architecture-specific executable and Windows App SDK runtime payload.

| Lane | Result |
| --- | --- |
| x64 Debug | Build passed; engine and WinUI stage CTest passed |
| x64 Release | Build passed; engine and WinUI stage CTest passed |
| x86 Debug (`Win32`) | Build passed; engine and WinUI stage CTest passed |
| x86 Release (`Win32`) | Build passed; engine and WinUI stage CTest passed |

The stage verifier also passed the embedded-manifest, compiled-XAML/MRT,
architecture, self-contained-runtime, and Qt-absence checks for each tested
lane. Its input smoke mode now verifies the text input scope used by the
Korean-language form, and its DPI smoke mode verifies a live XamlRoot scale and
non-zero layout. The WinUI-only CMake caches contain no Qt package entries.
The x64 and Win32/x86 Release stages were subsequently re-verified; each
passed the manifest, engine, input, theme, and DPI smoke modes. The
corresponding Debug stages were also re-verified successfully. Each WinUI CTest
lane additionally verifies the generated resource manifest against the catalog
file list, sizes, and SHA-256 hashes.

## Hosted runner validation

Commit `e4887b6` was validated by [GitHub Actions run #2](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/33255348376)
on the `windows-2025-vs2026` runner. The x64 and x86 Debug and Release jobs
all passed the VS 2026/v145 preflight, configure, build, Qt-free cache, CTest,
self-contained-stage, and artifact-upload steps. The x86 Release job also
uploaded `ClassMngr-windows-winui-x86-Release-memory`.

The retained [Linux x64 Release](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/33255348382)
and [macOS universal](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/33255348426)
workflows also passed their build, package, and artifact upload checks for the
same revision.

## Retained Windows Qt validation

The normal retained Windows Qt Debug route contains 68 tests; all 68 passed
with the isolated settings root after the provisional native-lane cleanup.
The explicit Phase 0 visual preset was then freshly configured and rebuilt with
VS 2026/v145. Its full 69-test run passed 68 tests; the only failure was the
visual-capture test rejecting this host's 125% display scale. The capture
contract intentionally accepts only the fixed 100%, 150%, and 200% review
matrix, so this is an environment gate rather than a source regression.

That run also exposed a stale startup-fixture expectation: the committed
fixture contains 16 `app_settings` rows, while the test expected 12. The
expectation was corrected to 16, rebuilt, and the startup-performance test
passed. The startup-settings test continues to confirm that the font-size
controller reapplies the setting loaded by `ActionRegistry` when its actions
are connected. The retained Windows Qt Release product was previously rebuilt
successfully with its deployed Qt runtime; the cleanup does not alter its Qt
branch.

A separate post-cleanup run produced a 16-row Phase 0 visual-capture artifact
set under
`artifacts/phase0/windows-qt-visual/20260829T115650210Z-7400/`; all 16 metadata
sidecars passed the repository validator. This automated validation does not
replace owner review of the PNG captures.

The local run used `CLASSMNGR_SETTINGS_ROOT` pointing into the build tree,
because the restricted validation environment cannot write the default
registry-backed `QSettings` location. This override is test-only; the normal
Windows application settings location remains unchanged.

## x86 memory baseline

Command:

```powershell
./scripts/measure_windows_winui_memory.ps1 `
  -StageDirectory dist/ClassMngr-windows-winui-x86/Release `
  -Platform Win32 `
  -WarmupSeconds 5 `
  -SampleSeconds 15
```

The generated report recorded:

- startup working set: 68.30 MiB;
- steady-state peak working set: 71.00 MiB;
- process peak working set: 71.00 MiB;
- steady-state target: 200 MiB (pass).

This is an idle-stage baseline. It does not establish the separate
representative peak budget required for import, reporting, PDF, or large-data
workloads. The
[`windows-winui-build.yml`](../../../.github/workflows/windows-winui-build.yml)
workflow now repeats this x86 Release capture on the hosted runner and uploads
the JSON report as a separate artifact.

## Open acceptance evidence

- owner-reviewed light/dark rendering at 100%, 150%, and 200% DPI;
- interactive keyboard focus and Korean IME composition;
- representative x86 peak-memory measurements and budget decision;
- retained Windows Qt visual review.
