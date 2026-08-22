# Memory Usage Reduction — Implementation Plan

## Objective

Reduce the application's steady-state memory footprint and startup allocation
cost without changing visible behavior. Prioritize avoiding construction of
rarely used pages, eliminating duplicated calendar records, replacing
item-per-cell tables, promptly releasing PDF resources, and bounding decoded
campus image size. Provide opt-in developer diagnostics that make future
memory regressions observable and explainable without imposing meaningful
overhead on normal users.

## Implementation Progress

### Completed — 2026-08-22

- Added `PageManager::isPageInstantiated()` and `isCurrentPage()` as
  non-constructing lifecycle test seams.
- Refactored `PageManager` to register factories and lazy-create Calendar,
  Classes, Campus Dashboard, and PDF Viewer. Lightweight/default pages remain
  eager for this first slice.
- Stored save mode, database state, and PDF-viewer preferences in the manager
  so a deferred page receives the current state upon creation.
- Deferred Classes and Campus cross-page signal wiring through the new
  `PageManager::pageCreated()` signal. Navigation, sidebar, and document
  callers now ensure a heavy page only when performing an operation on it.
- Added `tests/pagemanager_tests.cpp` to verify deferred construction, safe
  manager-wide lifecycle calls, and first-creation reuse.
- Validated the debug application build plus focused PageManager, Classes,
  and Campus Dashboard tests.
- Refactored `ClassesPage` to create its Details, Roster, Analytics,
  Evaluations, and Notes editors only when their section is first opened.
  Existing editors retain their state; loading and lifecycle actions now skip
  sections that have not been created.
- Added focused Classes-page coverage for deferred nested-editor construction
  and re-ran the debug build plus Classes and PageManager tests.
- Replaced `CalendarEventCache` date buckets of copied event payloads with
  sorted event-ID indexes backed by a single canonical event map. Range
  queries now deduplicate multi-day events through those IDs.
- Added Calendar retention coordination for the visible month, next 30 days,
  five-month prefetch window, and next-ten-events search range. Changing the
  retained ranges prunes stale date buckets, loaded-range metadata, and
  unreferenced event payloads.
- Added cache coverage for multi-day-event deduplication and eviction-safe
  asynchronous loading. Rebuilt the debug application and passed the focused
  Calendar, Classes, and PageManager test suite.
- Replaced the Class Analytics `QTableWidget` ranking grid with
  `ClassAnalyticsRankingModel` and `QTableView`. The existing delegate and
  grouped header retain grade badges, attention highlighting, alignment,
  sizing, selection, and translations without per-cell item allocations.
- Added model tests for the ten columns, display/grade/attention roles,
  headers, and reset behavior. Rebuilt the debug application and passed the
  focused Analytics, Calendar, Classes, and PageManager tests.
- Added idempotent `PdfViewerPage::releaseDocument()` behavior and a single
  PageManager transition hook that invokes it when leaving PDF Viewer. The
  viewer now detaches/closes its document, clears document-dependent state,
  resets its UI, and remains reusable for a later document.
- Added PageManager coverage using a real PDF fixture: load, verify output
  actions, navigate away, verify the document/actions are released, and
  return to the same viewer instance. Focused regressions pass.
- Replaced direct `QPixmap(path)` map loading with `QImageReader`, automatic
  orientation handling, and a 1,520-pixel decoded-source bounding box (the
  narrow-layout breakpoint with a 2× high-DPI allowance). Display layout
  continues to scale from that bounded source.
- Added a high-resolution map regression that verifies the retained decoded
  source stays within the cap; the complete Campus map layout suite passes.
- Hardened the PDF leave-transition hook to resolve the viewer through the
  instantiated-page registry before releasing it. This fixes the Debug
  startup-performance access violation in `PdfViewerPage::releaseDocument()`.
- Extended startup-performance JSON with report-only Windows working-set and
  private-byte snapshots taken immediately after `MainWindow` construction;
  the startup regression verifies both values are available on Windows while
  retaining the existing timing metrics and optional thresholds.
- Recorded a comparable empty-profile, headless Debug startup baseline using
  three isolated samples of the committed pre-change revision and the current
  worktree. Average startup peak working set fell from 271.4 MiB to 215.0 MiB
  (-56.4 MiB); private bytes fell from 187.5 MiB to 142.8 MiB (-44.7 MiB).
  The current construction-point report was 211.9 MiB working set and
  139.9 MiB private bytes. These are report-only baselines, not CI limits.
