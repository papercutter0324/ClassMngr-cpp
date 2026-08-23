# Windows Direct2D 1.3 and DirectComposition Port Plan

## Objective

Replace the Windows Qt application shell and UI with a native C++23 Windows
application rendered with Direct2D 1.3 and composed with DirectComposition, while
preserving ClassMngr behavior, `.tps` database compatibility, English/Korean
input, x64 and ARM64 releases, and the current update and installer workflows.
Linux continues to use Qt. The completed Windows package must not deploy Qt
Widgets, Qt Quick, QML, Qt PDF, or other Qt runtime libraries.

This is a native-platform port, not a change to Qt's rendering backend.
DirectComposition manages surfaces and visual composition but does not provide
controls, layout, text editing, accessibility, dialogs, or printing. The port
therefore includes a small ClassMngr Windows presentation layer built on Win32,
Direct2D 1.3, DirectWrite, WIC, DirectComposition, UI Automation, and native system
services.

## Current-State Findings

- `ClassMngrBuildSettings` currently links every production object library to
  the complete Qt stack: Core, Gui, Widgets, Network, SQL, PDF, PrintSupport,
  QML, Quick, Quick Controls, and Quick Widgets.
- Qt value and event types occur in `src/core`, `src/data`, `src/domain`, and
  feature services, not just in `src/ui` and feature `ui` folders. A native UI
  cannot safely reuse the current runtime until business and persistence code
  is separated from Qt.
- `PageManager` defines the major product surfaces: personal details, calendar,
  schedules, classes, testing classes, teacher directories, campus information,
  documents/PDF, rosters, speaking evaluations, and substitute preparation.
- Several high-risk workflows are coupled to Qt painting or platform behavior:
  editable schedule/roster/evaluation tables, PDF viewing and printing, report
  generation, file dialogs, spreadsheet imports, resource packs, signed
  updates, PowerPoint automation, themes, translations, and Korean input.
- Windows currently supports x64 and ARM64, uses an Inno Setup installer, and
  has Windows-specific signature verification, updater, memory diagnostics,
  print-dialog, and PowerPoint code that should be preserved or replaced
  deliberately.

## Target Architecture

The port uses a strangler architecture so the existing Linux application stays
shippable throughout the work.

```text
ClassMngrEngine (portable C++23, no Qt or platform UI types)
    |-- domain models, validation, rules, use cases
    |-- SQLite repositories and schema migrations
    |-- import/export and report layout models
    |-- settings, resources, updates, and platform-service interfaces
    |
    +-- ClassMngrLinuxQt (existing Qt UI plus Qt adapters)
    |
    +-- ClassMngrWindowsNative
            |-- Win32 application/window/input/accessibility shell
            |-- Direct2D 1.3 + DirectWrite + WIC renderer
            |-- DirectComposition visual tree and animations
            |-- Windows platform-service implementations
            +-- feature presenters/view models
```

The engine owns product rules and database transactions. Presentation targets
own navigation, view state, platform dialogs, rendering, input, accessibility,
printing, and OS integration. No Windows UI type crosses into the engine, and
no Qt type crosses the engine's public boundary.

Recommended repository layout after the boundary is established:

```text
src/engine/                 # Qt-free shared C++
src/platform/linux-qt/      # Qt application and adapters
src/platform/windows/       # native Windows executable and UI
tests/engine/               # platform-neutral contract tests
tests/linux-qt/             # retained Qt UI tests
tests/windows/              # renderer, UIA, integration, and UI tests
```

Move files only as they become portable; do not perform a repository-wide
rename before the new target can compile.

## Architecture Decisions to Record First

Create short ADRs before implementation for these decisions:

