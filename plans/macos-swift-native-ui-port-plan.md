# macOS Swift Native UI Port Plan

## Objective

Replace the macOS Qt application with a native Swift application while
preserving ClassMngr behavior, `.tps` database compatibility, English/Korean
input, Intel and Apple silicon distribution, document/report workflows,
PowerPoint automation, updates, and the current macOS 13-or-newer baseline
unless an explicit product decision raises it. Linux continues to use Qt.

For macOS, the native frameworks are SwiftUI and AppKit; UIKit is the iOS
framework. This plan interprets “Swift and its UI kit” as SwiftUI for the app
structure and ordinary screens, with AppKit bridges for desktop controls and
services that SwiftUI cannot express with sufficient fidelity. It does not use
Mac Catalyst.

## Current-State Findings

- The present macOS application is the same C++23/Qt runtime as Windows and
  Linux. Qt types and services occur throughout core, data, domain, feature,
  and presentation code, so replacing only widgets would leave a Qt-dependent
  macOS bundle.
- The product includes complex desktop surfaces that need more than basic
  SwiftUI forms: editable/virtualized tables, schedules, rosters, speaking
  evaluations, PDF viewing/printing, report generation, imports, resource
  packs, menus and shortcuts, and multi-step unsaved-change flows.
- macOS already has platform-specific behavior for menus/actions, file dialogs,
  printing, update installation, application bundle resource paths, font
  handling, and PowerPoint AppleScript automation.
- The current release is a universal `arm64;x86_64` app targeting macOS 13,
  packaged in a versioned DMG and ad-hoc signed by the local installer target.
  The final native release needs a production signing/notarization workflow.

## Target Architecture

The Swift app shares business rules and persistence with Windows and Linux
through a Qt-free C++ engine. It does not rewrite the database and validation
logic in Swift.

```text
ClassMngrEngine (portable C++23, no Qt/Foundation/UI types)
    |-- domain models, validation, rules, use cases
    |-- SQLite repositories and schema migrations
    |-- import/export and renderer-neutral report models
    |-- platform-service interfaces
    |
    +-- ClassMngrLinuxQt (existing Qt presentation and adapters)
    |
    +-- ClassMngrMacBridge (Objective-C++/Foundation facade)
            |
            +-- ClassMngrMac (Swift)
                    |-- SwiftUI application, scenes, navigation, forms
                    |-- AppKit table/text/menu/window/print bridges
                    |-- PDFKit/Core Graphics/ImageIO services
                    +-- Swift async state and feature view models
```

Use a narrow Objective-C++ facade rather than exposing the entire C++ object
graph directly to Swift. The facade owns opaque engine handles, converts
standard C++ values to immutable Foundation DTOs, maps failures to `NSError`,
and exposes cancellable asynchronous operations. Foundation types stop at the
bridge; SwiftUI types never enter it.

Recommended repository layout after the boundary is established:

```text
src/engine/                    # shared Qt-free C++
src/platform/linux-qt/         # retained Qt app/adapters
platform/macos/ClassMngrMac/   # Swift/SwiftUI/AppKit sources and assets
platform/macos/ClassMngrBridge/# Objective-C++ facade
tests/engine/                  # shared behavior and database fixtures
platform/macos/*Tests/         # Swift unit, snapshot, integration, and UI tests
```

Move current files incrementally. Keep the existing Qt macOS target available
until the native app satisfies the cutover criteria.

## Architecture Decisions to Record First

1. **UI framework split:** SwiftUI owns app lifecycle, navigation, settings,
   forms, detail views, alerts, and ordinary lists. AppKit owns or wraps
   high-density editable tables, precise text editing, menus/commands,
   advanced window behavior, printing, and any control whose SwiftUI version
   cannot meet macOS 13 parity.
2. **Engine boundary:** a Qt-free C++23 engine is the sole implementation of
   product rules, migrations, and database writes. Swift view models call an
   Objective-C++ facade; they do not issue SQL.