- Completed the full Debug CTest sweep: all 61 tests pass, including the
  startup-performance, PageManager/PDF lifecycle, Calendar cache, Classes,
  Analytics ranking-model, and Campus map suites.
- Completed Phase 9: added a developer-only, non-activating Memory Usage
  Monitor with Windows working-set/private-usage snapshots, baseline/peak/
  copy/export controls, a capped ten-minute history, and redacted JSON export.
  The monitor records page, PDF, calendar-retention, and campus-map events
  only after a developer opens it. Focus, formatting, history, redaction, and
  Windows snapshot coverage pass alongside the focused lifecycle regressions.
- Completed Phase 10: added low-overhead, feature-owned retained-memory
  providers for the page manager, Calendar cache, Classes shell/ranking model,
  PDF Viewer, and Campus Maps. The monitor now labels their sum as a partial
  estimate and separately shows the unattributed/shared/runtime comparison;
  it also reports every registered page as uncreated, hidden, or current with
  creation/activation times. Developers can safely navigate to a page and
  release only an already-loaded PDF. Added the Windows WPR/WPA and Visual
  Studio profiling playbook in `docs/memory-profiling-windows.md`. All 62
  Debug CTest tests pass, including attribution, page-lifecycle, PDF-release,
  Calendar cache, ranking-model, Campus map, and monitor-focus coverage.
- Completed Phase 11: extended the developer monitor with an application
  health summary (build/runtime/configuration, current page, database state,
  display scale, memory policies, and active background tasks) without paths
  or user data. The timeline is now capped by both entries and bytes, records
  lifecycle/cache/PDF/timing/slow-operation context, and is included in copy
  and JSON-report workflows. Added opt-in timing for page construction and
  activation, Calendar fetch/render, PDF open/release, and Campus decode.
  Startup diagnostics now emit `classmngr-scenario-report-v1` checkpoints,
  and the profiling guide documents the matching manual scenario. All 62
  Debug CTest tests pass.

## Confirmed Current Behavior

- `PageManager::initialize()` constructs every top-level page and
  `registerPages()` immediately adds every page to the stacked widget. This
  includes Calendar, Classes, Rosters, Speaking Evaluations, Campus, and PDF
  Viewer.
- `MainWindow::connectSignals()` directly obtains several page pointers during
  startup to connect cross-page signals. A page factory alone is insufficient:
  this startup wiring must also be deferred or it will recreate the eager
  construction path.
- `ClassesPage::buildUi()` eagerly creates its Details, Roster, Analytics,
  Evaluations, and Notes editors. `loadEditors()` then loads all of them when
  a class is selected, even when only one section is visible.
- `CalendarEventCache` stores canonical `CalendarEvent` values by ID and also
  stores another complete `CalendarEvent` value for every date the event spans.
  It has no bounded-retention policy, while `CalendarPage` prefetches future
  calendar ranges and may extend its search for the next ten events.
- `ClassAnalyticsPage` creates a `QTableWidgetItem` for every ranking cell.
  Its existing ranking delegate and header consume model roles and can be
  reused with a `QAbstractTableModel`.
- The Speaking Evaluation editor already uses `SpeakingEvalModel`,
  `SpeakingEvalTableView`, and `SpeakingEvalDelegate`; it should not be
  rewritten as part of the main table migration. Its batch-dialog tables
  should be profiled separately before changing them.
- `PdfViewerPage::loadPdf()` closes the document before loading a replacement,
  and its destructor closes it at teardown. Leaving the page does neither, so
  a loaded PDF remains resident until another PDF is opened or the application
  closes.
- `CampusMapPreview::setImagePaths()` loads full-resolution `QPixmap` objects
  into each image label's persistent source pixmap, then derives display-sized
  pixmaps during layout and resize operations.

## Implementation Principles

- Preserve page state after first creation. Lazy construction is not page
  eviction except where this plan explicitly releases a resource.
- Do not make a simple state query construct a page. For example, checking
  whether Campus is current must not instantiate Campus.
- Centralize page-factory, lifecycle, and signal-registration behavior in
  `PageManager`; do not create ad-hoc lazy construction in individual
  navigation call sites.
