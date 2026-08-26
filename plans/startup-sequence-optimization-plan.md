# Cross-Platform Startup Sequence Optimization Plan

## Purpose

Redesign the ClassMngr startup sequence so that it is efficient, minimal, predictable, and shared across Windows, macOS, and Linux.

The current Windows memory behavior makes the startup inefficiency especially visible: normal startup can reach roughly 600–700 MB even though the application can later operate below approximately 150 MB. However, the underlying causes identified in the startup sequence are largely platform-independent architectural issues rather than Windows-only defects.

This plan therefore treats Windows as the **primary regression and diagnostic platform for the current memory problem**, while all startup architecture changes should apply consistently across all supported operating systems.

The fundamental goal is:

> **ClassMngr must perform only the work required to display its first usable screen. Hidden pages, hidden feature resources, duplicate rendering, and unchanged global settings must not be initialized or reapplied simply because the application started.**

Implement this plan incrementally. Do not combine all phases into one large refactor. Complete, test, and measure each phase before proceeding so the effect of each change can be identified.

---

# Primary Goals

1. Establish one shared startup lifecycle for Windows, macOS, and Linux.
2. Remove duplicate font, theme, refresh, navigation, layout, and repaint work.
3. Construct only the UI needed for the first visible screen.
4. Ensure hidden features remain uninitialized until first use.
5. Ensure only one `ScheduleWidget` is constructed and rendered during normal My Workspace/Schedule startup.
6. Replace broad startup refresh cascades with explicit initial loading.
7. Separate page construction, activation, refresh, and deactivation.
8. Reduce schedule rendering allocation and QWidget churn.
9. Defer optional startup work without merely moving the same memory spike later.
10. Keep platform-specific startup code limited to work genuinely required by the platform.
11. Measure startup time, widget creation, render activity, peak memory, and steady-state memory.
12. Add regression coverage so future features do not reintroduce eager startup behavior.

---

# Architectural Principle

The startup architecture must remain platform-independent unless a specific operating system requires different behavior.

Use platform-specific code only for areas such as:

- native memory metrics;
- native windowing/platform workarounds;
- platform-specific file dialog behavior;
- operating-system integration.

Do **not** create separate Windows/macOS/Linux implementations of:

- page construction;
- page lifecycle;
- data loading;
- schedule initialization;
- lazy feature loading;
- resource acquisition;
- startup navigation;
- startup refresh logic.

Those should remain shared.

---

# Important Constraints

- Do not use `EmptyWorkingSet()`, `SetProcessWorkingSetSize()`, `HeapCompact()`, `malloc_trim()`, or similar APIs as the production fix.
- Do not reduce a process memory number artificially without reducing the unnecessary startup work that caused it.
- Windows-specific memory trimming may be used only as a diagnostic experiment, never as the startup architecture.
- Do not eagerly preload hidden features after the first frame merely to move the memory spike later.
- Do not introduce OS-specific startup paths when the same shared code can be used.
- Do not change unrelated UI behavior or styling.
- Keep existing cross-platform behavior intact.
- Preserve current feature-scoped resource-pack behavior unless a later phase explicitly improves it.
- Prefer small, reviewable commits corresponding to one phase or one tightly related group of changes.
- Capture measurements before and after every major phase.
- Do not optimize only for final memory. Peak startup memory matters.
- Do not perform expensive work in constructors or `showEvent()` unless it is demonstrably necessary.

---

# Phase 1 — Establish a Cross-Platform Startup Baseline — Complete (2026-08-26)

## Captured Windows Representative Baseline

Three representative startup captures using the deterministic `testing-copy.tps`
profile completed successfully. Interactive birthday prompts were suppressed only
for the profiling scenario; the normal post-show startup work remained enabled.

| Run | Startup complete | Peak working set | Working set at +30 s | Private usage at +30 s | Pages | ScheduleWidgets | Schedule renders (startup → +30 s) |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 6.592 s | 734.6 MiB | 732.5 MiB | 618.2 MiB | 8 / 11 | 3 | 9 → 10 |
| 2 | 6.623 s | 734.8 MiB | 733.0 MiB | 617.9 MiB | 8 / 11 | 3 | 9 → 10 |
| 3 | 6.929 s | 734.3 MiB | 732.4 MiB | 618.5 MiB | 8 / 11 | 3 | 9 → 10 |

