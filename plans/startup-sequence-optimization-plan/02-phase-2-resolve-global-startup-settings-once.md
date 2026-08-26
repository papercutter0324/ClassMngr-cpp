<!-- ClassMngr Startup Optimization Plan — Phase 2 -->

> **Codex scope:** Read `00-START-HERE.md` plus this file. Implement and validate **Phase 2 only**. Do not start later phases.

# Phase 2 — Resolve Global Startup Settings Once
## Objective
Apply locale, font, theme, and other application-wide visual settings once, before constructing the majority of the widget tree.

This behavior should be identical on all platforms unless a platform-specific workaround is genuinely required.

## Current Problem
The startup sequence currently performs repeated application-wide visual processing, including repeated font-size application and post-show restyling.

This can trigger:

- recursive widget font changes;

- `QApplication::allWidgets()` traversal;

- style unpolish/polish passes;

- geometry updates;

- layout invalidation;

- broad page refreshes;

- repainting.

## Tasks
### 2.1 Resolve startup preferences before MainWindow construction
Resolve:

- locale/language;

- font size;

- theme;

- other global display preferences;

before constructing the main UI where practical.

Desired shared sequence:
```text
QApplication

→ read startup preferences

→ apply locale

→ apply font

→ apply palette/stylesheet

→ construct MainWindow/widget tree
```
### 2.2 Keep one authoritative startup font application
Retain one startup font setup similar to:
```text
FontManager::setSizeOffset(savedOffset)

→ FontManager::applyGlobalFont(...)
```
Do not perform a second whole-application pass simply because `FontSizeController` is being connected.

### 2.3 Make identical font-size requests true no-ops
Update `FontManager::applyFontSize()` so that an unchanged offset immediately returns.

Conceptually:
```cpp
if (offset == s_sizeOffset)

    return;
```
When unchanged, do not:

- recursively apply fonts;

- assign inherited fonts;

- traverse all widgets;

- unpolish/polish widgets;

- invalidate layouts;

- update geometries;

- refresh menu fonts.

Ensure genuine runtime user changes still work correctly.

### 2.4 Change `FontSizeController::connectActions()`
Connecting actions should establish future signal handling only.

It must not reapply the current font-size state during startup when that state has already been applied.

### 2.5 Remove `reapplyStartupFontSize()`
Remove the post-show startup font-size reapplication.

Also remove startup-only work tied exclusively to it, including any:

- `refreshAllSidebars()`;

- `PageManager::refreshAll()`;

- broad layout invalidation;

- `updateGeometry()`;

- forced `repaint()`.

If one control has a real first-show sizing issue, fix that control directly.

### 2.6 Apply startup theme before large-scale widget construction
Where feasible, apply the saved application palette/stylesheet before constructing MainWindow and page widgets.

`ThemeController` should primarily handle future user changes, not reapply the already-resolved startup theme.

Avoid an explicit traversal/repolish of all widgets at startup when those widgets can instead be created under the correct theme.

### 2.7 Validate each platform
Check that:

- fonts render correctly;

- font-size preference is restored correctly;

- theme preference is restored correctly;

- runtime font/theme changes still work;

- macOS, Windows, and Linux retain expected appearance.

## Acceptance Criteria
Normal startup performs:

- one locale application;

- one initial font application;

- one initial theme application;

- zero identical-value font-size passes;

- zero application-wide post-show font reapply;

- zero startup-wide restyle solely because controllers were connected.

---