3. **Bridge ownership:** bridge objects define explicit lifetime, copying,
   nullability, threading, cancellation, and error rules. Returned DTOs are
   immutable snapshots; edits are submitted as commands with revision/conflict
   tokens rather than shared mutable C++ references.
4. **Concurrency:** one Swift actor or serialized engine session owns each open
   database. Long operations run off the main actor, publish progress through
   `AsyncSequence` or equivalent callbacks, and marshal UI state to the main
   actor. Cancellation propagates into engine operations.
5. **Compatibility:** SQLite schema/migrations, `.tps` semantics, resource-pack
   manifests/signatures, updates, and export formats are shared contracts. Use
   SQLite directly; do not migrate production data to SwiftData/Core Data.
6. **Deployment baseline:** preserve universal Apple silicon/Intel and macOS 13
   initially. Any move to a newer SwiftUI API requires an availability wrapper
   or a recorded decision to raise the minimum OS.
7. **Parallel operation:** the native app uses a distinct bundle identifier,
   settings suite, cache directory, and internal display name during the port.
   It opens only copied databases until compatibility gates pass.

## Implementation Phases

### Phase 0 — Freeze Behavior and Define Parity

1. Inventory screens, sheets, dialogs, menus, commands, shortcuts, drag/drop,
   services, imports/exports, print paths, background work, update states, and
   PowerPoint permission/failure paths from `PageManager`, `ActionRegistry`,
   `MenuBuilder`, and the feature UI directories.
2. Capture reference screenshots in light/dark mode and representative window
   sizes. Record VoiceOver labels/order, Full Keyboard Access, English/Korean
   IME behavior, menu placement, undo/redo, selection, and unsaved-change flows.
3. Create shared fixtures for empty, typical, large, legacy `.db`, current
   `.tps`, migrations, corrupt data, and rollback. Capture report PDFs and
   exports with English and Korean content.
4. Define a feature parity matrix and measurable budgets for launch, navigation,
   scrolling, large-data editing, report generation, working set, and energy.
5. Decide which visuals should remain product-identical and which should become
   idiomatic macOS behavior. Information, validation, and workflow semantics
   remain identical even when control appearance changes.

**Exit gate:** the current Qt macOS and Linux builds pass, and all required
native behaviors and compatibility fixtures are documented.

### Phase 1 — Establish Build and Project Coexistence

1. Add a Qt-free `ClassMngrEngine` CMake target and an Objective-C++ bridge
   target with a small versioned public interface. Initially expose only a
   health/version call to prove the toolchain and ownership model.
2. Create an Xcode project/workspace for `ClassMngrMac`, unit tests, snapshot
   tests, and UI tests. Prefer Xcode as the app/signing/asset-catalog authority;
   keep CMake as the portable engine authority.
3. Define a reproducible engine integration: build universal static artifacts
   or an XCFramework from the same engine sources, headers, compiler settings,
   and pinned dependencies used by CI. Prevent checked-in stale binaries from
   becoming the source of truth.
4. Add debug and release schemes for `arm64` and `x86_64`, with a universal
   archive job. Add CI that builds the Linux Qt target and both macOS targets
   during the transition.
5. Give native resources their own asset catalogs and bundle manifests while
   retaining shared source assets. Add a generated resource index so missing
   files fail the build rather than failing at runtime.

**Exit gate:** a signed-for-development Swift app launches on both architectures,
calls the engine through the bridge, and coexists with the Qt app.

### Phase 2 — Extract the Qt-Free Shared Engine

This phase should be coordinated with the Windows port; do the extraction once.

1. Convert domain data, validation, calendar/schedule rules, class transfers,
   speaking-evaluation calculations, and other presentation-free logic from Qt
   value types to standard C++ types.
2. Introduce task-oriented engine use cases rather than exposing repositories.
   Examples include database open/create, page snapshots, save commands,
   imports, calendar range loads, student transfers, and report-model creation.
3. Replace Qt SQL in shared persistence with SQLite prepared statements,
   explicit transactions, migrations, busy handling, and typed row mapping.
   Preserve foreign keys, schema versions, atomic saves, and rollback behavior.
