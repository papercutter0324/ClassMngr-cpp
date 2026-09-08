# Native WinUI calendar scheduler plan

## Purpose

Replace the current Phase 6 WinUI calendar prototype with a usable Day, Week,
and Month scheduler. It must preserve the Qt calendar's data, validation,
repeat-series behavior, filters, settings, and database compatibility while
using standard WinUI 3 controls wherever they provide the required behavior.

This is a follow-on implementation plan for Phase 6 migration-order item 3;
it does not change that phase's exit gate or make the current calendar slice
accepted by itself.

## Technology decision

Use a native C++/WinRT WinUI composition for the scheduler. Do not integrate
`Syncfusion.Scheduler.WinUI` into the current application.

The Syncfusion scheduler is a strong behavioral reference: it supports Day,
Week, and Month views; appointment templates and colored appointment mapping;
recurrence; built-in editing; and cell/appointment context flyouts. Its
published package and getting-started path, however, target .NET WinUI
applications, while this product is a C++/WinRT WinUI 3 application. Adding a
.NET host, projection bridge, or separate calendar process only for this
control would create a high-risk cross-runtime boundary and would undermine
the portable engine architecture.

`CalendarView` will be used as the native date navigator and selected-date
surface. It is deliberately **not** the appointment canvas: Microsoft defines
it as an always-visible date/range selector with month/year/decade navigation,
and its rendered day items do not provide a supported, virtualized per-day
appointment host. It therefore cannot by itself implement labeled event bars,
event-type dots, a time grid, overlap layout, or event hit-testing.

The main Month, Day, and Week canvases will be composed from native WinUI
`Grid`, `ScrollViewer`, `ItemsRepeater`/`ListView`, `Button`, `Flyout`,
`MenuFlyout`, `ContentDialog`, `CalendarDatePicker`, and `TimePicker`
controls. This keeps accessibility, keyboard navigation, focus, theme, DPI,
and localization in the WinUI control system, while adding only the custom
appointment layout that WinUI does not ship.

Before implementation, record the dependency decision in the Phase 6 evidence
and verify it against the installed package/version policy. The plan must not
silently add a .NET project or change the app host language.

Primary references:

