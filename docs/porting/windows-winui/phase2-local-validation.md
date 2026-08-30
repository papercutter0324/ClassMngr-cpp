# Phase 2 local validation record

Date: 2026-08-30 (Asia/Seoul)

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

The sub-prep pagination slice now includes a Qt-free
`SubPrepPaginationService` for teacher-section page-span detection, the
"Sub Notes" new-page threshold, and fallback placement on the last document
page. The retained Qt renderer delegates these decisions through a small
adapter while keeping text measurement, page counting, PDF geometry, and
drawing Qt-owned.

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
limit. Qt normalizes its native text before calling the policy; directory
creation, collision checks, and atomic file commits remain Qt-owned.

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
| Windows x64 Debug schedule-import service test | Passed: `ClassMngrEngineScheduleImportServiceTests` |
| Windows x64 Release schedule-import service test | Passed: `ClassMngrEngineScheduleImportServiceTests` |
| Windows x86 Debug schedule-import service test | Passed: `ClassMngrEngineScheduleImportServiceTests` |
| Windows x86 Release schedule-import service test | Passed: `ClassMngrEngineScheduleImportServiceTests` |
| Windows x64 Debug class-transfer service test | Passed: `ClassMngrEngineClassTransferServiceTests` |
| Windows x64 Release class-transfer service test | Passed: `ClassMngrEngineClassTransferServiceTests` |
| Windows x86 Debug class-transfer service test | Passed: `ClassMngrEngineClassTransferServiceTests` |
| Windows x86 Release class-transfer service test | Passed: `ClassMngrEngineClassTransferServiceTests` |
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
| Windows x64 Debug schedule report service test | Passed: `ClassMngrEngineScheduleReportServiceTests` |
| Windows x64 Release schedule report service test | Passed: `ClassMngrEngineScheduleReportServiceTests` |
| Windows x86 Debug schedule report service test | Passed: `ClassMngrEngineScheduleReportServiceTests` |
| Windows x86 Release schedule report service test | Passed: `ClassMngrEngineScheduleReportServiceTests` |
| Windows x64 Debug roster report service test | Passed: `ClassMngrEngineRosterReportServiceTests` |
| Windows x64 Release roster report service test | Passed: `ClassMngrEngineRosterReportServiceTests` |
| Windows x86 Debug roster report service test | Passed: `ClassMngrEngineRosterReportServiceTests` |
| Windows x86 Release roster report service test | Passed: `ClassMngrEngineRosterReportServiceTests` |
| Windows x64 Debug sub-prep pagination service test | Passed: `ClassMngrEngineSubPrepPaginationTests` |
| Windows x64 Release sub-prep pagination service test | Passed: `ClassMngrEngineSubPrepPaginationTests` |
| Windows x86 Debug sub-prep pagination service test | Passed: `ClassMngrEngineSubPrepPaginationTests` |
| Windows x86 Release sub-prep pagination service test | Passed: `ClassMngrEngineSubPrepPaginationTests` |
| Windows x64 Debug academic calendar service test | Passed: `ClassMngrEngineAcademicCalendarTests` |
| Windows x64 Release academic calendar service test | Passed: `ClassMngrEngineAcademicCalendarTests` |
| Windows x86 Debug academic calendar service test | Passed: `ClassMngrEngineAcademicCalendarTests` |
| Windows x86 Release academic calendar service test | Passed: `ClassMngrEngineAcademicCalendarTests` |
| Windows x64 Debug calendar-event rules test | Passed: `ClassMngrEngineCalendarEventRulesTests` |
| Windows x64 Release calendar-event rules test | Passed: `ClassMngrEngineCalendarEventRulesTests` |
| Windows x86 Debug calendar-event rules test | Passed: `ClassMngrEngineCalendarEventRulesTests` |
| Windows x86 Release calendar-event rules test | Passed: `ClassMngrEngineCalendarEventRulesTests` |
| Retained Windows Qt teacher-import regression | Passed: `ClassMngrTeacherImportTests` |
| Retained Windows Qt class-information lifecycle regression | Passed: `ClassMngrDataServiceLifecycleTests` |
| Retained Windows Qt class-assignment regression | Passed: `ClassMngrTestingClassRepositoryTests` |
| Retained Windows Qt class-transfer regression | Passed: `ClassMngrClassTransferTests` |
| Retained Windows Qt speaking-evaluation report widget | Passed: `ClassMngrSpeakingEvalReportWidgetTests` |
| Retained Windows Qt schedule-model regression | Passed: `ClassMngrSchedulePrintModelTests` |
| Retained Windows Qt schedule-PDF regression | Passed: `ClassMngrSchedulePrintPdfTests` |
| Retained Windows Qt sub-prep report regressions | Passed: `ClassMngrSubPrepPrintPdfTests`, `ClassMngrSubPrepPackageServiceTests` |
| Retained Windows Qt roster report regression | Passed: `ClassMngrRosterTemplatePrintServiceTests` |
| Retained Windows Qt batch-report target | Passed with `QT_QPA_PLATFORM=offscreen`; the native desktop rerun exposed host clipboard ownership failures in two existing AI/clipboard cases, not in report rendering or grade assembly |
| Retained Windows Qt non-visual regression suite | Passed: 78/78 tests |

All four engine lanes configured or regenerated successfully after the engine
source addition, compiled the new implementation, and passed the targeted
CTest selections with no Qt-dependent test process. The new schedule-import
service test passed in x64/x86 Debug and Release. The speaking-evaluation
report service, schedule-report service, roster-report service,
speaking-evaluation report metadata model, speaking-evaluation report content,
speaking-evaluation AI prompt service, academic-calendar schedule,
calendar-event rules, speaking-evaluation output policy, class-information,
schedule-read, schedule-import, class-transfer, and speaking-evaluation
template-policy, batch-report policy, and PowerPoint job service
implementations compiled and passed in all four engine lanes. Each lane's
integrated sweep then passed all twenty-five engine suites plus both WinUI
staging and manifest checks (27/27). The
retained Qt schedule-import
regression also passed, alongside the existing class-information, assignment,
class-transfer, schedule-model, schedule-PDF, sub-prep, roster-report, and
report-widget regressions, including the sub-prep pagination adapter,
academic-calendar, calendar-event, speaking-evaluation report metadata, and
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

## Remaining Phase 2 work

This is an in-progress record, not the Phase 2 exit gate. The next work is
migrating the remaining report/export adapters and models, connecting retained
Qt adapters to the extracted use-case boundaries, and producing cross-platform
fixture round trips.
