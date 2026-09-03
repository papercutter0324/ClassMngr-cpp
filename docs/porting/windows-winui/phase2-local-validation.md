# Phase 2 local validation record

Date: 2026-09-03 (Asia/Seoul)

Scope: Phase 2 portable-engine slices on the clean Phase 1 working tree.

## Extracted contract

`classmngr::engine::DatabaseFileFormat` now owns the `.tps`/legacy `.db`
path rules using UTF-8 `std::string_view` inputs and `std::string` outputs.
The retained Qt `DatabaseFileFormat` namespace is a conversion adapter only;
it no longer contains the extension, case, or suffix rules.

The Qt-free `ClassMngrEngine` target includes the contract and has a dedicated
headless test executable covering native and legacy paths, case-insensitive
extensions, UTF-8 names, directory names containing dots, blank paths, and
extensionless inputs.

`classmngr::engine::Error` and `Result<T>` now provide typed error codes,
UTF-8 messages, and optional native codes for persistence-facing APIs.
`classmngr::engine::SqliteDatabase` keeps SQLite handles private and provides
prepared parameter binding, typed integer/real/text/blob/null rows, a
configurable `std::chrono` busy timeout, foreign-key initialization, schema
version primitives, and RAII transaction rollback/commit. Windows links the
architecture-matched Windows SDK `winsqlite3` library; non-Windows builds use
the CMake SQLite3 package/target.

`classmngr::engine::DatabaseSchemaManager` now owns the six-version schema
sequence: initial tables, legacy columns, legacy-data preflight, constrained
foreign-key tables, indexes, and valid persisted row indexes. It preserves
transaction rollback and file-backed pre-constraint backups. `OpenDatabase`
normalizes UTF-8 filesystem paths, creates missing parent directories, opens
SQLite, and migrates the schema before returning the handle.

The first product CRUD slice is Qt-free `Classroom` plus `ClassRepository`.
It covers UTF-8 create/list/get/rename/delete behavior, testing-class
filtering, cascade cleanup, and typed invalid/not-found errors.

The testing-workspace slice now includes Qt-free `TestingClassService` and
`TestingBlockService` boundaries. Testing-class persistence owns required-field
validation, mixed-level choice catalogs, class-info/class-row writes,
assignment creation, ordering, membership, and transactional cleanup. The
testing-block service owns canonical weekday/HH:mm keys, plain-versus-special
assignment mapping, explicit replacement conflicts, testing-class validation,
UTF-8 rooms, typed SQLite row errors, and assignment rollback. The retained Qt
testing-class and testing-block repositories now convert through UTF-8 and use
cached file-backed engine connections; localized diagnostics and Qt-facing
containers remain adapters. Their Qt fixtures use temporary profiles because a
separate engine connection cannot share Qt `:memory:` storage.

The next product slice is Qt-free `Teacher` plus `TeacherValidator` and the
validated `TeacherService` use-case boundary. It preserves the existing
English/Korean name normalization rules, preferred-name derivation, Korean
phone formatting, birthday and enum validation, UTF-8 persistence, typed
errors, and transactional cleanup of `class_info.teacher_id` on deletion.

The teacher-directory slice now also includes Qt-free
`NativeEnglishTeacherService` and `GsTeamService` boundaries. They preserve
the existing directory normalization, position-priority ordering,
case-insensitive duplicate-name rules, parameterized upserts/deletes, and
atomic save behavior for the native-English and GS-team tables.

The class-information slice now includes Qt-free `ClassTime` and `ClassInfo`
models, the canonical grade/level/book configuration, schedule and class-info
validators, and a validated `ClassInfoService` boundary. It preserves
case-insensitive choice normalization, strict 24-hour and 12-hour schedule
parsing, duplicate-slot and interval-order diagnostics, color and note-length
rules, teacher-join read fields, empty-class defaults, and one transaction for
the class row plus regular and intensive time rows.

The schedule read slice now also includes a Qt-free `ClassScheduleService`.
It provides regular-class teacher assignments, renderer-neutral schedule
snapshots with regular and intensive times, testing-class filtering, and
typed conflict detection for candidate and stored intervals, including
overnight handling and the existing class-name fallback.

The schedule-builder slice now includes a Qt-free `ScheduleBuilderService`.
It owns schedule-time parsing, visible-day filtering, regular versus intensive
range selection, `:05`/`:55` offset handling, and renderer-neutral entry
assembly. The retained Qt `ScheduleBuilder` only loads the existing class
information service and converts values at the presentation boundary.

The portable naming slice now includes `ClassNamingService` for stable
class/teacher labels and ordering. The retained Qt `SidebarNodeNaming` file is
now a conversion adapter, so package planning and the existing Qt screens use
the same UTF-8 naming rules.

The class-tab navigation slice now includes a Qt-free
`ClassTabNavigationService` for adaptive/forced grade grouping, catalog-ordered
class tabs, schedule and day-label formatting, duplicate-label disambiguation,
and regular/intensive day filtering. The retained Qt model keeps its existing
`QList`/`QSet` API and translation behavior while converting values at the
presentation boundary.

The evaluation-default selection slice now includes a Qt-free
`EvaluationDefaultSelection` policy for term-name mapping, current/previous
term fallback, populated-row detection, and M1/M2/M3 school-level
classification. The retained Qt helpers convert terms and evaluation rows
through UTF-8 while keeping calendar lookup, settings policy, service
availability, and localized presentation at the adapter edge. The portable
blank-value contract treats standard ASCII whitespace as blank.

The upcoming-birthday slice now includes a Qt-free
`UpcomingBirthdaySchedule`. It owns birthday parsing, the today/this-week/
next-week date windows, calendar-year rollover, non-leap-year February 29
fallback, staff grouping, display-name fallback, and deterministic ordering.
The retained Qt schedule converts the three staff collections and keeps the
dialog presentation in Qt.

The schedule-import slice now includes Qt-free workbook-neutral models and a
`ScheduleImportService`. The retained workbook parser remains a Qt adapter;
the engine owns teacher/class match ranking, course meeting-pattern rules,
plan validation, read-only review preflight, stale-target checks, normal
snapshot replacement, intensive preserve/replace modes, intensive slot-state
snapshots, profile-name policy, and transactional schedule writes.

The class-transfer slice now includes Qt-free package models and a
`ClassTransferService`. It preserves versioned teacher/class keys, package
shape validation, teacher and class match previews, create/replace/skip
resolution rules, regular/intensive schedule preflight, roster and speaking
evaluation child-table transfer, export identity stripping, and one
transaction for teacher, class, and child-row writes. The retained Qt JSON and
file codec remains an adapter at this boundary; it does not own import rules.

The class-analytics slice now includes a Qt-free `SpeakingAnalyticsService`
for evaluation-name discovery, grade conversion and rounding, consolidated
criterion distributions, class-shape summaries, roster filtering, student
rankings, and year-to-date points. The portable student-name normalization
service owns the shared English/Korean matching rules. The retained Qt
analytics service and student-name utility now convert values at the edge;
the ranking table, charts, localization, and widget behavior remain Qt-owned.

The roster-model/validation slice now includes Qt-free `Roster` and
`RosterValidator` boundaries. It owns canonical English/Korean/season columns
(including the `Autumn` to `Fall` alias), UTF-8 whitespace normalization,
column and row limits, required names, English/Korean shape and length rules,
duplicate-pair detection, and row/column diagnostics. The retained Qt
validator converts `QString` values and issues at the edge; the table model,
editing behavior, and presentation remain Qt-owned.

The roster-persistence slice now includes a Qt-free `RosterService` for
class-scoped roster-column and sparse-cell loading and saving. It preserves
UTF-8 cell values, declared-column width fallback, replacement semantics, and
transactional rollback while returning typed invalid-class and missing-class
errors. The service deliberately leaves roster validation and text policy to
the existing `RosterValidator`. The retained Qt `RosterRepository` now converts
through UTF-8 and routes its file-backed load, save, student-count, and batch
save operations through the engine service. The engine also exposes one
transactional batch operation so the retained Qt all-or-nothing save contract
is preserved; localized operation context remains at the Qt adapter edge.

The speaking-evaluation report slice now includes a Qt-free
`SpeakingEvaluationReportService` for the six-metric overall-grade rule. It
preserves the retained `C` through `A+` mapping, the 0.4 fractional-average
rounding threshold, and `N/A` handling for incomplete or invalid metrics. The
retained report assembler, report widget, and roster score-import path now
delegate this calculation to the engine; rendering, PDF, and PowerPoint
resource/drawing details remain presentation adapters.

The schedule-report slice now includes a Qt-free `ScheduleReportService` and
renderer-neutral schedule model. It preserves visible-day ordering, regular
and intensive slot defaults, persisted overrides, intensive outer-row
trimming, testing-mode suppression and assignments, summary counts,
teacher-room labels, and deterministic 12/24-hour range labels. The retained
Qt schedule view model is now a conversion adapter; schedule PDF and sub-prep
drawing remain Qt-owned.

The roster-report slice now includes a Qt-free `RosterReportService` for the
by-day, daily, and per-class-with-extra-info cell-value models. It preserves
time-slot mapping, duplicate-slot diagnostics, stable daily ordering and
overflow page keys, teacher/room and Zoom fallback labels, student limits,
extra-column filtering/caps, and UTF-8 report values. The retained Qt roster
service converts domain data into the engine model; PDF geometry, templates,
and drawing remain Qt-owned.

The roster-report template-policy slice now includes a Qt-free
`RosterReportTemplateService` for stable template ordering, report orientation,
and all/current/selected class-id resolution. The retained Qt roster-print
service converts its `TemplateId` and scope values at the boundary while
keeping localized titles, page-size selection, PDF rendering, and drawing
presentation-owned.

The sub-prep report slice now includes a Qt-free
`SubPrepClassInformationService` for meeting-time compaction, visible-class
filtering, teacher grouping, grade/level/time ordering, and renderer-neutral
class details. The retained Qt model converts its value types at the edge;
localized day abbreviations, drawing, and PDF concerns remain
presentation-owned. The same slice
also includes a Qt-free `SubPrepPaginationService` for teacher-section
page-span detection, the
"Sub Notes" new-page threshold, and fallback placement on the last document
page. The retained Qt renderer delegates these decisions through a small
adapter while keeping text measurement, page counting, PDF geometry, and
drawing Qt-owned.

The Sub Prep package-planning slice now includes a Qt-free
`SubPrepPackageService`. It owns selected-date normalization and weekday
selection, class filtering for regular versus intensive schedules, stable
class ordering, Windows-safe unique class-folder names, roster document names,
and ordered relative document paths. The retained Qt package service converts
loaded records and delegates the plan, while filesystem staging, PDF rendering,
printing, atomic replacement, and localized messages remain Qt-owned.

The Sub Prep document-model slice now includes a Qt-free
`SubPrepDocumentService` aggregate for campus and Zoom information,
instructional text, schedule-report models, and nested teacher/class details.
The retained Qt document model converts the complete value graph through UTF-8
and delegates aggregate construction while the PDF renderer keeps text
measurement, pagination placement, geometry, and drawing presentation-owned.