The reports include shared checkpoint, widget, page, ScheduleWidget, render,
and deferred-deletion metrics, plus platform-native memory samples. This
baseline confirms the duplicate schedule construction/rendering and eager-page
work targeted by Phases 3–5.

## Objective

Create startup diagnostics that measure the real production startup path on all supported operating systems.

Windows should receive the most detailed memory instrumentation because it currently exposes the largest problem, but startup timing and application-level metrics should be consistent across all platforms.

## Tasks

### 1.1 Define shared startup checkpoints

Create one shared startup profiling mechanism with checkpoints such as:

```text
01 process-start
02 qapplication-created
03 preferences-resolved
04 locale-applied
05 font-applied
06 theme-applied
07 resource-system-initialized
08 splash-shown
09 services-created
10 main-window-shell-created
11 page-manager-initialized
12 controllers-connected
13 database-opened
14 navigation-data-loaded
15 startup-page-created
16 startup-page-loaded
17 window-shown
18 startup-complete
19 settled-1s
20 settled-5s
21 settled-30s
```

The exact names may be adapted to the existing profiling code, but the sequence should clearly show where memory, widget count, and startup duration change.

### 1.2 Record shared application-level metrics

At each important checkpoint record:

- elapsed startup time;
- total `QWidget` count;
- instantiated PageManager page count;
- instantiated `ScheduleWidget` count;
- schedule render count where applicable;
- number of widgets queued through `deleteLater()` during startup where practical.

These metrics should work on all platforms.

### 1.3 Add platform-specific memory metrics behind shared profiling

Use the same profiling interface but implement native memory collection per platform.

#### Windows

Record where available:

- `WorkingSetSize`;
- `PeakWorkingSetSize`;
- `PrivateUsage`;
- `PrivateWorkingSetSize`.

#### macOS

Record the best available equivalents for:

- resident memory;
- physical/private footprint where available;
- peak resident usage if practical.

Use native APIs or existing project-compatible facilities.

#### Linux

Record the best available equivalents for:

- RSS;
- PSS where practical;
- private dirty/private resident memory where practical;
- peak resident memory if available.

Do not block implementation of the architecture on perfect cross-platform metric equivalence. The purpose is to observe trends and regressions consistently.

### 1.4 Instrument page construction

Log when each PageManager page is actually instantiated:

```text
page-created: my-workspace
page-created: schedule
page-created: sub-prep
...
```

Distinguish:

```text
registered pages
```

from:

```text
instantiated pages
```

### 1.5 Instrument schedule rendering

For every schedule render during startup, record:

- ScheduleWidget purpose/owner;
- render start;
- render end;
- table items created;
- cell widgets created;
- cell widgets removed;
- cell widgets queued through `deleteLater()`.

Keep detailed render diagnostics profiling/debug-only.

### 1.6 Define two startup profiling scenarios

#### Minimal Startup

Used to understand framework/application baseline:

- no normal recent database load;
- no automatic update check;
- minimal nonessential startup activity.

#### Representative Startup

Used for regression testing:

- same startup sequence as normal production;
- deterministic or explicitly supplied database;
- normal saved settings;
- normal initial page/tab selection;
- includes normal post-show work unless that work is removed by later phases.

The representative path must not silently disable suspected code.

### 1.7 Capture baseline runs

Run at least three representative startup captures on Windows.

Where development hardware is available, also capture macOS and Linux baselines.

Record:

- peak memory;
- startup-complete memory;
- +5 second memory;
- startup duration;
- QWidget count;
- page count;
- ScheduleWidget count;
- schedule render count.

## Acceptance Criteria

Phase 1 is complete when the profiling output can answer:

- where the largest startup allocation/residency increase occurs;
- which pages/widgets existed at that point;
- how many schedules were constructed and rendered;
- whether startup performs work after the main window is shown;
- whether Windows is showing private commitment growth, working-set residency growth, or both;
- how startup behavior differs across Windows, macOS, and Linux.

Do not begin broad architectural changes without capturing the Windows representative baseline first.

---

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

# Phase 3 — Make PageManager Demand-Driven

## Objective

Register all application pages without constructing pages that are not required for the first visible screen.

This must be shared behavior on all operating systems.

## Tasks

### 3.1 Audit eager PageManager construction

Review all pages currently created by `PageManager::initialize()` or equivalent startup paths.