- Keep current public calendar query behavior while changing cache internals.
- Prefer model/view storage for large tabular data. Delegates should continue
  to paint presentation-specific visual details.
- Bound image decode size before conversion to `QPixmap`; scaling after a
  full-resolution decode does not achieve the memory goal.

## Implementation Steps

### Phase 1. Establish Memory Baselines and Test Seams

1. Record startup working-set/private-bytes and per-page memory behavior for
   representative profile sizes. Capture these checkpoints:

   - after initial window construction;
   - after opening each targeted page for the first time;
   - after returning to a lightweight page;
   - after opening a large PDF and navigating away;
   - after loading campus map images.

2. Extend the existing startup-performance metric output only if it can report
   memory reliably in the supported CI/runtime environment. Keep startup timing
   metrics intact; memory thresholds should initially be report-only until a
   stable baseline exists.

3. Add narrow test seams where behavior is otherwise invisible:

   - page-instantiated checks in `PageManager`;
   - cache-size or retained-range observability suitable for calendar tests;
   - PDF-loaded state or a testable release method;
   - bounded decoded-image dimensions in `CampusMapPreview` tests.

### Phase 2. Lazy-Create Heavy Top-Level Pages

1. Refactor `PageManager` to register page factories rather than eagerly
   creating every `BasePage` in `initialize()`.

   - Add an internal `ensurePage(PageType)` that creates a page only once,
     parents it to the manager, adds it to the stack, connects its common
     `BasePage` signals, applies current manager state, and returns it.
   - Have `showPage(PageType)` call `ensurePage()` before changing the current
     widget.
   - Keep `m_pages` (or an equivalent registry) limited to instantiated pages;
     retain a separate factory registry for all page types.
   - Add `isPageInstantiated(PageType)` and `isCurrentPage(PageType)` so
     callers can check state without forcing construction.
   - Make `setSaveMode()`, `setDatabaseOpen()`, `clearDatabaseState()`,
     `refreshAll()`, and `retranslatePages()` iterate only instantiated pages.
     Newly created pages must receive the current save mode, database state,
     document preferences, and language/theme-dependent setup immediately.

2. Make the following top-level pages lazy:

   - `CalendarPage`
   - `ClassesPage`
   - `RostersPage`
   - `SpeakingEvalPage`
   - `CampusDashboardPage`
   - `PdfViewerPage`

   Lightweight/default pages may remain eager initially to constrain the
   refactor. The no-database Campus fallback may create Campus when it is
   actually shown; that is still a first-open creation rather than startup
   preallocation.

3. Refactor `MainWindow` and `NavigationController` cross-page wiring.

   - Add a `PageManager::pageCreated(PageType, BasePage*)` signal or a
     dedicated one-time registration callback.
   - Move page-specific signal connections to that creation path. In
     particular, class-saved notifications, schedule-display-mode propagation,
     Campus section synchronization, and PDF document preferences must work
     whether the consumer page is created before or after the producer.
   - Store shared state such as schedule display mode in `PageManager` or a
     preference service, then apply it when a dependent page is first created.
   - Replace pointer comparisons such as
     `currentWidget() == campusDashboard()` with `isCurrentPage()` where they
     are only checking navigation state.

4. Update navigation call sites to obtain an ensured page only immediately
   before invoking an operation on it. For PDF documents, ensure the page,
   load the descriptor, and then show it. For pages that need data loaded
   before display, preserve the current load-before-show behavior after
   creation.

### Phase 3. Lazy-Create Heavy Editors Inside Classes

1. Keep the lightweight Classes shell (heading, navigation, and editor stack)
   but replace unconditional construction of all five editors with
   `ensureEditor(ClassesSection)`.

2. Create and add each editor to `m_editorStack` only when its section is
   first selected. Preserve the existing editor instance and unsaved state
   afterward; do not recreate it when switching sections.

3. Replace `loadEditors(classroom)` with logic that:

   - records the current class;
   - loads the currently active editor;
   - loads an editor on first activation or when it is known to be stale;
   - leaves uncreated sections unloaded.

4. Adjust save, discard, refresh, translation, database-clear, and output
   forwarding methods to operate only on editors that have been created.
   Preserve the existing unsaved-change confirmation behavior for the active
   editor.

