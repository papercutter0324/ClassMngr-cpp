# ADR 0002: WinUI 3 Windows Presentation

- Status: accepted
- Date: 2026-08-29
- Scope: the Windows-native ClassMngr product
- Supersedes: ADR 0001 decisions 1, 2, 4, and 5 where they prescribe the
  presentation stack, operating-system floor, custom control tree, and text
  editing implementation

## Decision

1. The Windows application uses WinUI 3 from a pinned stable Windows App SDK.
   XAML owns layout and the standard control tree; C++/WinRT owns Windows view
   models, commands, lifecycle, and platform adapters.
2. `ClassMngrEngine` remains portable C++23 and exposes no Qt, WinUI, WinRT,
   XAML, or Win32 UI types. Both Qt and WinUI presentations consume the same
   product rules, validation, persistence, and use cases.
3. ClassMngr does not build a general-purpose Win32/Direct2D UI toolkit.
   Direct2D and DirectWrite interop are allowed only for a measured,
   product-specific rendering surface that standard WinUI primitives cannot
   satisfy. Standard forms, navigation, text editing, focus, and input remain
   WinUI-owned.
4. The Windows support floor is Windows 10 version 1809 (build 17763), or a
   newer floor required by the pinned Windows App SDK. Build configuration,
   application manifest, bootstrap checks, installer messaging, and release
   documentation must agree on the selected floor.
5. Development and initial release deployment are unpackaged and
   self-contained. Inno Setup remains responsible for installation, upgrade,
   uninstall, shortcuts, and file associations; the existing signed-installer
   update workflow remains the release handoff. A move to MSIX or
   packaged-with-external-location requires a separate ADR.
6. The WinUI development product retains an isolated executable identity,
   AppUserModelID, settings namespace, install stage, and copied-database rule
   until cutover.
7. Windows x64 is the release gate. ARM64 release support remains deferred
   until runtime, performance, packaging, and parity evidence exists.

## Consequences

- The supported Windows floor moves from the provisional Direct2D plan's
  Windows 10 version 1703 to Windows 10 version 1809 or newer.
- The WinUI project uses the supported Windows App SDK XAML/MSBuild toolchain;
  CMake continues to own the portable engine, tests, resource catalog, and
  retained Qt products and orchestrates the complete native build.
- Windows App SDK packages and C++/WinRT build inputs are pinned and restored
  deterministically. Self-contained output is checked in staging and on a
  clean supported Windows installation.
- The existing minimal Win32 shell and Direct2D capability test are temporary
  Phase 1 evidence. Remove them after equivalent WinUI launch, manifest,
  deployment, and smoke coverage exists.
- Built-in WinUI focus, input, theming, text scaling, and automation semantics
  are preserved. Product tests still verify Korean IME, keyboard behavior,
  high contrast, and critical accessibility relationships.
- macOS and Linux remain Qt products and are unaffected by the Windows
  presentation choice.

## Retained Decisions from ADR 0001

- Shared engine interfaces use portable standard C++ types and typed errors.
- Schema, migrations, resources, updates, and exports are shared contracts.
- Windows presentation code never issues ad hoc SQL.
- Native development remains isolated from the shipping Qt Windows product
  until the cutover gate.

## Plan of Record

- [Windows WinUI 3 Port — Start Here](../../../plans/windows-winui3-port-plan/00-START-HERE.md)
