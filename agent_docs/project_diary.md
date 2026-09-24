# Project Diary

## Decisions and Lessons

- The repository is intentionally layered: domain models and validation are
  separate from SQLite repositories, feature services, and shared Qt UI. Keep
  new work at the narrowest appropriate layer.
- ClassMngrNext starts as a separate QCoreApplication console bootstrap. Keep
  it outside ClassMngrRuntime and avoid adding a second UI or legacy resource
  and deployment links; UI work belongs in the later shared-UI phase.
- Define next-generation build boundaries before assigning migrated sources.
  Keep their target names separate from legacy object targets, encode allowed
  dependency edges in CMake, and leave source-free interface targets out of the
  bootstrap's link graph until a later slice needs them.
- Measure Qt header dependencies per production object target before narrowing
  shared module links. Keep the legacy runtime's full required module union
  explicit for its executable, tests, and QML/resource graph.
- For explicit ownership, keep per-target source lists as the authority and use
  recursive globs only to detect handwritten files missing from those lists.
  Record QML and included `.inc` fragments too, while keeping generated outputs
  and configure templates outside the handwritten inventory. Compile reused
  test doubles once in object libraries; split variants when test targets need
  different compile-time behavior. Keep platform-specific test inventory
  aligned with platform-conditional target declarations: exclude inactive test
  sources on other platforms while retaining explicit ownership on the target
  platform.
- Keep formatter and static-analysis enforcement scoped to the new code until
  legacy code has been migrated. Generate module/resource reports only after
  targets and resource-pack declarations are finalized. Treat the update-only
  `roster-designs` pack as an optional runtime resource with no baseline RCC.
- ApplicationServices is the preferred application boundary. DataService
  remains as a compatibility facade while callers migrate; UI/controllers
  should not add direct repository usage.
- Feature-scoped assets are produced as standalone RCC resource packs. Do not
  assume every asset belongs in the main executable bundle; follow
  cmake/resources.cmake when changing packaging.
- Treat Packaged Release as each platform's actual distribution output from its
  Release install/deploy workflow; do not create a parallel package path.
  Cross-build ARM64 on the Windows x64 runner without executing its binaries.
- Validate the exact compiler/generator used by the shipping workflow. A local
  VS2026/MSVC build with Qt's VS2022 kit is supplemental evidence and does not
  replace a hosted VS2022 result.
- Build evidence must be checked against CMake: the project, BUILDING.md,
  active CI workflows, and release helper now agree on Qt 6.12.0. Keep future
  examples aligned with that minimum.
- The Phase 0 memory contract separates the final <250 MiB end-of-rewrite
  normal target from the temporary <512 MiB diagnostic ceiling. Legacy heavy
  routes retain both comparisons as trend evidence; do not treat an over-target
  legacy route as a Phase 0 failure.
- When native desktop automation is unavailable, a small in-process Qt
  controller can drive real modal dialogs and capture the packaged offscreen
  state. The Sub Prep route now retains both valid-generation and disabled-OK
  validation references, and the Speaking Evaluation route retains the
  PowerPoint renderer-selection reference without claiming external Office
  automation ran.
- Generated output folders may receive sandbox-only ACLs. Before staging
  retained artifacts, verify the exact path and grant the normal Git identity
  read access only to that generated evidence folder; do not discard the
  generated PDFs.
- For packaged feature visuals, an opt-in environment variable on an existing
  Heavy-route lifecycle keeps the production path unchanged while allowing
  in-process screenshots after real selection and re-entry operations. The
  Classes visual slice uses
  CLASSMNGR_STARTUP_CLASSES_VISUAL_OUTPUT_DIR and retains four language/theme
  variants; it records capture success in the same startup profile as the
  lifecycle assertions.
- For a fast asynchronous UI boundary, capture the loading state immediately
  after the real action and retain the event-loop poll as a fallback. The
  Schedule Import slice uses the existing large-workbook route to retain
  Loading workbook..., indeterminate progress, disabled source/load controls,
  and a validated screenshot for both cancel and apply outcomes without
  changing the production path.
- When a loading control is below the visible fold, an evidence-only probe may
  move the existing scroll bar before grabbing the real dialog and restore its
  value afterward. Calendar Import uses this to show Importing events... and
  the disabled Import Events button without changing the production UI path or
  the later Preferences reference state.
