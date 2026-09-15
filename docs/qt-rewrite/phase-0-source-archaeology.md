# Qt Rewrite Phase 0 - Source Archaeology

Status: In progress
Snapshot: `75755460` (`2026-09-15`)
Scope: current `Qt-Rewrite` source tree only

This is the first static map of the application that the rewrite must preserve.
The obsolete historical documents were removed from `docs/`; old build output
is still treated as non-authoritative until it is regenerated from this tree.

## Composition root and lifecycle

`src/main.cpp` is the current composition root. The startup sequence is:

1. Parse startup-performance arguments and optionally start `StartupProfiler`.
2. Create `QApplication` and install the platform-specific file-dialog style.
3. Resolve stored language, font-size, and theme preferences.
4. Apply translations, global fonts, and the theme.
5. Initialize `ResourcePackManager`, acquire the splash pack, and show
   `SplashScreen`.
6. Construct `UpdateService`/`UpdateController` when the startup path enables
   update checks.
7. Construct `MainWindow`, which creates services, the window shell, the page
   manager, sidebar, actions, controllers, menus, and the initial file state.
8. Show the window, dismiss the splash after the minimum duration, and start
   the automatic update check.

The existing startup checkpoints are `process-start`,
`qapplication-created`, `preferences-resolved`, `locale-applied`,
`font-applied`, `theme-applied`, `resource-packs-initialized`, `splash-shown`,
`main-window-shell-created`, `services-created`, `page-manager-initialized`,
`controllers-connected`, `database-opened`, `navigation-data-loaded`,
`startup-page-created`, `startup-page-loaded`, `window-shown`,
`startup-complete`, and the settled checkpoints at 1, 5, and 30 seconds.

`src/app/mainwindow.cpp` is the second composition root. It owns or wires:

- `ApplicationServices` and feature services.
- `FileController`, `SidebarController`, `NavigationController`,
  `EditController`, `ThemeController`, `LanguageController`,
  `FontSizeController`, and an externally owned `UpdateController`.
- The `Sidebar`, `PageManager`, `ActionRegistry`, and optional memory dialog.
- Startup file opening, no-database/database-loaded state, menu rebuilding,
  and cross-page signal connections.

`PageManager` is a `QStackedWidget` with factories for 11 `PageType` values. It
registers all factories during initialization but initially creates the
`MyWorkspace` page. `MyWorkspacePage` eagerly creates My Details and My
Schedule and creates the Calendar page when the Calendar tab is first opened.
Other page factories are lazy, but instantiated pages remain owned by the
stacked widget until the application exits. The explicit
`--startup-performance-workflow` harness drives all registered routes and
records page-enter/page-leave events plus memory checkpoints. It completes on
the representative fixture; the 96-class large fixture currently fails while
Sub Prep expands its schedule-derived class-information cards.

The document path is separate from startup catalog parsing. `DocumentCatalog`
retains document metadata and localized names without retaining resolved PDF
content. `NavigationController` resolves the selected document resource and
passes it to `PdfViewerPage::loadPdf()` only after the user navigates to that
document. When the viewer page is left, `PageManager` calls
`PdfViewerPage::releaseDocument()`, which closes the active QtPdf document and
clears its descriptor. The v2 contract keeps this on-demand behavior explicit
and requires the same release boundary for replacement, suspension, and page
release. The representative Release workflow now exercises that boundary by
opening/rendering the 38-page guide, releasing it, reopening/rendering it, and
releasing it again; profiler metrics show two loads, two renders, two releases,
and zero live documents at route completion. The retained frames and trace
are under `docs/qt-rewrite/visual-baseline/release/workflow/`.

## Static inventory

Counts include `.cpp`, `.h`, `.ui`, and `.qml` files in each source area.

| Area | Files | Bytes | Primary responsibility |
| --- | ---: | ---: | --- |
| `src/core` | 58 | 276,367 | Settings, language, theme, fonts, resources, update, shared utilities |
| `src/data` | 42 | 324,142 | Database session/schema, DataService, repositories |
| `src/domain` | 43 | 108,714 | Models, value objects, rules, validation |
| `src/ui` | 115 | 629,794 | Pages, dialogs, printing, shared widgets and styles |
| `src/features` | 283 | 2,287,963 | Feature workflows and feature-owned UI/services |
| `src/app` | 34 | 245,992 | Main window, menus, controllers, feature-service facade |
| `tests` | 73 | 1,321,440 | QtTest characterization and unit coverage |

## Navigation and page inventory

### Sidebar roots

- My Workspace.
- Classes.
- Sub Prep.
- Co-Teachers (root; child nodes are database-backed class/teacher data).
- Campus Staff: Korean Teachers (root), Native English Teachers, GS Team.
- Useful Links: Dropbox, Vacation Calendar, Yearly Calendar, Training Website,
  NET Website, LMS Website, and Highlights Library.
- Campus Directory: Information, Directions, Address, Housing, and Maps.

### Page manager routes