At minimum, evaluate these for lazy construction:

- My Classes;
- standalone Schedule;
- Sub Prep;
- Testing Classes;
- Teacher Info;
- Native English Teachers;
- GS Team;
- Classes;
- Campus Dashboard;
- PDF Viewer.

Calendar should remain lazy.

### 3.2 Register factories without instantiating pages

Desired pattern:

```cpp
registerFactory(PageType::SubPrep, ...);
registerFactory(PageType::TeacherInfo, ...);
...
```

Do not call `ensurePage()` during startup unless that page is necessary for the selected initial view.

### 3.3 Minimize the startup page graph

If startup lands on **My Workspace → Schedule**, aim for approximately:

```text
MainWindow
├─ navigation/sidebar shell
└─ PageManager
   └─ MyWorkspacePage
      └─ visible Schedule content
```

Any hidden My Workspace subpage should also be considered for lazy child creation where practical.

### 3.4 Ensure database state works for lazy pages

A page created after startup must correctly receive:

- database-open state;
- active database/service references;
- current theme/font inherited from the application;
- current navigation context;
- any required feature state.

Do not require eager page construction simply to propagate state.

### 3.5 Verify navigation behavior

All navigation destinations should remain visible and functional before their widgets are constructed.

## Acceptance Criteria

At startup:

```text
registered pages = all required application pages
instantiated pages = only those required for initial view
```

No hidden top-level feature should be created merely because it exists in navigation.

---

# Phase 4 — Eliminate Duplicate ScheduleWidget Startup Work

## Objective

Normal startup should create and render exactly one ScheduleWidget when the initial screen is My Workspace/Schedule.

This should be true on Windows, macOS, and Linux.

## Current Problem

Multiple eager features can create their own ScheduleWidget:

```text
MyWorkspacePage
└─ ScheduleWidget

standalone SchedulePage
└─ ScheduleWidget

SubPrepPage
└─ read-only ScheduleWidget
```

Each may perform its own schedule loading/rendering.

## Tasks

### 4.1 Keep the initial My Workspace schedule only

If My Workspace/Schedule is the initial destination, create only that schedule.

### 4.2 Make standalone SchedulePage lazy

Its ScheduleWidget should not exist until the user opens that page.

### 4.3 Make SubPrepPage lazy

Do not construct:

- its large input UI;
- keyboard-related UI;
- class information UI;
- read-only ScheduleWidget;

until Sub Prep is opened.

### 4.4 Add a ScheduleWidget startup assertion/diagnostic

During representative startup, verify:

```text
ScheduleWidget instances at startup-complete = 1
```

for the normal My Workspace/Schedule startup route.

### 4.5 Consider a shared schedule data layer later

Do not share actual QWidget instances.

If profiling shows repeated database/data transformation cost after multiple schedule views are opened, consider:

```text
ScheduleDataProvider
→ immutable/shared schedule snapshot
→ multiple schedule presentations
```

This is optional and should not block Phase 4.

## Acceptance Criteria

Normal startup creates one ScheduleWidget and renders one visible schedule.

Additional schedule views are created only on first use.

---

# Phase 5 — Replace Startup Refresh Cascades With Explicit Initial Loading

## Objective

Make startup a single controlled state transition instead of a series of global refreshes and temporary page/tab changes.

## Current Problem

The current startup path can perform overlapping work resembling:

```text
construct
→ load database
→ apply database state
→ refresh all
→ open My Workspace
→ open one tab
→ later switch tab
→ show
→ showEvent refresh
→ post-show refresh all
```

## Tasks

### 5.1 Resolve the intended startup destination early

Before loading page content, resolve:

- startup database;
- initial top-level page;
- initial subpage/tab;
- required navigation selection.

A lightweight `StartupContext` or existing equivalent may be used if it simplifies the flow.

Example:

```cpp
struct StartupContext {
    QString databasePath;
    PageType initialPage;
    WorkspaceTab initialWorkspaceTab;
};
```

Do not introduce this abstraction if existing types can express the same state cleanly.

### 5.2 Simplify normal database startup

Target:

```text
resolve database
→ open database
→ establish application database-open state
→ load minimal navigation/sidebar data
→ create intended initial page
→ select intended tab
→ load visible page once
→ show window
```

Do not load one tab and immediately replace it with another before the user sees it.