- A Schedule Import conflict warning is only created when the projected
  preview actually contains overlapping meetings; a large fixture that merely
  contains invalid patterns does not exercise that modal. For an evidence-only
  boundary, make the cancel fixture overlap deterministic day/time slots, poll
  the existing event loop for the asynchronously queued QMessageBox, capture
  it, and keep the apply fixture conflict-free so the transaction path remains
  independently measurable.
- The Calendar Import parser-failure slice is not accepted until it has fresh
  Release evidence. On this host, the preset MSBuild tree hit a FileTracker
  UnauthorizedAccessException, an ambient Ninja tree could not resolve MSVC
  standard headers, and a correctly initialized Visual Studio shell still did
  not finish CMake generation even with BUILD_TESTING=ON. Treat compiler
  probes/optional-component notices or missing executables as an environment
  blocker, not as evidence that the source compiles; preserve the committed
  implementation for the next clean build attempt.
- The Calendar Import parser-failure boundary uses a deterministic 69-byte
  malformed local HTTP response. Its focused Release route must retain the
  complete workflow/metrics/manifest/trace, prove the real parser error,
  re-enable Import Events, preserve the event count, and omit success
  checkpoints. The unchanged success route and the opt-in-cleared full suite
  are separate required checks.
- When the Calendar Import error status is below the Preferences viewport fold,
  the evidence-only probe must move the Calendar-tab scroll bar to its maximum
  before grabbing calendar-import-error.png, process the view update, and
  restore the prior value. A non-empty screenshot alone is insufficient;
  manually verify that the real error text and re-enabled Import Events control
  are visible.
- Multi-config CMake generators previously exposed the default
  Debug;Release;MinSizeRel;RelWithDebInfo set. The source-level configuration
  guard now limits them to Debug;Release, while single-config presets retain
  their explicit build type.
- Release presets explicitly set BUILD_TESTING=OFF, and test targets without
  a QML module opt out of Qt import scanning. This removed the unnecessary
  qmlimportscan target fan-out seen in the Debug tree without changing test
  source or link ownership.
- The tracked cmake tree remains split by source, resources, deployment,
  platform, and test concerns because those boundaries are active. Only the
  empty root-generated CMakeFiles residue was removed; root CMake output is
  ignored, and build/dist artifacts are preserved.
- The Phase 0 platform contract is Windows x64 plus macOS universal. By user
  decision, Windows ARM64 and Linux are unofficial ports deferred to later;
  do not list them as Phase 0 blockers. The exit gate requires all 24 routes
  for both supported targets; neither a single-platform run nor the retained
  macOS packaging baseline alone satisfies it.
- The scripts/phase0 runner uses existing opt-in packaged Qt routes and a
  fresh caller-selected evidence run directory; its plan mode is non-mutating.
  The validator checks artifacts, JSON/manifests, lifecycle checkpoints and
  memory trends, and can consolidate a macOS run. Use --require-exit-gate
  when automation must fail until all supported-platform evidence passes.
  Legacy 250 MiB measurements remain trend-only; 512 MiB is a diagnostic
  ceiling, not a Phase 0 pass criterion.

## macOS universal route-matrix lesson - 2026-09-17

- The first packaged Release matrix's only two failed routes were the Calendar
  Import success/error cases. Both failed at the test harness's
  QTcpServer::listen(QHostAddress::LocalHost) fixture before launching the
  packaged app, with Unknown error. Focused and final full reruns passed with
  unchanged source and binary hashes. This points to a transient host/loopback-
  bind limitation rather than a demonstrated product defect; the available
  logs do not establish a specific sandbox denial. If it recurs, investigate
  host local-listener permissions before changing product code.

## Windows route-matrix and output-reference lessons - 2026-09-17

- Run the complete 24-route Windows x64 matrix from a fresh Release package and
  a separate Debug harness build. Require both each route's process record and
  the runner's validator result to show normal completion and no timeout; a
  per-run pass still leaves Phase 0 open until macOS universal also has 24/24.
- The full route root is 147,732,123 bytes and includes repeat generated PDFs,
  a ZIP, and other machine artifacts. Keep its full root available for
  revalidation when needed; the compact checked-in audit bundle contains the
  original run manifest, validation summary, and per-file hashes, but is not a
  replacement evidence root.
- Retained PDF preview PNGs must composite rendered page pixels onto opaque
  white RGB before saving so transparent PDF backgrounds display as intended.
  The capture helper must keep the renderer's input and generated PDF intact;
  validate image type/alpha, PDF signatures/page counts, manifest byte sizes,
  and representative frames after capture.