4. Extract filesystem, preferences, network, signature verification, resource,
   process, clock, logging, and automation interfaces. Maintain Qt implementations
   for Linux and add native macOS implementations later.
5. Make report layout and document packaging platform neutral. Renderers receive
   page sizes, blocks, text styles, tables, images, and pagination decisions
   rather than Qt paint commands.
6. Add adapter tests proving that the existing Linux Qt UI can consume the new
   types and that no engine public header includes Qt or Foundation.

**Exit gate:** headless engine tests cover representative database CRUD,
migrations, imports, and report models; the Linux Qt application uses migrated
slices without regressions.

### Phase 3 — Design and Harden the Swift/Objective-C++ Boundary

1. Define a small facade organized around sessions and feature use cases, not
   one bridge class per existing C++ class. Keep ABI-sensitive templates,
   exceptions, and ownership types private to Objective-C++.
2. Convert identifiers and revisions to fixed-width scalar types; text to
   `NSString`; dates to an explicitly documented calendar/time-zone form;
   collections to typed immutable DTO arrays; binary assets to URLs or `NSData`
   based on size and lifetime.
3. Catch all C++ exceptions inside the bridge. Return typed errors with stable
   domains/codes, recovery suggestions, underlying diagnostics, and no private
   paths or user data in telemetry.
4. Specify thread safety. A database session is serialized; immutable snapshots
   may cross threads; callbacks never arrive after owner cancellation; progress
   and completion are delivered on documented queues.
5. Wrap callbacks in Swift `async throws` APIs and progress streams. Add tests
   for cancellation, deallocation during work, repeated open/close, bridge
   error mapping, Unicode normalization, large arrays, and memory leaks.
6. Add a bridge compatibility/version check so a mismatched engine fails fast
   with a developer-readable error.

**Exit gate:** Swift integration tests open and migrate fixtures, query and edit
one feature, cancel long work, and repeatedly destroy sessions without leaks or
thread sanitizer findings.

### Phase 4 — Build the Native macOS Shell and Design System

1. Implement the SwiftUI `App`, command groups, main scene, window restoration,
   navigation split view, page routing, sheets, alerts, status/progress, and
   settings. Match standard macOS placement for About, Settings, Window, Help,
   services, hide/quit, and document commands.
2. Mirror `PageManager`'s lazy page creation and state preservation through a
   navigation coordinator and feature view models. Loading state must not be
   triggered by a simple navigation-state query.
3. Create design tokens for colors, typography, spacing, corners, focus, and
   grades. Use semantic system colors/materials and Dynamic Type where possible;
   preserve ClassMngr-specific data colors only where they convey meaning.
4. Localize with String Catalogs for English and Korean. Generate an audit that
   detects missing/stale strings and preserve formatted placeholders. Test
   expansion, mixed scripts, normalization, locale dates, and right-to-left
   resilience even if RTL is not initially shipped.
5. Add native open/save panels, recent documents, clipboard, URL launch,
   drag/drop, app activation, termination with unsaved changes, and security-
   scoped URL handling if sandboxing is selected.
6. Establish accessibility identifiers and VoiceOver semantics at component
   creation time. Test Full Keyboard Access, focus rings/order, reduce motion,
   increase contrast, text size, and Voice Control.

**Exit gate:** users can launch, create/open a copied database, navigate a
localized native shell, change settings, and complete a keyboard/VoiceOver
smoke flow on macOS 13 and the newest supported macOS.

### Phase 5 — Create Reusable Native Desktop Components

1. Use ordinary SwiftUI controls for simple forms, buttons, toggles, pickers,
   lists, cards, and detail pages. Avoid custom drawing when a semantic system
   control meets the requirement.
