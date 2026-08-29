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
lane. The WinUI-only CMake caches contain no Qt package entries.

## Retained Windows Qt validation

The retained Windows Qt Debug suite was rebuilt and run with all 69 CTest
entries passing, including the Windows visual-capture and startup-performance
gates. The startup-settings test also confirms that the font-size controller
reapplies the setting loaded by `ActionRegistry` when its actions are connected.
The retained Windows Qt Release product was also rebuilt successfully with its
deployed Qt runtime.

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
workloads.

## Open acceptance evidence

- clean execution on the `windows-2025-vs2026` GitHub runner;
- owner-reviewed light/dark rendering at 100%, 150%, and 200% DPI;
- interactive keyboard focus and Korean IME composition;
- representative x86 peak-memory measurements and budget decision;
- retained Windows, Linux, and macOS Qt validation after the lane lands.