- In a noninteractive Windows session, PowerPoint COM can fail before any PDF
  is generated (0x80070520), and clipboard-dependent UI tests can fail while
  opening the system clipboard (0x800401d0). Record these as environment
  limits; do not claim native Office output or suppress the failing checks.

## macOS QtTest sandbox lesson - 2026-09-18

- Qt 6.12's macOS QWizard loads its default background through an
  NSWorkspace/LaunchServices lookup for com.apple.KeyboardSetupAssistant. In a
  restricted test process that lookup can return a nil URL, causing
  NSBundle to throw before test assertions run. Rerun the focused CTest target
  with normal macOS service access before treating this as an application
  regression. A later full macOS universal run with normal service access
  passed 67/67, including the wizard, updater, clipboard, and UI tests that
  failed under restriction. The user also confirmed the application setup flow
  works.

## Qt 6.12 Windows toolchain validation - 2026-09-18

- Qt 6.12's supported Windows compiler is Visual Studio 2022. The local host
  has Visual Studio 2026 only, so use the `windows-2022` hosted runner with
  the matching MSVC 2022 Qt kit for shipping-toolchain acceptance. Local
  VS2026 results remain supplemental. Keep the Windows packaged Release
  workflow on relevant pull requests so this check runs automatically.
- Windows CTest executables need the selected Qt `bin` directory in their
  runtime path even when a developer's machine already has it globally. Add
  that path through CTest environment modifications, and give settings tests
  a build-local `CLASSMNGR_SETTINGS_ROOT` so they do not read or change a
  developer's saved profile.
- Keep the startup performance CTest serial. Running it alongside the large
  batch-report test raised measured startup from about 3 seconds to 8 seconds
  and made the threshold-sensitive run fail; the isolated serial run passed.

## Linux Phase 0/1 follow-up — 2026-09-19

- On Linux, `/proc` pseudo-files report size zero. `QFile::atEnd()` may then
  report end-of-file before reading `/proc/self/status`; read lines until
  `readLine()` returns empty, and test the parser with an injected procfs root.
- Keep the Phase 0 Linux baseline supplemental to its completed Windows/macOS
  exit gate. A headless packaged run must use the staged package's Qt xcb
  plugin under Xvfb; an external development Qt plugin changes the runtime
  under measurement. Record sandbox X11 socket failures as failed evidence.
- The local updater test's loopback listener failures were caused by socket
  creation returning `EPERM` in the sandbox. Preserve those assertions and
  rerun on a host that permits loopback instead of skipping the tests.

## Async prompt title repair — 2026-09-19

- Keep synchronous and asynchronous acknowledge prompts on one fully configured
  `QMessageBox` path. On the Qt 6.12 macOS offscreen path, the requested title
  was not reliably exposed through the message-box setup; reapply it to the
  inherited widget after button configuration and before `exec()`/`open()`.
- Keep `PromptSnapshot::title` sourced from the actual dialog's
  `windowTitle()`. The focused dialog-service target passed after this repair;
  `propagateSizeHints()` and font-alias warnings remain non-fatal.
- A restricted full-suite result of 62/67 was caused by unrelated no-screen GUI
  and local-port binding failures. Preserve those assertions and rerun on a
  host with the required services rather than weakening coverage.

## Phase 2 domain-contract kickoff — 2026-09-19

- Start v2 domain work with standard-library-only typed identifiers and
  structured operation results. Keep the Domain target free of Qt and make
  category mistakes compile-time errors through distinct identifier tags.
- Attach header-only contracts to `ClassMngrNextDomain`, record them in the
  explicit source-ownership manifest, and test them through an app-less QtTest
  target before introducing application services.
- Keep Phase 1 hosted closure separate from Phase 2 implementation. The
  macOS action, quality, dialog-policy, and release evidence are now recorded
  as green on commit `0883009d`; Phase 1 is closed and Phase 2 may proceed.

## Phase 2 settings persistence — 2026-09-23

- When cutting `OptionState` persistence over to a typed port, attach the
  `onPersist` bridge before the first startup `set()`. Preserve exact stored
  integer values and existing malformed-read fallbacks, and keep `onChanged`
  wiring intact. Cover adapter round trips, invalid-write no-ops, startup
  canonicalization, and reload behavior.

## Phase 2 typed preference persistence — 2026-09-23