5. Treat the embedded Analytics and Evaluations editors as the highest-value
   deferrals because their construction and class-load paths create charts,
   tables, and evaluation data structures.

### Phase 4. Compact and Bound Calendar Event Caching

1. Replace duplicate date storage with shared records:

   ```cpp
   QHash<int, CalendarEvent> m_eventsById;
   QHash<QDate, QList<int>> m_eventIdsByDate;
   ```

   `m_eventsById` remains the sole canonical event payload. Each date bucket
   contains IDs only.

2. Preserve `CalendarEventCache`'s public return types during the first
   migration.

   - `eventsForDate()` resolves date-bucket IDs through `m_eventsById`.
   - `eventsInRange()` gathers IDs into a set before resolving and sorting,
     preventing a multi-day event from appearing more than once.
   - Insertion/replacement removes an event's prior date memberships before
     adding its current memberships, then keeps each date bucket sorted by the
     present ordering rules.

3. Add explicit retention management.

   - Add an API such as `setRetainedRanges()` or `evictOutside()` that accepts
     the ranges the UI still needs.
   - Retain the visible month, the current next-30-days range, the configured
     forward prefetch window, and any range needed by the next-ten-events
     search.
   - After a displayed-month change or next-ten search-window update, prune
     loaded range metadata and date buckets that are no longer retained.
   - Remove a canonical event only when no retained date bucket references its
     ID. Correctly handle multi-day events crossing a retention boundary.
   - Keep generation checks and active/pending request handling intact so a
     stale worker result cannot repopulate an evicted generation.

4. Establish an explicit, documented retention policy. Start with the current
   month/next-30-days requirements plus the existing five-month prefetch, then
   tune the window using the Phase 1 measurements rather than silently
   reducing visible functionality.

### Phase 5. Replace Item-Per-Cell Analytics Storage

1. Add `ClassAnalyticsRankingModel : QAbstractTableModel`, backed directly by
   `QList<SpeakingAnalytics::StudentRank>` or the ranking portion of the
   current snapshot.

2. Implement the ten existing columns and roles:

   - `Qt::DisplayRole` for rank, English name, Korean name, and formatted
     average;
   - `AnalyticsRankingRoles::Grade` for overall and criterion grade badges;
   - `AnalyticsRankingRoles::NeedsAttention` for the existing row highlight;
   - center alignment through `Qt::TextAlignmentRole` where needed.

3. Replace `QTableWidget` with `QTableView` in `ClassAnalyticsPage`.

   - Reuse `ClassAnalyticsRankingDelegate` and
     `ClassAnalyticsRankingHeader`; both are model/view-compatible.
   - Adapt column-width calculations to use model data rather than
     `QTableWidgetItem` instances.
   - Replace row clearing/population with a model reset or precise model
     notifications.
   - Preserve row selection, fixed row height, horizontal scrolling, current
     header grouping, light/dark rendering, and visible-row minimum height.

4. Do not rewrite the main Speaking Evaluation editor: it already has the
   intended `QTableView`/model/delegate architecture. Profile
   `speaking_eval_ai_batch_dialog.cpp` separately; migrate only a table proven
   to be large enough to matter.

### Phase 6. Release PDF Resources on Navigation Away

1. Add an idempotent `PdfViewerPage::releaseDocument()` method.

   - Detach the document from `QPdfView` if required to release view caches.
   - Call `QPdfDocument::close()`.
   - Clear `m_currentFilePath` and `m_documentDescriptor`.
   - Reset page/zoom/status UI and refresh output capabilities.
   - Leave the page widget and its reusable controls alive for the next PDF.

2. Invoke this method whenever the PDF page is left. Prefer a single
   `PageManager` page-transition hook so the lifecycle is explicit and cannot
   be bypassed by a navigation path. A `hideEvent()` override is acceptable
   only if it is verified for all `QStackedWidget` transitions.

3. Keep `loadPdf()` responsible for attaching/reopening the document and for
   replacement-document cleanup. Ensure asynchronous document status signals
   cannot repopulate cleared state after release.

### Phase 7. Downscale Campus Images During Decode

1. Replace direct `QPixmap(path)` loading in `CampusMapPreview::setImagePaths()`
   with `QImageReader`.

   - Read image dimensions and orientation metadata first.
   - Apply `setAutoTransform(true)`.
   - Set a bounded decode size before reading, preserving aspect ratio.
   - Convert the resulting bounded `QImage` to `QPixmap` only after decode.