The document-catalog slice now includes a Qt-free `DocumentCatalogService` for
locale fallback, UTF-8-safe identifier and relative-path/file-name/order
validation, parent-path derivation, duplicate and reachability filtering, and
renderer-neutral catalog model construction. The retained Qt parser keeps JSON
shape/type checks, filesystem and resource-root existence checks, absolute-path
reconstruction, active/embedded-root fallback, and localized diagnostics at the
adapter edge.

The speaking-evaluation grid slice now includes a Qt-free
`SpeakingEvaluationValidator` for score aliases, row normalization, structural
limits, student-name/score/comment/note rules, duplicate-name diagnostics, and
the configurable questionable Korean-length warning policy. The retained Qt
validator keeps its public API and delegates through a UTF-8 conversion adapter.

The report batch archive slice now includes a Qt-free `ZipArchiveWriter` for
stored standard-ZIP construction, UTF-8 entry names, CRC-32, DOS timestamps,
size/count/name validation, and atomic temporary-output replacement. The
retained Qt helper keeps its public API and localized diagnostics while
delegating archive construction to the engine.

The shared document-output slice now includes a Qt-free
`classmngr::engine::DocumentOutputResult` for output-status semantics and
UTF-8 message, PDF-path, and archive-path values. The retained Qt result model
keeps its public shape and converts to and from the engine contract at the
adapter boundary; rendering, filesystem commits, and localized presentation
remain platform-owned.

The academic-calendar slice now includes a Qt-free `AcademicCalendarSchedule`
model using standard-library calendar dates. It preserves default elementary
and middle-school term lengths, custom-year rollover, previous-fall
continuity, term/week lookup, saved-schedule validation, and the existing
settings semantics. The retained Qt schedule class is now a conversion and
JSON adapter; locale formatting and settings access remain Qt-owned.

The calendar-event slice now includes a Qt-free `CalendarEventRules`
boundary for exact event-type/time-status normalization, start-of-term
recognition, and literal campus-code token matching. The retained Qt event
model and filter delegate through UTF-8 adapters; event storage, date/time
values, and UI presentation remain in the existing adapters.

The calendar-event validation slice now includes a Qt-free `CalendarEvent`
model and `CalendarEventValidator` for ASCII string normalization, UTF-8
length limits, date/time consistency, recurrence bounds with deterministic
month-end stepping, and repeat-series caps. The retained Qt validator keeps
its public API and delegates through `QDate`/`QTime` and UTF-8 conversions.

The calendar-event persistence slice now includes a Qt-free
`CalendarEventService` for validated CRUD, date/range/upcoming/repeat-series
queries, ISO date and HH:mm row mapping, and transactional batch writes. The
retained Qt `CalendarEventRepository` delegates all eleven operations through
explicit UTF-8/date/time conversions while preserving its public result and
diagnostic shapes. File-backed adapter fixtures are used because the Qt and
engine SQLite connections are separate connections and cannot share a Qt
`:memory:` database.

The intensive-slot-state persistence slice now includes a Qt-free
`IntensiveSlotStateService` for ordered UTF-8 state reads, default-state
deletion, and upsert persistence with typed SQLite errors. The retained Qt
`IntensiveSlotStateRepository` preserves its Qt-facing result and diagnostic
shapes while converting through UTF-8 and delegating through a cached,
file-backed engine connection. Its adapter fixtures use temporary profiles
because the Qt and engine SQLite connections are separate connections and
cannot share a Qt `:memory:` database.

The teacher-import slice now includes a Qt-free `TeacherImportService` and
UTF-8 standard-library import models. It owns source-date validation,
Hangul-only Korean matching, case-insensitive directory matching, blank-field
preservation, typed ambiguity/duplicate diagnostics, transactional writes,
and monotonic `teacher_import/latest_source_date` persistence across Korean
teachers, Native English teachers, and GS Team members. The retained Qt
`TeacherImportRepository` converts the existing Qt plan and summary through
the boundary and uses a cached file-backed engine connection; parser and
localized presentation behavior remain Qt-owned.

The speaking-evaluation report boundary now includes a Qt-free metadata model
for elementary-grade parsing, class labels, advanced-template selection, and
deterministic report dates. Its output-policy companion owns schedule-aware
destination folders and batch archive paths; Qt supplies standard-path lookup,
localized fallbacks, and Unicode-safe student filenames at the adapter edge.

The speaking-evaluation content service now owns row-to-report assembly:
blank-student filtering, display-name normalization, source-row identity,
student/class/teacher fields, score and comment transfer, and composition with
the shared metadata model. The retained Qt dialog only converts `QStringList`
rows, supplies the current date and signature bytes, and creates widget/report
types.

The speaking-evaluation AI prompt service now owns observation-line
normalization, student/classmate name redaction, prompt eligibility and
composition, and batch-response marker parsing. The retained Qt AI prompt
callers only convert Qt values and expose the engine results to the dialogs.

The speaking-evaluation template-policy slice now owns the Standard and
Advanced template enum, page and signature geometry, PowerPoint resource
identifiers, score-table placement and shape, and neutral fill colors. The
retained Qt template header and PowerPoint job model convert that policy to
Qt geometry, resource-pack paths, and automation arguments; asset decoding,
text measurement, JSON transport, and drawing remain adapters.

The speaking-evaluation batch-report policy now owns report-count, output-mode,
PDF-destination, and exact-file validation, PowerPoint template-homogeneity
checks, and archive/individual-output decisions. The retained Qt batch service
converts its request and maps typed policy failures to localized messages while
retaining filesystem commits, PDF rendering, printing, and Office automation.

The speaking-evaluation PowerPoint job service now owns renderer-neutral job
content assembly: report-field mapping, UTF-8 paths and signature bytes,
overall-grade calculation, path-count validation, and homogeneous template
validation. The retained Qt job model converts the engine job to Qt values and
continues to own NFC normalization, comment text measurement, resource paths,
JSON serialization, and Office automation arguments.

The P2-01 speaking-report handoff is now complete. The retained Qt batch
service converts its edited `StudentReport` values once into the engine
`SpeakingEvaluationReportContent` model; the internal PDF renderer and
PowerPoint job model consume that typed content, with a Qt conversion only at
the PDF widget edge. Signature bytes remain an explicit Qt presentation input.
The existing engine-backed schedule, roster, and sub-prep report adapters
complete the other P2-01 slices. The focused retained-Qt batch-report target
built and passed 1/1 offscreen on Windows x64 Debug.

The P2-02 portable file/output contract is now implemented by the Qt-free
`classmngr::engine::FileSystem` and `StandardFileSystem`. It defines stable
UTF-8 path text, arbitrary byte reads and writes, directory creation,
temporary-directory staging and cleanup, existence checks, atomic file and
directory replacement, and file copy with typed invalid-path, missing-source,
partial-output, replacement, and cleanup errors. The retained Qt
`DataService::saveAs()` and `exportAs()` operations use atomic engine file
copy. Speaking-evaluation report commits use engine existence, copy, remove,
and atomic-replacement operations; Sub Prep uses engine directory creation,
temporary staging, cleanup, and directory replacement. The engine ZIP writer
keeps its streaming archive encoder in the portable layer and now uses the
shared engine filesystem contract for final archive replacement. Qt retains
path composition, localized diagnostics, PDF drawing/printing, resource
mapping, and Office automation.

Validation for this slice:

- `ClassMngrEngineFileSystemTests` built and passed on Windows x64 Debug,
  covering UTF-8 names, arbitrary bytes, parent creation, atomic file and
  directory replacement, missing/non-regular sources, and cleanup.
- `ClassMngrEngineZipArchiveWriterTests` built and passed on Windows x64
  Debug after the shared finalization handoff.
- The focused `applicationServicesOwnDatabaseFileOperations` case in
  `ClassMngrDataServiceLifecycleTests` built and passed on Windows x64 Debug,
  covering save-as, export, reopen, and closed-session failures.
- `ClassMngrSpeakingEvalBatchReportServiceTests` and
  `ClassMngrSubPrepPackageServiceTests` built and passed 1/1 offscreen on
  Windows x64 Debug.
- `git diff --check` passed. The lifecycle coverage identified two retained-
  compatibility fixture assumptions; P2-03 corrected the catalog values and
  legacy schema, and the complete registered lifecycle test now passes.

The portable speaking-evaluation output policy now also owns student PDF
filename composition, reserved-name protection, unsafe-character replacement,
case-insensitive suffix normalization, fallback naming, and the UTF-8 length
limit. It also plans the ordered batch filename list and rejects generated
name collisions before rendering. Qt normalizes its native text before calling
the policy; directory creation, filesystem collision checks, and atomic file
commits remain Qt-owned.

The schedule-report service now also owns the deterministic class-line format
used by schedule cells and PDF output, plus the bilingual Excel day labels and
Excel time-range compaction. Qt keeps font selection, colors, page geometry,
and drawing in the retained renderer.

The Qt-free engine now also exercises the committed eleven-case database-port
fixture corpus. The fixture test opens current and legacy profiles, verifies
representative bilingual class-transfer payloads plus calendar/campus rows,
checks migration backups and failure rollback, and writes, closes, reopens,
and imports a typical profile through the engine boundaries. Historical fixture
values that are readable but not valid for a new validated write are
canonicalized only in the test write payload, matching the existing engine
validation contract.

The retained Qt fixture verifier now adds explicit two-direction checks without
modifying the committed corpus: a temporary Qt `DataService` profile is opened
and inspected through the engine, and a temporary engine-written profile is
opened and inspected through Qt `QSQLITE` queries. Both checks assert the
schema version and bilingual teacher/class values.

The application-settings slice now has a Qt-free
`ApplicationSettingsService` for prepared upserts, SQLite value conversion,
single reads, and transactional batch rollback. The retained Qt
`SettingsRepository` converts its QVariant API through a cached file-backed
engine connection; teacher-import and schedule-import app-settings access now
uses the same service. The existing `app_settings.value TEXT` schema is
preserved, including its numeric-to-text affinity behavior.

The personal-details slice now includes a Qt-free `PersonalDetailsService`
over `ApplicationSettingsService`. It owns UTF-8 name/campus/Zoom settings,
`N/A` defaults, legacy `subPrep/...` fallback and promotion, signature
mode/font/text and opaque base64 storage, campus-only updates, and
transactional nine-setting saves. The retained Qt
`PersonalDetailsRepository` converts `QString` and image values at the edge
and delegates file-backed reads and writes through the engine; Qt retains
signature-image preparation and its existing localized/default API behavior.

The retained Qt `DatabaseSession` now routes ordinary file-backed profile path
preparation and schema migration through engine `OpenDatabase` before creating
the Qt `QSQLITE` connection. Exact `:memory:` sessions retain the Qt schema
compatibility path, and the Qt connection still enables foreign-key
enforcement. The focused lifecycle coverage verifies a legacy file-backed
profile is migrated and readable through the retained Qt connection.

The P2-03 retained-adapter audit is now complete. All sixteen retained Qt
repository implementations include the engine boundary and contain no direct
`QSqlQuery`/Qt-SQL persistence path; `DataService` likewise remains a facade
over the session repositories. `DatabaseSession` documents the split between
engine-owned file-backed preflight/migration and the temporary Qt repository
connection, with exact `:memory:` handling retained as the compatibility case.
`ApplicationServices` now wires production feature services with the session
only; the legacy `DataService*` constructors and fallback branches are
documented and remain available for direct migration callers and tests.
The lifecycle fixtures now use canonical engine catalog values and a complete
legacy teacher schema, while retained class-conflict errors preserve the
historical not-found message contract.