- Attach each typed OptionState persistence callback before its first startup
  mutation. Once every caller is explicit, remove the generic SettingsManager
  fallback so new options cannot silently bypass their typed contract.
- Preserve existing preference keys, values, malformed-read defaults, and
  purpose slugs while moving writes behind typed ports.
- Keep file-dialog directory values and purpose identifiers in the Qt-free
  Application contract. Let the QSettings adapter own storage compatibility,
  and compose the adapter and service in main before MainWindow. CMake should
  express the Application-to-legacy-UI dependency explicitly without making
  UI depend on Platform.

## Phase 2 Sub Prep summary query contract — 2026-09-23

- Keep schedule scope typed in Application: use explicit weekday and schedule
  mode values rather than localized labels or raw integers.
- Treat an empty class/day scope as a valid empty projection without a read;
  empty feature states are part of the UI contract, not invalid input.
- Keep the Phase 2 query boundary independent from the legacy Sub Prep page.
  Do not claim database batching or memory reduction until a persistence
  adapter and the full feature route provide evidence.

## Phase 2 Sub Prep details and selection lifecycle — 2026-09-23

- Read selected details by typed class ID and keep one detail value for the
  active selection; validate its bounded text and identity at each boundary.
- On every successful scope refresh, retain the selected class only if it is
  still visible and clear the previous detail value. Carry the expected
  teacher ID from the refreshed summary so stale detail results cannot attach
  another teacher's data to the selection.
- Leave fallback tab ordering to the view: the legacy grade/level ordering and
  the v2 summary ordering are different. These contracts are not connected to
  the page and do not establish persistence batching or memory improvement.

## Phase 2 Sub Prep print-source contract — 2026-09-23

- Keep operation-scoped source facts in one bounded owned value, with typed
  class and teacher references and stable adapter order. Reject malformed
  output as a whole; an empty scope should avoid the port call.
- The app-less query verifies value ownership and validation, not adapter
  release or output-stage lifetime. Keep SQL batching, PDF/package cleanup,
  parity, and memory claims behind their later integration gates.

## Phase 2 custom-color caller boundary — 2026-09-23

- Keep legacy settings access in the Platform adapter and pass its typed
  preference port into shared UI utilities. Preserve the color dialog's
  load-before-open and save-after-close behavior, including cancellation; test
  all process-global color slots and restore them after each test.

## Phase 2 calendar import plan — 2026-09-23

- Keep the existing six-field import signature as an opaque UTF-16 identity
  key when moving duplicate selection into a Qt-free Application contract.
  Preserve candidate order, initial parser skips, and batch save behavior; test
  identity-field normalization separately from the pure duplicate planner.
- Keep the importer's existing-event identity at the Qt/legacy boundary when
  moving the read behind a typed Platform port. A general event projection's
  capacity and unrelated metadata validation change legacy import behavior;
  return ordered UTF-16 signature keys and compare them against the canonical
  parser helper, including ranges beyond the projection limit.

## Phase 2 Sub Prep print-source Platform read — 2026-09-23

- Parse typed lexical IDs canonically before mapping to legacy integer keys;
  strings like `"01"` can alias `"1"` after conversion.
- Apply class, weekday, and mode filters in SQL before copying records. Use
  per-class and aggregate limit-plus-one sentinels so overflow is visible and
  returned materialization stays within the Application contract bounds.
- A valid 4,096-class scope exceeds older SQLite bind-variable defaults.
  Decimal formatting is safe after strict integer parsing; continue binding
  weekdays and limits.
- Preserve legacy omissions for classes without a usable teacher. A fixture
  for an orphan teacher assignment must seed that invalid state explicitly
  with foreign-key checks disabled only around the direct update.
- An adapter and focused tests do not prove page/PDF parity, roster/package
  migration, SQL batching, or a memory improvement; keep those gates open.

## Phase 2 Sub Prep selected-class details Platform read — 2026-09-23

- Expose teacher facilities as separate bounded fields so later page consumers
  do not need to parse a formatted string. Keep the selected-class details
  lookup on the active repository session, scoped to one class, with no
  schedule-table read or `DataService` fallback.
- Preserve the legacy empty-teacher fallback for unassigned or stale teacher
  references. Existing classes without class-info rows still yield blank
  details, while absent classes remain `NotFound`; canonical lexical IDs avoid
  integer aliases such as `"01"`.
- Adapter tests and a Debug build prove the read seam and value bounds only.
  Page/output wiring, full parity, and Release memory acceptance remain later
  gates.