2. Define the cap from intended display needs, including a high-DPI allowance.

   - The horizontal gallery already caps displayed image height at
     `MaximumImageHeight`.
   - The cap must also cover the maximum practical narrow-layout width without
     retaining arbitrary camera-resolution originals.
   - Use one named source-size policy constant/helper rather than unrelated
     magic dimensions in loading and layout code.

3. Keep the existing aspect-ratio layout behavior. `AspectRatioImageLabel`
   may continue to produce smaller display pixmaps from its bounded source,
   but it must never retain a full-resolution original.

4. Consider loading the Campus map preview only when the Maps section is first
   shown if Phase 1 measurements show it remains material after decode
   downscaling. This is secondary to bounded decoding and must preserve map
   controls, translations, and campus-switch behavior.

### Phase 8. Regression Coverage and Rollout

1. Extend or add tests for lazy pages:

   - targeted pages are absent after initialization;
   - first navigation creates exactly one instance;
   - repeated navigation reuses it and preserves state;
   - manager-wide refresh/clear/translation calls remain safe with uncreated
     pages;
   - deferred cross-page signals and stored preferences apply correctly.

2. Extend `calendar_event_cache_tests.cpp` for:

   - date lookup and range lookup with ID indexing;
   - multi-day event deduplication;
   - replacement across overlapping ranges;
   - bounded retention/eviction;
   - stale asynchronous results after invalidation or eviction.

3. Add ranking-model tests for data, roles, headers, empty snapshots, and
   update behavior. Retain existing visual/delegate coverage where available.

4. Add PDF lifecycle coverage using a small fixture PDF: load, navigate away,
   verify released state, reopen, and verify print/export capabilities follow
   the new document only.

5. Extend `campus_map_tests.cpp` with a high-resolution temporary image and
   verify the retained source/decoded pixmap dimensions do not exceed the
   chosen cap while responsive rendering remains correct at both layout modes.

6. Run focused test targets after each phase, then the full test suite and the
   Phase 1 performance/memory scenario before merging.

### Phase 9. Add a Non-Activating Memory Monitor

Create a modeless developer diagnostic window that can remain visible while
the user works in the main application. It must not activate or take keyboard
focus from the main window, including when it is shown or clicked.

1. Implement `MemoryUsageDialog` (or similarly named diagnostic tool) as a
   modeless `QDialog`/`QWidget`, shown with `show()` rather than `exec()`.

   - Use `Qt::Tool`, `Qt::WindowStaysOnTopHint`, and
     `Qt::WindowDoesNotAcceptFocus`.
   - Set `Qt::WA_ShowWithoutActivating` before showing it. Verify the native
     Windows behavior: opening it must preserve the main window's active
     editor and keyboard target.
   - Keep it out of the normal taskbar/app-switcher where the platform honors
     tool-window semantics. Persist only its geometry; do not restore it
     automatically on every application startup.
   - Make it reachable through a developer-only menu/action and optionally a
     documented shortcut. It should not appear in the standard end-user UI
     until the information is deliberately designed for support use.

2. Add a small platform abstraction for process memory snapshots instead of
   embedding Windows calls in the dialog.

   ```cpp
   struct ProcessMemorySnapshot {
       quint64 workingSetBytes;       // resident RAM now
       quint64 peakWorkingSetBytes;
       quint64 privateUsageBytes;     // private committed memory
       quint64 pagefileUsageBytes;    // if meaningfully distinct on Windows
       quint32 handleCount;
       quint32 threadCount;
       QDateTime capturedAt;
   };
   ```

   - On Windows, use `GetProcessMemoryInfo()` with
     `PROCESS_MEMORY_COUNTERS_EX`; document the exact mapping of displayed
     labels to Windows fields.
   - Return an unavailable/empty snapshot on unsupported platforms initially,
     rather than adding fragile platform-specific guesses. The dialog must
     still work and clearly label unavailable metrics.
   - Refresh on a modest, user-controllable interval (default one second) and
     stop its timer while hidden. Snapshot collection and formatting must not
     allocate large buffers or pollute the figures being measured.