Validation for P2-03:

- `ClassMngrDataServiceLifecycleTests` built and passed 15/15 cases through
  registered CTest on Windows x64 Debug.
- The lifecycle coverage includes session/repository lifetime, production
  ApplicationServices wiring, file-backed engine preflight, rollback/error
  propagation, and legacy-schema migration.
- The repository and DataService Qt-SQL audit passed with no direct Qt-SQL
  persistence implementations found in the retained adapters.
- `git diff --check` passed.

## Local validation

| Lane | Result |
| --- | --- |
| Windows x64 Debug engine tests | Passed: `ClassMngrEngineTests` and `ClassMngrEngineDatabaseFileFormatTests` |
| Windows x64 Release engine tests | Passed: `ClassMngrEngineTests` and `ClassMngrEngineDatabaseFileFormatTests` |
| Windows x86 Debug engine tests | Passed: `ClassMngrEngineTests` and `ClassMngrEngineDatabaseFileFormatTests` |
| Windows x86 Release engine tests | Passed: `ClassMngrEngineTests` and `ClassMngrEngineDatabaseFileFormatTests` |
| Retained Windows Qt file-format test | Passed: `ClassMngrDatabaseFileFormatTests` |
| Windows x64 Debug SQLite foundation test | Passed: `ClassMngrEngineSqliteDatabaseTests` |
| Windows x64 Release SQLite foundation test | Passed: `ClassMngrEngineSqliteDatabaseTests` |
| Windows x86 Debug SQLite foundation test | Passed: `ClassMngrEngineSqliteDatabaseTests` |
| Windows x86 Release SQLite foundation test | Passed: `ClassMngrEngineSqliteDatabaseTests` |
| Windows x64 Debug portable filesystem test | Passed: `ClassMngrEngineFileSystemTests` |
| Windows x64 Debug ZIP finalization regression | Passed: `ClassMngrEngineZipArchiveWriterTests` |
| Windows x64 Debug DataService save/export case | Passed: `applicationServicesOwnDatabaseFileOperations` in `ClassMngrDataServiceLifecycleTests` |
| Windows x64 Debug speaking-report output regression | Passed: `ClassMngrSpeakingEvalBatchReportServiceTests` (1/1 offscreen) |
| Windows x64 Debug Sub Prep staging regression | Passed: `ClassMngrSubPrepPackageServiceTests` (1/1 offscreen) |
| Windows x64 Debug schema/OpenDatabase test | Passed: `ClassMngrEngineDatabaseSchemaTests` |
| Windows x64 Release schema/OpenDatabase test | Passed: `ClassMngrEngineDatabaseSchemaTests` |
| Windows x86 Debug schema/OpenDatabase test | Passed: `ClassMngrEngineDatabaseSchemaTests` |
| Windows x86 Release schema/OpenDatabase test | Passed: `ClassMngrEngineDatabaseSchemaTests` |
| Windows x64 Debug class CRUD test | Passed: `ClassMngrEngineClassRepositoryTests` |
| Windows x64 Release class CRUD test | Passed: `ClassMngrEngineClassRepositoryTests` |
| Windows x86 Debug class CRUD test | Passed: `ClassMngrEngineClassRepositoryTests` |
| Windows x86 Release class CRUD test | Passed: `ClassMngrEngineClassRepositoryTests` |
| Windows x64 Debug testing-class service test | Passed: `ClassMngrEngineTestingClassServiceTests` |
| Windows x64 Release testing-class service test | Passed: `ClassMngrEngineTestingClassServiceTests` |
| Windows x86 Debug testing-class service test | Passed: `ClassMngrEngineTestingClassServiceTests` |
| Windows x86 Release testing-class service test | Passed: `ClassMngrEngineTestingClassServiceTests` |
| Windows x64 Debug testing-block service test | Passed: `ClassMngrEngineTestingBlockServiceTests` |
| Windows x64 Release testing-block service test | Passed: `ClassMngrEngineTestingBlockServiceTests` |
| Windows x86 Debug testing-block service test | Passed: `ClassMngrEngineTestingBlockServiceTests` |
| Windows x86 Release testing-block service test | Passed: `ClassMngrEngineTestingBlockServiceTests` |
| Retained Windows Qt testing-class repository regression | Passed: `ClassMngrTestingClassRepositoryTests` |
| Retained Windows Qt testing-block repository regression | Passed: `ClassMngrTestingBlockRepositoryTests` |
| Windows x64 Debug campus-record service test | Passed: direct VS 2026/v145 compile, link, and run; CMake target build is blocked by the existing MSBuild FileTracker access failure |
| Windows x64 Release campus-record service test | Passed: direct VS 2026/v145 compile, link, and run |
| Windows x86 Debug campus-record service test | Passed: direct VS 2026/v145 compile, link, and run; CMake target build is blocked by the existing MSBuild FileTracker access failure |
| Windows x86 Release campus-record service test | Passed: direct VS 2026/v145 compile, link, and run |
| Retained Windows Qt campus-record adapter smoke | Passed: Qt 6.11/MSVC file-backed UTF-8 save/load/update/list/delete link-level smoke |
| Windows x64 application-settings service test | Passed: direct VS 2026/v145 compile, link, and run |
| Windows x86 application-settings service test | Passed: direct VS 2026/v145 compile, link, and run |
| Retained Windows Qt settings adapter smoke | Passed: Qt 6.11/MSVC file-backed UTF-8 save/load, QVariant conversion, and cross-connection batch-rollback link-level smoke |
| Application-settings import call-site compile check | Passed: direct VS 2026/v145 x64 and x86 compile-only checks for teacher-import and schedule-import services |
| Windows x64 Debug personal-details service test | Passed: direct VS 2026/v145 compile, link, and run |
| Windows x86 Debug personal-details service test | Passed: direct VS 2026/v145 compile, link, and run |
| Retained Windows Qt personal-details adapter/lifecycle smoke | Passed: Qt 6.11/MSVC file-backed UTF-8 save/load/campus link-level smoke with `QT_QPA_PLATFORM=offscreen` |
| Retained Windows Qt DatabaseSession boundary | Passed: file-backed engine preflight/migration and Qt repository lifetime coverage in `ClassMngrDataServiceLifecycleTests` |
| Retained Windows Qt/ApplicationServices adapter audit | Passed: all sixteen repository implementations use engine adapters; production ApplicationServices wiring is session-only; full lifecycle CTest passed 15/15 |
| Windows x64 Debug teacher model/validator/use-case test | Passed: `ClassMngrEngineTeacherServiceTests` |
| Windows x64 Release teacher model/validator/use-case test | Passed: `ClassMngrEngineTeacherServiceTests` |
| Windows x86 Debug teacher model/validator/use-case test | Passed: `ClassMngrEngineTeacherServiceTests` |
| Windows x86 Release teacher model/validator/use-case test | Passed: `ClassMngrEngineTeacherServiceTests` |
| Windows x64 Debug class/teacher naming test | Passed: `ClassMngrEngineClassNamingServiceTests` |
| Windows x64 Release class/teacher naming test | Passed: `ClassMngrEngineClassNamingServiceTests` |
| Windows x86 Debug class/teacher naming test | Passed: `ClassMngrEngineClassNamingServiceTests` |
| Windows x86 Release class/teacher naming test | Passed: `ClassMngrEngineClassNamingServiceTests` |
| Windows x64 Debug upcoming-birthday schedule test | Passed: `ClassMngrEngineUpcomingBirthdayScheduleTests` |
| Windows x64 Release upcoming-birthday schedule test | Passed: `ClassMngrEngineUpcomingBirthdayScheduleTests` |
| Windows x86 Debug upcoming-birthday schedule test | Passed: `ClassMngrEngineUpcomingBirthdayScheduleTests` |
| Windows x86 Release upcoming-birthday schedule test | Passed: `ClassMngrEngineUpcomingBirthdayScheduleTests` |
| Windows x64 Debug native-English directory test | Passed: `ClassMngrEngineNativeEnglishTeacherServiceTests` |
| Windows x64 Release native-English directory test | Passed: `ClassMngrEngineNativeEnglishTeacherServiceTests` |
| Windows x86 Debug native-English directory test | Passed: `ClassMngrEngineNativeEnglishTeacherServiceTests` |
| Windows x86 Release native-English directory test | Passed: `ClassMngrEngineNativeEnglishTeacherServiceTests` |
| Windows x64 Debug GS-team directory test | Passed: `ClassMngrEngineGsTeamServiceTests` |
| Windows x64 Release GS-team directory test | Passed: `ClassMngrEngineGsTeamServiceTests` |
| Windows x86 Debug GS-team directory test | Passed: `ClassMngrEngineGsTeamServiceTests` |
| Windows x86 Release GS-team directory test | Passed: `ClassMngrEngineGsTeamServiceTests` |
| Windows x64 Debug class-information service test | Passed: `ClassMngrEngineClassInfoServiceTests` |
| Windows x64 Release class-information service test | Passed: `ClassMngrEngineClassInfoServiceTests` |
| Windows x86 Debug class-information service test | Passed: `ClassMngrEngineClassInfoServiceTests` |
| Windows x86 Release class-information service test | Passed: `ClassMngrEngineClassInfoServiceTests` |
| Windows x64 Debug class-schedule service test | Passed: `ClassMngrEngineClassScheduleServiceTests` |
| Windows x64 Release class-schedule service test | Passed: `ClassMngrEngineClassScheduleServiceTests` |
| Windows x86 Debug class-schedule service test | Passed: `ClassMngrEngineClassScheduleServiceTests` |
| Windows x86 Release class-schedule service test | Passed: `ClassMngrEngineClassScheduleServiceTests` |
| Windows x64 Debug schedule-builder service test | Passed: `ClassMngrEngineScheduleBuilderServiceTests` |
| Windows x64 Release schedule-builder service test | Passed: `ClassMngrEngineScheduleBuilderServiceTests` |
| Windows x86 Debug schedule-builder service test | Passed: `ClassMngrEngineScheduleBuilderServiceTests` |
| Windows x86 Release schedule-builder service test | Passed: `ClassMngrEngineScheduleBuilderServiceTests` |
| Windows x64 Debug schedule-import service test | Passed: `ClassMngrEngineScheduleImportServiceTests` |
| Windows x64 Release schedule-import service test | Passed: `ClassMngrEngineScheduleImportServiceTests` |
| Windows x86 Debug schedule-import service test | Passed: `ClassMngrEngineScheduleImportServiceTests` |
| Windows x86 Release schedule-import service test | Passed: `ClassMngrEngineScheduleImportServiceTests` |
| Windows x64 Debug class-transfer service test | Passed: `ClassMngrEngineClassTransferServiceTests` |
| Windows x64 Release class-transfer service test | Passed: `ClassMngrEngineClassTransferServiceTests` |
| Windows x86 Debug class-transfer service test | Passed: `ClassMngrEngineClassTransferServiceTests` |
| Windows x86 Release class-transfer service test | Passed: `ClassMngrEngineClassTransferServiceTests` |
| Windows x64 Debug class-analytics service test | Passed: `ClassMngrEngineSpeakingAnalyticsTests` |
| Windows x64 Release class-analytics service test | Passed: `ClassMngrEngineSpeakingAnalyticsTests` |
| Windows x86 Debug class-analytics service test | Passed: `ClassMngrEngineSpeakingAnalyticsTests` |
| Windows x86 Release class-analytics service test | Passed: `ClassMngrEngineSpeakingAnalyticsTests` |
| Windows x64 Debug roster validation test | Passed: `ClassMngrEngineRosterValidatorTests` |
| Windows x64 Release roster validation test | Passed: `ClassMngrEngineRosterValidatorTests` |
| Windows x86 Debug roster validation test | Passed: `ClassMngrEngineRosterValidatorTests` |
| Windows x86 Release roster validation test | Passed: `ClassMngrEngineRosterValidatorTests` |
| Windows x64 Debug roster persistence test | Passed: `ClassMngrEngineRosterServiceTests` |
| Windows x64 Release roster persistence test | Passed: `ClassMngrEngineRosterServiceTests` |
| Windows x86 Debug roster persistence test | Passed: `ClassMngrEngineRosterServiceTests` |
| Windows x86 Release roster persistence test | Passed: `ClassMngrEngineRosterServiceTests` |
| Retained Windows Qt roster persistence adapter regression | Passed: `ClassMngrDataServiceLifecycleTests` with schema-error, UTF-8, replacement, single-save rollback, batch rollback, and student-count coverage |
| Windows x64 Debug speaking-evaluation report service test | Passed: `ClassMngrEngineSpeakingEvaluationReportServiceTests` |
| Windows x64 Release speaking-evaluation report service test | Passed: `ClassMngrEngineSpeakingEvaluationReportServiceTests` |
| Windows x86 Debug speaking-evaluation report service test | Passed: `ClassMngrEngineSpeakingEvaluationReportServiceTests` |
| Windows x86 Release speaking-evaluation report service test | Passed: `ClassMngrEngineSpeakingEvaluationReportServiceTests` |
| Windows x64 Debug speaking-evaluation report model test | Passed: `ClassMngrEngineSpeakingEvaluationReportModelTests` |
| Windows x64 Release speaking-evaluation report model test | Passed: `ClassMngrEngineSpeakingEvaluationReportModelTests` |
| Windows x86 Debug speaking-evaluation report model test | Passed: `ClassMngrEngineSpeakingEvaluationReportModelTests` |
| Windows x86 Release speaking-evaluation report model test | Passed: `ClassMngrEngineSpeakingEvaluationReportModelTests` |
| Windows x64 Debug speaking-evaluation output/filename-policy test | Passed: `ClassMngrEngineSpeakingEvaluationReportOutputPolicyTests` |
| Windows x64 Release speaking-evaluation output/filename-policy test | Passed: `ClassMngrEngineSpeakingEvaluationReportOutputPolicyTests` |
| Windows x86 Debug speaking-evaluation output/filename-policy test | Passed: `ClassMngrEngineSpeakingEvaluationReportOutputPolicyTests` |
| Windows x86 Release speaking-evaluation output/filename-policy test | Passed: `ClassMngrEngineSpeakingEvaluationReportOutputPolicyTests` |
| Windows x64 Debug speaking-evaluation content test | Passed: `ClassMngrEngineSpeakingEvaluationReportContentTests` |
| Windows x64 Release speaking-evaluation content test | Passed: `ClassMngrEngineSpeakingEvaluationReportContentTests` |
| Windows x86 Debug speaking-evaluation content test | Passed: `ClassMngrEngineSpeakingEvaluationReportContentTests` |
| Windows x86 Release speaking-evaluation content test | Passed: `ClassMngrEngineSpeakingEvaluationReportContentTests` |
| Windows x64 Debug speaking-evaluation AI prompt test | Passed: `ClassMngrEngineSpeakingEvaluationAiPromptTests` |
| Windows x64 Release speaking-evaluation AI prompt test | Passed: `ClassMngrEngineSpeakingEvaluationAiPromptTests` |
| Windows x86 Debug speaking-evaluation AI prompt test | Passed: `ClassMngrEngineSpeakingEvaluationAiPromptTests` |
| Windows x86 Release speaking-evaluation AI prompt test | Passed: `ClassMngrEngineSpeakingEvaluationAiPromptTests` |
| Windows x64 Debug speaking-evaluation template-policy test | Passed: `ClassMngrEngineSpeakingEvaluationReportTemplateTests` |
| Windows x64 Release speaking-evaluation template-policy test | Passed: `ClassMngrEngineSpeakingEvaluationReportTemplateTests` |
| Windows x86 Debug speaking-evaluation template-policy test | Passed: `ClassMngrEngineSpeakingEvaluationReportTemplateTests` |
| Windows x86 Release speaking-evaluation template-policy test | Passed: `ClassMngrEngineSpeakingEvaluationReportTemplateTests` |
| Windows x64 Debug speaking-evaluation batch-report policy test | Passed: `ClassMngrEngineSpeakingEvaluationBatchReportPolicyTests` |
| Windows x64 Release speaking-evaluation batch-report policy test | Passed: `ClassMngrEngineSpeakingEvaluationBatchReportPolicyTests` |
| Windows x86 Debug speaking-evaluation batch-report policy test | Passed: `ClassMngrEngineSpeakingEvaluationBatchReportPolicyTests` |
| Windows x86 Release speaking-evaluation batch-report policy test | Passed: `ClassMngrEngineSpeakingEvaluationBatchReportPolicyTests` |
| Windows x64 Debug speaking-evaluation PowerPoint job test | Passed: `ClassMngrEngineSpeakingEvaluationPowerPointJobServiceTests` |
| Windows x64 Release speaking-evaluation PowerPoint job test | Passed: `ClassMngrEngineSpeakingEvaluationPowerPointJobServiceTests` |
| Windows x86 Debug speaking-evaluation PowerPoint job test | Passed: `ClassMngrEngineSpeakingEvaluationPowerPointJobServiceTests` |
| Windows x86 Release speaking-evaluation PowerPoint job test | Passed: `ClassMngrEngineSpeakingEvaluationPowerPointJobServiceTests` |
| Windows x64 Debug database fixture round-trip test | Passed: `ClassMngrEngineDatabaseFixtureRoundTripTests` |
| Windows x64 Release database fixture round-trip test | Passed: `ClassMngrEngineDatabaseFixtureRoundTripTests` |
| Windows x86 Debug database fixture round-trip test | Passed: `ClassMngrEngineDatabaseFixtureRoundTripTests` |
| Windows x86 Release database fixture round-trip test | Passed: `ClassMngrEngineDatabaseFixtureRoundTripTests` |
| Windows x64 Debug schedule report service test | Passed: `ClassMngrEngineScheduleReportServiceTests` |
| Windows x64 Release schedule report service test | Passed: `ClassMngrEngineScheduleReportServiceTests` |
| Windows x86 Debug schedule report service test | Passed: `ClassMngrEngineScheduleReportServiceTests` |
| Windows x86 Release schedule report service test | Passed: `ClassMngrEngineScheduleReportServiceTests` |
| Windows x64 Debug roster report service test | Passed: `ClassMngrEngineRosterReportServiceTests` |
| Windows x64 Release roster report service test | Passed: `ClassMngrEngineRosterReportServiceTests` |
| Windows x86 Debug roster report service test | Passed: `ClassMngrEngineRosterReportServiceTests` |
| Windows x86 Release roster report service test | Passed: `ClassMngrEngineRosterReportServiceTests` |
| Windows x64 Debug roster report template-policy test | Passed: `ClassMngrEngineRosterReportTemplateTests` |
| Windows x64 Release roster report template-policy test | Passed: `ClassMngrEngineRosterReportTemplateTests` |
| Windows x86 Debug roster report template-policy test | Passed: `ClassMngrEngineRosterReportTemplateTests` |
| Windows x86 Release roster report template-policy test | Passed: `ClassMngrEngineRosterReportTemplateTests` |
| Windows x64 Debug sub-prep pagination service test | Passed: `ClassMngrEngineSubPrepPaginationTests` |
| Windows x64 Release sub-prep pagination service test | Passed: `ClassMngrEngineSubPrepPaginationTests` |
| Windows x86 Debug sub-prep pagination service test | Passed: `ClassMngrEngineSubPrepPaginationTests` |
| Windows x86 Release sub-prep pagination service test | Passed: `ClassMngrEngineSubPrepPaginationTests` |
| Windows x64 Debug sub-prep class-information service test | Passed: `ClassMngrEngineSubPrepClassInformationServiceTests` |
| Windows x64 Release sub-prep class-information service test | Passed: `ClassMngrEngineSubPrepClassInformationServiceTests` |
| Windows x86 Debug sub-prep class-information service test | Passed: `ClassMngrEngineSubPrepClassInformationServiceTests` |
| Windows x86 Release sub-prep class-information service test | Passed: `ClassMngrEngineSubPrepClassInformationServiceTests` |
| Windows x64 Debug sub-prep document-model test | Passed: `ClassMngrEngineSubPrepDocumentTests` |
| Windows x64 Release sub-prep document-model test | Passed: `ClassMngrEngineSubPrepDocumentTests` |
| Windows x86 Debug sub-prep document-model test | Passed: `ClassMngrEngineSubPrepDocumentTests` |
| Windows x86 Release sub-prep document-model test | Passed: `ClassMngrEngineSubPrepDocumentTests` |
| Windows x64 Debug document-catalog policy test | Passed: `ClassMngrEngineDocumentCatalogTests` |
| Windows x64 Release document-catalog policy test | Passed: `ClassMngrEngineDocumentCatalogTests` |
| Windows x86 Debug document-catalog policy test | Passed: `ClassMngrEngineDocumentCatalogTests` |
| Windows x86 Release document-catalog policy test | Passed: `ClassMngrEngineDocumentCatalogTests` |
| Windows x64 Debug speaking-evaluation grid-validation test | Passed: `ClassMngrEngineSpeakingEvaluationValidatorTests` |
| Windows x64 Release speaking-evaluation grid-validation test | Passed: `ClassMngrEngineSpeakingEvaluationValidatorTests` |
| Windows x86 Debug speaking-evaluation grid-validation test | Passed: `ClassMngrEngineSpeakingEvaluationValidatorTests` |
| Windows x86 Release speaking-evaluation grid-validation test | Passed: `ClassMngrEngineSpeakingEvaluationValidatorTests` |
| Windows x64 Debug report ZIP writer test | Passed: `ClassMngrEngineZipArchiveWriterTests` |
| Windows x64 Release report ZIP writer test | Passed: `ClassMngrEngineZipArchiveWriterTests` |
| Windows x86 Debug report ZIP writer test | Passed: `ClassMngrEngineZipArchiveWriterTests` |
| Windows x86 Release report ZIP writer test | Passed: `ClassMngrEngineZipArchiveWriterTests` |
| Windows x64 Debug document-output result test | Passed: `ClassMngrEngineDocumentOutputResultTests` |
| Windows x64 Release document-output result test | Passed: `ClassMngrEngineDocumentOutputResultTests` |
| Windows x86 Debug document-output result test | Passed: `ClassMngrEngineDocumentOutputResultTests` |
| Windows x86 Release document-output result test | Passed: `ClassMngrEngineDocumentOutputResultTests` |
| Windows x64 Debug sub-prep package-planning service test | Passed: `ClassMngrEngineSubPrepPackageServiceTests` |
| Windows x64 Release sub-prep package-planning service test | Passed: `ClassMngrEngineSubPrepPackageServiceTests` |
| Windows x86 Debug sub-prep package-planning service test | Passed: `ClassMngrEngineSubPrepPackageServiceTests` |
| Windows x86 Release sub-prep package-planning service test | Passed: `ClassMngrEngineSubPrepPackageServiceTests` |
| Windows x64 Debug academic calendar service test | Passed: `ClassMngrEngineAcademicCalendarTests` |
| Windows x64 Release academic calendar service test | Passed: `ClassMngrEngineAcademicCalendarTests` |
| Windows x86 Debug academic calendar service test | Passed: `ClassMngrEngineAcademicCalendarTests` |
| Windows x86 Release academic calendar service test | Passed: `ClassMngrEngineAcademicCalendarTests` |
| Windows x64 Debug calendar-event rules test | Passed: `ClassMngrEngineCalendarEventRulesTests` |
| Windows x64 Release calendar-event rules test | Passed: `ClassMngrEngineCalendarEventRulesTests` |
| Windows x86 Debug calendar-event rules test | Passed: `ClassMngrEngineCalendarEventRulesTests` |
| Windows x86 Release calendar-event rules test | Passed: `ClassMngrEngineCalendarEventRulesTests` |
| Windows x64 Debug calendar-event validator test | Passed: `ClassMngrEngineCalendarEventValidatorTests` |
| Windows x64 Release calendar-event validator test | Passed: `ClassMngrEngineCalendarEventValidatorTests` |
| Windows x86 Debug calendar-event validator test | Passed: `ClassMngrEngineCalendarEventValidatorTests` |
| Windows x86 Release calendar-event validator test | Passed: `ClassMngrEngineCalendarEventValidatorTests` |
| Windows x64 Release calendar-event service test | Passed: `ClassMngrEngineCalendarEventServiceTests` |
| Retained Windows Qt calendar-event repository regression | Passed: `ClassMngrCalendarEventRepositoryTests` (11/11 cases) |
| Retained Windows Qt calendar-event cache regression | Passed: `ClassMngrCalendarEventCacheTests` |
| Windows x64 Debug intensive-slot-state service test | Passed: `ClassMngrEngineIntensiveSlotStateServiceTests` |
| Windows x64 Release intensive-slot-state service test | Passed: `ClassMngrEngineIntensiveSlotStateServiceTests` |
| Windows x86 Debug intensive-slot-state service test | Passed: `ClassMngrEngineIntensiveSlotStateServiceTests` |
| Windows x86 Release intensive-slot-state service test | Passed: `ClassMngrEngineIntensiveSlotStateServiceTests` |
| Retained Windows Qt intensive-slot-state repository regression | Passed: current Qt 6.11/MSVC objects manually linked with the engine service; 5/5 cases under `QT_QPA_PLATFORM=offscreen` |
| Windows x64 Debug teacher-import service test | Passed: `ClassMngrEngineTeacherImportServiceTests` |
| Windows x64 Release teacher-import service test | Passed: `ClassMngrEngineTeacherImportServiceTests` |
| Windows x86 Debug teacher-import service test | Passed: `ClassMngrEngineTeacherImportServiceTests` |
| Windows x86 Release teacher-import service test | Passed: `ClassMngrEngineTeacherImportServiceTests` |
| Retained Windows Qt teacher-import regression | Passed: `ClassMngrTeacherImportTests` under the x64 Debug Qt build, including file-backed adapter, UTF-8 matching, field preservation, rollback, and date monotonicity |
| Windows x64 Debug speaking-evaluation persistence service test | Passed: `ClassMngrEngineSpeakingEvaluationPersistenceServiceTests` |
| Windows x64 Release speaking-evaluation persistence service test | Passed: `ClassMngrEngineSpeakingEvaluationPersistenceServiceTests` |
| Windows x86 Debug speaking-evaluation persistence service test | Passed: `ClassMngrEngineSpeakingEvaluationPersistenceServiceTests` |
| Windows x86 Release speaking-evaluation persistence service test | Passed: `ClassMngrEngineSpeakingEvaluationPersistenceServiceTests` |
| Retained Windows Qt speaking-evaluation persistence adapter smoke | Passed: temporary Qt 6.11/MSVC link-level file-backed save/load, UTF-8, dirty-cell, and roster-score import checks |
| Retained Windows Qt upcoming-birthday regression | Passed: `ClassMngrUpcomingBirthdaysTests` |
| Retained Windows Qt class-information lifecycle regression | Passed: `ClassMngrDataServiceLifecycleTests` |
| Retained Windows Qt class-assignment regression | Passed: `ClassMngrTestingClassRepositoryTests` |
| Retained Windows Qt class-transfer regression | Passed: `ClassMngrClassTransferTests` |
| Retained Windows Qt analytics adapter compile check | Passed: Qt 6.11/MSVC compile-only checks for `speaking_analytics.cpp` and `student_name_utils.cpp` |
| Retained Windows Qt roster validation adapter compile check | Passed: Qt 6.11/MSVC compile-only check for `roster_validator.cpp` |
| Retained Windows Qt speaking-evaluation report widget | Passed: `ClassMngrSpeakingEvalReportWidgetTests` |
| Retained Windows Qt schedule-builder regression | Passed: `ClassMngrScheduleBuilderTests` |
| Retained Windows Qt schedule-model regression | Passed: `ClassMngrSchedulePrintModelTests` |
| Retained Windows Qt schedule-PDF regression | Passed: `ClassMngrSchedulePrintPdfTests` |
| Retained Windows Qt sub-prep report regressions | Passed: `ClassMngrSubPrepPrintPdfTests`, `ClassMngrSubPrepPackageServiceTests` |
| Retained Windows Qt roster report regression | Passed: `ClassMngrRosterTemplatePrintServiceTests` |
| Retained Windows Qt database-port interoperability regression | Passed: `ClassMngrDatabasePortFixtureTests` with temporary Qt-written → engine-read and engine-written → Qt-read profiles |
| Retained Windows Qt document-catalog regression | Passed: `ClassMngrDocumentCatalogTests` |
| Retained Windows Qt speaking-evaluation validation regression | Passed: `ClassMngrSharedPolicyTests` through the UTF-8 adapter |
| Retained Windows Qt report ZIP adapter compile check | Passed: Qt 6.11/MSVC compile-only check for `zip_archive_writer.cpp` |
| Retained Windows Qt document-output adapter compile check | Passed: Qt 6.11/MSVC compile-only UTF-8 conversion round-trip for `document_output_result.h` |
| Retained Windows Qt calendar-event validator adapter check | Passed: Qt 6.11/MSVC compile-only check for `calendar_event_validator.cpp` |
| Retained Windows Qt batch-report target | Passed with `QT_QPA_PLATFORM=offscreen`; the native desktop rerun exposed host clipboard ownership failures in two existing AI/clipboard cases, not in report rendering or grade assembly |
| Retained Windows Qt non-visual regression suite | Passed: 78/78 tests |