## Phase 2 Sub Prep schedule-summary Platform read — 2026-09-23

- Keep class, day, and schedule-mode filters inside the persistence query so
  irrelevant schedule rows are not copied into the summary operation. The
  selected mode should remain independently readable even if the other mode's
  schedule table is unavailable.
- Build one bounded summary per requested class and aggregate roster counts in
  a scoped batch; do not load student roster rows for the list view. Preserve
  the legacy zero-count fallback when the roster aggregate cannot be read.
- The adapter and its focused tests establish a bounded read contract only.
  Wiring the projection into the live page, replacing per-class widget trees,
  output parity, and Release memory evidence remain separate acceptance work.

## Phase 2 Sub Prep live class-information view - 2026-09-23

- Bind navigation to a compact summary model and keep the selected typed ID
  and one selected-details value in Application state. The page renders one
  reusable detail panel; it must not recreate a hidden widget tree per class.
- Refresh the projection in place on schedule-mode changes and preserve a
  selection only while its class remains in the current schedule scope.
- Release the selected details and summary projection from the page lifecycle
  hook on deactivation; mark the page stale so re-entry creates fresh state.

## Phase 2 Sub Prep information-sheet output - 2026-09-24

- Keep the print query scope aligned with the dialog's own selected class IDs,
  weekdays, and active schedule mode. This replaces the old broad all-class
  reads for the main information sheet.
- Preserve every teacher field used by the renderer, including the full
  `preferredDisplayName()` fallback chain. The bounded Application value now
  includes English name, Korean name, preferred name, and preferred
  romanization; a tested UI-boundary mapper builds the compatibility model.
- This only migrates the information sheet. The roster-PDF stage still loads
  full legacy class, teacher, and roster records, and the document model still
  copies the renderer model. Keep those output costs and end-to-end parity open.

## Phase 2 Sub Prep renderer model lifetime - 2026-09-24

- Make the renderer document a synchronous view over the request's
  `classInformation` list. A `std::reference_wrapper<const QList<...>>` makes
  the borrowed boundary visible and removes a second rich class/teacher model
  allocation. Move the page request into the package request so the same list
  is not duplicated at that ownership transfer.
- Keep the request alive through the synchronous renderer call. The package
  service still retains that request through roster generation, so the next
  output slice must release its main-sheet values before materializing roster
  output.

## Phase 2 Sub Prep package stage release - 2026-09-24

- Let `SubPrepPackageService::generate` own the operation request. The page
  moves its package request into the call so a large information-sheet model
  is not copied at the UI/service boundary.
- Render the main sheet before loading full roster output records, then clear
  the request's Sub Prep document input before roster materialization. Preserve
  the existing package tree, document order, and error status behavior.
- The roster source is still legacy class/teacher/roster data. Keep its typed
  operation contract as the next independent migration slice.

## Phase 2 Sub Prep roster-output contract - 2026-09-24

- Keep the roster output projection separate from the information-sheet
  projection. Both are scoped to the dialog's selected class/day/mode, but
  roster cells exist only for the output operation and should be released
  after it finishes.
- Query validation after a read-port call protects the Application boundary,
  but does not bound adapter allocation. The Platform/repository adapter must
  reject excess columns, rows, cells, and bytes while reading, before building
  full `Roster` values. Do not claim the roster memory gate until measured in
  the packaged Release route.
- The repository read uses `MAX(row_index)` over only requested physical
  columns before allocating the dense compatibility rows. It streams selected
  values and SQL-truncates each fetched value before checking the original
  byte length, so oversized text is rejected without materializing a full
  cell or silently truncating it.

## Phase 2 Sub Prep roster-output Platform read - 2026-09-24

- Parse canonical typed class IDs before mapping them to the legacy integer
  keys. Preserve the regular schedule query's default teacher filter and opt
  into unassigned classes only for roster output, whose package includes them.
- Share teacher facts across selected classes, and subtract class/teacher and
  schedule text already copied before passing remaining row/cell/text budgets
  to the bounded roster repository read.
- Keep adapter coverage grounded in valid domain fixtures: teacher preferred
  names must match a display choice, class levels must be valid for their
  grades, roster data must contain all base columns and acceptable student
  names, and same-day meetings must not overlap.
- The Platform source and three focused CTest suites verify the read seam.
  Package mapping, complete output parity, packaged Release memory evidence,
  and Phase 2 acceptance remain open.

