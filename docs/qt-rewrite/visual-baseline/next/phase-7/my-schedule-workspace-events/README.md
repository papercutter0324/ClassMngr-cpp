# My Schedule and Workspace Events captures

Copied from the fresh standard output at
`build/macos-clang-debug/my-workspace-visual-capture/`: 13 PNGs and the
[manifest](manifest.json). Captures use Fusion at 1024×768. Schedule and
upcoming-event surfaces cover English and Korean, light and dark. One additional
Next capture covers the 96-class scale scenario.

| Locale/theme | Legacy schedule | Next schedule | Next events |
|---|---|---|---|
| English / light | [PNG](legacy_schedule_en_US_light.png) | [PNG](next_schedule_en_US_light.png) | [PNG](next_upcoming_events_en_US_light.png) |
| English / dark | [PNG](legacy_schedule_en_US_dark.png) | [PNG](next_schedule_en_US_dark.png) | [PNG](next_upcoming_events_en_US_dark.png) |
| Korean / light | [PNG](legacy_schedule_ko_KR_light.png) | [PNG](next_schedule_ko_KR_light.png) | [PNG](next_upcoming_events_ko_KR_light.png) |
| Korean / dark | [PNG](legacy_schedule_ko_KR_dark.png) | [PNG](next_schedule_ko_KR_dark.png) | [PNG](next_upcoming_events_ko_KR_dark.png) |

The [96-class Next schedule](next_schedule_96_classes_en_US_light.png) is an
additional scale capture. No legacy Calendar image is included: legacy builds
its deferred `QQuickWidget` month grid while constructing the Upcoming Events
panel. The Next Upcoming Events capture is therefore surface-only and does not
establish Calendar parity.

## Scope and visible differences

Next initially selects My Schedule. Details, Schedule, and Events load only
when their tabs open. My Schedule shows read-only regular schedules in a table;
Calendar shows the next 10 events in a read-only table with date, time, title,
and type. Neither adds birthday or daily-dismissal behavior from candidate #20.

The legacy schedule capture is a weekly grid with Regular, Intensive, Testing,
and Import controls. Next shows a read-only regular-schedule table, so their
representations differ. In Korean Next, the captured weekday label is
“월요일”; the canonical store value remains `Monday`. Calendar editing, filters,
preferences, the month grid, and full visual parity are deferred to candidates
#33–34. Schedule editing, intensive/testing schedules, import, and output are
deferred to #30–32.

## Models, lifecycle, and memory

| Evidence | Legacy schedule | Next schedule / events |
|---|---:|---:|
| Widgets | 81 | 125 |
| Table model/views | — | 2 |
| Visible rows | 13 schedule rows | 6 representative schedule rows; 10 events |
| Additional scale capture | — | 96 schedule rows |
| Schedule/event rows after suspension | — | 0 |
| Personal Details reads in these captures | — | 0 |

Leaving a tab, suspending the page, or replacing/closing the workspace clears
the projections; re-entry loads current data. The manifest records
process-wide observations from one cumulative process: the initial working set
was 38,338,560 bytes, the maximum recorded peak working set was 101,302,272
bytes, and private usage reached 39,470,040 bytes. These are whole-process
snapshots, not per-page allocations or a memory budget.

## Verification and gates

The independent verification build command was:

```text
cmake --build build/macos-clang-debug --target ClassMngrNextMyWorkspacePageTests ClassMngrNextMyWorkspaceVisualCapture ClassMngrNext -j 6
```

The focused CTest selection passed 5/5:
`ClassMngrNextMyWorkspacePageTests|ClassMngrNextMyWorkspaceVisualCapture|ClassMngrMyWorkspacePageTests|NextStartupWorkspaceDependencyBoundary|ClassMngrCalendarEventRepositoryTests`.
Resource inventory `--check` and `git diff --check` passed. No full suite,
Windows run, or final Phase 7 exit gate was verified. Calendar parity and the
7A screenshot comparison, memory evidence, and 7A/Phase 7 exit gates remain
open.