All four engine lanes configured or regenerated successfully after the engine
source addition, compiled the new implementation, and passed the targeted
CTest selections with no Qt-dependent test process. The new schedule-import,
schedule-builder, and teacher-import service tests passed in x64/x86 Debug
and Release. The speaking-evaluation
report service, schedule-report service, roster-report service, roster-report
template policy,
speaking-evaluation report metadata model, speaking-evaluation report content,
speaking-evaluation AI prompt service, academic-calendar schedule,
calendar-event rules, speaking-evaluation output policy, class-information,
schedule-read, schedule-import, class-transfer, and speaking-evaluation
template-policy, batch-report policy, PowerPoint job service, and sub-prep
class-information, schedule-builder, class/teacher naming, upcoming-birthday,
Sub Prep document-model, document-catalog policy, speaking-evaluation grid-validation,
report ZIP writer, and Sub Prep package-planning implementations compiled and passed in all four
engine lanes. The new class-naming, upcoming-birthday, document-catalog,
speaking-evaluation validator, and Sub Prep document-model and package-planning
tests also passed in all four lanes. The Qt-free
fixture round-trip test also passed in all four lanes against the committed
eleven-case database-port corpus, including engine write/reopen and class-
transfer import coverage. The retained Qt fixture verifier additionally passed
the explicit temporary Qt-written → engine-read and engine-written → Qt-read
checks. Each lane's integrated sweep then passed all
thirty-eight engine suites plus both WinUI staging and manifest checks (40/40).
The
retained Qt schedule-import, schedule-builder, upcoming-birthday,
speaking-evaluation validation, and report ZIP adapter compile check
also passed, alongside the existing class-information, assignment,
class-transfer, schedule-model, schedule-PDF, sub-prep, roster-report, and
report-widget regressions, including the sub-prep document-model and
document-catalog, class-information, and pagination adapters,
academic-calendar, calendar-event, speaking-evaluation report metadata,
class-analytics, roster-validation, and
speaking-evaluation report content and AI prompt adapters, schema-manager,
updater, and teacher-import
coverage. The
batch-report target was also exercised offscreen;
a normal desktop run was blocked only by the host clipboard being
owned/unavailable in two existing AI UI cases. A narrowed source audit found
no Qt, WinUI, WinRT,
Direct2D/DirectWrite, or Win32 UI dependency in `src/engine`.
The complete retained Windows Qt non-visual CTest sweep also passed all 78
registered tests after the engine test executables were rebuilt in the Qt
build tree.