3. Make the summary immediately useful.

   - Display current and peak working set, private usage, process handles,
     thread count, and captured time in human-readable binary units.
   - Add system total/available physical memory only if it is labelled as a
     system metric rather than ClassMngr-owned memory.
   - Provide **Capture baseline**, **Reset peak**, and **Copy summary**
     actions. Baseline comparison should show absolute and percentage deltas.
   - Provide a compact rolling history (for example, 5--10 minutes of sampled
     totals) and a small graph only if it does not complicate the window. This
     makes delayed growth visible; the current value alone does not.

4. Record contextual events in the same history.

   - Automatically tag samples when a page is first instantiated, shown, or
     hidden; when a PDF is loaded/released; when calendar retention changes;
     and when campus maps are decoded or cleared.
   - Let a developer insert a named marker before reproducing a suspected
     leak, e.g. “before opening large PDF.”
   - Export the snapshots and event markers as JSON for bug reports. Do not
     export student data, document paths, database locations, or other
     personally identifying content; use page/feature identifiers and sizes
     only.

5. Test behavior rather than asserting machine-dependent byte counts.

   - Verify showing, interacting with, hiding, and reopening the tool does
     not change the main window's focus target.
   - Unit-test byte formatting, baseline-delta calculation, history capping,
     and redacted JSON export with a fake snapshot provider.
   - Add Windows-only coverage that a snapshot reports nonzero working-set and
     private-usage values for the test process, using permissive assertions.

### Phase 10. Attribute Memory by Feature, With Explicit Limits

Do not present an invented exact “RAM used by each page” total. General Qt,
C++ heap, allocator arenas, fonts, plugins, and shared data cannot be assigned
reliably to one page by the operating system. Instead, show two clearly
separate views: authoritative process totals and feature-owned, instrumented
resources.

1. Define a low-overhead reporting interface for deliberately owned retained
   data.

   ```cpp
   struct MemoryBreakdownEntry {
       QString category;              // e.g. "Calendar event cache"
       QString owner;                 // e.g. "Calendar"
       quint64 retainedBytes;
       quint64 itemCount;
       QString detail;                // e.g. retained date range
       bool isEstimated;
   };
   ```

   - Providers report their retained payload/container capacity where that is
     cheap and meaningful. They must not scan a large model or deep-copy data
     during every one-second refresh.
   - The dialog labels these rows as **attributed retained memory** and never
     claims that their sum equals process private usage.
   - Show the summed attributed value and an **unattributed/shared/runtime**
     remainder as an explanatory comparison only, not a false allocation
     accounting identity.

2. Instrument the memory-heavy components already targeted by this plan.

   | Owner | Initial diagnostics | Useful detail |
   | --- | --- | --- |
   | Page manager | instantiated page count and first-creation time | page type and current/hidden state |
   | Calendar | canonical events, date-index entries, retained ranges, pending requests | event/index counts and range boundaries |
   | Classes | instantiated editors, active class, ranking-model rows | editor lifecycle and row counts |
   | PDF Viewer | loaded/released state and source size | page count, document byte size when known |
   | Campus Maps | decoded source dimensions and retained pixmap bytes | image count and image-size cap |
   | Images/caches | cache entries, configured limit, evictions | count, byte estimate, last eviction |

3. Add providers only at ownership boundaries. Start with the preceding
   components; expand to other pages only after a measured issue identifies a
   meaningful retained resource. Avoid a global `new`/`delete` hook in the
   application: it is expensive, difficult to attribute correctly, and can
   alter the memory behavior under investigation.

4. Add a **page lifecycle** view in the monitor.

   - For every registered page, display whether it is uncreated, instantiated
     and hidden, or current; record creation time and last activation.
   - Add safe developer actions to navigate to a page and release resources
     only where an existing, explicit release API permits it (currently PDF).
     Do not add generic page destruction or cache-clearing actions: they can
     invalidate unsaved state and would make a diagnostic window unsafe.

5. Treat native allocation tracing as a separate opt-in investigation path.

   - Document a repeatable Windows Performance Recorder / Windows Performance
     Analyzer or Visual Studio memory-profiling workflow for cases where the
     monitor shows persistent unexplained growth.
   - Include the diagnostic JSON export, build revision, and reproduction
     steps with that trace. The in-app monitor identifies *when* and which
     owned resources grew; a native heap trace determines allocator call
     stacks when attribution is insufficient.

### Phase 11. Add Complementary Developer Health Diagnostics