1. **Windows UI stack:** Win32 owns top-level windows and the message loop;
   Direct2D 1.3 renders; DirectWrite handles text; WIC decodes images;
   DirectComposition owns the visual tree, transforms, clipping, opacity, and
   animation. The graphics baseline is `d2d1_3.h` with `ID2D1Factory3`,
   `ID2D1Device2`, and `ID2D1DeviceContext2`; later Direct2D interfaces are
   optional enhancements, never a hidden requirement. Native common dialogs
   are used for files, folders, and printing.
2. **Operating-system baseline:** because Direct2D 1.3 is a Windows 10 API
   generation, support Windows 10 and Windows 11 only. Record and enforce the
   exact minimum supported Windows 10 release, Windows SDK, and app manifest
   requirements before the native shell ships.
3. **Qt-free shared engine:** use UTF-8 `std::string`, `std::chrono` date/time
   types, standard containers, and a typed result/error model at public
   boundaries. SQLite uses the SQLite C API rather than Qt SQL. Select and pin
   one portable JSON implementation after a license and footprint review.
4. **Retained view model:** one lightweight semantic view tree drives layout,
   hit testing, focus traversal, painting invalidation, and UI Automation peers.
   It is not a general-purpose toolkit; it implements only ClassMngr controls.
5. **Text entry:** use native edit controls initially where practical. Custom
   editors must integrate Text Services Framework/IME, selection, clipboard,
   undo, and accessibility before replacing native editors. Korean input is a
   release gate.
6. **Compatibility:** `.tps` schema, migrations, resource-pack manifest,
   update metadata/signatures, and user-visible export formats remain shared
   contracts. Platform-native implementations must pass the same fixtures.
7. **Parallel operation:** ship the Qt Windows build until the native target
   reaches the cutover gates. Give the native executable a separate internal
   target name and isolated settings namespace during development.

## Implementation Phases

### Phase 0 — Freeze Behavior and Establish Port Contracts

1. Inventory every screen, dialog, menu command, shortcut, drag/drop path,
   import/export operation, background job, and platform-specific behavior.
   Build the inventory from `PageManager`, `ActionRegistry`, `MenuBuilder`, the
   feature `ui` folders, and the existing test registrations.
2. Capture golden screenshots at representative DPI values in light and dark
   themes. Record keyboard-only flows, English/Korean IME behavior, screen
   reader labels, print/PDF samples, and the unsaved-change rules.
3. Create cross-platform database fixtures covering empty, typical, large,
   legacy `.db`, current `.tps`, migration failure, and rollback cases. Verify
   byte-level or semantic round trips as appropriate.
4. Define a parity matrix for Windows x64 and ARM64. Each feature records data
   read/write parity, input/accessibility, visual state, error behavior,
   printing/export, and performance status.
5. Set explicit budgets for startup, steady-state memory, resize latency,
   scrolling, first paint, and device recovery based on the current release.

**Exit gate:** the inventory and fixtures identify all user-visible behavior,
and the current Linux and Windows Qt builds pass unchanged.

### Phase 1 — Split the Build Without Changing Products

1. Add `ClassMngrEngine` and narrow adapter targets to CMake. Initially, the
   engine may contain only a small proven slice; it must never link a Qt target.
2. Keep the existing Qt executable as `ClassMngrLinuxQt` conceptually, while
   preserving its public output name. Add a guarded `ClassMngrWindowsNative`
   target only on Windows.
3. Require a Windows SDK that supplies `d2d1_3.h` and link the Direct2D 1.3
   import library in native Windows builds. Compile a small `Factory3` /
   `Device2` / `DeviceContext2` capability check in CI so an older SDK cannot
   silently lower the renderer baseline.
4. Separate dependencies by layer. Qt linkage belongs only to the Linux Qt
   presentation/adapters and legacy transition targets. Windows links the
   required SDK libraries explicitly, including Direct2D, DirectWrite, D3D11,
   DXGI, DirectComposition, WIC/COM, UI Automation, and Windows security APIs.
5. Add presets for native Windows x64/ARM64 debug and release builds that do
   not require a Qt prefix. Keep the current presets until cutover.