The document-output result target was subsequently configured, built, and
passed in all four x64/x86 Debug/Release WinUI lanes. Its focused CTest
 selections passed without a Qt-dependent test process, and a direct Qt
6.11/MSVC compile-only check exercised the retained model's UTF-8 conversion
round trip.

The calendar-event validator target was subsequently configured, built, and
passed in all four x64/x86 Debug/Release WinUI lanes. Its focused CTest
selections covered normalization, UTF-8 data, date/time diagnostics,
recurrence bounds, month-end stepping, and series caps. A direct Qt 6.11/MSVC
compile-only check exercised the retained adapter against the current engine
headers; the full Qt CMake regeneration was not used because
the existing Qt build tree stalled during regeneration.

The roster-persistence target was subsequently configured, built, and passed
in all four x64/x86 Debug/Release WinUI lanes. Its focused CTest selections
covered typed class-id errors, missing classes, UTF-8 sparse rows, width
fallback, replacement, malformed-cell filtering, and transaction rollback.

The retained Qt roster repository was then rebuilt against the engine adapter
and its file-backed save/load/count path was exercised through
`ClassMngrDataServiceLifecycleTests`; the same regression covered injected
single-save and batch-save failures and verified rollback. The retained class
transfer export path passed through `ClassMngrClassTransferTests`. The complete
engine CTest selection passed in all four x64/x86 Debug/Release WinUI lanes.

