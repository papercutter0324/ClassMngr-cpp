# My Workspace Consolidation Audit

Completed: 2026-08-26

## Current architecture

```text
PageManager (top-level QStackedWidget)
├── PersonalDetailsPage       PageType::PersonalDetails  (eager)
├── SchedulePage              PageType::MySchedule       (eager)
├── CalendarPage              PageType::Calendar          (lazy)
└── other unrelated pages
```

The sidebar routes its three separate entries directly to those three top-level
pages:

```text
my_info_information → PersonalDetails
my_info_schedule    → MySchedule
my_info_calendar    → Calendar
```

The target should instead be:

```text
PageManager
└── MyWorkspacePage           PageType::MyWorkspace
    ├── Details tab   → PersonalDetailsPage (reused child)
    ├── Schedule tab  → SchedulePage        (reused child; default)
    └── Calendar tab  → CalendarPage        (created on first selection)
```

`MyWorkspacePage` needs a named `WorkspaceTab` API, e.g.
`openTab(WorkspaceTab::Schedule)`.  Navigation code should use that API and
never use a tab index.

## Existing components to reuse

| Area | Existing implementation | Notes |
| --- | --- | --- |
| Details | `features/my_info/ui/personal_details_page.*` | A `BasePage` with autosave, explicit save/discard, current data loading, and `scrollToTop()`. Its page header must be suppressible when embedded. |
| Schedule | `features/schedule/ui/schedule_page.*` | A `BasePage` that owns one persistent `ScheduleWidget`; it emits `classInfoSaved`, `displayModeChanged`, `scheduleImportRequested`, and `testingClassesRequested`. Its page-level title/subtitle must be suppressible when embedded. |
| Calendar | `features/calendar/ui/calendar_page.*` | A `BasePage` with QML calendar, event cache/model, and upcoming-events panel. Construction currently builds the UI and immediately requests data. |
| Existing tab styling | `ui/shared/widgets/navigation_tab_widget.*` | Use this rather than introducing a new unstyled tab implementation. |

Because source files are collected recursively by `cmake/sources.cmake`, a new
`src/features/my_info/ui/my_workspace_page.{h,cpp}` needs no manual source-list
entry.

## Page-manager and lifecycle impacts

- `PageType`, `PageManager::pageTypeIdentifier`, factories, eager initialization,
  resource checks, the page-action selector, and `pagemanager_tests` all name
  the three current top-level pages.
- `PageManager::confirmCurrentPageCanLeave()` delegates only to the current
  top-level `BasePage`. `MyWorkspacePage` must therefore delegate
  `hasUnsavedChanges()`, `saveChanges()`, `discardChanges()`, `setSaveMode()`,
  `clearDatabaseState()`, `refresh()`, and translation to its instantiated
  children. This is required to preserve the Details autosave prompt when
  leaving My Workspace.
- `PageManager::setDatabaseOpen()` only calls top-level pages. The workspace
  parent must propagate the state to its child pages, including a Calendar
  created after the state was set.
- Page-manager resource handling currently treats Details and Calendar as
  campus-resource users. `MyWorkspace` must be classified accordingly before
  the old page types are removed, otherwise campus data can be unavailable in
  the embedded child pages.
- There are no raw top-level `QStackedWidget` page indexes in the affected
  routing. The relevant identifier is the `PageType` enum; the new internal
  stack should use a named `WorkspaceTab` enum.

## Navigation and signal routes to update

- Sidebar definitions: `sidebar_definitions.cpp` has separate
  `my_info_information`, `my_info_schedule`, and `my_info_calendar` page nodes.
  Phase 6 replaces these with one `My Workspace` node.
- `NavigationController::handleMyInfo()` maps those keys to the current three
  `PageType` values and calls each page's `scrollToTop()`. It must become a
  `MyWorkspace` route plus `openTab()` calls for intentional deep links.
- `MainWindow` connects the personal schedule's `SchedulePage` signals to
  sidebar refresh, importing, display-mode synchronization, and testing-class
  navigation. Keep these connections pointed at the reused `SchedulePage`
  through a workspace accessor.
