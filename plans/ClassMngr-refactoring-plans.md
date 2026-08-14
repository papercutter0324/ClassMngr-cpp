# ClassMngr Refactoring Plans

Repository reviewed: [`papercutter0324/ClassMngr-cpp`](https://github.com/papercutter0324/ClassMngr-cpp)  
Baseline commit: [`1a626b7bb8c07d6a4b4036928afe3443c4068ff7`](https://github.com/papercutter0324/ClassMngr-cpp/commit/1a626b7bb8c07d6a4b4036928afe3443c4068ff7) (`main`, 2026-08-12)  
Current status reviewed: `a74d744` plus the active macOS Phase 1 baseline batch (`main`, 2026-08-14)

## Executive Assessment

The repository is not suffering from uncontrolled copy-paste. A token-based scan of 197 `.cpp` files found 59 clone groups, representing about 1.97% of C++ lines. The more important problems are repeated architectural patterns, inconsistent error handling, and a few very large compilation and UI units.

The original assessment prioritized the following work; its current status is:

1. **Completed:** replace repeated CMake test declarations with target libraries and helper functions.
2. **Completed:** move substantial non-template implementations out of headers.
3. **Completed foundations:** extract shared page, autosave, student-name, schedule-value, SQL-error, network-response, filename, and document-output components.
4. **Completed:** replace production `DataService` consumers with narrow feature services while retaining an internal compatibility adapter.
5. **In progress:** centralize message prompts and file selection while giving complex dialogs a common shell; only the final behavior-heavy dialog batch and enforcement remain.
6. **Planned:** establish validation at the domain, UI, service, repository, and database layers.

Do not create helpers for every short clone. Consolidate code when the copies represent one policy that must remain consistent.

## Current Implementation Snapshot

- **Plan 1:** Phases 2–6 are complete. The root `CMakeLists.txt` has fallen from
  2,803 to 190 lines; production libraries, modular CMake files, and
  `classmngr_add_qt_test()` are in use. Shared schedule, SQL, network, filename,
  student-name, and document-output policies exist. `PageHeader`,
  `ScrollablePageBody`, and `AutosaveCoordinator` are deployed across the
  targeted pages. Phase 5 established `DatabaseSession` and migrated production
  call sites to narrow feature-service adapters. Phase 6 decomposed schedule
  import, schedule rendering/interaction, speaking-report output, and sub-prep
  document modeling/PDF rendering into focused coordinators and collaborators.
- **Plan 2:** Phases 1–5 are complete. The four original inline dialogs and all
  custom dialog surfaces now use named classes and the shared `DialogShell`;
  contract coverage and CI enforce the shared dialog boundaries.
- **Plan 3:** Phase 1 is in progress. Schema setup now returns `Status`, runs
  transactionally, and prevents a database session from opening after any DDL
  or legacy-column upgrade failure. `SqlQueryUtils` now preserves driver,
  database, native-code, query, action, and record-identity context.
  `ClassRepository` and `TeacherRepository` writes now expose `Status` or
  `Result<int>`, with transactional compound deletes. The remaining repository
  write contracts and validation boundaries remain mixed.

The latest native macOS universal Debug build and all 54 tests pass. Tests that
provide focused production overrides use a macOS-only flat-namespace shared
test runtime assembled from the existing production objects; this replaces the
obsolete `-multiply_defined suppress` workaround without recompiling production
sources.

---

# Plan 1 — Consolidate Bloat and Duplicate Code

## Baseline Findings and Targets

This table records the original `1a626b7` assessment. Progress against it is
tracked in the phase status notes below.

| Priority | Finding | Evidence | Consolidation target |
| --- | --- | --- | --- |
| P0 | Build configuration is extremely repetitive. | [`CMakeLists.txt`](https://github.com/papercutter0324/ClassMngr-cpp/blob/1a626b7bb8c07d6a4b4036928afe3443c4068ff7/CMakeLists.txt) is 2,803 lines and declares 50 test executables separately. | Production libraries plus `classmngr_add_qt_test()` and separate CMake modules. |
| P0 | Database write/error behavior is inconsistent. | Some repositories return `Status`/`Result`; others ignore `query.exec()`, return `void`, or use default objects as error sentinels. A simple scan found 44 directly unchecked `query.exec()` calls in `src/data`. | `SqlResult` helpers, consistent `Status`/`Result<T>`, and RAII transactions. |
| P1 | `DataService` has become a broad forwarding façade. | [`data_service.cpp`](https://github.com/papercutter0324/ClassMngr-cpp/blob/1a626b7bb8c07d6a4b4036928afe3443c4068ff7/src/data/data_service.cpp) is 1,287 lines and forwards roughly 60 operations to 16 repositories. | `DatabaseSession` plus narrow feature services; retain `DataService` temporarily as an adapter. |
| P1 | Page header, scroll container, dirty state, and autosave logic recur across features. | The same title/subtitle construction appears in about 19 files. Five pages independently define the same 750 ms autosave delay, with related logic in additional pages. | `PageHeader`, `ScrollablePageBody`, and `AutosaveCoordinator`. |
| P1 | Student-name normalization and duplicate-pair validation are split between roster and speaking evaluation models. | [`roster_model_validation.cpp`](https://github.com/papercutter0324/ClassMngr-cpp/blob/1a626b7bb8c07d6a4b4036928afe3443c4068ff7/src/features/roster/ui/roster_model_validation.cpp) and [`speaking_eval_model.cpp`](https://github.com/papercutter0324/ClassMngr-cpp/blob/1a626b7bb8c07d6a4b4036928afe3443c4068ff7/src/features/speaking_eval/ui/speaking_eval_model.cpp) contain matching duplicate-name logic and similar English/Korean rules. | `StudentNameNormalizer` and `StudentNameValidator`, leaving model-specific message presentation in each feature. |
| P1 | Shared policies are duplicated in repository and updater files. | `canonicalWeekday`, `canonicalStartTime`, SQL error formatting, normalized directory names, `isLocalHttpUrl`, and HTTP-success checks appear in multiple files. | Small policy modules: `ScheduleValue`, `SqlError`, `DirectoryValidation`, and `NetworkReplyPolicy`. |
| P1 | Large implementation-heavy headers increase build time and binary duplication. | [`sidebar_controller_p.h`](https://github.com/papercutter0324/ClassMngr-cpp/blob/1a626b7bb8c07d6a4b4036928afe3443c4068ff7/src/app/controllers/sidebar_controller_p.h) is 558 lines and is included by seven translation units. `widget_sizing.h`, `student_name_utils.h`, and `schedule_import_rules.h` also contain hundreds of non-template implementation lines. | Declarations in headers; definitions in `.cpp` files or one private implementation unit. |
| P2 | Several classes have too many responsibilities. | `schedule_import_review_dialog.cpp` is 3,075 lines; `speaking_eval_batch_report_service.cpp` is 2,511; `schedule_widget.cpp` is 2,101; `schedule_import_repository.cpp` and `sub_prep_print_service.cpp` exceed 1,700. | Separate state/model, orchestration, rendering, persistence, and view code. |
| P2 | Print services repeat result and output mechanics. | Schedule, sub-prep, roster, and PDF services define similar `Sent/Canceled/Failed` results and repeat save/print destination behavior. | `DocumentOutputResult` and `DocumentOutputService`; keep feature renderers separate. |
| P2 | General filename policy is only partly shared. | `FileNameUtils` handles JSON names, while speaking-evaluation output implements another filename sanitizer. | General `safeBaseName()`/`safeFileName()` policy used by JSON, PDF, PPTX, and ZIP outputs. |

## Target Structure

```text
src/
  app/
    services/                 # feature-facing application services
  core/
    io/                       # file names, output paths, document output
    network/                  # shared URL/status policy
    validation/               # generic validation result types
  data/
    database/                 # session, migration runner, transaction, SQL errors
    repositories/             # persistence only
  domain/
    validation/               # teacher, class, time, student, calendar rules
  ui/shared/
    dialogs/                  # Plan 2
    pages/                    # page header/body and autosave coordinator
```

## Implementation Phases

### Phase 1. Characterization and Build Baseline

Status: partially complete as of 2026-08-14.

- Record clean build and test results on Windows, Linux, and macOS.
- Add a duplication-report CI job using a fixed configuration, but use it as guidance rather than a hard percentage target.
- Add focused characterization tests before changing SQL behavior, autosave behavior, dialog return values, and import/export filenames.
- Capture compile duration and executable size so header/CMake changes can be measured.

Exit criteria: current behavior is protected, and baseline build/test/size measurements are recorded.

Focused characterization coverage has expanded throughout the refactor. The
fixed duplication report and baseline recorder are in place, including a
manually dispatched Windows/Linux/macOS artifact workflow. Pre- and post-target
Windows measurements and the native macOS measurement are recorded; Linux
remains pending until that workflow is run on its native hosted runner.

The current Windows snapshot configures in 64.775 seconds, clean-builds in
169.845 seconds with two jobs, produces an 89,556,480-byte Debug executable,
and passes all 53 tests in 88.716 seconds. The current macOS universal snapshot
configures in 9.215 seconds, clean-builds in 391.841 seconds with two jobs,
produces a 159,974,232-byte Debug executable, and passes all 54 tests in 56.647
seconds. Its compilation database contains 270 production entries for 270
unique sources, with no production source recompiled. Completing this phase now
requires only the native Linux workflow artifact and its measurements.

### Phase 2. CMake and Compilation Bloat

Status: completed on 2026-08-13.

- Create production targets such as `ClassMngrCore`, `ClassMngrData`, `ClassMngrUiShared`, and feature libraries. The application and tests should link these targets instead of recompiling long source lists.
- Add a helper similar to:

  ```cmake
  classmngr_add_qt_test(
      NAME RosterModel
      SOURCES tests/roster_model_tests.cpp
      LIBRARIES ClassMngrRoster Qt6::Test
      RESOURCES roster_test_resources
  )
  ```

- Move tests to `cmake/tests.cmake`, deployment to `cmake/deployment.cmake`, resources to `cmake/resources.cmake`, and platform-specific logic to dedicated modules.
- Move non-template bodies from the identified large headers into `.cpp` files. In particular, stop defining anonymous-namespace helper implementations in `sidebar_controller_p.h`, because each including translation unit receives its own copy.

Exit criteria: each production source compiles once per configuration; adding a test normally requires one helper call; the root CMake file becomes an orchestration file rather than the entire build definition.

Implemented with the `ClassMngrCore`, `ClassMngrData`, `ClassMngrDomain`,
`ClassMngrUiShared`, `ClassMngrFeatures`, `ClassMngrAppServices`, and
`ClassMngrRuntime` targets; modular `cmake/` files; and
`classmngr_add_qt_test()`. Implementation-heavy bodies were moved out of
`sidebar_controller_p.h`, `student_name_utils.h`,
`schedule_import_rules.h`, and `widget_sizing.h`.

### Phase 3. Low-Risk Shared Policies

Status: completed on 2026-08-13.

- Extract canonical weekday/time parsing from the testing repositories into a domain utility returning typed values or `Result<T>`.
- Extract SQL error construction and checked execution helpers. Prefer helpers that preserve the query text/action and `QSqlError`, not a boolean-only wrapper.
- Extract the duplicated update/resource-pack HTTP URL and status rules.
- Generalize `FileNameUtils` and replace the speaking-evaluation-specific duplicate.
- Move student-name normalization from a header-only implementation to a `.cpp`; add a shared validator for English names, Korean names, and duplicate name pairs.
- Introduce one shared `DocumentOutputResult` for print/save operations.

Exit criteria: each policy has one implementation and table-driven tests; feature files still own feature-specific wording and presentation.

Implemented through `ScheduleValueParser`, `SqlQueryUtils`,
`HttpRequestPolicy`, generalized `FileNameUtils`, compiled
`StudentNameUtils`, and `DocumentOutputResult`, with shared-policy coverage.

### Phase 4. Shared Page and Autosave Components

Status: completed on 2026-08-13.

- Add `PageHeader` to own the title/subtitle labels, fonts, object names, margins, and retranslation hooks.
- Add `ScrollablePageBody` or a small `PageScaffold` builder for the repeated `QScrollArea`/content/layout setup.
- Add `AutosaveCoordinator` with:
  - the 750 ms debounce;
  - save-mode switching;
  - dirty/clean state;
  - suppression while loading;
  - invalid-state blocking;
  - `saveRequested(bool interactive)` and state-changed signals.
- Migrate one representative page first (`ClassNotesPage`), then `ClassDetailsPage`, `TeacherInfoPage`, `PersonalDetailsPage`, roster, speaking evaluation, and testing classes.
- Keep actual serialization and save operations in each page or its feature service.

Exit criteria: migrated pages no longer own an autosave timer or duplicate save-button state rules; behavior remains covered by tests.

The shared components are now used by Class Notes, Class Details, Teacher Info,
Personal Details, Testing Classes, roster editing, and speaking evaluation.
Their layout, dirty-state, debounce, save-mode, loading, and validity behavior
is covered by `ClassMngrPageComponentsTests` and feature tests.

### Phase 5. Replace the `DataService` God Façade Incrementally

Status: complete as of 2026-08-14.

- Introduce `DatabaseSession` to own the connection, schema/migrations, transactions, and repository lifetime.
- Add narrow application services such as `TeacherService`, `ClassService`, `ScheduleService`, `CalendarService`, `RosterService`, and `SpeakingEvaluationService`.
- Put cross-repository use cases—class deletion, transfer/import, conflict checking—in those services, not in UI pages or low-level repositories.
- Have `ApplicationServices` expose the narrow interfaces.
- Keep the existing `DataService` API as a deprecated forwarding adapter during migration. Remove methods only after all call sites move.

Exit criteria: a page/controller depends only on the feature operations it uses; repositories contain persistence logic rather than UI-facing orchestration.

`DatabaseSession` now owns the active database/repository lifetime, and
`ApplicationServices` exposes Settings, Teacher, Class, Schedule, Calendar,
Roster, and Speaking Evaluation services. These services retain a legacy
`DataService` adapter for compatibility-backed tests. The schedule UI slice
now uses `SettingsService` for display preferences, `ClassService` for schedule
construction and class editing, and `ScheduleService` for testing assignments
and intensive-slot state. `ScheduleWidget`, `ScheduleBuilder`, Schedule editor,
Schedule settings, and Testing Assignment no longer depend directly on
`DataService`. Navigation and sidebar class/teacher management now use narrow
services as well, including class transfer/import dialogs and orchestration.
`FileController` now routes database-file lifecycle operations through
`ApplicationServices`, and the Classes and staff-directory pages use their
feature services. Calendar event editing, display preferences, academic
schedule persistence, settings, and event import now use `CalendarService` and
`SettingsService`. Roster navigation, print configuration, and roster-output
data assembly now use the class, teacher, settings, schedule, and roster
services. Schedule import preview, reconciliation, profile settings, and commit
now use the schedule, class, teacher, and settings services. The initial setup
wizard also uses the class, teacher, and settings services for its imports and
first-run data entry. My Information personal details and class summaries now
use the settings, class, teacher, and roster services. Class navigation and
schedule display preferences, along with schedule output profile lookup, now
use `SettingsService`. Sub-prep settings, calendar lookup, class summaries,
package assembly, and print-dialog preferences now use the settings, calendar,
class, teacher, and roster services. The shared class-details color picker also
uses `SettingsService`; no production page, controller, or shared UI helper
accesses `DataService` directly. The façade remains only behind
`ApplicationServices` and the narrow-service compatibility path.

### Phase 6. Decompose Oversized Units

Status: completed on 2026-08-14.

- `ScheduleImportReviewDialog`: extract a review model, resolution policy, validation summary builder, and table/delegate/view components. The dialog should coordinate them.
- `ScheduleWidget`: extract layout geometry, event hit-testing, interaction state, painting, and context-menu commands.
- `SpeakingEvalBatchReportService`: split report-data assembly, asset resolution, PowerPoint automation, PDF/ZIP output, and filename/output-directory policy.
- `ScheduleImportRepository`: move matching and plan validation to a schedule import service; keep SQL read/write and transaction work in the repository.
- `SubPrepPrintService`: separate document model building from rendering and output.

Exit criteria: no listed class simultaneously owns UI construction, domain policy, persistence, and output orchestration.

The planned splits are complete: schedule import matching/validation/review and
review presentation, schedule widget geometry/hit-testing/interaction/
delegates/cell rendering, speaking report assembly/assets/PowerPoint/
PDF-output policy, and sub-prep document modeling now live in focused units.
`ScheduleImportReviewPresentation` owns preview-model construction, imported
and existing labels, change markup, sorting, and projected-conflict text;
`ScheduleImportResolutionControls` owns teacher/class cards, action choices,
default selections, color-button presentation, and callback wiring.
`ScheduleCellWidgetFactory` owns class, multi-class, testing, essay, lunch, and
empty cell construction. `ScheduleTableRenderer` now owns table initialization,
localized column headers, row and cell population, stale-widget cleanup, and
fixed-height/scroll geometry.
Sub-prep PDF layout, pagination, and drawing now live in `SubPrepPdfRenderer`;
`SubPrepPrintService` is limited to request-to-model conversion and save/print
orchestration. Schedule Import Review now retains warning lifecycle, state
orchestration, conflict prompting, and submission rather than presentation or
resolution-control construction. Schedule Widget now retains display state,
service-backed interaction, model construction, commands, and UI coordination
rather than cell or table rendering. Further speaking report renderer slices
continue with `SpeakingEvalPowerPointWorkspace`, which now owns platform-specific
temporary-directory preparation, the macOS PowerPoint sandbox handoff, staged
PDF validation/copying, and cleanup. `SpeakingEvalPowerPointJobModel` now owns
template profiling, NFC text normalization, per-student automation jobs,
comment-size and overall-grade derivation, homogeneous-template validation, and
Windows automation JSON serialization. `SpeakingEvalPowerPointScripts` owns the
embedded Windows PowerShell and macOS AppleScript programs plus macOS argument
encoding. `SpeakingEvalBatchReportService` is now limited to request validation,
renderer orchestration, archive/commit/print sequencing, progress, and result
mapping. The listed coordinators no longer simultaneously own UI construction,
domain policy, persistence, and output orchestration, satisfying the phase exit
criterion.

## Guardrails

- Do not introduce a generic “utils” dumping ground.
- Prefer policy-specific modules with tests over template-heavy generic helpers.
- Do not expose repositories directly to widgets as the final architecture.
- Do not refactor all large files at once; use one independently reviewable responsibility split per pull request.
- Preserve translations by keeping user-visible strings in UI/application layers.

---

# Plan 2 — Unified Dialog System

## Baseline State

At `1a626b7`, the repository contained more than four dialog idioms:

| Idiom | Approximate usage |
| --- | ---: |
| Static `QMessageBox` convenience calls | 112 |
| Configured `QMessageBox` instances | 7 |
| Static `QFileDialog::get...` calls | 11 |
| Configured `QFileDialog` instances | 5 |
| Ad-hoc inline `QDialog` construction | 4 |
| Custom `QDialog` subclasses | 21 |

There is already useful shared work—[`UnsavedChangesDialog`](https://github.com/papercutter0324/ClassMngr-cpp/blob/1a626b7bb8c07d6a4b4036928afe3443c4068ff7/src/ui/shared/utils/unsaved_changes_dialog.cpp), [`FileDialogIconStyle`](https://github.com/papercutter0324/ClassMngr-cpp/blob/1a626b7bb8c07d6a4b4036928afe3443c4068ff7/src/ui/shared/styles/file_dialog_icon_style.cpp), and `TextFitDialogButtonBox`—but call sites bypass those policies. Some file dialogs use static native helpers, others explicitly construct non-native dialogs, and several calls use `nullptr` as the parent.

Current production usage is centralized: `QFileDialog` construction is confined
to `FileDialogService`; `QMessageBox` construction is confined to
`UserPromptService`, apart from the intentional `QMessageBox::aboutQt()` call.
There are no remaining top-level ad-hoc `new QDialog` call sites from the
baseline audit. One inline AI-prompt-preview dialog remains nested within the
large speaking-evaluation report surface and is tracked with that final
migration.

## Target Architecture

Use three cooperating components rather than one oversized dialog manager.

### 1. `UserPromptService`

Own message, confirmation, destructive confirmation, text choice, and unsaved-changes prompts.

Suggested API:

```cpp
enum class PromptSeverity { Information, Warning, Error };
enum class PromptChoice { Accepted, Rejected, Canceled, Destructive };

struct PromptRequest
{
    QWidget* parent = nullptr;
    QString title;
    QString message;
    QString details;
    PromptSeverity severity = PromptSeverity::Information;
    QString acceptText;
    QString rejectText;
    bool destructive = false;
};

class IUserPromptService
{
public:
    virtual ~IUserPromptService() = default;
    virtual void showMessage(const PromptRequest&) = 0;
    virtual PromptChoice confirm(const PromptRequest&) = 0;
};
```

The production implementation uses `QMessageBox`; tests use a fake that records requests and returns scripted choices.

### 2. `FileDialogService`

Own open/save/directory selection and all common file-dialog policy:

- consistent parent and modality;
- native versus non-native behavior by platform;
- `FileDialogIconStyle` installation;
- common sidebar locations;
- remembered directory by purpose;
- filters and default suffixes;
- overwrite behavior;
- normalized/canonical returned paths.

Use typed requests such as `OpenFileRequest`, `SaveFileRequest`, and `DirectoryRequest`, returning `std::optional<QString>` or `QStringList`.

### 3. `StandardDialog` / `DialogShell`

Complex feature dialogs should remain real classes, but inherit or compose a common shell that provides:

- title and optional subtitle/header;
- standard margins and spacing;
- `TextFitDialogButtonBox`;
- default/escape button behavior;
- minimum/maximum sizing and screen clamping;
- geometry persistence by stable dialog key;
- theme/language refresh hooks;
- consistent object names and accessibility labels.

Do not force complex feature dialogs through `UserPromptService`; the service should launch them only when central coordination is useful.

## Migration Phases

### Phase 1. Define Policy and Tests

Status: completed on 2026-08-13.

- Decide parent ownership: controllers use `MainWindow`; child dialogs use their owning page/dialog. Avoid `nullptr` except before the main window exists.
- Decide native-file-dialog policy per platform.
- Define standard button order, destructive wording, default buttons, escape behavior, details text, and error severity.
- Test the service interfaces with fake implementations and test the Qt implementations for correct button/result mapping.

Implemented by `IUserPromptService`/`QtUserPromptService`,
`IFileDialogService`/`QtFileDialogService`, recording fakes, and focused Qt
tests. The agreed ownership, platform, button, path, and test-seam rules are
recorded in [`docs/dialog-policy.md`](../docs/dialog-policy.md). Call-site
migration remains intentionally scoped to Phases 2 and 3.

### Phase 2. File Dialogs First

Status: completed on 2026-08-13.

- Migrate `FileController`, PDF viewer actions, sidebar transfer, roster/schedule/sub-prep printing, speaking-evaluation export, setup signature selection, and import dialogs.
- Remove direct static `QFileDialog::get...` calls.
- Route remembered directories through typed purpose keys such as `TeacherProfile`, `ImportWorkbook`, `ExportReport`, `SignatureImage`, and `GeneratedPdf`.

All production open/save/directory flows now use `IFileDialogService`, with
typed remembered-directory purposes and a test override seam. The PDF viewer's
`Open after saving` accessory is represented in the typed save request/result;
the underlying custom Qt layout remains contained inside `FileDialogService`.

Exit criteria: direct `QFileDialog` use exists only inside `FileDialogService` and narrowly justified custom file-browser dialogs.

### Phase 3. Standard Messages and Confirmations

Status: completed on 2026-08-13.

- Migrate simple information/warning/error calls.
- Convert destructive questions to typed confirmation requests with safe defaults.
- Fold `showUnsavedChangesDialog()` into the prompt service as a typed three-way result.
- Keep detailed domain error creation outside the dialog service; the service presents an already-formed user-facing error.

All production notices and confirmations now use `IUserPromptService`, including
typed destructive decisions, non-blocking notices, bespoke multi-action prompts,
and the service-owned `Save` / `Discard` / `Cancel` unsaved-changes result. The
former `showUnsavedChangesDialog()` wrapper has been removed.

Exit criteria: direct `QMessageBox` use is limited to the Qt prompt implementation and exceptional Qt-owned dialogs such as About Qt.

### Phase 4. Inline and Complex Dialogs

Status: completed on 2026-08-13.

- Replace the four inline `QDialog` constructions in `about_dialog.cpp`, `mainwindow.cpp`, `menu_builder.cpp`, and the sidebar controller private implementation (moved from `sidebar_controller_p.h` to `sidebar_controller_p.cpp` during an earlier split) with named dialog classes or prompt/file service requests.
- Migrate custom dialogs to `DialogShell` in small batches, beginning with settings/import/export dialogs and leaving large editor/report dialogs until behavior is covered.
- Persist geometry through the shell rather than separately in each dialog.

The shared `DialogShell` now owns standard margins and spacing, text-fitting
button boxes, default and escape behavior, screen clamping, stable-key geometry
persistence, accessibility naming, and language/theme refresh hooks. The four
inline cases have been removed: license and sidebar record selection use named
dialogs, Preferences is a named shell dialog, and initial profile guidance uses
`UserPromptService`.

The first custom-dialog batch now uses the shell: About, teacher import, class
import/export, Classes navigation settings, and Schedule settings. Larger
dialogs remain split into coverage-led batches. The second batch migrated
Academic Calendar settings, Schedule import and its reconciliation review, and
speaking-evaluation batch export. The third batch migrated testing assignment,
Schedule print options, speaking-evaluation notes, and AI batch review. The
third batch also corrected custom Import/Apply button roles so the shell cannot
accept before dialog-specific validation completes.

The final behavior-heavy batch migrated Calendar Event, Schedule editor,
Roster print options, Sub-prep print options, shared PDF print/preview, Update,
and Speaking Evaluation report. The report's inline AI prompt preview now also
uses the shell. Validated Save, Generate, Print, and report actions retain
non-accepting button roles so their existing checks run before the dialog can
close. No production custom dialog now subclasses `QDialog` directly.

The Windows production app builds successfully and DialogShell contract tests
pass 6/6. Focused updater, sub-prep, roster-output, speaking-report, and schedule
suites also pass. `ClassMngrScheduleWidgetTests` now runs to completion despite
the existing forced-link duplicate-symbol warnings in its test harness.

### Phase 5. Enforcement

Status: completed on 2026-08-14.

- Add a lightweight CI check that rejects new direct `QMessageBox` and `QFileDialog` calls outside approved files.
- Add dialog contract tests for default/escape buttons, destructive prompts, retranslation, and parent ownership.

Service mapping and shell contract suites now cover prompt choices, file-dialog
policy, default/Escape behavior, geometry persistence, accessibility, migrated
dialog inheritance, validated custom-action roles, retranslation, and parent
ownership. `scripts/check_dialog_policy.py` and its pull-request workflow reject
new production `QMessageBox` and `QFileDialog` dependencies outside the shared
services and reviewed Qt integration points. Shell inheritance contracts cover
every production custom dialog surface.

## Acceptance Criteria

- One policy controls all simple messages and confirmations.
- One policy controls all file/directory selection.
- Complex dialogs share visual, sizing, keyboard, geometry, and accessibility behavior.
- Dialog behavior can be tested without opening modal UI by injecting fake services.
- No controller/page uses a static Qt dialog helper directly.

---

# Plan 3 — Data and Input Validation

Status: in progress as of 2026-08-14; Phase 1 mutation contracts are complete
and read-contract failure observability is in progress.

Plan 1 has supplied several prerequisites (`Result`/`Status`, `SqlQueryUtils`,
`DatabaseTransaction`, `StudentNameUtils`, and `ScheduleValueParser`). They do
not yet satisfy this plan’s end-to-end validation, repository-contract, schema,
or migration requirements.

## Current Risks

1. Validation is concentrated in a few UI models rather than applied consistently at every boundary.
2. Roster and speaking evaluation now share `StudentNameUtils`, but validation still returns feature-local results rather than common structured, field-addressed issues.
3. `CalendarEventDialog::accept()` contains important date/time validation that can be bypassed by non-dialog callers.
4. `TeacherInfoPage` trims and saves form data but still lacks the domain validator planned for Phase 3.
5. `ClassDetailsPage` checks schedule conflicts but does not run one complete `ClassInfo` validator.
6. Repository mutation contracts now use `Status`/`Result<T>`, but read
   contracts still vary between default objects, empty collections, and
   `Result<T>`.
7. Schema setup failures are now observable and transactional, but legacy
   column evolution remains ad hoc until Phase 5 introduces numbered
   migrations.
8. The schema has few foreign keys and `CHECK` constraints, and foreign-key enforcement is not enabled explicitly.
9. Repository writes are checked, but several read paths still execute SQL
   without exposing query failure separately from a missing or empty result.

## Validation Model

Separate normalization, validation, and presentation.

```cpp
enum class ValidationSeverity { Warning, Error };

struct ValidationIssue
{
    QString code;          // stable, non-translated identifier
    QString field;         // e.g. "teacher.phoneNumber"
    int row = -1;
    int column = -1;
    ValidationSeverity severity = ValidationSeverity::Error;
    QVariantMap arguments; // data used to format a translated message
};

using ValidationIssues = QList<ValidationIssue>;
```

- Normalizers return canonical values and never silently replace an invalid value with an unrelated default.
- Validators return stable issue codes and structured locations.
- UI code translates and displays issues.
- Application services reject hard errors before persistence.
- Repositories still defend database invariants; they never assume the UI validated correctly.

## Rules by Domain

### Teacher

- Require at least one usable name according to the product rule.
- Validate Korean/English name character policy through the shared student/person-name components where appropriate.
- Validate preferred name is one of the generated choices.
- Parse birthday into `QDate` or a dedicated optional date type instead of storing an unchecked string at service boundaries.
- Normalize phone numbers for comparison while preserving a display form.
- Validate `internetType` and `projectionType` as enums rather than silently falling back in repository mappers.
- Set explicit maximum lengths for notes, credentials, room, and names.

### Class and Schedule

- Require a valid class id for updates and a recognized grade/level combination.
- Validate reading/essay selections against the selected grade when applicable.
- Validate colors with `QColor::isValidColor` and canonicalize to one format.
- Represent weekday and start/end times with typed values; require `end > start`.
- Reject duplicate slots inside the same class before database conflict checks.
- Run conflict validation in `ClassService`, not only in `ClassDetailsPage`.

### Roster and Speaking Evaluation

- Share name normalization and duplicate-pair detection.
- Keep feature differences—required columns, score list, comment length—as separate rule sets.
- Validate all imported/pasted ranges before applying them; return cell-addressed issues.
- Treat warnings separately from blocking errors.

### Calendar

- Move title, date range, timed/all-day/unconfirmed consistency, recurrence bounds, and occurrence-limit rules out of `CalendarEventDialog` into `CalendarEventValidator`.
- Validate both individual events and generated recurrence series before saving.

### Imports, Files, and JSON

- Follow `read -> parse -> normalize -> validate -> review -> commit`.
- Preserve workbook/sheet/cell or JSON-path locations on every diagnostic.
- Version import/export schemas and reject unsupported required versions explicitly.
- Validate output paths, extensions, collisions, and writable destinations through shared IO policy.

## Implementation Phases

### Phase 1. Make Failures Observable

Status: in progress as of 2026-08-14.

- Change `DatabaseSchemaManager::ensureSchema()` to return `Status` and make database open fail if schema creation/migration fails.
- Add checked SQL execution helpers that preserve action, driver error, native error code, and relevant record identity.
- Convert all mutating repository methods to `Status` or `Result<T>`; remove `void` writes and zero/default-object error sentinels.
- Wrap every multi-statement write/delete in `DatabaseTransaction`.
- Start with `ClassRepository`, `TeacherRepository`, `CalendarEventRepository`, `CampusRecordRepository`, `SettingsRepository`, and `IntensiveSlotStateRepository`.

Exit criteria: no unchecked SQL write; callers cannot report success when persistence failed.

The first slice is complete: `DatabaseSchemaManager::ensureSchema()` returns
`Status`; every table, legacy-column, and index statement uses checked SQL;
schema changes run inside `DatabaseTransaction`; and `DatabaseSession::open()`
closes the connection and returns the detailed failure before constructing any
repositories. SQL execution errors now retain the driver text, database text,
native error code, query text, action, and optional record identity. Regression
tests cover contextual SQL errors, transactional rollback after a mid-schema
failure, and failed session initialization. The named repository contract
migrations and compound-write transactions remain in progress.

The second slice converts every `ClassRepository` and `TeacherRepository`
mutation to checked `Status`/`Result<int>` contracts and propagates those
contracts through `DataService`, `ClassService`, and `TeacherService` to the
production callers. Class and teacher compound deletes now use
`DatabaseTransaction`; injected failures verify that earlier roster deletions
and teacher-assignment changes roll back.

The third slice converts campus-record, settings, and intensive-slot-state
mutations to checked `Status`/`Result<int>` contracts and propagates failures
through `DataService` and the narrow settings/schedule services. Campus updates
no longer report success for missing records, schedule slot toggles keep their
current display and report failed persistence, and grouped settings writes use
one `DatabaseTransaction` so personal-details and Sub Prep saves cannot commit
partially. Injected SQL failures cover setting rollback, both intensive-slot
write branches, campus update/delete identity, and unavailable-service paths.
`CalendarEventRepository` was the remaining named repository for the next
Phase 1 slice.

The fourth slice converts all calendar-event mutations to checked contracts.
Single writes return `Result<int>`; single, repeat-series, and full-calendar
deletes return `Status`; and multi-event recurrence creation, recurrence edits,
and workbook imports use one transactional batch save. Calendar UI workflows
now report persistence errors and withhold refresh/reset-success signals after
failure. Trigger-driven tests cover contextual insert/update/delete errors and
prove a mid-batch failure rolls back earlier occurrences. The initial named
repositories are complete; Phase 1 continues with the remaining legacy write
contracts in `ClassInfoRepository`, `RosterRepository`, and
`SpeakingEvalRepository`.

The fifth slice converts the remaining `ClassInfoRepository`,
`RosterRepository`, and `SpeakingEvalRepository` mutations to checked `Status`
contracts. Class information, single and batch roster saves, and speaking
evaluation saves now use `DatabaseTransaction` and contextual checked SQL.
Their service and UI callers propagate failures; notably, the roster editor no
longer clears its dirty state after a failed save. Trigger-driven regression
tests prove rollback after failures partway through class-time replacement,
single and batch roster replacement, and speaking-evaluation cell updates. A
repository-header and SQL-call audit finds no remaining legacy mutation
contract or unchecked SQL write. Phase 1 continues with read contracts that
still conflate query failure with missing/default data.

The sixth slice starts read-contract migration with `CampusRecordRepository`.
Single-campus and campus-list reads now return `Result<T>` through
`DataService`; a missing campus, an unavailable profile, and a failed query are
observable errors, while a successful empty list remains distinct. The reads
also use contextual checked SQL, eliminating two previously unchecked query
executions. Class-time conflict detection now follows the same contract through
`ClassService`; failure to load the current class or comparison rows blocks a
save and is shown instead of being logged and treated as "no conflicts." This
removes the final bare `query.exec()` call from repository code. Lifecycle
coverage exercises successful-empty, missing-record, unavailable-session, and
failed-query outcomes. The next read slice should convert the core class and
teacher lookup contracts before the more structurally complex class-info,
roster, and speaking-evaluation loads.

The seventh slice converts the core class and teacher singleton/list reads to
`Result<T>` across their repositories, `DataService`, and narrow feature
services. Checked SQL now distinguishes successful empty lists, missing ids,
unavailable profiles, and query failures with record identity. Class transfer
and schedule import propagate those failures; schedule construction and import
resolution controls also return checked results. Navigation and destructive
actions fail closed, primary page/list loads display contextual errors, and
output/package workflows abort instead of rendering from fabricated empty
records. Regression coverage now includes each success and failure category.
The next read slice is the structurally compound class-info, roster, and
speaking-evaluation loads, followed by remaining settings/calendar collection
reads.

### Phase 2. Shared Validation Types and Core Rules

Status: partially complete through Plan 1 foundations.

- Add the structured issue types and a `ValidationResult` helper (`hasErrors`, `warnings`, `forField`, `merge`).
- Extract student-name, weekday/time, color, filename, and enum validation.
- Add table-driven Qt tests for Unicode, Korean suffixes, whitespace, time formats, color formats, duplicate pairs, and reserved filenames.

Student-name, weekday/time, color, and filename helpers already exist with
shared-policy coverage. The common `ValidationIssue`/`ValidationResult` model
and enum validation remain to be implemented.

### Phase 3. Domain Validators and Service Boundaries

Status: not started.

- Implement `TeacherValidator`, `ClassInfoValidator`, `ClassTimeValidator`, `CalendarEventValidator`, `RosterValidator`, and `SpeakingEvalValidator`.
- Invoke validators from feature services before repositories.
- Ensure imports call the same validators after parsing so manual and imported data obey the same invariants.
- Make conflict checks and uniqueness checks return structured issues rather than immediately opening dialogs.

Exit criteria: data cannot bypass validation by avoiding a particular dialog or page.

### Phase 4. UI Validation Experience

Status: not started.

- Use `QValidator`, input masks, combo restrictions, and maximum lengths only for rules that are safe while typing.
- Add `FormValidationBinder` to map `ValidationIssue.field` to widgets, set accessible error text, apply the error role/style, and show concise inline messages.
- Disable manual Save on hard errors. Autosave should remain dirty but pause while invalid, then resume after correction.
- On submit, focus and scroll to the first error. Show one summary prompt only when helpful; do not open one message box per field.
- Allow non-blocking warnings to save after explicit confirmation where product policy requires it.

### Phase 5. Database Constraints and Migrations

Status: not started.

- Enable `PRAGMA foreign_keys = ON` for every connection and verify it.
- Add foreign keys with deliberate `ON DELETE` behavior, especially class-to-roster/evaluation/times/info relationships and evaluation-to-data.
- Add `NOT NULL`, `CHECK`, and uniqueness constraints for stable invariants such as booleans, recognized state values, valid row indexes, and required identifiers.
- Replace ad-hoc `ensureTableColumn()` evolution with numbered, transactional migrations using `PRAGMA user_version`.
- Before adding constraints, run a preflight report against existing profiles and repair or quarantine invalid legacy rows. Back up a profile before a destructive migration.

### Phase 6. Regression and Failure Testing

Status: not started beyond existing characterization tests.

- Unit-test every validator and normalizer.
- Repository tests: constraint failures, transaction rollback, injected SQL failure, foreign-key cascade/restrict behavior, and migration from representative old schemas.
- UI tests: first-error focus, inline error text, Save disabled, autosave pause/resume, warning confirmation, and accessible descriptions.
- Import tests: mixed valid/invalid rows, duplicate names, invalid times, unsupported schema versions, and source-location diagnostics.
- Add fuzz/property-style tests for workbook/JSON parsers and filename/name normalizers where practical.

## Acceptance Criteria

- Every persistence mutation returns `Status` or `Result<T>` and is checked.
- Schema initialization and migrations are transactional and can fail database opening cleanly.
- Hard validation rules exist outside widgets and are reused by manual entry, import, and programmatic callers.
- UI validation is inline, accessible, and compatible with autosave.
- Database constraints protect stable invariants and referential integrity.
- Existing invalid profiles receive a defined migration/preflight path rather than failing without explanation.

---

# Recommended Pull-Request Sequence

Current execution status: sequence items 2, 3, 7–9, 12, and 13 are complete.
Items 1, 4, and 5 are partially complete. Items 6 and 10–11 are the remaining
validation/persistence stream. The immediate next step is the Plan 3 Phase 1
compound class-info, roster, and speaking-evaluation read-contract work.

1. Add characterization tests and measurements; no behavior change.
2. Introduce CMake target/test helpers and split CMake modules.
3. Move implementation-heavy headers into `.cpp` files.
4. Add checked SQL helpers; make schema setup return `Status`.
5. Convert the first repository group to `Status`/`Result` and RAII transactions.
6. Add shared validation types plus student-name and schedule-value rules.
7. Introduce `UserPromptService` and `FileDialogService`; migrate `FileController` as the pilot.
8. Migrate remaining static messages and file dialogs; add CI enforcement.
9. Add `PageHeader`, page scaffold, and `AutosaveCoordinator`; migrate one page at a time.
10. Add domain validators and `FormValidationBinder`; migrate teacher, class, calendar, roster, and speaking evaluation.
11. Add migration runner, foreign keys, and database constraints with legacy-data preflight.
12. Introduce narrow feature services and retire `DataService` methods incrementally.
13. Decompose the largest UI/output/import units after the shared foundations are stable.

This order deliberately fixes error observability before adding more validation, and establishes testable dialog interfaces before changing every call site.