Use the same developer-tools surface for small, cheap observability features
that help distinguish a memory problem from an I/O, lifecycle, or background
work problem.

1. Add an application diagnostics summary containing:

   - application version, Git revision/build timestamp, Qt version, OS, CPU
     architecture, and enabled build flags;
   - current page and page-lifecycle states;
   - database-open state without exposing its path or contents;
   - active background task count, task names/categories, start time, and
     cancellation state; and
   - current language, theme, display scale, and relevant memory policy
     settings (calendar retention and image decode cap).

2. Add a structured, bounded developer event log.

   - Capture lifecycle events, failed asynchronous requests, cache evictions,
     PDF load/release outcomes, and slow operation markers.
   - Keep a fixed maximum entry/byte limit and redact user data. Provide
     copying/export alongside the memory timeline so a support report has the
     context necessary to interpret a spike.

3. Add timing instrumentation around known expensive transitions.

   - Measure first page construction, page activation, calendar fetch/render,
     PDF open/release, and campus image decode. Report elapsed time and an
     optional “slow operation” threshold.
   - Reuse a monotonic clock and emit events only in developer builds or while
     diagnostics are enabled. This keeps the production hot path clean.

4. Add a reproducible scenario runner for tests and manual profiling.

   - Describe actions such as open Classes, load a large calendar range, open
     and leave a PDF, visit Campus Maps, then return home and wait for a short
     settling interval.
   - Have the existing startup-performance JSON evolve into a scenario report
     containing elapsed time and memory checkpoints. Store results as CI
     artifacts or local developer output, not fixed global limits at first.
   - Once multiple controlled baselines are collected, add broad regression
     guardrails that fail only on sustained, material increases; keep them
     opt-in or informational on heterogeneous CI runners.

## Diagnostics Rollout Order

1. Define the process-snapshot interface and fake provider; add Windows
   implementation and platform-safe tests.
2. Implement the non-activating memory window with totals, baseline capture,
   and bounded history. Verify focus behavior manually on supported Windows
   versions before exposing the action.
3. Add page lifecycle, calendar, PDF, campus, and Classes attribution
   providers, each with a cheap test-visible snapshot method.
4. Add contextual event markers, redacted JSON export, and the bounded event
   log.
5. Add timing and scenario-report integration; collect several representative
   baselines before deciding on any automated budgets.
6. Publish the native-profiler playbook and expand attribution only where the
   scenario data identifies a blind spot.

## Files Expected to Change

