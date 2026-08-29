# Windows WinUI 3 Bootstrap Evidence

This directory records the Phase 1 WinUI presentation bootstrap. The target is
an unpackaged, self-contained C++/WinRT application that links the Qt-free
`ClassMngrEngine` static library and keeps its development identity separate
from the retained Qt product.

## Pinned inputs

| Input | Pin | Role |
| --- | --- | --- |
| Windows App SDK meta-package | `2.4.0` | Stable release dependency set |
| Windows App SDK WinUI component | `2.3.6` | WinUI 3 controls and XAML build targets |
| Windows App SDK Runtime component | `2.4.0` | Self-contained Windows App Runtime payload |
| Windows App SDK Base component | `2.0.4` | Shared native build/deployment targets |
| Windows App SDK Foundation component | `2.3.9` | WinUI native dependency |
| Windows App SDK Interactive Experiences component | `2.1.6` | WinUI native dependency |
| WebView2 | `1.0.3719.77` | WinUI component build dependency |
| Windows SDK MSIX build tools | `1.7.251221100` | Native MSBuild packaging targets |
| C++/WinRT | `3.0.260818.1` | XAML projection and generated runtime-class code |
| Windows SDK | `10.0.26100.0` | MSVC target SDK selected by the CMake presets |
| Visual Studio | `18 2026` | CMake generator; uses the `v145` MSVC toolset |
| Windows SDK Build Tools | `10.0.26100.4654` | Native MSBuild/SDK build targets |
| Windows minimum | `10.0.17763.0` | Windows 10 version 1809 floor |

The package versions are declared in
[`packages.config`](../../../src/platform/windows/winui/packages.config), and
the classic MSBuild project imports only those exact package directories. The
WinUI bootstrap imports only the WinUI/runtime component targets; unrelated
Windows App SDK AI, ML, Search, and Widgets targets are not part of this lane.
The Windows App SDK self-contained properties are kept in
[`ClassMngrWinUI.vcxproj`](../../../src/platform/windows/winui/ClassMngrWinUI.vcxproj).
The build orchestration resolves a Visual Studio 18 installation through
`vswhere`, requires the selected platform's `v145` toolset, and passes that
toolset explicitly to MSBuild. The stage verifier applies the same VS 2026
constraint before resolving `dumpbin.exe`.

## Project boundary

```text
ClassMngrEngine (CMake static library; no Qt or Windows UI headers)
        |
        +-- ClassMngrWinUI.vcxproj (MSBuild, WinUI 3 XAML, C++/WinRT)
                |
                +-- Debug/Release x64 and Win32 stages
```

The CMake target `ClassMngrWindowsWinUI` restores the MSBuild packages,
passes the architecture-specific engine library and generated build metadata,
builds the XAML project, and copies the resource manifest and font licenses
into the stage. The PowerShell verifier checks PE architecture, self-contained
runtime payload, compiled XAML/MRT resources, embedded manifest behavior, and
absence of Qt files/imports.

## Local commands

```powershell
./scripts/verify_windows_vs2026.ps1
cmake --fresh --preset windows-x64-winui-debug
cmake --build --preset windows-x64-winui-debug --parallel 2
ctest --test-dir build/windows-x64-winui-debug -C Debug --output-on-failure

cmake --fresh --preset windows-x86-winui-debug
cmake --build --preset windows-x86-winui-debug --parallel 2
ctest --test-dir build/windows-x86-winui-debug -C Debug --output-on-failure

cmake --fresh --preset windows-x64-winui-release
cmake --build --preset windows-x64-winui-release --parallel 2
ctest --test-dir build/windows-x64-winui-release -C Release --output-on-failure

cmake --fresh --preset windows-x86-winui-release
cmake --build --preset windows-x86-winui-release --parallel 2
ctest --test-dir build/windows-x86-winui-release -C Release --output-on-failure
```

Release presets keep the engine and staged WinUI tests enabled. The CI matrix
exercises all four architecture/configuration combinations and uploads each
stage separately.

The latest local matrix and memory result are recorded in the
[Phase 1 local validation record](phase1-local-validation.md). It is evidence
for the bootstrap lane, not a Phase 1 acceptance record; interactive and clean
runner gates remain open.

To collect the initial idle-process memory evidence for an installed stage,
run the measurement helper from a Windows desktop session. It records both the
steady-state sample maximum and the process peak, and enforces the shared
200 MiB steady-state target:

```powershell
.\scripts\measure_windows_winui_memory.ps1 `
  -StageDirectory dist/ClassMngr-windows-winui-x86/Release `
  -Platform Win32
```

The generated JSON report is an input to the separate peak-budget decision;
this bootstrap does not claim an x86 release budget from a single idle sample.

## Remaining Phase 1 evidence

The bootstrap source contains a representative Korean text form and explicit
light/dark, input/focus-contract, manifest, and live-layout DPI smoke modes.
The following evidence must still be collected on a Windows runner before
Phase 1 can be accepted:

- clean x64 and x86 Debug/Release builds and launches (the local matrix now
  passes; clean-runner confirmation remains);
- owner-reviewed light/dark, 100%/150%/200% DPI, keyboard focus, and Korean IME
  results;
- representative x86 steady-state and peak working-set measurements against
  the shared 200 MiB target (the latest local idle baseline is 71.00 MiB; a
  representative peak-budget run remains);
- retained Windows, macOS, and Linux Qt validation after the new lane lands.

No public updater or installer metadata is changed by this bootstrap.