6. Make resource and translation generation callable without `rcc`, QML
   tooling, or Qt Linguist on the native target. During transition, generate
   both the Qt resources and native resource manifests from the same inputs.

**Exit gate:** CI builds the current Linux Qt app and a minimal native Windows
shell from the same tree; the native target has no Qt dependency.

### Phase 2 — Extract the Portable Engine

Extract in vertical slices rather than mechanically converting all Qt types.

1. Move domain structs, enums, validation, formatting-independent rules, class
   transfers, scheduling rules, calendar recurrence, and speaking-evaluation
   calculations to standard C++ types.
2. Introduce explicit engine use cases such as `OpenDatabase`, `SaveClass`,
   `ImportSchedule`, `LoadCalendarRange`, and `GenerateReportModel`. UIs call
   use cases rather than repositories directly.
3. Replace Qt SQL in the portable data layer with SQLite prepared statements,
   transactions, migrations, busy handling, and typed row mapping. Preserve
   current foreign-key and rollback guarantees.
4. Extract interfaces for filesystem paths, preferences, networking, secure
   signature verification, process launch, clock, logging, resource lookup,
   and cancellation. Supply Qt adapters for Linux and Windows adapters for the
   native app.
5. Convert resource-pack logic to operate on filesystem/byte-stream interfaces.
   Preserve signature policy and mounted-pack precedence without exposing RCC.
6. Separate report *content and layout decisions* from drawing. The engine
   produces page descriptions, table geometry, styled text runs, and asset
   references; Qt, Direct2D, and later macOS renderers consume them.
7. Run the existing Qt application through conversion adapters after every
   slice. Do not duplicate validation or SQL permanently in the Windows UI.

**Exit gate:** the engine can open/migrate a database, execute representative
CRUD/import operations, and produce report models in headless tests with no Qt
loaded. The Linux Qt app uses that engine for the migrated slices.

### Phase 3 — Build the Windows Graphics and Windowing Foundation

1. Create an RAII COM and graphics-device layer:

   - per-process Direct3D 11 device with BGRA support;
   - DXGI device/factory and composition-compatible swap chains or surfaces;
   - Direct2D 1.3 `ID2D1Factory3`/`ID2D1Device2`/`ID2D1DeviceContext2`;
   - DirectWrite factory and text-format cache;
   - WIC imaging factory;
   - DirectComposition device, target, root visual, and child visuals.

2. Implement hardware rendering with WARP fallback. Treat device removal,
   display changes, suspend/resume, and graphics-driver reset as normal
   recoverable states. Recreate device-dependent resources without discarding
   application state.
3. Make every top-level window Per-Monitor DPI Aware V2. Store geometry in
   device-independent pixels, snap strokes intentionally, and recreate
   resolution-dependent resources on DPI transitions.
4. Implement frame scheduling and invalidation. Render only dirty views,
   coalesce layout work, stop presenting when occluded/minimized, and commit
   DirectComposition changes once per frame.
5. Add diagnostic overlays and counters for redraw regions, surface count,
   GPU/CPU frame time, device recreation, decoded-image memory, and dropped
   frames. Keep diagnostics disabled in normal use.
6. Add deterministic renderer tests using WIC bitmap targets for shapes, text,
   clipping, gradients, icons, tables, themes, and DPI scaling. Use tolerant
   pixel comparison rather than exact GPU output matching.

**Exit gate:** a resizable, DPI-correct, themeable test window renders and
animates through Direct2D 1.3/DirectComposition, survives device loss, and passes
x64/ARM64 smoke tests.

### Phase 4 — Implement the Semantic Control, Input, and Accessibility Layer

1. Implement only the primitives required by the feature inventory: stacks,
   grids, scroll containers, split/navigation panes, text, images, buttons,
   toggles, menus, tabs, cards, lists, tables, charts, progress, and overlays.
2. Build a single focus and input router for pointer, mouse, wheel, keyboard,
   accelerators, touch, pen, drag/drop, capture, hover, and context menus.