- [Microsoft CalendarView guidance](https://learn.microsoft.com/en-us/windows/apps/develop/ui/controls/calendar-view)
- [Microsoft date and time control guidance](https://learn.microsoft.com/en-us/windows/apps/develop/ui/controls/date-and-time)
- [Syncfusion Scheduler overview](https://www.syncfusion.com/scheduler-sdk/winui-scheduler)
- [Syncfusion Month display modes and templates](https://help.syncfusion.com/winui/scheduler/month-view)
- [Syncfusion context-flyout commands](https://help.syncfusion.com/scheduler-sdk/winui/schedule/context-flyout-commands)

## Existing contract to preserve

### Qt behavior and data contract

The retained Qt page already establishes the feature baseline:

- `EventCalendar.qml` and `MonthGridDelegate.qml` render a custom monthly
  grid, date navigation, color-coded labeled bars, click-empty-day add, and
  click-event edit.
- `CalendarEventDialog` supports title; start/end dates and times; all-day;
  unconfirmed/unknown time; the six engine event types; daily/weekly/monthly
  recurrence through an end date; delete; and this-event versus this-and-
  following-series edits.
- `CalendarEventCache` and `CalendarEventModel` load the visible range on
  demand and retain requested ranges. Existing campus and start-of-term filters
  and the upcoming-events panel must continue to agree with the canvas.

The engine data contract is authoritative and must remain unchanged:

| Concern | Existing authority | Required scheduler behavior |
| --- | --- | --- |
| Event fields | `classmngr::engine::CalendarEvent` | Keep ID, title, event type, time status, all-day, start/end date/time, and `repeatSeriesId`. |
| Valid event types | `CalendarEventRules` | Use the canonical `Vacation`, `Holiday`, `Workshop`, `CM`, `Meeting`, and `Other` types; do not persist presentation colors. |
| Validation | `CalendarEventValidator` | Validate title, dates, times, status/all-day combination, recurrence end date, and occurrence cap before saving. |
| Persistence | `CalendarEventService` | Load ranges, save one event, create a repeat series atomically, and update/delete either an occurrence or the suffix of a series. |
| Recurrence representation | Existing persisted expansion | Keep the current daily/weekly/monthly expanded-row model and `repeatSeriesId`; do not convert existing databases to Syncfusion RRULE storage. |
| Display/filter settings | `ApplicationSettingsService` and existing calendar keys | Preserve first day of week, campus scope, and hide-start-of-term behavior. |

## Target interaction and layout

### Shared chrome

1. Keep the Calendar and Preferences pivot structure and the existing
   sliding-tab, focus-restoration, reduced-motion, localization, and database
   state rules.
2. Add a top `CommandBar`: Previous, Today, Next, view selector (`Day`,
   `Week`, `Month`), display selector (`Labels`, `Dots`), and Add event. The
   keyboard-accessible selector state is persisted as a presentation preference
   only after a separate settings-schema review; it must never alter event
   data.
3. Place an always-visible native `CalendarView` navigator alongside the main
   canvas when width permits. It uses the same first-day-of-week setting and
   follows the current displayed/selected date. On narrow widths, move it into
   a `TeachingTip`/flyout opened from the date header rather than duplicating
   navigation.
4. Apply all existing event filters before projecting data to any view; show
   a consistent no-database, loading, empty, and error state.

### Month view

1. Render the seven-column, five-or-six-row date grid using native `Grid` and
   virtualized/recycled day-cell presenters. Do not overlay arbitrary visual
   children onto `CalendarView` day items.
2. In **Labels** mode, show the first image's compact rounded colored bar:
   event title, type-derived background, accessible foreground contrast,
   ellipsis, and a localized `+N more` affordance. A multi-day/all-day event
   is represented consistently across each covered day; it must not disappear
   from either endpoint.
3. In **Dots** mode, show up to the defined number of type-colored dots in
   each date cell, equivalent to the second image. Selecting a date opens an
   agenda beneath the month grid (or a responsive adjacent pane) with the
   full, ordered event list, colored labels, times/status, and edit action.
4. The mode switch changes presentation only. Both modes expose the same
   events to UI Automation and allow keyboard selection. A date containing
   more events exposes its total count in accessible name/help text.
5. Left-click/tap a blank day cell opens Add Event prefilled for that date;
   left-click/tap an event opens Edit Event. Right-click a blank cell opens the
   cell menu; right-click an event opens the appointment menu.

### Day and Week views

1. Render all-day/spanning events in an all-day strip above the scrollable
   hourly grid. Render timed events in 15- or 30-minute slots using a single
   shared slot-height constant and a visible time ruler. Unknown/unconfirmed
   events appear in an explicit non-timed/status area rather than inventing a
   time.
2. Day view has one date column; Week view has seven columns starting on the
   configured first day of week. The native `CalendarView` selection moves the
   Day view to that date and anchors Week view to the week containing it.
3. Implement deterministic overlap placement: partition each day into
   overlapping clusters, assign columns, calculate width/span, and keep a
   keyboard and automation target for every appointment. Do not use text
   clipping as the only way to distinguish colliding events.
4. Initial scope is view, selection, display, and CRUD parity. Drag/drop and
   resize are explicitly deferred until after this plan's acceptance; if added,
   they must use the same edit validation/save path and be separately tested.

### Add, edit, delete, and recurrence dialog

1. Implement one native `ContentDialog` editor used by Add, Edit, command bar,
   keyboard, and both context menus. Use `TextBox`, `CalendarDatePicker`,
   `TimePicker`, `CheckBox`, `ComboBox`/`RadioButtons`, and native buttons;
   avoid recreating Qt widgets or an HTML form.
2. Fields: title, event type, time status, all-day, start/end date/time,
   repeat enabled, frequency (daily/weekly/monthly), repeat-until date, and
   an edit/delete scope chooser for a series occurrence. Disable or clear
   time fields according to the current `CalendarEventValidator` rules.
3. Save calls the existing engine service: one-off `save`, recurrence
   `createRepeatSeries`, series-suffix edit `updateRepeatSeriesFromDate`, and
   single/suffix delete through the existing remove operations. Generate a
   `repeatSeriesId` at the WinUI boundary only when creating a new recurrence.
4. Surface field-level localized validation from engine issue codes; preserve
   entered values and focus the first invalid control. Require a native delete
   confirmation and clearly name the selected recurrence scope.

### Context menu and keyboard contract

1. A cell `MenuFlyout` provides Add Event and, where meaningful, Go to Day / Go
   to Week. It receives the exact clicked cell date, not merely the currently
   selected date.
2. An appointment `MenuFlyout` provides Edit, Delete, and view navigation;
   its data context carries the exact event ID and recurrence occurrence.
3. The `ContextRequested` handler must work for mouse, keyboard context-menu
   key, and touch hold. Ensure custom canvas elements are focusable and expose
   semantic names such as title, date/time, event type, recurrence, and menu
   availability.
4. Add keyboard equivalents: arrow navigation through date/slot cells; Enter
   to add/open; Shift+F10/Menu to open the matching context menu; Escape to
   close without saving; and focus return to the originating cell or event.

## Implementation slices

1. **Architecture spike and acceptance baseline.** Verify the current
   C++/WinRT package boundary and record the no-Syncfusion decision; capture
   paired Qt/WinUI screenshots and semantic baselines for empty, populated,
   filtered, recurring, all-day, and collision data. Define native scheduler
   view-model/projection types and the date/time conversion boundary.
2. **Shared data presenter.** Extract the existing calendar loading/filtering
   logic from `MainWindow.xaml.cpp` into a Windows-owned presenter with
   visible-range requests, invalidation after writes, no-database/error state,
   and one event projection consumed by all three views and the agenda.
3. **Native navigation and month canvas.** Replace the hand-built prototype
   calendar grid with the command bar, `CalendarView` navigator, labels mode,
   dot mode, selected-day agenda, responsive layout, and mode preference.
4. **Day/week canvas.** Add the all-day/status lanes, virtualized time grid,
   collision-layout algorithm, shared date navigation, scrolling, selection,
   and non-timed event presentation.
5. **Native editor and menus.** Replace the current narrow editor path with
   the reusable `ContentDialog`, field validation, recurrence scope handling,
   click-empty creation, event editing, delete confirmation, and cell/event
   `MenuFlyout` behavior.
6. **Hardening and evidence.** Localize all new strings; add accessibility,
   keyboard, Korean IME, DPI, light/dark/high-contrast, memory, and
   cross-platform database evidence; then update the Phase 6 ledger only when
   the stated acceptance tests pass.

## File-level implementation map

| Area | Planned ownership |
| --- | --- |
| Calendar host, XAML resources, control templates, and event handlers | New focused files under `src/platform/windows/winui/` (for example `calendar_scheduler.*`), called from `MainWindow.xaml.cpp`; remove calendar-specific rendering from the window once migrated. |
| Native state/projection and range loading | New Windows presenter/view-model, using only `CalendarEventService`, `ApplicationSettingsService`, and engine value objects. |
| Event editor, context menus, and converters | New Windows-owned dialog/menu helper and XAML resource dictionary. |
| Portable business logic and schema | Preserve `src/engine/include/classmngr/engine/calendar_event*.h`, `calendar_event_service`, rules, validator, and their tests unless a newly demonstrated engine defect requires a separately reviewed change. |
| Qt compatibility baseline | Preserve `src/features/calendar/ui/`; use it for parity comparison, not as a dependency of the WinUI implementation. |
| Build/test integration | Update `ClassMngrWinUI.vcxproj`, package policy only if a native dependency is approved, stage verifier hooks, and focused tests. |

## Acceptance and test matrix

| Scenario | Required proof |
| --- | --- |
| View/navigation | Day, Week, and Month select the correct dates; Previous/Today/Next and `CalendarView` remain synchronized; first-day-of-week setting shifts Week and Month correctly. |
| Month labels/dots | Both modes show the same filtered event set and canonical type colors; labels expose overflow; dots expose counts and selected-day agenda. |
| Timed layout | Day/Week show all-day, timed, multi-day, unknown, unconfirmed, and overlapping events without loss or inaccessible controls. |
| CRUD | Empty-cell click, Add command, event click, cell/event right-click, keyboard menu, save/cancel/delete, and error recovery all use the same service operations. |
| Recurrence | Daily, weekly, and monthly creation; invalid end/cap rejection; edit/delete this occurrence and this-and-following; preserved repeat IDs and cross-platform reload. |
| Data/filtering | No database, empty database, engine error, visible-range loading, campus filter, hide-start-of-term filter, and upcoming-events agreement. |
| Quality | English variants and Korean strings; Korean IME in title; screen-reader names; keyboard-only flow; 100/125/150/200% DPI; light/dark/high contrast; x64/x86 builds. |
| Compatibility | Engine tests plus Windows-to-Qt and Qt-to-Windows fixture round trips prove no schema or recurrence representation change. |

Run the focused calendar engine tests, the WinUI Phase 6 calendar semantic
hook, both architecture builds, staged verification, and new deterministic
native UI tests. Add paired screenshots only after the semantic tests pass;
screenshots are evidence of visual parity, not a replacement for behavioral,
accessibility, or persistence coverage.

## Out of scope without a later decision

- A .NET bridge, host rewrite, or separate Syncfusion-based calendar process.
- RRULE migration, recurrence exceptions beyond the existing persisted-series
  behavior, resources, time zones, reminders, and online calendar sync.
- Drag/drop and resize before the baseline scheduler is accepted.
- Modifying the Qt implementation except for a separately approved parity bug.
