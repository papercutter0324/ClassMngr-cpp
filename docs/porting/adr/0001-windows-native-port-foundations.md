# ADR 0001: Windows native port foundations

- Status: accepted for the port.
- Date: 2026-08-23
- Scope: `ClassMngrWindowsNative` after Phase 1; the existing Qt products are
  unchanged by this ADR.

## Decision

1. Win32 owns top-level windows and the message loop. Direct2D 1.3 renders;
   DirectWrite lays out text; WIC decodes images; DirectComposition owns the
   visual tree, transforms, clipping, opacity, and animation. The required
   graphics interfaces are `ID2D1Factory3`, `ID2D1Device2`, and
   `ID2D1DeviceContext2` from `d2d1_3.h`. Native common dialogs are used for
   files, folders, and printing.
2. The native product supports Windows 10 version 1703 (build 15063) and later
   plus Windows 11, x64 and ARM64. Version 1703 is the functional runtime
   floor because the required Per-Monitor DPI Awareness V2 mode is first
   available there; `ID2D1DeviceContext2` independently requires Windows 10.
   The Phase 1 build SDK is pinned to `10.0.26100.0`, which is installed on the
   baseline Windows build host and supplies `d2d1_3.h` with
   `ID2D1Factory3`, `ID2D1Device2`, and `ID2D1DeviceContext2`.

   The native executable embeds an application manifest with all of the
   following declarations:

   - Windows 10/11 compatibility GUID
     `{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}` in `supportedOS`;
   - `dpiAwareness` set to `PerMonitorV2, PerMonitor, System` in the 2016
     Windows Settings namespace;
   - `longPathAware` set to `true` in that same namespace.

   Phase 1 CMake/CI must select `10.0.26100.0`, fail if `d2d1_3.h` is absent,
   compile its capability target with `NTDDI_VERSION >= NTDDI_WIN10_RS2`, and
   inspect the built manifest for these declarations. The native launcher or
   installer must reject a Windows 10 build below 15063 with an actionable
   message. This contract is not inferred from the Qt build.

   This is a functional API floor. A release channel may impose a newer
   vendor-serviced deployment floor, but it may not silently lower the native
   API or DPI contract recorded here.
3. Shared engine public interfaces are Qt-free and platform-UI-free. They use
   UTF-8 `std::string`, `std::chrono`, standard containers, and typed
   result/error values. SQLite access moves to the SQLite C API during Phase 2;
   choosing and pinning a JSON library requires a separate license/footprint
   review.
4. One small semantic view tree is the source for layout, hit testing, focus,
   invalidation, rendering, and UI Automation. It implements only controls in
   the Phase 0 inventory; it is not a general-purpose widget toolkit.
5. Editable text starts with native edit controls where possible. A custom
   editor cannot replace them until it passes Korean IME, Unicode-grapheme
   navigation, selection, clipboard, undo/redo, password, and accessibility
   checks.
6. `.tps` schema/migrations, legacy `.db` migration behavior, resource-pack
   manifests, update metadata/signatures, and supported exports are shared
   contracts. Windows presentation code never issues ad hoc SQL.
7. The native executable, app identity, and settings namespace are isolated
   from the Qt Windows product until the cutover gate. Native development opens
   copied databases only.

## Consequences

- Phase 1 must build a minimal native shell without linking any Qt target and
  must fail configuration/CI when SDK `10.0.26100.0`, `d2d1_3.h`, the required
  capability interfaces, or the required application-manifest declarations are
  unavailable.
- Phase 3 owns device-loss recovery, WARP fallback, Per-Monitor DPI Awareness
  V2, and DirectComposition scheduling; they may not be delegated to a UI
  framework.
- UIA and Korean IME are first-class parity requirements, not finish-stage
  polish.
- The Phase 0 matrix and fixture corpus become release inputs for both
  architectures.

## Evidence

- [ID2D1DeviceContext2 requirements](https://learn.microsoft.com/en-us/windows/win32/api/d2d1_3/nn-d2d1_3-id2d1devicecontext2)
  identify Windows 10 as its minimum supported client.
- [IDCompositionDesktopDevice requirements](https://learn.microsoft.com/en-us/windows/win32/api/dcomp/nn-dcomp-idcompositiondesktopdevice)
  describe the Win32 DirectComposition desktop-device requirement.
- [High-DPI desktop application development](https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows)
  identifies Windows 10 version 1703 as the Per-Monitor DPI Awareness V2 floor.
- [Application manifests](https://learn.microsoft.com/en-us/windows/win32/sbscs/application-manifests)
  documents the Windows 10/11 compatibility GUID and `dpiAwareness` behavior.
- [Maximum path length limitation](https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation)
  documents the `longPathAware` manifest declaration.