2. Build AppKit-backed `NSViewRepresentable` components for editable tables and
   other high-density views. Likely components include:

   - virtualized `NSTableView`/`NSOutlineView` with reusable cells;
   - spreadsheet-style cell editing, selection, paste, and validation;
   - frozen/grouped headers and resizable columns;
   - schedule grid with hit testing and drag selection;
   - rich text/notes editor where native text services are required;
   - print preview or PDFKit wrappers.

3. Give each wrapper a Coordinator that translates delegate callbacks into
   explicit Swift commands. Prevent feedback loops when Swift state updates an
   AppKit view that is actively editing.
4. Use native NSText input behavior so Korean IME composition, Unicode grapheme
   navigation, marked text, spell checking, clipboard, undo, dictation, and
   accessibility work without reimplementing them.
5. Add unit tests for data sources and selection/edit transactions, snapshot
   tests for stable layouts, and UI tests for keyboard/IME/VoiceOver behavior.

**Exit gate:** a component gallery proves all table, schedule, editing, theme,
localization, and accessibility requirements needed by product screens.

### Phase 6 — Port Features in Vertical Slices

Each slice includes engine calls, bridge DTOs, Swift view model, SwiftUI/AppKit
view, loading/error/empty states, localization, accessibility, automation tests,
and database compatibility tests. Suggested order:

1. campus and staff directories as the first mostly read-only slice;
2. personal details and My Classes/My Schedule;
3. class details and notes;
4. calendar and calendar preferences;
5. rosters and student transfers;
6. schedules, testing classes, assignments, and spreadsheet imports;
7. speaking evaluations, analytics, AI-comment workflow, and batch export;
8. substitute-preparation and document catalog/PDF workflows.

Use `ObservableObject`/`@Published` on the macOS 13 baseline, with explicit
loading and error state machines. Do not let views retain engine handles or
perform business validation. For large tables, request bounded snapshots or
paged data rather than copying the entire database on every keystroke.

**Exit gate:** every item in the parity matrix is complete and the native app
can safely edit a database opened afterward by the Linux Qt application.

### Phase 7 — Replace Qt Platform Services and Output

1. Use ImageIO/Core Graphics for bounded image decode, orientation, color
   profile handling, thumbnails, and export. Register bundled fonts through
   Core Text only where licensing and visual parity require them.
2. Use PDFKit for viewing, navigation, selection/search, zoom, and printing.
   Release document/page caches on navigation away and test large-document
   memory behavior.
3. Render portable report/page models with Core Graphics/Core Text. Use
   `NSPrintOperation` and native print panels for printing. Compare pagination,
   fonts, Korean text, tables, images, and margins against committed fixtures.
4. Implement filesystem, ZIP/package, spreadsheet-import, and temporary-
   workspace adapters with atomic replacement, coordinated access, cancellation,
   and cleanup on failure.
5. Preserve PowerPoint export behind the engine automation interface. Port the
   existing AppleScript contract to a Swift/macOS service, retain Unicode
   normalization, request Automation permission at the point of use, explain
   failures, time out safely, and clean temporary files.
6. Use `URLSession` for downloads and Security/CryptoKit or a reviewed native
   crypto implementation for signature verification while keeping the current
   manifest/signature protocol and fixtures. Implement app replacement through
   a signed helper or updater mechanism with rollback; do not overwrite a
   running signed bundle directly.
7. Replace RCC packs with signed on-disk native resource packs and a generated
   bundle index. Test built-in fallback, version selection, corrupt pack
   rejection, atomic update, and old-pack cleanup.

**Exit gate:** documents, PDFs, printing, reports, imports/exports, PowerPoint,
resource packs, and application updates work without Qt and pass golden and
failure-path tests.

### Phase 8 — Sign, Notarize, Harden, and Cut Over

1. Decide App Sandbox policy from actual requirements, especially user-selected
   databases/documents, PowerPoint Apple Events, external websites, updates,
   printing, and temporary workspaces. Record required entitlements and avoid
   broad exceptions where scoped access works.
2. Add production Developer ID signing, hardened runtime, entitlements, archive,
   notarization, stapling, DMG creation, checksum generation, and Gatekeeper
   validation. Test clean-machine install, upgrade, rollback, and uninstall.