3. Use DirectWrite layout for read-only text. For editable text, begin with
   HWND edit controls positioned over composed content or a proven TSF-backed
   editor. Do not ship a custom text editor lacking IME composition, Unicode
   grapheme navigation, selection, clipboard, undo/redo, and password rules.
4. Map the semantic view tree to UI Automation providers with names, roles,
   state, value/range/table patterns, relationships, focus events, and live
   regions. Test with Narrator, Accessibility Insights, high contrast, and
   keyboard-only navigation.
5. Respect system metrics for reduced motion, contrast, text scaling, cursor
   size, locale, input language, and double-click timing. Preserve the existing
   English/Korean UI language switch without replacing the active IME.
6. Add layout and behavior tests that do not depend on pixels: measurement,
   hit testing, focus order, commands, selection, scrolling, validation, and
   accessibility properties.

**Exit gate:** a control gallery passes keyboard, Korean IME, Narrator, high
contrast, 100–300% DPI, touch, and automated semantic tests.

### Phase 5 — Port the Shell and First Read-Only Feature Slice

1. Port startup, settings, splash/progress, main window, menu/accelerators,
   sidebar, navigation history, theme selection, language selection, update
   notifications, and database open/create flow.
2. Mirror `PageManager`'s lazy construction semantics in a Windows navigation
   coordinator. Preserve page state after first creation and do not construct
   pages merely to query navigation state.
3. Port one representative read-only vertical slice first, preferably campus
   or staff directory. It exercises database queries, images, scroll/layout,
   details, navigation, localization, and resource loading without editable
   table risk.
4. Add native file/folder pickers, message/task dialogs, clipboard, URL launch,
   recent-file behavior, and error presentation behind engine interfaces.
5. Run side-by-side workflow and screenshot comparisons with the Qt release;
   match behavior and information hierarchy while adopting native Windows
   focus and accessibility conventions.

**Exit gate:** users can launch, create/open a database, navigate the native
shell, and complete the chosen read-only feature with parity.

### Phase 6 — Port Data-Entry Features by Risk

Port each slice end to end, including loading, editing, validation, autosave,
undo/redo, conflicts, empty/error states, and accessibility. Suggested order:

1. personal details and teacher directories;
2. class details and notes;
3. calendar viewing/editing and preferences;
4. rosters and student transfers;
5. schedules, imports, testing classes, and assignment dialogs;
6. speaking-evaluation grid, analytics, AI-comment workflow, and batch flows;
7. substitute-preparation and bundled-document workflows.

For virtualized tables, retain only visible row/cell visuals, expose table/grid
UIA patterns, support selection and keyboard editing, and keep model mutation
in engine use cases. Exercise large datasets, rapid navigation, and unsaved
changes at every slice gate.

**Exit gate:** the parity matrix is complete for all interactive features and
the native app can safely edit databases also used by Linux Qt.

### Phase 7 — Replace Qt-Dependent Media, Output, and OS Services

1. Use WIC for images and thumbnails with bounded decode dimensions and color
   profile/orientation handling.
2. Use a separately reviewed PDF backend: prefer Windows' PDF APIs when they
   meet rendering, search, zoom, and print requirements; otherwise adopt a
   pinned, security-maintained PDF library. Direct2D itself is not a PDF
   parser. Release document and page caches when leaving the viewer.
3. Render print/report page descriptions through Direct2D-compatible print
   paths and native print UI. Compare pagination, fonts, tables, Korean text,
   images, and margins to committed PDF fixtures.
4. Port spreadsheet parsing, document packaging/ZIP, and file exports to the
   portable engine or Windows adapters. Preserve atomic writes and cleanup of
   temporary workspaces.
5. Retain the current Windows PowerPoint workflow initially behind the engine
   automation interface. Harden process cancellation, timeout, temporary-file
   cleanup, and user-facing privacy/permission notices.