### 5.3 Remove `PageManager::refreshAll()` from normal startup

Do not globally refresh every instantiated page after opening the database.

Only load:

- the visible page;
- navigation data required to display the shell.

Reserve `refreshAll()` for genuine global invalidation scenarios.

### 5.4 Remove duplicate sidebar rebuilds

Do not rebuild sidebar/navigation because of:

- font startup;
- theme startup;
- first show;
- page activation;

unless sidebar data changed.

### 5.5 Avoid duplicate visible-page refreshes

Audit startup interactions among:

- `applyDatabaseLoadedState()`;
- `showStartupDatabasePage()`;
- page `showEvent()`;
- `refresh()`;
- post-show timers.

Ensure only one path owns initial visible-page loading.

## Acceptance Criteria

Normal startup performs:

- one database open;
- one initial navigation-data load;
- one sidebar population;
- one initial page selection;
- one initial tab selection;
- one visible-page data load/render;
- no startup `refreshAll()`.

---

# Phase 6 — Define a Shared Page Lifecycle

## Objective

Make page behavior predictable across all platforms and prevent constructors or show events from becoming hidden initialization paths.

## Tasks

### 6.1 Establish lifecycle responsibilities

Use existing architecture where possible, but enforce these semantics.

#### Construction

Allowed:

- create lightweight UI structure;
- connect signals;
- initialize trivial local state;
- create lightweight models.

Avoid:

- expensive database queries;
- resource-pack acquisition;
- full dataset loading;
- expensive rendering.

#### First-use preparation

Allowed:

- acquire feature resources;
- create expensive child widgets;
- initialize feature-specific infrastructure.

#### Activation

Allowed:

- load visible data if stale;
- update content required for the current navigation context.

#### Refresh

Use only when data or relevant preferences changed.

#### Deactivation

Where beneficial:

- pause timers;
- release temporary resources;
- discard short-lived caches.

Do not destroy/recreate pages simply because the user navigates away.

### 6.2 Audit heavy `showEvent()` work

Review all page `showEvent()` implementations.

A `showEvent()` should not cause a complete data reload when activation has already loaded current data.

In particular, remove duplicate schedule refresh behavior.

### 6.3 Introduce stale/dirty state where appropriate

Examples:

```cpp
bool m_needsRefresh = true;
```

or a data-generation/version mechanism.

When hidden data changes:

```text
mark page stale
```

When the page becomes active:

```text
refresh only if stale
```

### 6.4 Keep behavior shared

Do not create Windows-only lazy behavior.

All pages should follow the same lifecycle regardless of platform.

## Acceptance Criteria

First use:

```text
construct once
→ prepare once
→ load/render once
```

Returning to an unchanged page:

```text
activate
→ no unnecessary query
→ no unnecessary full render
```

---

# Phase 7 — Reduce Schedule Rendering Allocation Churn

## Objective

Reduce transient allocations, QWidget creation, deferred deletion, and repeated full-table rebuilding.

Do this after unnecessary schedule construction and refreshes have been removed.

## Tasks

### 7.1 Skip unchanged renders

Determine the effective schedule render input, such as:

- schedule data;
- display mode;
- relevant date/day selection;
- schedule-related preferences.

If it is unchanged, return without rebuilding.

### 7.2 Do not render hidden ScheduleWidgets

When schedule data changes:

- visible schedule → refresh if needed;
- hidden schedule → mark stale.

Render the hidden schedule only when activated.

### 7.3 Reduce clear-and-rebuild behavior

Audit:

- `ScheduleTableRenderer::render()`;
- `clearCellWidgets()`;
- `QTableWidgetItem` recreation;
- `setCellWidget()` usage;
- `deleteLater()` volume.

Where practical:

- update only changed cells;
- reuse stable items/widgets;
- avoid replacing identical cell contents;
- avoid unnecessary widget destruction/recreation.

Do not replace safe `deleteLater()` usage with unsafe immediate deletion just to reduce a metric.

### 7.4 Long-term renderer modernization

Plan a separate migration from:

```text
QTableWidget
+ QTableWidgetItem
+ QWidget per schedule cell
```

to:

```text
QTableView
+ QAbstractTableModel
+ QStyledItemDelegate/custom delegate painting
```

Treat this as a separate implementation step with visual regression checks.

## Acceptance Criteria

An unchanged schedule refresh request results in:

```text
full render = false
new table items = 0
new cell widgets = 0
```

Hidden schedules do not render until activated.

---

# Phase 8 — Defer Nonessential Startup Work

## Objective

Make the first usable screen available before performing tasks unrelated to that screen.

This phase must reduce actual startup work, not merely postpone it.

## Classification

### Required before first usable window

Likely:

- QApplication;
- preferences;
- locale;
- font;
- theme;
- minimal resource infrastructure;
- core services;
- requested database;
- navigation shell;
- initial page;
- visible initial data.

### Optional after startup-complete

Evaluate:

- automatic update check;
- birthday/notification checks;
- noncritical maintenance;
- nonessential metadata refresh.

### First-use only

Keep feature-specific initialization lazy:

- PDF;
- Calendar/QML;
- Sub Prep;
- Campus;
- analytics;
- speaking evaluations;
- report/template rendering;
- document payloads;
- other heavy feature resources.

## Important Rule

Do not schedule all hidden-feature initialization at `+1s` or `+5s`.

If the feature is not used, its startup cost should never occur.

## Acceptance Criteria

The main window becomes usable before optional work begins.

No second major allocation spike appears shortly after startup because deferred eager initialization was simply moved later.

---

# Phase 9 — Refine Resource-Pack Initialization

## Objective

Ensure feature resource packs are discovered, validated, and mounted only as needed.

## Tasks

### 9.1 Audit resource initialization

Separate:

```text
lightweight pack registry/metadata discovery
```

from:

```text
expensive file hashing
pack validation
pack mounting
feature parsing
```

where safe.

### 9.2 Keep feature packs demand-driven

Examples:

```text
campus resources → first Campus use
templates → first template/report use
document assets → first document use
PDF resources → first PDF use
```

### 9.3 Avoid unnecessary full startup hashing

If large packs are hashed every startup, evaluate safe alternatives such as validation:

- after installation/update;
- when file metadata indicates change;
- on first acquisition after change.

Do not weaken integrity protections without justification.

### 9.4 Keep pack behavior cross-platform

Resource-pack lifecycle should be shared unless platform storage behavior genuinely requires a difference.

## Acceptance Criteria

Normal startup does not read, hash, parse, or mount large feature packs unrelated to the initial page.

---

# Phase 10 — Make Startup Data Queries Efficient

## Objective

Avoid repeated database calls and repeated transformation of data required by the initial shell.

## Tasks

### 10.1 Audit sidebar loading

Review for N+1 patterns similar to:

```text
load teachers
load classes
for each class:
    load classInfo
```

### 10.2 Create purpose-built query/service snapshots where justified

If needed, add a service/repository call returning exactly the sidebar/navigation data needed for startup.

Keep database access out of UI implementation details.

### 10.3 Reuse startup data carefully

If the same teacher/class relationship data is immediately required by:

- sidebar;
- navigation filters;
- initial schedule;

reuse a lightweight immutable snapshot where practical.

Do not create a broad cache layer unless profiling shows a need.

### 10.4 Avoid duplicate query work during activation

A page should not reload data already obtained during the same startup transaction unless its required representation differs materially.

## Acceptance Criteria

Startup navigation/sidebar uses a small, predictable query count independent of class count where practical.

Sidebar construction occurs once.

---

# Phase 11 — Introduce Explicit Startup Completion

## Objective

Define one clear point where initialization ends and normal application operation begins.

## Tasks

### 11.1 Add a shared `startup-complete` transition

Use an existing coordinator/controller if suitable, or add a small startup coordinator if necessary.

At this point:

- MainWindow is visible and interactive;
- startup page is loaded;
- splash is closed/destroyed;
- splash resource lease is released;
- startup-only temporary objects are released;
- profiling captures final startup metrics;
- normal event-loop behavior is active;
- optional post-startup tasks may begin.

### 11.2 Audit post-show timers

Review startup-related:

```cpp
QTimer::singleShot(0, ...)
```

and similar mechanisms.

Classify each as:

- required before startup completion;
- optional after startup completion;
- redundant and removable.

### 11.3 Avoid hidden startup tails

After `startup-complete`, there should not be a cascade of hidden page initialization, global restyling, or refresh-all behavior.

## Acceptance Criteria

Profiling shows one explicit:

```text
startup-complete
```

checkpoint.