3. Run Swift/C++ unit tests, bridge integration tests, UI tests, accessibility
   audits, snapshot tests, database round trips, output goldens, update tests,
   and long-session/leak tests on Intel and Apple silicon.
4. Profile launch, navigation, table scrolling, PDF memory, report generation,
   CPU, energy, hangs, and bridge allocation. Meet the Phase 0 budgets and use
   Instruments, Thread Sanitizer, Address Sanitizer where applicable, and Main
   Thread Checker before release.
5. Conduct a beta using a distinct bundle ID and copied databases. Exercise
   upgrade from the last Qt release and downgrade/rollback behavior before
   allowing production data.
6. Switch the public bundle ID/name and DMG workflow only after two release
   candidates meet every gate. Remove Qt frameworks/plugins, `qt.conf`, QML,
   Qt translations, Qt deployment steps, and the macOS Qt preset only then.
7. Retain all Linux Qt resources, CMake targets, deployment logic, and tests.
   Delete shared-looking macOS Qt branches only after verifying they have no
   Linux consumer.

## Test Strategy

- **Shared engine:** rules, validation, migrations, transactions, imports,
  resources, updates, and renderer-neutral report models.
- **Bridge:** ABI/version, lifetime, nullability, exception/error conversion,
  Unicode, dates/time zones, cancellation, reentrancy, threading, and leaks.
- **Swift:** view-model state machines, command availability, navigation,
  validation presentation, autosave, conflicts, and undo/redo.
- **UI:** XCUITest for menus, shortcuts, windows/sheets, drag/drop, file panels,
  tables, Korean IME, Full Keyboard Access, VoiceOver identifiers, and app
  termination with unsaved work.
- **Visual/output:** SwiftUI/AppKit snapshots plus PDF/report golden comparisons
  with tolerances for platform-native typography.
- **Cross-platform:** macOS writes/Linux reads and Linux writes/macOS reads for
  databases, imports, resource metadata, and supported exports.
- **Reliability/performance:** corrupt inputs, denied permissions, Automation
  denial, network loss, cancellation, sleep/wake, memory pressure, large data,
  repeated database/PDF open-close, launch time, energy, and working set.

## Principal Risks and Mitigations

- **Duplicated product rules in Swift:** expose task-oriented engine use cases;
  Swift owns presentation state only, and compatibility tests cover every write.
- **Bridge complexity and memory bugs:** keep the facade narrow and immutable,
  document ownership/queues, catch exceptions, and run sanitizers and repeated
  lifetime tests from the first vertical slice.
- **SwiftUI macOS 13 control limitations:** use AppKit wrappers deliberately for
  dense editing rather than forcing fragile SwiftUI-only implementations.
- **Linux regressions during extraction:** migrate use cases incrementally and
  require the Linux Qt suite on every engine change.
- **PowerPoint permissions and sandbox conflicts:** prove Apple Events behavior
  in an early signed spike and keep internal PDF reports as a supported fallback.
- **Signing/updater discovered too late:** create a notarized native shell and
  exercise update handoff before most feature work is complete.
- **Universal-build dependency drift:** build both architectures from pinned
  sources in CI and verify the final binary slices and deployment target.

## Completion Criteria

The macOS port is complete when:

- the distributed app is implemented in Swift/SwiftUI/AppKit and contains no
  Qt frameworks, plugins, QML, RCC, or Qt translation dependency;
- all parity-matrix workflows pass on Apple silicon and Intel, including Korean
  IME, Full Keyboard Access, VoiceOver, printing, PDF, PowerPoint, updates, and
  resource packs;
- `.tps` databases and supported exports round-trip with the Linux Qt build;
- the Objective-C++ bridge passes lifetime, cancellation, concurrency, and
  sanitizer tests;
- the app is Developer ID signed, hardened, notarized, stapled, and validated
  through clean install and update/rollback tests;
- performance and energy meet the recorded budgets; and
- the Linux Qt build and release workflow remain supported and green.