## Phase 2 Sub Prep package roster-source integration - 2026-09-24

- Keep the package service dependent on the typed read port. The page can
  construct the session-backed adapter for the synchronous call, while the
  package service maps bounded, owning Application values into renderer
  inputs. This removes its direct service/database reads.
- Validate UTF-8 round trips at the Application-to-Qt renderer boundary and
  parse legacy integer keys canonically. Keep folder-name sanitization in the
  existing helper (`:` becomes `.` on Windows); tests should assert the safe
  path form rather than an unsanitized display string.
- Five focused Windows x64 Debug suites pass across package, page, PDF,
  Application query, and Platform source. This verifies scoped input mapping
  and pre-commit failure cleanup, not full output parity or Release memory.

## Phase 2 Sub Prep output-reference parity - 2026-09-24

- Use the retained 96-class `Sub Prep.pdf` and Daily roster PDF as package
  output oracles. Compare every page's point dimensions and extracted text;
  on the Windows baseline target, render each page at 150 DPI and compare the
  pixels. Both outputs match the reference (19 and 16 pages respectively).
- PDF container hashes differ between runs, so compare semantic text and page
  rendering rather than raw PDF bytes. The Windows pixel comparison is exact.
- Print-only cancellation removes temporary PDFs and commits no folder. Errors
  while reading or mapping the roster source likewise leave no package staging
  directory. Release memory and all feature visual-state gates still require
  their own acceptance run.


## Phase 2 Sub Prep packaged memory measurement - 2026-09-24

A passing packaged route and output parity do not close the Sub Prep memory
gate. Record both one- and five-second settled measurements: F9 stayed below
512 MiB and improved over legacy peaks, while its approximately 307 MB settled
working set remains above the 250 MiB end-of-rewrite target.


## Phase 2 calendar import batch save - 2026-09-24

A duplicate-only calendar import is a valid empty batch. Preserve the planner's
accepted input order and send the whole batch through one service call so the
repository transaction still rolls back earlier rows if a later insert fails.


## Phase 2 calendar reset mutation - 2026-09-24

Keep the calendar reset availability check ahead of the destructive prompt.
After confirmation, route deletion through the typed Platform port and map its
owned UTF-8 error back to the existing warning UI. Verify the service error
path with a database trigger and confirm the seeded event survives the failed
delete.


## Phase 2 calendar availability boundary - 2026-09-24

Availability checks belong at the Platform boundary with the event reads and
writes. Keep import-start and dialog-opening guards behaviorally unchanged, but
do not keep a raw `CalendarService*` in the feature when the adapter can answer
the same question. A source search after the cutover confirmed no direct
calendar-service getter calls remain under `src/features/calendar/`.


## Phase 2 calendar edit-draft flow - 2026-09-24

When the calendar projection is already typed, pass its values directly into
`CalendarEventEditDraft`. Avoid converting through the old `CalendarEvent`
model and back before the dialog. Keep the dialog's Qt conversion private to
its UI boundary and drive save, repeat, and delete requests from the typed
draft.


## Phase 2 calendar display-preference boundary - 2026-09-24

Use the `ApplicationServices*` entry point for the preferences adapter when a
panel currently stores `SettingsService*` only to construct that adapter.
Keep default reads and unavailable saves as successful no-ops, with atomic
multi-key writes owned by Platform.


## Phase 2 academic calendar preference ports - 2026-09-24

When a feature provider only needs persisted values, inject its Application
preference ports and construct Platform adapters at the UI or service
composition boundary. Keep legacy SettingsService ownership inside Platform;
the provider should not recreate its own adapters during each read or write.


## Phase 2 calendar color preference boundary - 2026-09-24

Let the Platform preference adapter own the unavailable-service check. A
calendar UI caller can read an empty stored color and use its existing
default-color policy, or issue a save that becomes a no-op when settings are
unavailable, without holding SettingsService.


## Phase 2 calendar current-campus availability - 2026-09-24

When a feature must preserve a larger preference-loading gate, expose the
availability check through the Application port and map it in Platform.
This keeps the feature from reaching for SettingsService while preserving
unavailable defaults and avoiding unrelated repository reads. Keep the
contract comment explicit about availability versus an empty value.


## Phase 2 calendar import signature query - 2026-09-24