Memory and widget counts do not immediately jump afterward because additional hidden startup work was still pending.

---

# Phase 12 — Platform-Specific Validation

## Objective

Verify that the shared optimized startup architecture behaves correctly and efficiently on Windows, macOS, and Linux.

This phase validates platform differences without introducing separate startup architectures.

## Windows Validation

Measure:

- peak working set;
- working set at startup-complete;
- private usage;
- private working set where available;
- widget/page/schedule counts;
- startup duration.

Investigate any large difference between:

```text
working set
```

and:

```text
private usage/private working set
```

to distinguish real allocation from resident/cache behavior.

### Windows target direction

After optimization, aim approximately for:

- steady private memory around 150–175 MB or lower;
- early steady working set around 200–225 MB or lower;
- peak working set ideally below approximately 250 MB;
- peak-to-steady ratio preferably below approximately 1.5x.

Treat these as initial engineering targets, not hard CI thresholds until measurements are stable.

## macOS Validation

Measure:

- resident memory;
- physical/private footprint where available;
- peak memory;
- startup duration;
- widget/page/schedule counts.

Confirm that the optimized sequence reduces startup work even if macOS already reports low memory.

Do not treat the current lower macOS memory number as proof that eager initialization is harmless.

## Linux Validation

Measure:

- RSS;
- PSS where practical;
- private resident/dirty memory where practical;
- startup duration;
- widget/page/schedule counts.

Confirm that no Linux-specific regression was introduced by changing initialization order.

## Behavioral Cross-Platform Validation

On all three platforms verify:

- application starts with no database;
- application starts with most-recent database;
- explicitly supplied database startup works;
- correct startup page/tab is selected;
- lazy pages open correctly on first use;
- theme preference is correct;
- font preference is correct;
- runtime theme changes work;
- runtime font-size changes work;
- navigation works before lazy pages exist;
- PDF/Calendar/Sub Prep/Campus initialize correctly on first use;
- application shutdown remains clean.

## Acceptance Criteria

All supported platforms follow the same startup lifecycle.

Any remaining OS-specific startup code has a documented platform-specific reason.

---

# Phase 13 — Add Permanent Startup Regression Coverage

## Objective

Prevent future features from gradually restoring eager startup behavior or large transient memory usage.

## Tasks

### 13.1 Preserve a deterministic representative startup database

Include enough realistic data to exercise the normal initial page:

- multiple teachers;
- multiple classes;
- regular schedules;
- intensive schedules where applicable;
- roster data;
- representative saved settings.

### 13.2 Record shared regression metrics

At minimum:

```text
startup duration
QWidget count
instantiated page count
ScheduleWidget count
schedule render count
```

At:

```text
window-shown
startup-complete
+5s
```

### 13.3 Record native memory metrics per platform

Use the platform metrics established in Phase 1.

Do not require identical numeric values between operating systems.

Track each platform against its own baseline and trend.

### 13.4 Track peak as well as steady state

A result such as:

```text
startup-complete memory = 150 MB
```

is not sufficient if:

```text
peak startup memory = 650 MB
```

Both must be reported.

### 13.5 Add structural regression assertions where practical

Examples:

For the normal My Workspace/Schedule route:

```text
ScheduleWidget count at startup-complete == 1
```

and:

```text
SubPrepPage instantiated == false
PdfViewer instantiated == false
CampusDashboard instantiated == false
```

until opened.

Structural assertions may be more stable across CI machines than hard memory thresholds.

### 13.6 Establish numeric thresholds only after stable measurements

Prefer trend/regression detection first.

Once representative machines/CI environments provide stable data, establish platform-specific warning/failure thresholds.

## Acceptance Criteria

A future feature cannot silently make itself part of startup without:

- increasing structural startup metrics;
- increasing memory/timing metrics;
- or failing explicit lazy-instantiation assertions.

---

# Recommended Implementation Order

| Order | Phase | Expected Impact |
|---:|---|---|
| 1 | Cross-platform startup instrumentation | Required for proof |
| 2 | Single-pass global settings | Very high |
| 3 | Demand-driven PageManager | Very high |
| 4 | Remove duplicate ScheduleWidgets | High |
| 5 | Remove refresh/navigation cascades | High |
| 6 | Shared page lifecycle | High / architectural |
| 7 | Schedule renderer optimization | High longer-term |
| 8 | Defer nonessential work | Medium |
| 9 | Resource-pack refinement | Low–medium |
| 10 | Startup query optimization | Low–medium memory, useful speed |
| 11 | Explicit startup completion | Architectural cleanup |
| 12 | Cross-platform validation | Required |
| 13 | Permanent regression coverage | Prevents recurrence |