| File | Planned change |
| --- | --- |
| `src/ui/shared/pages/pagemanager.h` | Add factory/lifecycle APIs, instantiated-page state, and deferred creation hooks. |
| `src/ui/shared/pages/pagemanager.cpp` | Replace eager registration with factories; apply state and common signal wiring on creation. |
| `src/app/mainwindow.cpp` | Defer page-specific connections and retain cross-page preferences without eagerly obtaining heavy pages. |
| `src/app/controllers/navigation_controller.cpp` | Use page-state helpers and ensure pages only immediately before use. |
| `src/features/classes/ui/classes_page.h` | Track lazily created section editors and their load state. |
| `src/features/classes/ui/classes_page.cpp` | Add `ensureEditor()`, load only active editors, and make lifecycle operations tolerate absent editors. |
| `src/features/calendar/ui/calendar_event_cache.h` | Replace duplicate date payload storage and add retention APIs/state. |
| `src/features/calendar/ui/calendar_event_cache.cpp` | Implement ID indexes, membership cleanup, range eviction, and preserved query behavior. |
| `src/features/calendar/ui/calendar_page.h` | Add retained-range coordination if needed. |
| `src/features/calendar/ui/calendar_page_events.cpp` | Inform cache retention as visible/prefetch/next-ten ranges change. |
| `src/features/classes/ui/class_analytics_ranking_model.h` | Declare the ranking table model. |
| `src/features/classes/ui/class_analytics_ranking_model.cpp` | Implement model columns and delegate roles. |
| `src/features/classes/ui/class_analytics_page.h` | Replace `QTableWidget` members/helpers with model/view equivalents. |
| `src/features/classes/ui/class_analytics_page.cpp` | Bind snapshot rankings to `QTableView` and model. |
| `src/ui/shared/pages/pdf_viewer_page.h` | Declare PDF release lifecycle behavior. |
| `src/ui/shared/pages/pdf_viewer_page.cpp` | Implement release/reset behavior and integrate it with document loading. |
| `src/features/campus/ui/campus_map_preview.h` | Define decoded-source sizing policy or test-visible diagnostics. |
| `src/features/campus/ui/campus_map_preview.cpp` | Decode images with bounded `QImageReader` dimensions. |
| New `src/diagnostics/process_memory_provider.h` | Define platform-neutral process-memory snapshot API and test fake. |
| New `src/diagnostics/windows_process_memory_provider.cpp` | Obtain Windows working set, private usage, peak, handles, and thread counts. |
| New `src/diagnostics/memory_breakdown_provider.h` | Define cheap, explicitly attributed retained-resource reporting. |
| New `src/diagnostics/developer_event_log.*` | Store bounded, redacted lifecycle, memory-marker, and slow-operation events. |
| New `src/ui/diagnostics/memory_usage_dialog.*` | Provide the non-activating live monitor, lifecycle/breakdown views, history, and export. |
| `src/app/mainwindow.*` and developer action/menu registration | Add a developer-only entry point without changing normal user UI. |
| `src/ui/shared/pages/pagemanager.*` | Surface page lifecycle snapshots and publish safe lifecycle markers. |
| Calendar, Classes, PDF, and Campus ownership boundaries | Implement cheap component-specific breakdown providers and contextual events. |
| Startup/scenario performance test harness | Emit repeatable memory/timing checkpoints as JSON. |
| `tests/calendar_event_cache_tests.cpp` | Cover shared records and eviction. |
| `tests/classes_page_tests.cpp` and/or new page-manager test target | Cover lazy top-level and nested page construction. |
| Analytics model/widget test target | Cover ranking model behavior and presentation roles. |
| PDF viewer test target | Cover document release after leaving the page. |
| `tests/campus_map_tests.cpp` | Cover bounded high-resolution image decoding. |
| New diagnostics test target | Cover snapshot formatting, baseline deltas, history caps, redaction, and fake breakdown providers. |
| Relevant CMake test registration | Register any new test target. |

## Recommended Pull-Request Sequence

1. Add measurement/test seams and characterization coverage.
2. Introduce top-level `PageManager` factories with deferred signal wiring.
3. Defer internal Classes editors and their data loading.
4. Compact calendar storage, then add eviction once compatibility tests pass.
5. Migrate the Class Analytics ranking table to model/view.
6. Release the PDF document on page exit.
7. Bound campus image decoding.
8. Run full regression, compare memory measurements, and tune only the
   documented retention/image-size policies if the data justifies it.
9. Add the process-level, non-activating memory monitor and focus tests.
10. Add feature-attribution providers, event/timing diagnostics, scenario
    reports, and the native-profiler investigation playbook.

## Acceptance Criteria

- Startup does not construct Calendar, Classes, Rosters, Speaking Evaluations,
  Campus, or PDF Viewer unless that page is the initial page actually shown.
- First navigation to each lazy page constructs it once; later navigation
  reuses it without breaking data loading, navigation, saved state, actions,
  translations, or cross-page signal behavior.
- Classes does not construct or load inactive heavy editors until their
  sections are selected.
- Calendar stores one `CalendarEvent` payload per event ID, date buckets store
  IDs only, and cache memory remains bounded as users navigate across months.
- The analytics ranking table has no per-cell `QTableWidgetItem` allocation;
  its existing visual behavior and roles remain intact.
- Leaving PDF Viewer closes/unloads the active document and removes its
  document-dependent state; opening another document still works normally.
- Campus images are decoded at a bounded size before becoming retained
  pixmaps, while map layout and visual quality remain appropriate at supported
  window sizes and device pixel ratios.
- A developer can open a modeless memory monitor without changing the active
  main-window focus target; it shows current/peak process metrics, a bounded
  history, baseline deltas, and redacted export.
- The monitor distinguishes authoritative process totals from explicitly
  attributed retained resources, and reports the lifecycle of each page
  without claiming a misleading exact per-page allocation total.
- Diagnostics are opt-in/developer-only, bounded in memory use, avoid student
  data and file paths in their exports, and add negligible work while hidden.
- Focused and full regression tests pass, and the before/after measurements
  demonstrate reduced startup allocation and reduced retained memory after
  the targeted workflows.
