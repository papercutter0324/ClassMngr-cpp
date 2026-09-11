# Superseded Windows Direct2D/DirectComposition Port Plan

The Windows presentation direction changed to WinUI 3 on 2026-08-29. This file
is retained only so historical links continue to resolve.

Use the current plan of record:

- [Windows WinUI 3 Port — Start Here](windows-winui3-port-plan/00-START-HERE.md)

The accepted Phase 0 baseline and generic Qt-free engine/build-boundary work
remain valid. The former plan to build the complete shell and control layer
directly with Win32, Direct2D, and DirectComposition is superseded. Direct2D
may still be used for narrowly scoped rendering surfaces under WinUI 3.

Historical baseline artifacts remain under
[`docs/porting/windows-direct2d`](../docs/porting/windows-direct2d/README.md).
