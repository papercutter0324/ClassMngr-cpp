# Phase 3 — Schema v6 Behavior Baseline

Status: Source-derived baseline for F1318, 2026-10-22

This records the current source contract before Phase 3 persistence changes. It
does not assert that a runtime database was inspected. The inventory comes
from the initial DDL and conditional legacy-column declarations in
[`database_schema_manager.cpp`](../../src/data/database/database_schema_manager.cpp#L176-L412),
the final constrained table definitions in the same file
[#L414-L616](../../src/data/database/database_schema_manager.cpp#L414-L616),
and the migration list below.

## Schema v6

The schema has 17 tables. `INTEGER PRIMARY KEY AUTOINCREMENT` is abbreviated
`INTEGER PK AUTOINCREMENT`; `PRIMARY KEY` is abbreviated `PK`. Columns not
marked `NOT NULL` or `PK` are nullable. The four explicit secondary indexes
are listed after the table inventory.

| Table | Columns | Constraints and defaults |
|---|---|---|
| `app_settings` | `key TEXT`, `value TEXT` | `key` PK |
| `roster_columns` | `id INTEGER PK AUTOINCREMENT`, `class_id INTEGER`, `name TEXT`, `position INTEGER`, `width INTEGER` | `class_id NOT NULL` → `classes(id) ON DELETE CASCADE`; `position NOT NULL CHECK(position >= 0)` |
| `roster_data` | `class_id INTEGER`, `row_index INTEGER`, `col_index INTEGER`, `value TEXT` | `class_id NOT NULL` → `classes(id) ON DELETE CASCADE`; `row_index`, `col_index NOT NULL`, each checked `>= 0`; composite PK `(class_id, row_index, col_index)` |
| `speaking_evaluations` | `id INTEGER PK AUTOINCREMENT`, `class_id INTEGER`, `evaluation_name TEXT` | `class_id NOT NULL` → `classes(id) ON DELETE CASCADE`; `UNIQUE(class_id, evaluation_name)` |
| `speaking_eval_data` | `evaluation_id INTEGER`, `row_index INTEGER`, `col_0` … `col_10 TEXT` | `evaluation_id NOT NULL` → `speaking_evaluations(id) ON DELETE CASCADE`; `row_index NOT NULL CHECK(row_index >= 0)`; composite PK `(evaluation_id, row_index)` |
| `campuses` | `id INTEGER PK AUTOINCREMENT`, `name TEXT`, `building_name TEXT`, `address TEXT`, `phone_number TEXT`, `office_number TEXT`, `transit_steps TEXT`, `arrival_info TEXT`, `image_path TEXT`, `office_wifi TEXT`, `office_wifi_password TEXT`, `printer_name TEXT`, `printer_steps TEXT`, `photocopier_code TEXT`, `housing_locations TEXT` | `name NOT NULL` |
| `teachers` | `id INTEGER PK AUTOINCREMENT`, `teacher_kr TEXT`, `teacher_en TEXT`, `preferred_romanization TEXT`, `preferred_name TEXT`, `room_number TEXT`, `birthday TEXT`, `phone_number TEXT`, `wifi_name TEXT`, `wifi_password TEXT`, `internet_type TEXT`, `zoom_id TEXT`, `zoom_password TEXT`, `projection_type TEXT`, `notes TEXT` | `internet_type NOT NULL DEFAULT 'WiFi' CHECK(internet_type IN ('WiFi', 'LAN', 'Both', 'N/A'))`; `projection_type NOT NULL DEFAULT 'HDMI' CHECK(projection_type IN ('HDMI', 'Zoom', 'Any', 'N/A'))` |
| `native_english_teachers` | `id INTEGER PK AUTOINCREMENT`, `name TEXT`, `position TEXT`, `phone_number TEXT`, `birthday TEXT`, `nationality TEXT`, `email TEXT` | — |
| `gs_team` | `id INTEGER PK AUTOINCREMENT`, `name TEXT`, `korean_name TEXT`, `position TEXT`, `phone_number TEXT`, `birthday TEXT` | — |
| `classes` | `id INTEGER PK AUTOINCREMENT`, `name TEXT` | — |
| `class_info` | `class_id INTEGER`, `teacher_id INTEGER`, `class_grade TEXT`, `class_level TEXT`, `reading_book TEXT`, `essay_book TEXT`, `class_color TEXT`, `font_color TEXT`, `notes TEXT`, `time_filler_activities TEXT` | `class_id` PK → `classes(id) ON DELETE CASCADE`; `teacher_id` → `teachers(id) ON DELETE SET NULL`; `class_color DEFAULT '#FFFFFF'`; `font_color DEFAULT '#000000'` |
| `testing_classes` | `class_id INTEGER`, `room TEXT` | `class_id` PK → `classes(id) ON DELETE CASCADE`; `room NOT NULL` |
| `class_times` | `id INTEGER PK AUTOINCREMENT`, `class_id INTEGER`, `day TEXT`, `start_time TEXT`, `end_time TEXT` | `class_id NOT NULL` → `classes(id) ON DELETE CASCADE` |
| `class_intensive_times` | `id INTEGER PK AUTOINCREMENT`, `class_id INTEGER`, `day TEXT`, `start_time TEXT`, `end_time TEXT` | `class_id NOT NULL` → `classes(id) ON DELETE CASCADE`; `UNIQUE(day, start_time)` |
| `intensive_slot_states` | `id INTEGER PK AUTOINCREMENT`, `day TEXT`, `start_time TEXT`, `state TEXT` | `UNIQUE(day, start_time)` |
| `schedule_testing_blocks` | `id INTEGER PK AUTOINCREMENT`, `day TEXT`, `start_time TEXT`, `room TEXT`, `class_id INTEGER` | `day`, `start_time NOT NULL`; `room NOT NULL DEFAULT ''`; `class_id` → `classes(id) ON DELETE SET NULL`; `UNIQUE(day, start_time)` |
| `calendar_events` | `id INTEGER PK AUTOINCREMENT`, `title TEXT`, `event_type TEXT`, `time_status TEXT`, `repeat_series_id TEXT`, `all_day INTEGER`, `start_date TEXT`, `start_time TEXT`, `end_date TEXT`, `end_time TEXT` | `title NOT NULL`; `event_type NOT NULL DEFAULT 'Other' CHECK(event_type IN ('Vacation', 'Holiday', 'Workshop', 'CM', 'Meeting', 'Other'))`; `time_status NOT NULL DEFAULT 'Timed' CHECK(time_status IN ('Timed', 'Unknown', 'Unconfirmed'))`; `all_day NOT NULL DEFAULT 0 CHECK(all_day IN (0, 1))` |

### Explicit secondary indexes

- `idx_schedule_testing_blocks_class_id` on `schedule_testing_blocks(class_id)`
- `idx_calendar_events_dates` on `calendar_events(start_date, end_date, start_time, title)`
- `idx_calendar_events_end_dates` on `calendar_events(end_date, start_date, start_time, title)`
- `idx_calendar_events_repeat_series` on `calendar_events(repeat_series_id, start_date, id)`

### Conditional legacy columns

Schema setup conditionally adds missing compatibility columns. These are the
declarations in `SchemaColumns`; they are not additional tables:

- `campuses`: `building_name`, `address`, `phone_number`, `office_number`,
  `transit_steps`, `arrival_info`, `image_path`, `office_wifi`,
  `office_wifi_password`, `printer_name`, `printer_steps`,
  `photocopier_code`, `housing_locations` (`TEXT`).
- `teachers`: `preferred_romanization`, `preferred_name`, `birthday`,
  `phone_number` (`TEXT`); `internet_type TEXT DEFAULT 'WiFi'`;
  `projection_type TEXT DEFAULT 'HDMI'`.
- `native_english_teachers.email TEXT`; `class_info.time_filler_activities TEXT`;
  `schedule_testing_blocks.class_id INTEGER`.
- `calendar_events`: `event_type TEXT DEFAULT 'Other'`,
  `time_status TEXT DEFAULT 'Timed'`, `all_day INTEGER DEFAULT 0`, and
  `repeat_series_id TEXT`.

## Versioning and opening policy

[`database_schema_manager.h`](../../src/data/database/database_schema_manager.h#L9-L33)
sets both latest and minimum supported versions to 6. The version is SQLite's
`PRAGMA user_version`; migrations 1–6 are named in
[`database_schema_manager.cpp`](../../src/data/database/database_schema_manager.cpp#L691-L763)
and [#L1183-L1189](../../src/data/database/database_schema_manager.cpp#L1183-L1189):

1. Initial schema.
2. Legacy column upgrades.
3. Legacy data preflight.
4. Foreign keys and stable constraints.
5. Schema indexes.
6. Valid persisted row indexes.

Only explicit `AllowEmptyDatabase` creation intent for a version-0 database
with no user objects initializes it; session creation uses this path only when
the target file was missing. An existing file, including an empty version-0
file, is opened with existing-file policy and rejected if its version is 0–5.
Future versions are rejected too. Thus ordinary current opening does not run
migrations on existing older profiles; a newly created database runs
migrations 1–6 to reach v6. The database manager enables foreign keys and
finishes setup with `PRAGMA foreign_key_check`.

Each migration wraps its DDL/data work and `user_version` write in a
`DatabaseTransaction`. The migration dispatcher contains a pre-migration
`VACUUM INTO` backup branch for migrations 4 and 6 when its existing-profile
conditions apply, with sidecars named `.pre-schema-v4-backup` and
`.pre-schema-v6-backup`. This is not normal v6-open behavior: the current
minimum-version policy rejects existing v0–5 files before migration, and new
empty-database initialization has no existing profile to back up. Migrations 4
and 6 temporarily disable and then re-enable connection foreign-key
enforcement around their table rebuilds.

## Current file and session behavior

`.tps` is the native suffix and `.db` the legacy suffix; both are accepted
inputs. Both go through the same SQLite `DatabaseSession` and schema-manager
path, with no suffix-specific schema migration. Existing-only opens use a
SQLite URI in read-write mode so SQLite does not create a missing file. A
session opens a candidate connection and repositories, then swaps it in only
after success; it owns the repositories and releases them before closing and
removing the connection. `FileController` wires `WorkspaceCoordinator`
through the workspace use case and gateway to
`ApplicationServicesWorkspacePort`, which forwards to `ApplicationServices` /
`DatabaseSession`. Create/replacement and Initial Setup recovery are separate
lifecycle policies. Save-as/export copies use `QSaveFile` and a 64 KiB buffer.

Sources: [`DatabaseFileFormat`](../../src/core/database_file_format.cpp#L7-L88),
[`DatabaseSession`](../../src/data/database/database_session.cpp#L76-L207)
and [#L210-L264](../../src/data/database/database_session.cpp#L210-L264),
[`FileController` wiring](../../src/app/controllers/file_controller.cpp#L199-L245),
[`WorkspaceCoordinator`](../../src/next/application/workspace_coordinator.h#L12-L85),
[`ApplicationServicesWorkspacePort`](../../src/next/platform/application_services_workspace_port.h#L17-L105),
and [`DatabaseFileOperations`](../../src/data/database/database_file_operations.cpp#L98-L155).

## Existing test coverage and limits

Source coverage currently pins v6 initialization and foreign-key behavior,
cascade deletion, explicit empty-database creation, rejection of each version
0–5 without writes or backup, persisted row-index constraints, foreign-key
integrity failure, and rejection of a future version in
[`database_schema_manager_tests.cpp`](../../tests/database_schema_manager_tests.cpp#L71-L82)
and [#L85-L370](../../tests/database_schema_manager_tests.cpp#L85-L370).
Startup performance coverage verifies version-0 fixture rejection without
changes, corrupt `.db` rejection with byte preservation, and locked legacy
startup followed by continued unsupported-version-0 rejection after lock
release. The lock case does not migrate or establish that the SQLite lock
caused rejection; see
[`startup_performance_tests.cpp`](../../tests/startup_performance_tests.cpp#L2313-L2518).
Workspace-lifecycle startup coverage also checks older-schema rejection while
preserving the active workspace and source file in
[`file_controller_workspace_lifecycle_tests.cpp`](../../tests/file_controller_workspace_lifecycle_tests.cpp#L2582-L2664).
Calendar repository coverage checks the calendar end-date index in
[`calendar_event_repository_tests.cpp`](../../tests/calendar_event_repository_tests.cpp#L611-L647).

There is no full table-by-table DDL-shape matrix and no direct test of all four
explicit secondary indexes. This baseline is derived from declarations and
tests; no runtime database metadata inspection or new tests were performed for
this documentation slice.