The retained Qt class-information repository was then converted to a UTF-8
adapter over the engine class-information and schedule services. Its six
public operations now share the engine persistence, teacher-join, schedule,
and conflict rules, while the Qt-facing result and diagnostic shapes remain
unchanged. The adapter and engine sources passed direct VS 2026/Qt 6.11
compile checks. A rebuilt retained-Qt assignment test using the updated
adapter passed all 8 cases. The normal Qt CMake regeneration/rebuild was
attempted but remained blocked by the existing MSBuild FileTracker access
failure and regeneration stall, so the full current-binary class-information
lifecycle result is not claimed here.

The retained Qt class repository was then converted to a UTF-8 adapter over
the engine class repository. Its six public CRUD operations now share engine
class filtering, persistence, typed errors, and transactional child-row
cleanup. Class deletion explicitly preserves the former Qt cleanup order and
operation/class-id diagnostics while rolling back on any child-table failure.
The class-assignment fixture was changed from Qt `:memory:` storage to a
temporary file so the Qt and engine connections exercise the same profile;
the updated adapter, Data project, and focused test source passed direct VS
2026/Qt 6.11 compile checks. The focused engine class-repository test passed
in all four x64/x86 Debug/Release WinUI lanes after rebuilding the engine
target. A normal current Qt binary
rebuild was attempted but remained blocked by the existing CMake regeneration
and MSBuild FileTracker stall, so no current-binary Qt class-repository test
pass is claimed here.

The retained Qt teacher repository was then converted to a UTF-8 adapter over
the engine teacher service. All six public CRUD operations now share engine
validation, normalization, ordering, typed not-found handling, and
transactional class-assignment cleanup on delete. The lifecycle fixture was
updated to use valid teacher values and verifies canonical phone formatting;
the adapter passed a direct VS 2026/Qt 6.11 compile-only check, and the full
Qt-free engine/WinUI CTest sweep passed 43/43. A normal current Qt lifecycle
rebuild was attempted but remained blocked by the existing CMake regeneration
and MSBuild FileTracker stall, so no current-binary Qt lifecycle pass is
claimed for this slice.

The retained Qt Native English teacher repository was then converted to a
UTF-8 adapter over the engine Native English teacher directory service. Its
list and atomic save/delete operations now share engine ordering,
normalization, uniqueness, and transaction rules while preserving the Qt
facade. The adapter compiled as part of the real VS 2026/Qt 6.11 Data project;
the regular Qt CMake rebuild remains blocked by the existing regeneration /
MSBuild FileTracker stall, so no current-binary directory regression is
claimed for this slice.

The retained Qt GS Team repository was then converted to a UTF-8 adapter over
the engine GS Team directory service. Its list and atomic save/delete
operations now share engine ordering, normalization, uniqueness, and
transaction rules while preserving the Qt facade. Static adapter checks
passed, and the existing `ClassMngrEngineGsTeamServiceTests` executable passed
in all four x64/x86 Debug/Release WinUI lanes. The regular Qt CMake rebuild
again remained blocked by the existing regeneration / MSBuild FileTracker
stall, so no current-binary retained-Qt directory regression is claimed for
this slice.

The retained Qt schedule-import repository was then converted to a UTF-8
adapter over the engine `ScheduleImportService`. Its nested preview and plan
models now cross the boundary through explicit string, collection, and enum
conversions, while matching, validation, intensive-mode handling, and
transactional writes remain engine-owned. Static adapter checks passed, and
the existing `ClassMngrEngineScheduleImportServiceTests` executable passed in
all four x64/x86 Debug/Release WinUI lanes. The regular Qt CMake rebuild again
remained blocked by the existing regeneration / MSBuild FileTracker stall, so
no current-binary retained-Qt schedule-import regression is claimed for this
slice.

The retained Qt class-transfer repository was then converted to a UTF-8
adapter over the engine `ClassTransferService`. Its complete nested package
and plan model graphs now cross the boundary through explicit string,
collection, enum, and UTC timestamp conversions, while package matching,
schedule preflight, and transactional writes remain engine-owned. Static
forbidden-symbol and engine-usage checks passed. A direct Qt 6.11/MSVC
compile-only check passed, and a temporary link-level Qt/engine smoke harness
passed package build, preview, and import with UTF-8 values and an unset export
timestamp. The existing `ClassMngrEngineClassTransferServiceTests` executable
passed in all four x64/x86 Debug/Release WinUI lanes. The normal Qt CMake
regeneration was attempted but remained blocked by the existing MSBuild
FileTracker access failure/regeneration stall, so no current-binary retained-
Qt class-transfer regression is claimed for this slice.

The retained Qt calendar-event repository was then converted to a UTF-8,
date/time conversion adapter over the Qt-free `CalendarEventService`. The
service owns all eleven persistence operations, validation, row mapping, and
transactional batch behavior; the adapter retains the existing Qt-facing
results and localized operation context. The x64 Release engine calendar
selection passed all 3/3 rules, validator, and service tests. The rebuilt Qt
repository regression passed all 11 cases, and the calendar cache regression
passed. The broader lifecycle target ran against the current binary but
reported two existing unrelated fixture assertions, so it is not counted as
a clean full-suite result for this slice.

The retained Qt intensive-slot-state repository was then converted to a UTF-8
adapter over the Qt-free `IntensiveSlotStateService`. The service owns ordered
state reads, default-state deletion, upsert behavior, row mapping, and typed
SQLite failures; the adapter retains the Qt-facing result and localized
operation context. The focused engine test passed in all four x64/x86
Debug/Release WinUI lanes. The current adapter and test sources passed direct
VS 2026/Qt 6.11 compilation, and a manual link-level run of the current
objects passed all 5 cases with `QT_QPA_PLATFORM=offscreen`. The normal Qt
CMake regeneration remained blocked by the existing stale/missing generated
project and MSBuild FileTracker stall, so no CMake-generated current-binary
Qt gate is claimed for this slice.

The retained Qt speaking-evaluation repository was then converted to a UTF-8
adapter over the Qt-free `SpeakingEvaluationPersistenceService`. The engine
now owns evaluation lookup/creation, fixed 25x11 grid row creation and
persistence, dirty-cell versus full-save behavior, typed schema and rollback
failures, and roster-score import assembly using the shared overall-grade rule.
The focused native persistence test passed in all four x64/x86 Debug/Release
WinUI lanes. A temporary Qt 6.11/MSVC link-level smoke harness also passed
file-backed Qt-to-engine save/load with UTF-8 values, dirty-cell updates, and
roster-score import through the retained adapter. The regular Qt CMake
regeneration was attempted but remains blocked by the existing QML generation
and MSBuild FileTracker stall, so no CMake-generated current-binary Qt
lifecycle gate is claimed for this slice.

The retained Qt teacher-import repository was then converted to a UTF-8
adapter over the Qt-free `TeacherImportService`. The focused native service
test passed in all four x64/x86 Debug/Release WinUI lanes. The retained Qt
teacher-import regression passed after its file-backed fixtures explicitly
released active Qt read queries before the separate engine connection wrote;
this covers the existing parser plus import matching, field preservation,
rollback, and latest-source-date behavior.

The retained Qt testing-class and testing-block repositories were then
converted to UTF-8 adapters over the Qt-free `TestingClassService` and
`TestingBlockService`. Their engine tests cover CRUD, mixed-level catalogs,
assignment creation and cleanup, canonical schedule keys, plain/special
filtering, explicit replacement, typed schema failures, and assignment
rollback. The native focused selections passed in all four x64/x86
Debug/Release WinUI lanes. Rebuilt retained Qt regressions passed for both
repositories; their fixtures use temporary file-backed profiles so Qt and
engine SQLite connections exercise the same database. The testing-block
fixture also verifies that class-over-plain replacement retains the existing
explicit-confirmation behavior.

The retained Qt campus-record repository was then converted to a UTF-8
adapter over the Qt-free `CampusRecordService`. The service owns all fourteen
campus text fields, CRUD/save behavior, name ordering, typed invalid-id,
not-found, and schema errors, while the adapter preserves the Qt-facing
result and diagnostic shapes. The focused native service source compiled,
linked, and ran successfully in direct VS 2026/v145 x64 and x86 Debug/Release
lanes.
The adapter source compiled against Qt 6.11/MSVC, and a temporary file-backed
Qt smoke harness passed UTF-8 save/load/update/list/delete through the adapter.
Both Windows CMake trees reconfigured and regenerated the new target, but
normal target builds remain blocked by the existing MSBuild FileTracker
`UnauthorizedAccessException` during `ZERO_CHECK`/compile tracking; no
CMake-generated current-binary campus test result is claimed.

The application-settings service test was then compiled, linked, and run
successfully in direct VS 2026/v145 x64 and x86 lanes. It covers UTF-8 text,
TEXT-column numeric affinity, BLOB and NULL values, upsert replacement,
missing-setting behavior, malformed duplicate rows, and transactional batch
rollback. The retained Qt settings adapter and both changed import call sites
passed direct Qt 6.11/MSVC compile-only checks; a link-level file-backed Qt
smoke passed UTF-8 settings, QVariant integer/bool conversion, and rollback
across the separate Qt and engine SQLite connections. The normal Qt CMake
target build remains blocked by the existing MSBuild FileTracker
`UnauthorizedAccessException`, so no CMake-generated current-binary settings
regression is claimed for this slice.

The retained Qt `DatabaseSession` was then connected to the portable database
boundary. Ordinary file-backed opens now use engine `OpenDatabase` for UTF-8
path normalization, parent-directory preparation, and schema migration before
the Qt `QSQLITE` connection is created; exact `:memory:` opens retain the Qt
schema initializer and Qt-side foreign-key setup. The focused lifecycle source
compiled and `git diff --check` passed. A current CMake target rebuild was
attempted, but the existing MSBuild FileTracker `UnauthorizedAccessException`
recurred during `ZERO_CHECK`/compile tracking, so no current-binary lifecycle
pass is claimed for this slice.

The personal-details settings slice was then connected to the retained Qt
adapter. The focused engine service compiled, linked, and passed directly in
VS 2026/v145 x64 and x86 Debug harnesses, covering defaults, UTF-8 round trips,
legacy fallback/promotion, explicit-empty handling, mode normalization,
campus updates, malformed setting errors, rollback, and closed-database
propagation. The retained adapter, feature-service boundary, and lifecycle
test source compiled against Qt 6.11/MSVC; a manually linked offscreen
lifecycle selection passed the file-backed personal-details save/load/campus
interoperability check. CMake reconfiguration passed for x64 Qt and x64/x86
WinUI, while the normal CMake target rebuild remains blocked by the existing
MSBuild FileTracker `UnauthorizedAccessException`, so no CMake-generated
current-binary lifecycle result is claimed.

The retained Qt teacher and class-information validators now route
normalization, phone formatting, nested schedule conversion, and validation
through the Qt-free engine contracts. Direct VS 2026/v145 Qt compile checks
passed, the focused `ClassMngrSharedPolicyTests` selection passed 1/1, and the
current engine CTest lane passed all 43 available executables; nine generated
targets were absent and therefore not run. The normal Qt CMake target build
remains blocked by the existing MSBuild FileTracker
`UnauthorizedAccessException`.