---

# Target Cross-Platform Startup Sequence

The final shared startup sequence should conceptually be:

```text
process start
│
├─ create QApplication
│
├─ resolve startup preferences
│  ├─ locale
│  ├─ font
│  └─ theme
│
├─ apply locale once
├─ apply font once
├─ apply theme once
│
├─ initialize minimal resource registry
├─ show splash if enabled
│
├─ resolve startup database
├─ create core ApplicationServices
├─ open database
│
├─ construct MainWindow shell
│  ├─ actions/menus
│  ├─ navigation/sidebar shell
│  └─ register PageManager factories
│
├─ load startup navigation data once
│
├─ construct only requested initial page
│
├─ load/render only visible initial content
│
├─ show MainWindow
├─ close/destroy splash
├─ release startup-only resources
│
├─ STARTUP COMPLETE
│
└─ normal event loop
   │
   ├─ optional update check
   ├─ optional notifications
   │
   └─ feature initialization on first use
      ├─ Sub Prep
      ├─ standalone Schedule
      ├─ Calendar/QML
      ├─ PDF
      ├─ Campus
      ├─ Classes/Analytics
      ├─ Speaking Evaluations
      └─ other feature-specific resources
```

---

# Permanent Architectural Rules

## Rule 1 — Startup is shared across platforms

Windows, macOS, and Linux should use the same startup lifecycle unless the operating system genuinely requires different code.

Platform-specific memory accounting is not a reason for platform-specific page initialization.

## Rule 2 — Never perform startup work on a hidden page

Registering a page must not construct it.

A hidden page must not query data or render merely because the database opened.

## Rule 3 — Global visual state is established before the widget tree

Locale, font, palette, and stylesheet should be resolved before constructing most widgets.

Do not reapply unchanged global settings after the window is shown.

## Rule 4 — Activation does not mean full refresh

Returning to a page should not automatically reload or rebuild it.

Refresh only when relevant data or preferences changed.

## Rule 5 — Feature initialization cost belongs to that feature

PDF, Calendar, Sub Prep, Campus, analytics, speaking evaluations, reports, and similar features should initialize when first used.

## Rule 6 — Do not move eager startup work later

Deferred initialization is only useful if it is genuinely optional.

Do not initialize every feature a few seconds after startup.

## Rule 7 — Measure peak memory

A startup change is not successful merely because the process eventually settles.

Peak memory, startup duration, widget count, page count, and render count matter.

## Rule 8 — Fix platform differences at the correct layer

If Windows retains resident memory differently than macOS, investigate and measure that behavior.

Do not compensate by creating a Windows-only page lifecycle or startup architecture.

---

# Completion Criteria

This plan is complete when all of the following are true:

- Windows, macOS, and Linux use the same core startup lifecycle.
- Platform-specific startup code exists only where technically required.
- Startup profiling represents the actual production path.
- Locale is applied once.
- Startup font is applied once.
- Startup theme is applied once.
- No application-wide post-show font/style reapplication occurs.
- No `PageManager::refreshAll()` is used during normal startup.
- Only pages required for the initial view are instantiated.
- Exactly one ScheduleWidget exists during normal My Workspace/Schedule startup.
- The visible initial schedule renders once.
- Hidden schedules do not render.
- Sub Prep, standalone Schedule, PDF, Calendar, Campus, and other heavy features remain lazy.
- Sidebar/navigation data is populated once.
- The intended initial page/tab is selected directly rather than through temporary intermediate tabs.
- Expensive work is removed from constructors and redundant `showEvent()` paths.
- Startup has an explicit `startup-complete` checkpoint.
- Peak and steady memory are both measured.
- Windows no longer exhibits an unexplained 600–700 MB normal-startup spike, or any remaining peak is measured and clearly attributable.
- macOS and Linux show no startup regressions.
- Runtime theme/font changes remain correct.
- Lazy feature first-use behavior is correct.
- Cross-platform startup regression coverage exists.
- Structural startup assertions prevent hidden pages/features from becoming eager again.