6. Reimplement update networking and signature verification with Windows
   services while preserving manifest/signature compatibility and rollback.
   Keep the installer/update handoff outside the rendering layer.

**Exit gate:** PDF viewing, printing, all report/export paths, resource-pack
updates, application updates, and PowerPoint automation pass golden fixtures
and failure-path tests without Qt.

### Phase 8 — Harden, Package, and Cut Over

1. Run unit, integration, UI Automation, render-golden, database compatibility,
   import/export, updater, installer, and long-session soak tests on x64 and
   ARM64 hardware. Include GPU device loss and WARP fallback.
2. Profile cold/warm startup, first navigation, resize, table scrolling, large
   PDF use, report generation, working set, committed memory, GPU memory, and
   handles. Investigate regressions against Phase 0 budgets.
3. Validate Windows signing, SmartScreen reputation workflow, Inno Setup
   upgrade/uninstall, file associations, per-user data paths, crash recovery,
   and updates from the last Qt release.
4. Produce a release candidate that can be installed beside an isolated Qt
   build but operates on copied databases only. Complete structured beta and
   rollback exercises before allowing production data.
5. Switch the public `ClassMngr.exe` and installer to the native target. Remove
   Windows `windeployqt`, QML deployment, Qt DLL/plugin licenses, and Windows
   Qt preset requirements only after two release candidates meet every gate.
6. Keep Linux Qt code, resources, deployment, and tests intact. Delete Windows
   Qt-only branches only after the native release has a supported rollback
   path and no shared Linux consumer.

## Test Strategy

- **Engine contracts:** domain rules, repositories, migrations, transactions,
  imports, resource manifests, signatures, updates, and report layout.
- **Cross-platform compatibility:** Windows writes/Linux reads and Linux
  writes/Windows reads for each supported database version and export format.
- **Rendering:** offscreen WIC goldens plus on-device DirectComposition smoke
  tests at multiple DPI/theme settings.
- **Interaction:** semantic view tests, Windows UI Automation end-to-end tests,
  keyboard-only flows, Korean and English IME, touch, and drag/drop.
- **Reliability:** device removal, display hot-plug, sleep/resume, low memory,
  denied files, corrupt databases/resources, network loss, cancellation, and
  forced update/report failures.
- **Performance:** representative small and large profiles with explicit frame,
  memory, startup, and output-generation budgets.

## Principal Risks and Mitigations

- **Accidentally building a general-purpose UI toolkit:** restrict primitives
  to inventoried ClassMngr needs and deliver vertical product slices early.
- **IME or accessibility regression in custom controls:** use native editors
  first, design UIA semantics with the view tree, and make Korean/Narrator tests
  phase gates rather than end-stage polish.
- **Engine extraction destabilizes Linux:** migrate one use case at a time,
  retain Qt adapters, and require Linux regression tests on every engine change.
- **Database divergence:** one schema/migration implementation and shared
  fixtures; platform UIs never issue ad hoc SQL.
- **GPU/device lifecycle bugs:** centralize device ownership, distinguish
  device-independent resources, support WARP, and continuously test recovery.
- **Output parity takes longer than screen parity:** define renderer-neutral
  report models early and port printing/PDF before declaring feature complete.
- **Two active Windows products confuse settings or data:** use distinct app
  IDs/settings paths during development and test only on copied production data.

## Completion Criteria

The Windows port is complete when:

- the release executable and installer contain no Qt runtime or Qt-generated
  QML/resource dependency;
- all parity-matrix workflows pass on x64 and ARM64, including Korean input,
  keyboard navigation, Narrator, high contrast, printing, PDF, PowerPoint,
  updates, and resource packs;
- `.tps` databases and supported exports round-trip with the Linux Qt build;
- Direct2D 1.3/DirectComposition device loss and WARP fallback are reliable;
- startup, interaction, memory, and output performance meet the recorded
  budgets; and
- the Linux Qt build and its release workflow remain supported and green.