- Returning from Testing Classes uses `PageType::MySchedule` and refreshes
  `mySchedulePage()`. It must return to `PageType::MyWorkspace`, select
  Schedule, then refresh the workspace schedule child.
- Initial setup completion and startup both open `PageType::MySchedule` and
  select `my_info_schedule`; both move to My Workspace's default Schedule tab.
- The Calendar preferences page currently calls
  `MainWindow::ensureCalendarPage()`, which would construct Calendar merely by
  opening Preferences. Preserve that API but make it resolve the workspace's
  lazy Calendar child; this is an intentional construction path and should be
  covered by a test.

## Details behavior and persistence

- `PersonalDetailsRepository` stores values in the teacher-profile settings
  service under `myInfo/*` keys, with legacy `subPrep/*` fallbacks for Zoom.
  The move must not alter those keys or the data model.
- The N/A checkbox is persisted as `myInfo/zoomNotAvailable`. When checked,
  it disables the Zoom ID and password fields; their existing values remain.
  When unchecked, blank fields normalize to `N/A` during save.
- Password masking is not configured in `PersonalDetailsPage`; it is currently
  a normal `QLineEdit`. Embedding must retain that exact behavior until a
  separate UX change is requested.
- Details reloads only when visible and clean; switching workspace tabs must
  not destroy it or discard its dirty state.

## Signature compatibility

- The existing system is image based, not a drawing canvas: the user imports a
  supported image, white backgrounds are removed, and the result is normalized
  to a transparent PNG by `SignatureImage::prepareForEmbedding()`.
- The PNG bytes are Base64-encoded in `myInfo/signatureImage`. Consumers receive
  PNG bytes through `PersonalDetailsRepository::load().signatureImage`.
- Confirmed consumers are the initial setup wizard and Speaking Evaluation
  report/export paths. The report renderer takes a `QByteArray` image and draws
  it inside the template signature bounds.
- Typed signatures should render to the same transparent PNG byte format before
  saving. That preserves the initial-setup and Speaking Evaluation contracts.
- `resources/assets/fonts/JustAnotherHand-Regular.ttf` is already bundled.
  Phase 4 will need three additional clearly distinct open-license fonts plus
  their licence texts and resource entries in `cmake/resources.cmake`.

## Calendar behavior and lazy-load gap

- Calendar is already lazy at the top-level page-manager boundary: it is not
  instantiated during `PageManager::initialize()`.
- Once constructed, `CalendarPage::buildCalendarContent()` immediately calls
  `refreshCalendarData()`. The current refresh requests the visible month,
  today through +30 days, and a background prefetch spanning the following five
  months. It also supports expanding searches for the next ten events.
- Selecting another calendar month assigns `m_calendarVisibleMonth` and calls
  `refreshCalendarData()` again. `CalendarEventCache` prevents duplicate fully
  loaded/pending ranges, but the page deliberately retains and prefetches more
  than one month today.
- Phase 5 must retain lazy construction inside `MyWorkspacePage` and refactor
  Calendar's initial/request behavior to exactly one current-month query. The
  cache can remain, but its request and retention strategy must be changed from
  range-prefetching to month-on-demand caching.

## Test updates required

- Replace `pagemanager_tests` assertions for eager Details/MySchedule and lazy
  Calendar with assertions for a single eager `MyWorkspacePage`, its default
  Schedule child, and an uncreated Calendar child.
- Update `sidebar_structure_tests` for the single workspace destination.
- Add workspace tests for default tab, named deep links, state persistence,
  delegation of unsaved Details changes, and one-time Calendar construction.
- Add Calendar cache/service tests proving startup, opening My Workspace, and
  opening Details/Schedule issue no Calendar query; first Calendar activation
  issues only the current-month query; revisiting a loaded month does not query
  again.
- Add typed-signature tests for four font selections, live preview generation,
  PNG persistence/reload, and a Speaking Evaluation report consuming the output.

## Recommended Phase 2 implementation boundary

Create the shell and its named tabs only. Reuse persistent Details and Schedule
children, suppress their duplicate headers, set Schedule as default, and leave
the Calendar tab as a placeholder. Do not yet remove the old top-level routes,
alter signature persistence, or refactor Calendar data loading.