The standalone retained Qt `ClassTimeValidator` was then converted to a UTF-8
adapter over the same Qt-free engine validator. The engine class-time issues
now carry their portable row/column metadata; the Qt adapter restores the
legacy invalid-value, end-order, and duplicate-row argument maps at the
presentation boundary. Direct VS 2026/v145 Qt and engine compile checks passed.
A manually linked Qt 6.11/MSVC `ClassMngrSharedPolicyTests` binary passed with
explicit normalization, row/column, and diagnostic-argument assertions. The
normal Qt CMake target build remains blocked by the existing MSBuild FileTracker
`UnauthorizedAccessException`, so no CMake-generated current-binary gate is
claimed for this slice.

The retained Qt `ClassInfoConfig` catalog is now a conversion adapter over the
Qt-free engine catalog, preserving the existing `QStringList` API. Focused
parity coverage compares all six public catalog lists and lookup behavior for
canonical grades and levels, GraVoca entries, fallback branches, and
case-sensitive invalid inputs. Direct VS 2026/v145 Qt compile and MOC checks
passed, and CMake configure/generate succeeded. The focused Qt target rebuild
remains blocked by the existing MSBuild FileTracker
`UnauthorizedAccessException`, so no current-binary Qt parity result is
claimed for this slice.

## Qt formal deprecation gate — 2026-09-02

The three formal Qt 6 deprecation groups from the companion audit are now
migrated: UTC conversions use `QTimeZone::UTC`, mouse handlers use
`position().toPoint()`, and the seven fixed-argument `invokeMethod` calls use
typed variadic arguments. The Qt 6.12.0 Debug audit build uses
`QT_DISABLE_DEPRECATED_UP_TO=0x060C00`; `ClassMngrData`, `ClassMngrUiShared`,
the five affected Qt test targets, and the enabled Windows visual-capture test
all compiled successfully. The five focused CTest selections passed 1/1.

The retained `ClassMngrClassTransferTests` executable must run with the same
Qt version used to build it. The machine-wide Qt 6.11.1 PATH caused the
reported `QPdfView::wheelEvent` entry-point error for the Qt 6.12 build. With
Qt 6.12.0 on PATH the executable loads, but its current run has 8 passing and
7 database-dependent setup failures; no clean class-transfer runtime pass is
claimed here. The next Phase 2 work remains the report/export adapters,
remaining retained adapters, and broader fixture coverage.

## Calendar-event import extraction — 2026-09-02

The Qt-free `CalendarEventImportService` now owns workbook event extraction:
month-grid date mapping, legend classification, note ranges and cancellation,
campus-note title suffixes, event normalization, and duplicate signatures. The
retained Qt parser now only converts the existing workbook representation to
the engine contract and converts the resulting events back to its legacy Qt
API; ZIP/XML workbook decoding and network/database orchestration remain Qt
adapters.

The Qt-free `ClassMngrEngineCalendarEventImportServiceTests` target built and
passed 1/1 in the Windows x64 WinUI engine lane. The retained
`ClassMngrCalendarImportTests` target built and passed 1/1 in the Qt 6.12.0
deprecation-audit lane. Both builds used `/p:TrackFileAccess=false` to bypass
the host's existing MSBuild FileTracker `UnauthorizedAccessException`; the
normal Qt 6.11.1 lane still cannot regenerate because the project requires Qt
6.12.0.

## Schedule-import review validation extraction — 2026-09-02

The Qt-free `ScheduleImportService::validateImport` now exposes the same plan,
existing-state, and projected-conflict checks used before transactional import
writes without opening a transaction or mutating the database. The retained Qt
`ScheduleImportRepository`, `DataService`, and `ScheduleService` forward the
plan through the existing boundary conversion. The review dialog calls this
authoritative preflight after its Qt-side completeness and localized warning
checks; projected conflict text and review controls remain presentation-owned.

The focused `ClassMngrEngineScheduleImportServiceTests` target built and passed
1/1 in all four Windows WinUI x64/x86 Debug/Release lanes. The retained Qt
`ClassMngrScheduleImportDialogTests` target built and passed 1/1 in the Qt
6.12.0 deprecation-audit lane. Both builds used `/p:TrackFileAccess=false` to
bypass the host's existing MSBuild FileTracker `UnauthorizedAccessException`.
The separate existing `ClassMngrScheduleImportTests` apply cases were
subsequently corrected as a retained-fixture baseline issue; the existing
import/apply path was not changed in this slice.

## Schedule-import matching and pattern rules — 2026-09-02

The Qt-free `ScheduleImportRules` contract now owns schedule day-group
compatibility, meeting-day matching, regular/intensive fallback selection,
class-option eligibility, supported meeting patterns, and weekday-pattern
validation. `ScheduleImportService` uses that contract instead of carrying a
second implementation. The retained Qt rules file now only converts between
`QList`/`QString` and UTF-8 engine values and restores Qt-localized pattern
messages and weekday names.

The focused `ClassMngrEngineScheduleImportServiceTests` target built and
passed 1/1 in all four Windows WinUI x64/x86 Debug/Release lanes. The retained
Qt `ClassMngrScheduleImportDialogTests` target built and passed 1/1 in the Qt
6.12.0 deprecation-audit lane. The broader `ClassMngrScheduleImportTests`
selection's two apply fixtures were subsequently corrected as a retained
Qt/engine connection-boundary issue; its rule and parser coverage was not
changed by this fixture correction. The changed builds used
`/p:TrackFileAccess=false` to bypass the host's existing MSBuild FileTracker
permission issue.

## Schedule-import apply fixture baseline — 2026-09-02

The two retained Qt apply fixtures
`intensiveModesPreserveOrReplaceAbsentHours` and
`skippedExactMatchPreservesItsSchedule` were holding verification `QSqlQuery`
cursors open while invoking a second engine-backed apply. Because the retained
Qt repository and engine service use separate SQLite connections, those active
read cursors could keep a read lock across the transactional write and produce
the recorded apply failures.

The fixtures now call `QSqlQuery::finish()` before their second apply. This
releases the read cursors at the test boundary while preserving the intended
coverage: intensive `UpdateExisting` keeps an absent class's intensive hours,
`ReplaceWithNew` clears them, skipped exact matches preserve their schedules,
and profile-name changes occur only when explicitly requested. Production
engine import/apply code is unchanged.

Both focused QtTest cases passed in implementation validation, and
`git diff --check` passed. A fresh Qt 6.12 CMake target rebuild on this host
remains blocked by the MSBuild FileTracker `UnauthorizedAccessException`, so no
fresh full CTest run is claimed from this host.

## Import/file-codec and resource policy boundaries — 2026-09-03

The schedule workbook adapter now supports cancellation-aware Qt parsing while
conversion, typed errors, validation, cancellation checkpoints, and atomic
apply semantics are owned by the Qt-free schedule-import service. Calendar
workbook imports now normalize, deduplicate, validate, and persist through one
engine transaction. Teacher import matching and plan validation are delegated
to `TeacherImportService`; the retained file/template codec keeps only workbook
decoding and localized diagnostics. Class-transfer and campus JSON codecs now
have representative valid, legacy, malformed, and unsupported fixtures, and
the superseded schedule matcher/plan/state validator duplicates were removed.

Resource-pack manifest validation, precedence/fallback, installed metadata,
download-integrity, HTTPS URL, signature-requirement, and document-catalog
source-selection policy now live in `ClassMngrEngine`. Qt retains JSON and
filesystem decoding, QResource mounting, network downloads, staging,
installation, and localized error presentation. Campus resource references
are normalized and rebased through the same portable policy boundary.

Validation for these targets:

- x64 and x86 WinUI Debug `ClassMngrEngine` builds passed with
  `--parallel 8`.
- The five affected engine test executables passed in both x64 and x86
  WinUI Debug lanes; focused resource-pack and document-catalog policy tests
  passed as well.
- The x64 Qt Debug retained calendar-import, teacher-import, campus-map,
  document-catalog, resource-pack, and schedule-import test executables passed
  offscreen. The new cancellation and codec-fixture cases passed.
- x64 and x86 WinUI application-link builds passed with `--parallel 8`.
- The existing class-transfer executable still has seven database-dependent
  setup failures; the new malformed/unsupported fixture case passes.
- `git diff --check` passed.

## Platform-service interfaces — 2026-09-03

P2-06 now defines the remaining platform-service seams in the Qt-free engine:
`SettingsStore`, `NetworkClient`, `SignatureVerifier`, `ProcessLauncher`,
`Clock`, `Logger`, `ResourceProvider`, and `CancellationToken`/source. The
contracts use UTF-8 strings, byte buffers, standard-library clocks and
containers, and typed `Result`/`Status` errors. `SystemClock`, `NullLogger`,
and thread-safe cancellation implementations provide small portable defaults.

The retained Qt implementation is isolated in
`src/core/platform/qt_platform_services.*`. It translates `QSettings`,
`QNetworkAccessManager`, `UpdateSignatureVerifier`, `QProcess`, Qt logging,
and `QFile`/`QFileInfo` to the engine contracts; no Qt type is included by the
engine header or implementation. The WinUI lane remains responsible only for
future platform-specific adapters and does not pull Qt into `ClassMngrEngine`.

The headless `ClassMngrEnginePlatformServicesTests` target uses in-memory
settings and resources, a deterministic clock, controlled network/signature/
process fakes, and cancellation/error assertions.

Validation for this slice:

- `cmake --build --preset windows-x64-winui-debug --parallel 8 --target ClassMngrEnginePlatformServicesTests -- /p:TrackFileAccess=false /p:BuildProjectReferences=false` passed, and the executable exited 0.
- `cmake --build --preset windows-x86-winui-debug --parallel 8 --target ClassMngrEnginePlatformServicesTests -- /p:TrackFileAccess=false /p:BuildProjectReferences=false` passed, and the executable exited 0.
- `cmake --build --preset windows-x64-debug --parallel 8 --target ClassMngrCore -- /p:TrackFileAccess=false /p:BuildProjectReferences=false` passed with the Qt adapter sources compiled.
- The engine source audit found no Qt, WinUI, WinRT, or Win32 UI includes.
- `git diff --check` passed for the tracked changes.

## Remaining Phase 2 work

