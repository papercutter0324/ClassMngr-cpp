# Phase 2 local validation record

Date: 2026-08-31 (Asia/Seoul)

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

The upcoming-birthday slice now includes a Qt-free
`UpcomingBirthdaySchedule`. It owns birthday parsing, the today/this-week/
next-week date windows, calendar-year rollover, non-leap-year February 29
fallback, staff grouping, display-name fallback, and deterministic ordering.
The retained Qt schedule converts the three staff collections and keeps the
dialog presentation in Qt.

The schedule-import slice now includes Qt-free workbook-neutral models and a
`ScheduleImportService`. The retained workbook parser remains a Qt adapter;
the engine owns teacher/class match ranking, course meeting-pattern rules,
plan validation, stale-target checks, normal snapshot replacement, intensive
preserve/replace modes, intensive slot-state snapshots, profile-name policy,
and transactional schedule writes.

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
| Windows x64 Debug schema/OpenDatabase test | Passed: `ClassMngrEngineDatabaseSchemaTests` |
| Windows x64 Release schema/OpenDatabase test | Passed: `ClassMngrEngineDatabaseSchemaTests` |
| Windows x86 Debug schema/OpenDatabase test | Passed: `ClassMngrEngineDatabaseSchemaTests` |
| Windows x86 Release schema/OpenDatabase test | Passed: `ClassMngrEngineDatabaseSchemaTests` |
| Windows x64 Debug class CRUD test | Passed: `ClassMngrEngineClassRepositoryTests` |
| Windows x64 Release class CRUD test | Passed: `ClassMngrEngineClassRepositoryTests` |
| Windows x86 Debug class CRUD test | Passed: `ClassMngrEngineClassRepositoryTests` |
| Windows x86 Release class CRUD test | Passed: `ClassMngrEngineClassRepositoryTests` |
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
| Retained Windows Qt teacher-import regression | Passed: `ClassMngrTeacherImportTests` |
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
CTest selections with no Qt-dependent test process. The new schedule-import
and schedule-builder service tests passed in x64/x86 Debug and Release. The speaking-evaluation
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

## Remaining Phase 2 work

This is an in-progress record, not the Phase 2 exit gate. The committed
fixture corpus now has Qt-generated read, migration, and engine write/reopen
coverage on Windows, plus explicit temporary Qt-written → engine-read and
engine-written → Qt-read checks. The roster persistence adapter is now
connected to the extracted engine use case. The next work is migrating the
remaining report/export adapters and models, connecting the other retained Qt
adapters to extracted use-case boundaries, and extending fixture evidence
across each migrated persistence slice.