Keep import duplicate detection on its dedicated legacy range query: the
general calendar projection is bounded and applies stricter metadata rules
that are not part of import identity. Expose the dedicated read as a Qt-free
Application contract, and keep QString normalization and UTF-16 key creation
at the Platform boundary. Remove the old concrete API once the workflow uses
the contract; do not bridge through the capped projection.


## Phase 2 personal display-name adapter migration - 2026-09-24

When replacing a legacy store pointer with an existing typed preference
adapter, preserve the caller's event order and transformation rules. In
Sub Prep, name persistence occurs after folder selection and replacement
confirmation but before the later filesystem `mkpath`; a subsequent package
creation failure does not undo that write. Keep that baseline ordering while
moving unavailable-store handling into Platform.


## Phase 2 personal display-name adapter constructor removal - 2026-09-24

Migrate the remaining display-name consumers through the existing
`ApplicationServices&` adapter before removing the `SettingsService*` overload.
Keep each caller's availability guard, whitespace/trim behavior, fill-only-if-
blank rule, and aggregate save path. For PageManager-backed tests, distinguish
missing resource-pack setup failures from failures in the migrated preference
path; report the test limitation rather than treating the whole target as green.


## Phase 2 class-navigation helper removal - 2026-09-24

Once production callers use the typed Application/Platform boundary, remove an
obsolete legacy preference helper together with its source-owner entries and
stale includes. If a header had also been providing an unrelated model type,
include that model directly. Keep the typed adapter and page behavior suites;
an unused helper's removal should not remove active preference coverage.


## Phase 2 calendar-import campus-code query - 2026-09-24

Keep the importer-specific campus-code read separate from general campus
projections when their validation or size limits would change legacy behavior.
Move repository/resource access to Platform, return owning UTF-8 values through
a Qt-free Application port, and preserve the repository's ordering, trim,
blank, duplicate, and silent-empty fallback rules.


## Phase 2 CalendarPage campus metadata query - 2026-09-24

Keep CalendarPage's metadata lookup separate from the importer's code-only
query. Return owning UTF-8 values from the Application contract and leave the
repository/resource path in Platform. When adapting optional codes, omit only
empty strings; whitespace-only values remain observable until the page's
legacy cleanup step. Compare alias construction against the committed baseline
when no page-specific behavior test exists.


## Phase 2 PersonalSignatureImagePort caller cutover - 2026-09-24

When moving read-only UI callers from a settings-service pointer to an
ApplicationServices owner, keep the read adapter's decoding/preparation path
and each caller's existing availability guard intact. Run the exact feature
cases in addition to the whole page suite; missing shared resource packs can
fail unrelated top-level page tests in an isolated tree.


## Phase 2 custom-color adapter constructor cleanup - 2026-09-24

Keep caller-owner cleanup distinct from a previously completed preference
behavior migration. Pass the ApplicationServices owner through existing typed
adapters, and verify every caller compiles even when only a subset has direct
picker tests. Preserve the dialog/save order and cancellation behavior by
leaving the shared ColorUtils flow unchanged.


## Phase 2 Sub Prep typed settings gate removal - 2026-09-24

Sub Prep page tests link a `DataService::isOpen()` stub whose database-open
flag defaults to true; setting it false exercises unavailable typed preference
ports. The fake service has no live session, so do not call `closeDatabase()`.
Set the stub flag before constructing or querying `ApplicationServices` and
assert both service availability and the page-level no-op behavior.


## Phase 2 My Information campus chooser query - 2026-09-24

Keep feature-specific campus metadata queries narrow when directory consumers
need different fields. My Information needs only owning UTF-8 IDs and chooser
labels; a CalendarPage-specific query should not become its implicit contract.
Compare repository order, trim/fallback behavior, and combo ID payloads against
the pre-migration page. A repository codec may normalize blank fields before
the adapter sees them, limiting fixture coverage of defensive empty-value
branches.

## Phase 2 Sub Prep campus detail query - 2026-09-24

Keep campus query projections specific to each feature: Sub Prep needs office
details that My Information's chooser contract does not. When moving Qt records
to owning UTF-8 values, retain original IDs for selection while applying the
existing trim/fallback rules to labels. Test the adapter through repository
fixtures so repository ordering and record omission remain part of the
observed contract.

## Phase 2 Personal Details atomic-save caller cutover - 2026-09-24

Keep the UI callers on the `ApplicationServices` owner when removing raw
settings-service adapter constructors. For My Information, retain the
availability return before autosave cancellation or field normalization, and
test that unavailable saves leave entered values and dirty state intact.
Preserve the atomic `saveAll` operation and test adapter rollback separately
from caller ownership changes.

