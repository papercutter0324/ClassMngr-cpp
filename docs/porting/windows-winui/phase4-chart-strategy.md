# Phase 4 chart strategy

## Decision

Use WinUI composition/XAML primitives first for charts: `Grid`, `Canvas`,
`Border`, `Rectangle`, `Path`, `TextBlock`, and standard `ToolTip`/focusable
legend controls. Chart navigation, selection, focus, and context actions stay
in the WinUI visual tree.

No Direct2D/DirectWrite chart surface is approved in Phase 4.

## Escalation threshold

A specialized Direct2D/DirectWrite drawing surface may be proposed only when a
representative primitive chart has a recorded profiling or fidelity failure.
The proposal must scope the native surface to rendering; its keyboard focus,
automation peer, tooltip, selection, commands, and input hit targets remain
WinUI controls. It must also include device-loss/recreation, text scaling,
high-contrast, and Korean glyph coverage evidence.

This prevents a chart optimization from becoming a second interaction toolkit
or bypassing the shared UX/accessibility contract.