This is an in-progress record, not the Phase 2 exit gate. The committed
fixture corpus now has Qt-generated read, migration, and engine write/reopen
coverage on Windows, plus explicit temporary Qt-written → engine-read and
engine-written → Qt-read checks. The roster persistence adapter is now
connected to the extracted engine use case, and the class-information, class,
teacher, class-transfer, calendar-event, intensive-slot-state, speaking-
evaluation, and teacher-import adapters now share extracted engine use cases.
The testing-class and testing-block repositories now share extracted engine
use cases as well. The campus-record repository now shares the extracted
engine campus-record use case as well. The application-settings repository,
teacher-import service, and schedule-import service now share the extracted
engine application-settings use case as well.
The retained Qt `DatabaseSession` now uses engine `OpenDatabase` for ordinary
file-backed profile preparation and schema migration, with the Qt schema
initializer retained only for exact `:memory:` compatibility.
The personal-details settings service and retained adapter now share the
engine settings boundary for file-backed reads and writes as well.
The retained Qt teacher, class-information, and standalone class-time
validators now share the extracted engine validation boundaries as well.
The retained Qt `ClassInfoConfig` catalog adapter now shares the extracted
engine grade, level, and book configuration while preserving its existing Qt
lookup API as well.
The retained Qt schedule-import review now shares the extracted engine
read-only plan and projected-conflict validation boundary as well.
The retained Qt schedule-import matching and meeting-pattern helpers now share
the extracted engine rule contract while keeping conversion and localization
at the Qt boundary as well.
The retained Qt schedule-import parser, calendar-import service, teacher-import
repository, class-transfer codec, and campus codec now share the extracted
P2-04 import/file-codec boundaries. Resource-pack manifest, precedence,
integrity, signature, and catalog-source policy now share the extracted P2-05
engine boundary while Qt retains parsing, mounting, downloads, staging, and
installation. The P2-06 platform-service interfaces now have Qt adapters and
headless contract coverage. P2-07 per-slice cross-platform fixture evidence
is complete; the P2-08 retained-adapter/legacy-rule cleanup is now recorded
below.
The retained Qt schedule-time formatter now delegates display-time and
range-label formatting to the Qt-free schedule-report service through explicit
UTF-8 conversion. Focused parity coverage covers valid and invalid labels,
12/24-hour output, 50/55-minute endings, cross-period labels, and direct engine
equivalence. The engine schedule-report test executable passed directly, and
`git diff --check` passed. The Qt 6.12 CMake target was generated, but its
current build remains blocked by the host MSBuild FileTracker
`UnauthorizedAccessException`; the new Qt CTest executable was therefore not
available to run.
The retained Qt `ClassTabNavigation` model now shares the extracted engine
grouping, ordering, schedule-label, duplicate-label, and day-filter rules while
preserving its existing Qt model shape and localized fallback labels as well.
The focused native class-tab navigation test passed in both x64 and x86 Debug
WinUI lanes, and the retained Qt `ClassTabNavigationModelTests` target built
and passed 1/1 in the x64 Debug Qt lane.
The retained Qt evaluation-default selection helpers now share the extracted
engine term naming, population, fallback, and grade-classification policy.
The focused `ClassMngrEngineEvaluationDefaultSelectionTests` target built and
passed 1/1 in x64/x86 Debug and Release WinUI lanes. The retained Qt target was
not rebuilt because this host exposes Qt 6.11.1 while the project requires Qt
6.12.0; no current-binary Qt parity result is claimed.
The calendar-event workbook import rules now share the Qt-free engine boundary.

## Speaking-evaluation PowerPoint job-model adapter — 2026-09-02

The retained Qt PowerPoint job-model adapter now exposes a typed
`Result<BatchJob>` boundary over the engine's renderer-neutral PowerPoint job
service. It converts engine validation failures into localized Qt errors and
returns them to the batch-report service before template or automation setup,
replacing the previous empty-job sentinel. Successful student mapping and
automation JSON remain compatible. NFC normalization, comment-fit sizing,
resource-path resolution, and automation transport remain at the Qt boundary
and are covered by focused parity assertions.

Validation for this slice:

- The Qt x64 Debug `ClassMngrSpeakingEvalBatchReportServiceTests` target built
  successfully in `build/phase2-qt-x64-debug` using
  `/p:TrackFileAccess=false`.
- Registered CTest `ClassMngrSpeakingEvalBatchReportServiceTests` passed 1/1
  in 47.76 seconds, including invalid-input, JSON mapping, NFC, and comment-fit
  coverage.
- The existing Windows x64 Release
  `ClassMngrEngineSpeakingEvaluationPowerPointJobServiceTests` executable
  passed directly.
- `git diff --check` passed.

An attempted rebuild in the current `build/windows-x64-debug` tree reached
MSBuild but failed on the host's generated-object `Permission denied` errors
and subsequent `D8040` child-process failure. That is recorded as a build-tree
environment limitation; no clean current-tree full build is claimed.

## MSVC generated-object permission fix — 2026-09-02

The repository build configuration now disables MSVC `/MP` compilation by
default and sets Visual Studio's `TrackFileAccess=false` on generated targets.
The host reproduced generated-object `Permission denied` and `D8040` failures
when parallel FileTracker/compiler work overlapped with interrupted build
processes; object ACLs themselves were valid. Hosts with a stable FileTracker
environment can explicitly opt back in with
`-DCLASSMNGR_ENABLE_MSVC_PARALLEL_COMPILE=ON`.

Validation for the fix:

- CMake regenerated the current Windows x64 Debug projects with
  `TrackFileAccess=false` and without `MultiProcessorCompilation`/`/MP`.
- A clean `ClassMngrEngine` rebuild from fresh objects passed successfully
  using the generated project settings.
- The existing engine PowerPoint-job regression remains runnable, and the
  previously validated Qt batch-report target remains green.

## Calendar-event cache adapter — 2026-09-02

The retained Qt `CalendarEventCache` now preflights ordinary file-backed
database paths through engine `OpenDatabase` before opening its worker Qt
`QSQLITE` connection. The adapter keeps the existing Qt cache/model and
foreign-key setup, while engine path normalization, parent-directory creation,
and schema initialization are shared by both presentation stacks. Exact
`:memory:` handling remains on the existing Qt path.

The focused regression uses a nested Korean profile/database path that does not
exist before the request, then verifies that the cache loads an empty range and
the preflight-created database file exists. The test target now links
`ClassMngrEngine` explicitly.

Validation for this slice:

- CMake configure/generate succeeded in the Qt 6.12.0 x64 validation tree.
- `git diff --check` passed.
- The focused cache test sources and the owning `ClassMngrFeatures` target
  passed compile-only MSVC validation with FileTracker disabled.
- The ordinary dependency build first hit the host's existing FileTracker
  `UnauthorizedAccessException` and generated-PDB contention. After the
  owning feature and Qt runtime libraries were rebuilt sequentially with
  project references disabled, the focused test linked successfully; the full
  test executable and the new focused case both exited 0 with
  `QT_QPA_PLATFORM=offscreen`.

The retained Qt adapter audit is now recorded as complete; the next work is
extending fixture evidence across each migrated persistence slice.
Report/export adapter boundaries, portable file/output contracts, and retained
database/application-service adapter cleanup are now recorded as complete P2
targets.

## Database-port cross-slice fixture evidence — 2026-09-02

The engine-written → retained-Qt direction of the database-port round trip now
verifies representative persisted values for the calendar-event, roster,
speaking-evaluation, and campus-record slices in addition to teacher, class,
and class-information data. The profile is written through the Qt-free engine
services and reopened through the retained Qt SQLite connection, with Korean
and English values checked at the storage boundary. The fixture generator also
uses the canonical teacher preferred-name choice so its temporary Qt-written
profile remains valid under the current engine validator.

The engine speaking-evaluation row/column constants are now declared by the
shared speaking-evaluation contract and reused by class-transfer headers,
removing a duplicate-definition hazard when both contracts are included.

Validation for this slice:

- `git diff --check` passed.
- The Qt 6.12.0 x64 `ClassMngrDatabasePortFixtureGenerator` target built in
  `build/phase2-qt-x64-debug` with serialized MSVC compilation and
  `TrackFileAccess=false`.
- The focused fixture verification executable exited 0 for
  `tests/fixtures/database-port` with `QT_QPA_PLATFORM=offscreen`.

The aggregate fixture gate is stronger in this direction, but it does not
close the full P2-07 per-slice fixture matrix; retained-adapter cleanup remains
tracked under P2-08.

## P2-07 — Per-slice interoperability matrix — 2026-09-03

The database-port evidence now covers the migrated persistence slices
individually rather than relying on aggregate fixture opening. The Qt-free
engine test writes, reads, closes, and reopens teacher, class, class-info,
roster, speaking-evaluation, calendar-event, campus, application-settings,
personal-details, intensive-slot-state, testing-class/testing-block,
teacher-directory/import, and schedule data. It also retains typed invalid
teacher input, trigger-backed partial roster rollback, a deterministic typed
`SQLITE_BUSY` case, current and legacy migration checks, rejected fixtures,
and source-immutability checks.

The retained Qt fixture generator verifies the seven shared Qt/engine slices
(teacher, class, class-info, calendar-event, roster, speaking-evaluation, and
campus) in both directions. Each direction performs value checks after the
writer closes and the other stack reopens the profile; the Qt generator emits
`[retained-qt]` diagnostics so this result is distinct from compile-only
validation.

Validation matrix:

| Lane | Result | Evidence |
| --- | --- | --- |
| WinUI x64 Debug, headless engine | PASS | `ClassMngrEngineDatabaseFixtureRoundTripTests` direct run and `ctest -C Debug -L headless --output-on-failure`: 1/1 |
| WinUI x86 Debug, headless engine | PASS | `ClassMngrEngineDatabaseFixtureRoundTripTests` direct run and `ctest -C Debug -L headless --output-on-failure`: 1/1 |
| Qt 6.12.0 x64 Debug, retained adapter | PASS | `ClassMngrDatabasePortFixtureGenerator --verify-directory tests/fixtures/database-port` and `ClassMngrDatabasePortFixtureTests`: 1/1 |

The unfiltered legacy engine sweep is not used as P2-07 evidence because the
existing x64/x86 build trees have nine unrelated registered test binaries
absent; the focused database-port headless target is the recorded matrix.

## P2-08 — Retained-adapter and legacy-rule cleanup — 2026-09-03

The retained-adapter audit searched the Qt feature, repository, codec,
resource-pack, and `DataService` layers for rules that already have a
Qt-free engine owner. The duplicate native-English-teacher and GS-team name
checks in `StaffDirectoryPage` now defer to the engine directory services;
the page retains table-row conversion, localized warning presentation, and
the legacy `MM-dd` birthday input preflight, for which those services do not
currently expose an equivalent rule. The roster duplicate-name highlighting
continues to use the engine-backed `StudentNameUtils` conversion, with row and
cell diagnostics remaining presentation-only.

The stale `DataService` migration markers were removed after confirming that
class information and intensive-slot persistence are delegated through the
engine-backed repositories, class-time parsing/conflict rules are owned by
the engine class-schedule service, and roster score-import construction is
owned by the engine speaking-evaluation persistence service. `DataService`
remains a source-compatible facade; it contains no direct SQL rule
implementation. The file-backed `DatabaseSession` engine preflight and the
Qt named-`:memory:` path remain intentional compatibility boundaries.

Intentionally retained Qt responsibilities are:

- rendering, Qt widget/model wiring, localized diagnostics, and presentation
  state;
- printing, PDF/PowerPoint drawing, and Office/process automation;
- workbook/JSON/XML/ZIP codecs and their Qt-side decode/encode orchestration;
- resource mounting and `QResource`/`QFile` staging or installation;
- filesystem path composition and final filesystem commits.

Validation for this cleanup:

- source audit confirmed the migrated directory and persistence rules have
  engine-backed owners and no stale `DataService` migration markers remain;
- `ClassMngrEngineNativeEnglishTeacherServiceTests` and
  `ClassMngrEngineGsTeamServiceTests` passed directly in the WinUI x64 and x86
  Debug lanes; the P2-07 retained-Qt database-port result remains recorded
  separately;
- the retained Qt `ClassMngrStaffDirectoryPageTests` target build was attempted
  with FileTracker disabled, but the host reproduced its existing MSBuild
  `FileTracker` access failure before a new Qt binary was produced; no current
  Qt binary pass is claimed;
- `git diff --check` passed.