## Phase 2 Personal Signature Preferences caller cutover - 2026-09-24

Keep read-only preference adapters on the `ApplicationServices` owner, with
availability checked inside the adapter and at any broader page guard that
protects neighboring operations. Test null/unavailable results without adding
writes; retain existing defaults, normalization, and UTF-8 conversions.

## Phase 2 current-campus preferences caller cutover - 2026-09-24

Keep the current-campus preference cutover separate from campus-directory
queries. Retain the page-level availability guard around its other reads, keep
the stored-ID/name correction timing, and verify that unavailable typed reads
and writes keep their existing result behavior.

## Phase 2 Personal Zoom preferences caller cutover - 2026-09-24

When removing raw settings-service callers from a preference adapter, keep
legacy fallback/migration rules explicit. Migrate only when the primary key is
absent, ignore migration-write failure when returning the legacy value, and
test primary precedence separately from the migration failure path.

## Phase 2 typed settings availability guards - 2026-09-24

Reuse a typed persistence-availability query when a page only needs to gate
settings operations. Keep the guard ahead of widget reads and mutations, and
ahead of save-side effects such as autosave cancellation and field
normalization. Directly test unavailable initialization and validation paths;
source inspection alone left the validation path without a regression check.

## Phase 2 Class Notes save boundary - 2026-09-24

When a legacy validator counts `QString` units, use a UTF-16 owning type at a
Qt-free contract boundary; a UTF-8 byte limit would change accepted inputs.
Test the page's default adapter path through real persistence in addition to
separate adapter and page-fake tests, so composition wiring and dirty-state
clearing are covered end to end.

## Phase 2 Sub Prep calendar interval query - 2026-09-24

Use the product's current-and-following-year window instead of carrying the
legacy all-years range forward. Preserve full vacation/holiday intervals so
the dialog can connect blocks across its lookahead; do not apply a generic
projection cap. Capture one reference date for both query bounds and dialog
defaults to avoid a year rollover mismatch.

## Phase 2 plan and exit-gate audit - 2026-09-24

Distinguish app-less contract coverage from production UI integration and
baseline-fixture parity. A coordinator boundary can preserve snapshots in its
tests while the FileController replacement path still closes the active
database before the new open/create succeeds. A source scan of `src/next` also
does not by itself describe the legacy bridge behind outer ApplicationServices
adapters.

## Phase 2 Calendar import planning parity - 2026-09-24

Exercise the live importer with a required local workbook fixture and a loopback
URL override so production parser, typed query/planner, and persistence wiring
are all covered without a changing external sheet. Keep parser signature
deduplication distinct from planner duplicate-candidate handling: the parser
removes those candidates before the planner sees them. Include required binary
fixtures in the slice commit; a passing local test is insufficient if a clean
checkout cannot obtain its workbook.

## Phase 2 Schedule Import state validation - 2026-09-24

A persisted snapshot staying unchanged does not prove a validator ran before
writes; a later transaction rollback can produce the same observation. Use an
aborting SQLite BEFORE UPDATE trigger with a distinct failure message on a
proposed write, then assert the typed validation error wins and persisted data
is unchanged. Convert Qt names, days, and times at the repository boundary so
the Application contract remains standard-C++ only. Keep that claim scoped to
the Application contract; Platform adapters and the Next entry point use Qt.

## Phase 2 Schedule Import matching and preview - 2026-09-24

For a Qt-free matching contract, keep Unicode normalization at the existing
repository edge: carry explicit grade, level, and room match keys produced by
`QString::simplified().toCaseFolded()`, while keeping raw room labels for
preview display. The application projection can then preserve ranked matching
without depending on Qt. Verify edge normalization through the required
workbook integration path by seeding a room with surrounding whitespace. Keep
the checked-in fixture mandatory so preview parity still runs from a clean
checkout. The optional external workbook check remains a separate supplement
and may skip when its environment variable is absent.

F39 moved live projected-overlap review into a Qt-free Application projection
shared with apply validation. Return both conflicting intervals and their class
labels so the UI can localize warnings at its edge while apply validation uses
the same conflict ordering and half-open overlap semantics. Keep adjacency,
weekdays, Normal/Intensive and skipped/preserved schedules, a required
conflict-workbook review path, and pre-write no-mutation evidence explicit in
tests.
