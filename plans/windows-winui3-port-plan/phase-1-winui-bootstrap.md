# Phase 1 — Build Split and WinUI Bootstrap

> Progress is tracked in [00-START-HERE.md](00-START-HERE.md).

## Goal

Prove an isolated WinUI 3 C++/WinRT application can build, test, install, and
launch from the same repository without Qt, while every retained Qt product
continues to build and test.

## Existing Foundation to Preserve

- Product selection separates Qt desktop and Windows-native configurations.
- `ClassMngrEngine` is Qt/Win32-free and has an ordinary CTest executable.
- A provisional `ClassMngrWindowsNative` shell proves native-only configure,
  resources, manifests, staging, and binary smoke tests.
- The retained Windows Qt transition target and its tests remain operational.
- Native Debug/Release presets and a native foundation workflow exist.

These are useful boundary tests, but the provisional Win32/Direct2D shell is
not the target UI.

## Build Architecture

1. Keep CMake authoritative for `ClassMngrEngine`, shared native libraries,
   tests, resource catalogs, and retained Qt products.
2. Create the Windows UI from the supported WinUI 3 C++/WinRT project
   structure so the official XAML compiler and Windows App SDK MSBuild targets
   remain authoritative. Do not reproduce those targets manually in CMake.
3. Add a deterministic orchestration target/script that builds the engine for
   the selected configuration and architecture, then builds the WinUI project
   against that exact engine artifact.
4. Pin the stable `Microsoft.WindowsAppSDK` and C++/WinRT package versions.
   Lock restore inputs so local and CI builds resolve the same toolchain.
5. Keep source and generated boundaries explicit: XAML-generated files never
   enter `src/engine`, and engine public headers never include WinRT types.
6. Define `x64` and `Win32`/`x86` project platforms that use
   architecture-specific engine outputs, Windows App SDK runtime files,
   staging directories, and binary checks. Do not allow one architecture to
   consume the other's generated files or libraries.

## Implementation Sequence

1. Add a minimal `App.xaml`, `MainWindow.xaml`, C++/WinRT application class,
   and one engine-backed version/about value.
2. Select unpackaged, self-contained Windows App SDK deployment for the
   development and initial release path. Preserve Inno Setup ownership of
   shortcuts, uninstall, file associations, and update handoff.
3. Update the native manifest and installer checks to enforce Windows 10 1809
   or the pinned SDK's newer supported floor.
4. Retain the distinct development executable name, AppUserModelID, settings
   namespace, staging directory, and copied-database rule.
5. Add deterministic smoke modes that create and close the real WinUI window,
   verify the embedded manifest, call the engine, and exit without user data.
6. Add a small representative form containing text input, a standard button,
   focus traversal, light/dark resources, and Korean IME composition.
7. Validate 100%, 150%, and 200% DPI on an interactive runner or reviewed
   device and emit the same capture metadata contract used by Phase 0.
8. Prove a clean self-contained stage launches without Qt and without a
   separately installed Windows App SDK runtime.
9. Add x86 Debug and Release build configurations, engine tests, installed
   application smoke tests, manifest checks, and PE architecture checks. Keep
   x86 stages and artifacts distinct from x64.
10. Audit every native dependency for x86 availability. A dependency without
    an x86 build blocks its feature slice until it is replaced or isolated
    behind an architecture-neutral process boundary.
11. Capture x86 startup, steady-state, and representative peak memory. The
    steady-state target is 200 MiB; define a separate safe peak budget from
    PDF, import, report, and large-data measurements before publishing x86.
12. After equivalent WinUI checks exist, remove the provisional window class,
   Direct2D capability gate, and SDK links that are not consumed by a retained
   specialized renderer.
13. Keep Windows Qt, Linux Qt, and macOS Qt regression and packaging routes
    green throughout the conversion.

## Validation

- Fresh native restore/build contains no Qt discovery or Qt binary imports.
- Engine tests run independently of the WinUI app.
- Debug and Release WinUI builds pass on Windows x64 and x86.
- Each architecture links only matching engine, Windows App SDK, VC runtime,
  and third-party binaries; PE and staged-runtime checks reject mixed output.
- Installed smoke, manifest, engine-call, theme, DPI, focus, and Korean IME
  checks pass.
- Self-contained staging contains all required Windows App SDK components and
  launches on a clean supported Windows environment for x64 and x86.
- x86 smoke and memory reports record the 200 MiB steady-state result and the
  measured worst-case peak; x86 publication is not part of this phase.
- Running the x86 lane under WOW64 is sufficient for build support. A public
  x86 release additionally requires validation on an actual supported 32-bit
  Windows installation.
- Existing Qt suites and release staging remain unchanged and green.
- No provisional WinUI artifact appears in public updater metadata.

## Exit Gate

A real WinUI 3 XAML application builds and launches from Qt-free,
self-contained x64 and x86 stages, calls the shared engine, passes
representative input and visual smoke checks, and coexists safely with the
retained Qt products. x64 is release-capable; x86 is build-and-test supported.