| `PageType` | Current implementation | Loading behavior |
| --- | --- | --- |
| `MyWorkspace` | My Details, My Schedule, Calendar tabs | Workspace plus schedule are initial; Calendar is deferred until its tab is opened |
| `MyClasses` | My Classes page and nested class/evaluation navigation | Factory-created on demand |
| `Schedule` | Schedule page and schedule editor/import/output workflows | Factory-created on demand |
| `Classes` | Class selection plus Details, Roster, Analytics, Evaluations, Co-Teacher, Notes | Page is lazy; section editors are created as needed |
| `TestingClasses` | Details, Roster, Notes for testing classes | Factory-created from schedule/testing flow |
| `TeacherInfo` | Teacher details, connectivity, notes, Korean keyboard | Factory-created for selected teacher |
| `NativeEnglishTeachers` | Staff directory table | Factory-created from sidebar |
| `GsTeam` | GS team directory table | Factory-created from sidebar |
| `CampusDashboard` | Information, Directions, Address, Housing, Maps | Used for no-database state and admin management |
| `SubPrep` | Substitute preparation sections, class information, output | Factory-created on demand |
| `PdfViewer` | PDF viewing/copy/save/zoom/navigation | Viewer/session-owned; no PDF body at startup, load only on explicit request, release on close/replacement/leave |

### Nested and feature-owned pages

- Setup wizard: Resources, Teacher Import, Schedule Import, Personal Details,
  Teacher Entry, Class Details, Class Times, and Completion.
- Class sections: Details, Roster, Analytics, Evaluations, Co-Teacher, and
  Notes. Evaluations own speaking-evaluation tabs and report dialogs.
- Testing Classes: Details, Roster, and Notes.
- Campus: Information, Directions, Address, Housing, and Maps.
- Workspace tabs: My Details, My Schedule, and Calendar.
- Schedule dialogs: editor, import, review/reconcile, testing assignment, and
  print/output.
- Roster workflows: editor, transfer menu, import scores, and print/template
  output.
- Speaking evaluation workflows: table editing, notes, report dialog, AI batch
  prompt/review, batch export, and PowerPoint/PDF output.

## Menu and action inventory

`MenuBuilder` currently creates these menus:

| Menu | Actions |
| --- | --- |
| File | New Teacher Profile, Open, Recent Files, Save, Save As, Export As, Close, Exit |
| Edit | Undo, Redo, Cut, Copy, Paste, Preferences |
| Classes | New Class, Delete Class, Import Classes, Export Classes |
| Teachers | New Teacher, Delete Teacher, Upcoming Birthdays, Import Teachers |
| Print / Export | Print current page, Save current page as |
| Help | Check for Updates, About |
| Admin (admin mode) | Manage Campuses |
| Developer (developer-enabled build) | Memory Usage Monitor |

`ActionRegistry` also owns option actions for save mode, theme, language, font
size, PDF page spacing, PDF viewer background, AI comment provider, AI comment
voice, sidebar tooltips, sidebar marquee, automatic update checks, and the
macOS PowerPoint data-access notice.

Global shortcuts are the Qt standard New/Open/Save/Save As, Undo/Redo,
Cut/Copy/Paste sequences plus `Ctrl+Shift+M` for the developer memory monitor.
Roster and speaking-evaluation tables add local Delete, Undo, Redo, `Ctrl+D`,
and clipboard handling. These table-local shortcuts must be captured in the
visual/behavioral baseline as well as the action inventory.

## Context menus and permission paths

Static context-menu sites are currently present in the sidebar, teacher
details, roster editor/transfer, and speaking/class editing surfaces. The
important permission gates are:

- Database-backed actions are disabled in the no-database state.
- Campus-management actions and editable campus controls require admin mode.
- Developer tools are enabled in debug builds or through
  `CLASSMNGR_ENABLE_DEVELOPER_TOOLS` in non-debug builds.
- Roster transfer actions are disabled for invalid/full destinations.
- Import review actions remain disabled until validation/reconciliation is
  complete.
- Output actions are enabled from `PageOutputCapabilities` of the current page.

The next Phase 0 pass will turn these static findings into automated action
state snapshots and screenshots for English/Korean and light/dark themes.

## Architectural seams and compatibility surfaces

The rewrite must account for these current seams before moving ownership:

- `ApplicationServices` and feature services still expose `DataService`-backed
  compatibility behavior.
- `DataService` remains a broad facade above repositories and database session
  lifecycle.
- `ResourcePackManager`, `ResourcePackLease`, `ResourcePaths`, and fallback
  filesystem resolution span core, feature, and UI code.
- `PageManager` owns page factories and retains instantiated widget trees.
- Global settings, font registration, language translators, theme application,
  and update checks happen before `MainWindow` construction.
- `QQuickWidget` is embedded in Calendar and must preserve its injected QML
  context and platform behavior.
- PDF, image, font, document-catalog, roster-template, and PowerPoint paths
  cross UI, feature services, and external application boundaries.

These are inventory findings, not approvals to preserve the same design in v2.
They define the compatibility surface to be replaced or narrowed.

## Rewrite scope decision

The developer-only Memory Usage Monitor, its in-app process-memory dialog, and
its feature-attribution/lifecycle diagnostics are explicitly excluded from v2.
The startup-performance profiler is separate engineering instrumentation and
remains available for automated startup measurements.
