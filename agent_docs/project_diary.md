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
- Navigation parity needs a dirty-page case with an open workspace as well as
  closed-session and clean-landing cases. Verify Cancel preserves the complete
  form and page state, and verify Discard against persisted data so UI reset is
  not mistaken for discarding the edit.
- For a typed Campus Dashboard writer, reuse the owning Qt-free campus snapshot
  across read and save contracts, preserve the repository codec's canonical
  `image_main` rule, and stop selection/create transitions when a save fails so
  dirty edits remain reachable.
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
- For settings repositories that return the same invalid QVariant for an
  absent key and stored SQL NULL, prove read non-materialization by querying
  row existence/count directly; QVariant validity alone cannot distinguish
  the two states.
- When a Platform adapter replaces a legacy feature-service call with direct
  repository access, preserve the service's normalization, validation, and
  changed-cell persistence semantics at the boundary; the service may own more
  than storage routing.

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

## Phase 2 Domain schedule-time value - 2026-09-24

Represent accepted weekday/minute intervals with a validated Domain value and
keep untrusted raw input plus display labels at the Application boundary. Map
invalid values to the existing labeled validation error before conflict
projection, then carry the typed value through review/apply overlap checks.
This avoids repeating validation during pairwise conflict comparisons while
preserving the half-open rule.

## Phase 2 Schedule Import review-decision contract - 2026-09-24

Keep choice acceptance in one Qt-free Application contract used by both review
readiness and repository plan validation. Leave workbook/content checks at the
feature edge, and keep current-state validation immediately before database
writes. Pair the required fixture's successful parse-preview-apply path with
the existing conflict/rejection fixture so baseline parity has both outcomes.


## Phase 2 failure-atomic workspace replacement - 2026-09-24

When repository adapters hold `QSqlDatabase&`, moving a database wrapper out of
a temporary candidate leaves those references dangling even if the SQL
connection handle itself remains registered. Keep the referenced database
object at a stable address and transfer ownership of that object together with
the repositories. Exercise the first settings write after open, failed and
successful replacement, same-path reopen, and candidate-connection cleanup;
the original snapshot tests alone did not expose the lifetime defect.


## Phase 2 Class Transfer review decisions - 2026-09-24

Dialog readiness is only a preview; it cannot authorize apply against choices
the user made earlier. Rebuild repository matches from current database state
and validate every choice against those matches immediately before the existing
apply preflight. Preserve the distinct rules for zero, one, and ambiguous
teacher matches, and keep UI messages localized at the edge.

## Phase 2 fixture boundary evidence - 2026-09-24

For import parity, a required local fixture must traverse the production path
from parsing through the actual dialog-produced plan to repository apply.
Separate parser/apply and dialog-plan tests can both pass while their
connection remains untested. Keep optional external samples supplemental.

## Phase 2 Domain course catalog - 2026-09-24

When rejection must preserve persisted state, seed representative existing
rows and compare their values before and after the operation. Zero-row counts
only show that no rows exist; they do not prove that invalid apply left prior
records unchanged. Keep the Domain catalog as the single source for business
validation and convert to Qt lists only in the existing UI adapter.

## Phase 2 Korean teacher key - 2026-09-24

When promoting a legacy text identity rule into Domain, preserve its exact
code-unit ranges and leave trimming, normalization, empty-value policy, and
localized errors at their existing call boundaries. Test both the shared
value and each production caller so adapters do not silently change identity
matching.

## Phase 2 weekly course meeting-day rule - 2026-09-24

Keep allowed weekday patterns in `Domain::Course` as typed `Domain::Weekday`
values, while Schedule Import retains workbook parsing, course-name
normalization, and localized diagnostics at the feature edge. Preserve the
legacy separation between Course validity and pattern policy: unsupported
`M3 Zeus` has no pattern error, but Course validation still rejects the pair.
Use one Course rule for both parse partitioning and apply validation. A
fixture-derived prohibited pattern plus seeded persisted-state snapshots
proves validation runs before writes; the existing Skip case with prohibited
E5/Zeus Tuesday behavior also needs to remain covered.

## Phase 2 persisted schedule entry - 2026-09-25

Represent a persisted class meeting as a Qt-free value with typed `ClassId` and
validated `ScheduleTime`, and construct it only after the repository resolves
the real database ID. Keep the existing UI `ScheduleEntry` projection separate.
At the persistence adapter, retain the original SQL day/time strings and verify
they match the typed value so migration does not normalize stored text or alter
write order. For rejection parity, seed existing rows and compare snapshots;
empty-table row counts do not prove prior state was preserved.


## Phase 2 Calendar Import use case - 2026-09-25

Keep each opaque import signature paired with its save request through
duplicate planning so accepted indexes cannot select a different request.
Preserve exact UTF-16 identity, including unusual code units, and keep workbook,
network, campus, signal, and localized error handling at the feature edge. A
Qt-free use case can compose existing query, planner, and batch-save contracts;
an observer at the adapter boundary preserves existing profiler timing. Verify
the contract app-less and run a required checked-in fixture through the actual
production service path.


## Phase 2 Calendar Import signature identity - 2026-09-25

Use one Qt-free Application value for the duplicate key shared by workbook
candidate deduplication and the database signature query. Keep Qt title/type/
status normalization and ISO date formatting at both adapter edges, then pass
the normalized UTF-16 fields into the common six-field formatter. Preserve the
legacy order, separators, exact code units, all-day bit, and exclusion of
times/row metadata; check placeholder-like title text against Qt's existing
multi-argument formatter semantics.

## Phase 2 typed Calendar Import signature flow - 2026-09-25

After introducing a shared identity value, carry that type through parser,
query, planner, and use-case boundaries instead of converting it back to raw
UTF-16 strings between layers. Hash the encapsulated exact code units for
membership while preserving the parser's emitted order. When adapting legacy
assertions to a new value type, keep `QCOMPARE` expected/actual diagnostics.

## Phase 2 shared Calendar event timing - 2026-09-25

Share Gregorian date, clock-format, and event-ordering rules in a Qt-free
Domain contract while keeping feature-specific errors and validation order in
Application. Fixed-format validation must require separators at their exact
positions; accepting a separator only when present can let digits pass in its
place. Add malformed-separator regression cases. Preserve the existing rule
that cross-day events may have an earlier or equal end clock time. Keep Sub
Prep's queried interval to the current and following calendar years at most.

## Phase 2 Roster Score Import parity - 2026-09-25

- This legacy workflow imports already-saved speaking evaluations into the
  roster; it does not parse a workbook. Verify parity through the real widget
  slot, autosave, and a fresh service read of persisted roster values.
- Include aggregate-score and partial-name-pair cases. A blank English or
  Korean component must not match a roster student, while a mixed six-score
  evaluation must produce its expected grade through the import path.

## Phase 2 Calendar event vocabulary - 2026-09-25

- Keep vocabulary membership in Qt-free Domain while each Application request
  retains raw strings, boundary trimming, its own length limit, validation
  order, and error wording. Shared classification should not make projections
  stricter or combine operation-specific diagnostics.

## Phase 2 Course grade-band classification - 2026-09-25

- Expose grade-only classification without requiring a valid grade/level
  catalog pair. Keep Qt trimming and uppercasing at each feature boundary and
  preserve the distinct Classes, evaluation-default, and Schedule policies.
- Test consumer policy through the shared helper when full service setup would
  add unrelated infrastructure; verify the production service calls that same
  helper and record the narrower integration coverage.

## Phase 2 Speaking Evaluation aggregate grade - 2026-09-26

- Put the six-score average and rounding in one Qt-free Domain rule, but keep
  parsing policy at each caller: the repository trims saved labels and report
  paths require exact labels. Exercise all valid score combinations, the
  rounding transition, and invalid/missing values; verify import and report
  consumers agree on the same mixed input.

## Phase 2 Evaluation Default Selection - 2026-09-26

- Keep the current/previous term cycle in a Qt-free Application contract and
  leave calendar dates, saved-service access, and exact legacy labels at the
  feature edge. A fixed-date integration fixture with persisted schedules
  verifies the production `forClass` path while app-less cases exhaust the
  cycle, All, and invalid period behavior. Record focused executor and
  independent verification separately when their target results differ.

## Phase 2 Schedule Import matching identities - 2026-09-26

- Use typed TeacherId/ClassId values and an optional suggestion in the
  app-less matching contract; convert to legacy integers at the repository
  edge. Preserve small legacy rules independently: nonpositive class IDs are
  not match suggestions but remain in the initially-absent inventory, while
  teacher IDs are not positivity-filtered. Test each rule instead of deriving
  them from the new type representation.

## Phase 2 Schedule Import state validation identities - 2026-09-26

- Carry typed teacher/class IDs through apply-state snapshots, resolutions,
  links, and projections. Translate action-specific absence and sentinel rules
  only at the repository adapter, and preserve numeric ordering for integer
  database IDs when using typed IDs as map keys. Test mismatched teacher keys
  in skip decisions and conflict ordering across IDs such as 2 and 10. Keep an
  explicit residual note when the full action/sentinel matrix lacks direct
  assertions.

## Phase 2 Schedule Import review-decision identities - 2026-09-26

- Represent an optional selected class target as `std::optional<ClassId>` in
  the app-less contract. Convert positive legacy IDs at each feature boundary
  and map nonpositive sentinel values to absence. Keep UpdateExisting required,
  CreateNew target-free, and Skip target-optional; directly test that absent
  Skip targets remain valid. Preserve issue order/details and convert typed IDs
  only for the UI's legacy class-label lookup.

## Phase 2 Evaluation Default Selection read failure - 2026-09-26

- Prove failure-path behavior through the real `ApplicationServices` adapter:
  verify the repository returns an error after the read table is removed, then
  assert the production default selector returns empty. In the same fixture,
  first assert a successful empty current-period read still selects the
  previous term. Use a fixed date, valid saved schedule, policy, and class data
  so the database-read result is the only changed condition.

## F60 audit correction - 2026-09-26

- `ScheduleImportDialogTests::reviewWarnsForDuplicateClassTargets` directly
  asserts that the conflict warning contains the resolved existing class label
  `E5 Athena`. Do not carry this as an uncovered F60 gap; inspect the named
  production test before retaining a coverage residual in a later audit.

## Phase 2 Schedule Import sentinel matrix - 2026-09-26

- Test legacy sentinels at the narrowest reachable boundary, and separate
  upstream validation from adapter behavior. When an earlier contract
  normalizes an ID before the adapter sees it, end-to-end success cannot prove
  that adapter conversion; state that observability limit rather than claiming
  direct coverage.

## Phase 2 Schedule Import optional preview adapter - 2026-09-26

- When a typed projection uses an optional suggestion but the legacy preview
  requires an integer sentinel, assert the no-match case through the production
  repository adapter. Pair the adapter assertion with the app-less optional
  contract check so both representations stay aligned.

## Phase 2 student name-pair identity - 2026-09-26

- Represent a composite identity as separate typed fields instead of joining
  with a delimiter. Keep trimming at the Qt feature edge, preserve legacy
  duplicate overwrite order, and test the real import path for boundary trim
  and persisted results. When current validation forbids duplicate pairs,
  exercise legacy stored duplicates by filling an in-range persisted row.

## Phase 2 legacy profile migration parity - 2026-09-26

- Test legacy profile migration through the production FileController and
  workspace coordinator, not only the schema manager. Assert both the
  user-visible unassigned-teacher sentinel and the stored NULL repair. Backup
  filenames label the migration being applied: `.pre-schema-v4-backup`
  contains the prior schema version 3.

## Phase 2 teacher display-name precedence - 2026-09-26

- Share legacy name precedence as a Qt-free Domain rule, while keeping
  `QString::trimmed()` at each platform input edge. Preserve adapter-specific
  empty-name outputs in their adapters (`N/A` for schedule summaries and
  empty for class details); the Domain value should represent the selected
  name only.

## Phase 2 Schedule Import class target consistency - 2026-09-26

- Keep the app-less review-decision and apply-state validation contracts
  aligned on action/target combinations. Test the state validator directly,
  including valid targetless CreateNew. Record upstream validation limits
  separately: a production path rejected before the repository cannot prove
  that repository conversion branch.

## Phase 2 calendar campus visibility - 2026-09-26

- When moving a Qt regex rule into an app-less policy, preserve Qt trimming,
  code normalization, and Unicode case-fold behavior at the adapter edge.
  Case-folded ASCII input needs lowercase letters in its token-boundary class.
  Test letters adjacent to campus tokens and Unicode neighbors such as Kelvin
  sign and dotless i; ordinary visible-by-default results can otherwise hide
  a broken match assertion.

## Phase 2 Schedule Import shared state projection - 2026-09-26

- Use `std::variant<ClassId, CandidateIndex>` for persisted and not-yet-created
  classes instead of encoding candidates as fake negative database IDs. Keep
  new-ID lookup at the repository edge after insert. When validation and
  persistence must agree, compute one schedule projection and pass that same
  result through both paths; mark untouched intensive rows `KeepExistingRows`
  so validation includes them without rewriting their row identities.
- In Qt tests, do not place a braced container initializer directly in a
  `QCOMPARE` argument: its commas are parsed as macro separators. Assign it to
  a local first, then compare the local values.

## Phase 2 Class Transfer matching policy - 2026-09-26

- Keep `QString::simplified().toCaseFolded()` at the Qt adapter edge when
  moving matching into a Qt-free policy. Verify assumptions about Qt Unicode
  folding against the supported Qt version; the F70 regression covers ASCII
  case and whitespace and does not claim exhaustive Unicode equivalence.

## Phase 2 Class Transfer typed review targets - 2026-09-26

- Keep category-typed targets and optional absence in the app-less contract.
  Translate only the exact legacy `-1` sentinel to absence at adapters; reject
  other nonpositive IDs there and preserve the existing action-specific error
  message and validation precedence.
- When both the UI and repository build the same typed request, test both
  adapter boundaries. Keep fixture-backed successful replacement assertions
  alongside the existing create and conflict/no-write paths.

## Phase 2 fixture-backed teacher replacement - 2026-09-26

- For a fixture-backed profile replacement, vary every non-identity teacher
  field before applying the fixture and compare all fields afterward. Keep the
  identity names stable so preview matching still selects the same destination.

## Phase 2 Class Transfer skip behavior - 2026-09-26

- A skipped imported class must not trigger replacement of its matched teacher.
  Preserve a fixture-backed collision rejection case, then verify Skip plus
  teacher ReplaceExisting succeeds with the class, teacher, schedule, roster,
  and evaluation state unchanged.

## Phase 2 synthetic Intensive workbook coverage - 2026-09-26

- Keep authored worksheet cells and expected parsed/persisted rows as separate
  literals. A synthetic workbook verifies parser-to-apply behavior, but cannot
  establish historical baseline parity without an independent legacy workbook
  or output oracle.

## Phase 2 typed duplicate name-pair grouping - 2026-09-26

- Put row grouping in the existing Qt-free `StudentNamePair` header to avoid
  adding an unassigned standalone source while the handwritten source-owner
  manifest is protected. Keep trim and incomplete-row decisions at adapters,
  preserve diagnostic formatting, and return groups and row indexes in
  first-seen order.
- Do not repurpose duplicate name pairs as durable student identity or change
  independent score-import lookup semantics such as last-write-wins.

## Phase 2 Class Transfer weekly overlap policy - 2026-09-26

- Existing `Domain::ScheduleTime` deliberately models same-day end-after-start intervals; it cannot stand in for Class Transfer's weekly intervals, where end-at-or-before-start means an overnight meeting and Sunday may overlap early Monday. Keep these semantics explicit in a separate typed policy.
- Keep ordered policy decisions and typed conflict references in the Qt-free layer; leave parsing of legacy text, class labels, localized diagnostics, and rendered-message deduplication in the repository adapter.
- Verify both the pure overlap ordering and the repository conversion path. The checked conflict fixture now locks the combined category diagnostic and no-write behavior, while adapter cases cover Sunday wrap and equal endpoints; this is regression evidence and must not be described as historical parity without a separate legacy oracle.

## Phase 2 Schedule Import duplicate-target fixture rejection - 2026-09-26

- Derive multiple review candidates from the checked workbook and direct them to the same seeded destination to exercise the real apply validation path. Assert the exact rejection and compare a complete persisted-state snapshot to prove no writes occurred.
- Keep checked-fixture regression separate from historical baseline parity claims; a permanent workbook alone does not establish an independently sourced legacy-output oracle.

## Phase 2 Class Transfer validated weekly interval values - 2026-09-26

- Keep interval construction behind a Qt-free factory that validates category, weekday, and minute-of-day bounds. The factory owns equal-endpoint rollover and Sunday week overflow; the repository remains responsible for parsing and legacy diagnostics.
- A checked-fixture regression test is useful adapter evidence, but does not establish historical baseline parity without an independent legacy-output oracle.

## Phase 2 Korean Teacher Import sparse updates - 2026-09-26

- Keep sparse profile merging in a Qt-free policy with the matched typed identity, while leaving Qt's Unicode-aware `QString::trimmed()` and database row binding at the repository edge. Limit the policy to fields the import owns; assert manually maintained profile fields survive.
- Verify both changed and unchanged imports. A database update trigger proves the all-blank no-op skips the UPDATE statement, and a checked workbook path verifies identity, persisted merge values, duplicate avoidance, and source date.
- Treat the checked workbook as regression evidence only unless an independently sourced legacy-output oracle is available.

## Phase 2 common-input differential regression - 2026-09-26

- A later checked-in workbook can still support a useful differential check when both the legacy revision and current code run the same bytes against an identical seed. Record the baseline and fixture provenance, compare semantic preview and persisted-state values, and avoid generated IDs or ordering unless they are stable.
- Label this as common-input differential evidence when the fixture postdates the baseline. It strengthens one path but does not establish broad parity or prove the workbook represents historical production data.

## Phase 2 Schedule Import overlap differential - 2026-09-26

- For legacy/current overlap comparisons, pin the fixture's parsed teacher identity and room values, the ordered conflict diagnostic, and the full persisted snapshot. Include regular and intensive schedules plus settings so rejection-before-write evidence covers rollback scope, not just newly imported rows.
- Preserve common-input wording when the workbook postdates the legacy revision, even when parser, preview, rejection, and database outputs match exactly.

## Phase 2 Teacher Import plan validation boundary - 2026-09-26

- Move deterministic import-plan rules into a Qt-free policy using adapter-normalized identity keys and explicit date validity. Keep Qt normalization, `QDate` interpretation, translation, SQL, and the established validation order at the repository boundary so the policy can be app-less without changing user-facing diagnostics.
- Preserve typed matching identities while keeping display names separate; a valid empty Korean key remains a distinct supported value and must not be confused with an uninitialized key.
- Keep generic review-decision resolution at the adapter boundary, then pass the resolved ordered Korean keys into the plan policy. The policy can validate selection-to-plan correspondence before date and roster checks without depending on review UI or Qt types.

## Phase 2 generated Schedule Import baseline comparison - 2026-09-27

- A source-generated workbook helper that exists in the legacy baseline can support a synthetic baseline comparison when both revisions consume the same pinned bytes and deterministic seed. Pin semantic parse, preview, apply, and full persisted-state outputs; label the result synthetic rather than historical production-workbook parity.
- Normal Schedule Import apply replaces the regular schedule snapshot: unrelated teacher/class metadata and settings remain while prior regular time rows are cleared. Tests should make that full-snapshot behavior explicit in expected state.

## Phase 2 GS Team sparse update policy - 2026-09-27

- Keep Qt normalization at the repository boundary and pass normalized fields into the Qt-free merge policy. The policy preserves the matched row ID, blank fields, and no-op/change result; the adapter should skip SQL for unchanged rows. A database trigger is a direct way to prove the no-op issued no UPDATE.

## Phase 2 intensive Schedule Import baseline comparison - 2026-09-27

- Baseline-era source helpers and inline worksheet data can establish synthetic differential evidence when both revisions use the same generated workbook bytes and deterministic seed. Pin parsed, preview, applied, and full persisted state. Keep provenance explicit: this does not show that the generated workbook represents historical production data.

## Phase 2 Teacher Import match cardinality - 2026-09-27

- A Qt-free count classifier can centralize the zero/unique/multiple decision while leaving identity semantics at the repository. Integrate it in every import namespace and pin exact per-namespace ambiguity diagnostics and rollback; include counts above two in the pure-policy test to establish the 2+ boundary.

## Phase 2 Teacher Import transaction boundary - 2026-09-27

- For a multi-namespace import use case, put reads, writes, latest-source metadata, commit, and rollback behind one transaction-bound persistence port. Test a late metadata-write failure after successful row writes to prove atomic rollback across every affected table and setting. Keep SQL and localized adapter diagnostics outside the Qt-free Application contract.

## Phase 2 Teacher Import common-input evidence - 2026-09-27

- A baseline-present source-generated test plan can establish repository-level behavior, but it does not cover workbook parsing or historical production output. Extend the same input upstream through generated workbook bytes, validation, explicit review choices, and apply when those paths exist in both revisions. If the current full test source no longer compiles at the legacy baseline, use a narrow harness that preserves the entire legacy repository and schema implementation.
- Keep `CalendarPage::filterUpcomingEvents` in the page boundary while its UI filters and next-ten search are composed there. A smaller duplicated start-of-term classification rule is a later app-less contract candidate; the next slice instead prioritizes Teacher Import parse-to-apply evidence for Gate 2.

## Phase 2 Teacher Import generated-workbook parity - 2026-09-27

- Pin generated workbook bytes and compare semantic output on baseline/current when extending parity from repository plans through validation, explicit review, and apply. A successful parse-to-apply case strengthens one path only; rejection parity and historical production-workbook evidence remain separate gaps.
- After a parity-only slice, select bounded app-less behavior to keep Gate 1 progress moving. The Calendar start-of-term predicate is a compact candidate when its normalization, event-type fallback, option semantics, and recognized phrases can be preserved and tested independently of Qt.

## Phase 2 Qt-free Calendar text policy - 2026-09-27

- When moving Qt text classification into a Qt-free policy, match the source framework's full whitespace behavior at the byte-decoding boundary. Qt 6.12 treats U+0085/NEL as whitespace; omitting it changed both title simplification and event-type trimming. A direct framework-versus-policy probe plus app-less regressions caught the mismatch before commit. For ASCII aliases, verify whether non-ASCII lowercase mappings can affect the exact target letters rather than carrying an unbounded Unicode-table dependency.

## Phase 2 negative validation differential - 2026-09-27

- For a baseline-present invalid input, compile each revision's own validator path against one pinned byte sequence and compare the full semantic result. A direct rerun of both saved executables can independently confirm the captured output. Keep validation rejection evidence distinct from repository no-write or rollback claims when the path exits before persistence.

## Phase 2 Calendar repeat planning - 2026-09-27

- Preserve monthly recurrence by advancing from the previous occurrence and clamping to that month's last day (Jan 31 → Feb 28/29 → Mar 28/29). Treat the until date as an inclusive occurrence-start cutoff, preserve the seed's fixed day duration and fields, clear per-occurrence IDs, and reject plans outside supported date or count bounds in the Qt-free Application policy.

## Phase 2 synthetic rollback differential - 2026-09-27

- For cross-revision database rollback evidence, execute the same deterministic plan and failure injection against each revision's own repository and schema sources. Normalize the failure stage and affected persisted state; omit backend-specific error text. State exactly which failure path was exercised, since one synthetic rollback case does not establish workbook parity or behavior for other failure stages.

## Phase 2 Calendar suffix-edit policy - 2026-09-27

- Move repeat-series edit date shifting and field propagation into a Qt-free Application planner fed by an ordered value snapshot. Keep the platform adapter responsible for query order and Qt/domain conversion. Preserve Technical failure behavior for invalid or unrepresentable dates so the adapter does not attempt persistence.
## Phase 2 malformed UTF-8 validation parity - 2026-09-27

- Inject malformed UTF-8 into the XML member before rebuilding a source-generated ZIP so the fixture remains structurally valid and CRC-correct. Run that exact emitted byte sequence through revision-specific parsers; ZIP rewriters can change archive metadata even when member contents match.
- Observe the validator result before pinning assertions. This invalid shared-string marker returns `UnsupportedTemplate` with no preview metadata; it is not an unreadable-workbook result and does not establish persistence rejection or historical workbook parity.

## Phase 2 Calendar repeated-series creation use case - 2026-09-27

- When a UI directly combines an existing pure planner with a persistence port, move that orchestration into Application so success, planner rejection, and port failure can be tested through a fake port. Keep Qt conversions, user feedback, and cache invalidation at the UI boundary.

## Phase 2 Calendar single-event save boundary - 2026-09-27

- Put request validation and save-port result propagation in an app-less use case, then keep Qt mapping and feedback in the UI/platform edges. A one-occurrence repeat save deliberately clears that event's series ID; the suffix-edit path preserves the series and stays separate. Compare seeded create/update rows as persistence parity, not workbook provenance.


## Phase 2 Calendar suffix-delete validation boundary - 2026-09-27

- Keep repeat-series suffix-delete validation in the Application request/use case and reuse the domain canonical-date policy. Preserve adapter validation before service lookup, the current diagnostic, the original series-ID bytes, and the existing Qt date conversion. Leave single-event deletion on its own typed port until a concrete shared scope contract is needed.

## Phase 2 Calendar single-event deletion boundary - 2026-09-27

- Keep ordinary event deletion and repeat-series suffix deletion as separate Application contracts. Preserve typed IDs across the use case boundary; keep legacy integer parsing and service/error/exception conversion in Platform. Seeded repository parity establishes repository behavior only, not UI or historical-workbook parity.
- For differential SQLite evidence, run the same fixture through each revision's own QtTest repository target when a standalone harness fails before database setup. Assert seeded sibling and unrelated snapshots are nonempty before comparing full rows, row count, and sequence state.

## Phase 2 Calendar delete-all boundary - 2026-09-27

- Keep availability separate from the confirmed destructive operation. The Application use case forwards the availability check and delete command independently; the UI owns confirmation/cancel and feedback. Seeded cross-revision parity should report only repository behavior, including row count and sequence state.

## Phase 2 Calendar repeat-series edit boundary - 2026-09-27

- Validate typed repeat-series edit requests in Application before invoking the port. Keep Platform's validation guard for direct callers, the existing planner and service orchestration in their current layers, and UI warnings/cache invalidation at the UI edge.

## Phase 2 Calendar repeat-series repository parity - 2026-09-28

- For suffix-update parity, seed and pin every row before and after, including unrelated events on the selected date; assert ID order, row count, and `sqlite_sequence`. Exercise the same batch-save fixture against each revision's own repository source closure. When the baseline lacks an unrelated newer API, omit only its incompatible test from the temporary harness and keep the parity fixture byte-for-byte the same.

## Phase 2 Calendar event-by-ID query - 2026-09-28

- Keep the app-less lookup request bounded and typed, with exact identifier forwarding and structured result propagation. Retain legacy numeric conversion and service/error handling in Platform; verify UI failure behavior separately from the Qt-free Application and Platform tests. The Sub Prep date window remains January 1 of the reference year through December 31 of the next year at most.

## F108 - compare repository state, including retained sequence state

A delete-path parity fixture should compare the complete ordered surviving rows and the database sequence alongside removed rows and the final count. Deleting selected events can leave `sqlite_sequence` unchanged; pin that behavior rather than assuming it tracks row count. Build both sides from each revision's own repository source closure and identical seed/request, then limit the claim to that repository transition.

## F109 - make baseline inputs independent of new planners

For repository parity of generated records, pass identical explicit facts to each revision's own repository implementation. Do not let a current-only Application planner manufacture the baseline input. Compare returned IDs in order, every persisted column, seeded unrelated rows, row count, and `sqlite_sequence`; label the result as repository-level parity.

## F110 - keep per-event visibility composable and order-preserving

A Qt-free predicate can compose independent display policies while Qt adapters retain preference loading and Unicode normalization. Keep list filtering at the caller so it can preserve cache order and avoid another materialized projection; verify both existing consumers still apply their separate prefilters and append visible rows in order.

## F111 - distinguish session-bound from legacy-only service construction

To prove a session-bound operation cannot fall back through a compatibility facade, bind it to a closed session while a separate DataService is open, then assert both the result and persisted state. Keep the sessionless compatibility construction tested separately. Report service availability state independently from content-read isolation when they use different ownership rules.

## F112 - isolate the complete CalendarService read surface

Exercise every CalendarService content-read method with a closed bound session and a separately open DataService, then repeat through the legacy-only constructor. Keep writes and deletes out of the read-isolation slice. If MSVC reports an invalid generated-file path in a fresh build, rerun from a short temporary path and report the retry evidence without asserting an unproven root cause.

## F113 - isolate output reads while preserving legacy construction

For operation-scoped output adapters, prove a closed bound session plus a separate open DataService cannot supply fallback content, even when the service still reports available. Repeat through sessionless legacy construction and retain normal adapter regressions, especially established failure handling such as zero student count. Isolate fresh verification builds from repository build caches and clean only generated scratch paths after checking containment.

## F114 - close live Calendar mutation fallbacks

When several rejected operations share a seeded legacy store, check the store immediately after each operation as well as at the end of the sequence; the final state alone can hide which call mutated it. Make a bound session authoritative for availability and each live Next Calendar mutation, while separately preserving DataService-only behavior. Re-run the same independent verifier after strengthening assertion granularity.

## F115 - test the service contract beyond its adapter guard

If an adapter checks availability before calling a service, that does not prove the service method itself cannot fall back when called directly. Test the service with a closed bound session plus an open legacy store, verify every persisted field stays unchanged, and separately preserve the sessionless legacy save path.

## F116 - remove the redundant active status query - 2026-09-28

When a v2 adapter's bound services already provide session-authoritative availability, remove its separate `ApplicationServices::hasOpenDatabase()` check so that adapter does not query legacy open state. Verify the closed-session behavior with the lifecycle contrast and audit the exact production call path; a service's retained `DataService*` and the Workspace adapter remain separate isolation work.

## F117 - keep profile editing app-less without duplicating validation - 2026-09-28

Put validate/update/reload ordering and normalized-value flow in the app-less use case, while leaving prompts, dirty-state handling, and page feedback in the UI. Keep the semantic validator injectable and require a production adapter to map the existing `TeacherValidator` result, including field, severity, and bounded arguments; do not create a second normalization rule set. Keep page integration separate until its session-bound persistence path is ready.

## F118 - label common-input conflict evidence precisely - 2026-09-28

When a checked-in fixture postdates the baseline, run its identical bytes against each revision's own production source closure and assert the same normalized review plan, exact diagnostic, full persisted-state snapshot (including `sqlite_sequence`), and zero writes. Record this as common-input regression evidence, not historical input parity.

## F119 - hash the committed snapshot for handoffs - 2026-09-28

Compute handoff SHA-256 values from a fresh archive of the exact commit and compare the archived files to the commit's Git blobs. The initial F119 handoff listed five values that did not match either the Windows working-tree bytes or the fresh archive; do not attribute such a mismatch to line endings without evidence. The corrected archive hashes and independent build/test results now establish the F119 snapshot.

## F120 - remove the operation edge without callback coupling - 2026-09-28

Keep the existing `ApplicationServices` workspace API and route its implementations to the canonical session plus a shared file-operation helper. Make the compatibility facade resolve current repositories from its session so direct session replacement cannot leave cached pointers dangling. A refresh callback from the new workspace path would retain the dependency being removed; prefer live access and verify both the call graph and facade lifecycle.

## F122 - compare Schedule Import Skip final state without erasing row identity - 2026-09-28

The legacy Skip path can recreate schedule rows, advancing `class_times.id` and `sqlite_sequence` even when meeting values are preserved. Assert consumer-visible meeting order before and after, then compare exact baseline/current final snapshots including IDs and sequences. Do not infer a physical no-op from the user-facing Skip choice; label hand-authored seeded comparisons separately from historical workbook provenance.

## F123 - keep live validation feedback while routing saves through the use case - 2026-09-29

Keep `TeacherInfoPage`'s live `TeacherValidator` feedback, then use the app-less
edit contract as the authoritative save gate. Map the validator result into
bounded application issues and restore it in the form binder so warnings still
allow saves and remain visible after canonical reload. Test through a real
temporary session so a legacy `DataService` fallback cannot mask the
integration.

## F124 - normalize the unassigned teacher sentinel at the application edge - 2026-09-29

Translate the UI's `-1` co-teacher sentinel to an absent typed teacher ID in
the app-less use case. The platform adapter should reload the session-backed
`ClassInfo` and change only its teacher assignment before saving; defaulting
after a failed read can overwrite class details, notes, or schedules. Test the
full persisted class-owned fields and both assignment and unassignment.

## F125 - validate bounded notes before calling the save port - 2026-09-29

Put the existing UTF-16 length check in the app-less save orchestration before
port invocation, while keeping the platform adapter defensive for direct
callers. Exercise the exact limit and oversized no-write behavior through the
use case, then verify the page preserves its manual warning and dirty state.

## F126 - normalize schedule text before typed conversion - 2026-09-29

The class schedule UI and validator accept normalized `h:mm AP` values, while
the strict schedule parser accepts only `HH:mm`. Convert only after the
existing UI validation and normalization, and cover AM/PM, noon, and midnight
at the page boundary. Keep the app contract Qt-free and pass typed minute
values to the session-backed adapter.

## F127 - preserve raw schedule rows in a screen query - 2026-09-29

Use ordered Qt-free raw text rows for editor reads; `ScheduleTime` cannot
represent malformed stored rows and would make the read projection lossy.
Keep class, teacher, and roster read outcomes independent so one failed source
does not discard successful display data. After save, refresh title data from
the query but overlay the just-saved class fields so the title stays current
without reloading the form or marking it dirty.

## F128 - persist the selected schedule slot state without moving its transition - 2026-09-29

The slot-state table is globally keyed by weekday and start time, and removes
an override when the selected state equals the slot default. Keep the view
model's transition choice in the UI; send a typed weekday, minute, selected
state, and default state through the use case so the session adapter can
preserve default deletion and write behavior for all shared toggle callers.

## F129 - keep roster table saves lossless and test the enclosing gate - 2026-09-29

Carry a full ordered roster snapshot through the Qt-free save request, including
every row position, UTF-16 values, and column widths; pass the existing
questionable-name confirmation as an explicit flag. Verify UI boundaries too:
invalid-cell focus, silent autosave failure with dirty state retained, and
class-selection rollback when the session-backed save fails.

## F130 - preserve changed-cell semantics through the platform boundary - 2026-09-29

An empty evaluation change list means write all cells, while a non-empty list
limits persistence to those coordinates. Test both cases at the platform edge:
put a distinct unlisted value in the in-memory matrix, save one listed cell,
then assert the unlisted edit did not overwrite the previously stored value.

## F131 - preserve exact evaluation read keys and separate UI fallback - 2026-09-29

Read evaluation names exactly as stored; do not trim a query key when the
repository lookup is exact. Keep an empty successful result distinct from a
structured read failure in the application boundary, while preserving the
page's existing blank, clean grid for either outcome.

## F132 - keep raw roster reads separate from widget normalization - 2026-09-29

Carry stored column order, widths, every raw row, and UTF-16 values through the
Qt-free read snapshot without applying the widget's 25-row presentation limit.
Let the existing roster model own required-column, name, width, and visible-row
normalization; test the raw adapter limit and UI presentation separately.

## F133 - batch compact ClassesPage navigation inputs - 2026-09-29

Keep requested classes in order when class metadata or its teacher is missing.
A fixed set of batched repository reads preserves raw regular and intensive
schedule strings while removing per-class lookups; assert statement counts
across multiple classes so a moved N+1 loop cannot pass.

## F134 - keep schedule input reads compact and lossless

Route the existing ordered repository batch through a Qt-free application
snapshot and session-backed adapter; do not duplicate SQL or call it a query
optimization. Preserve raw meeting text because the builder's day and time
parsing has asymmetric malformed-value behavior. Keep widget preview, slot
state, and testing-assignment sources separate from this read.

For preview verification, distinguish a model already installed in
`ScheduleWidget` from `ScheduleImportReviewDialog`'s pre-model renders: two
`buildUi()` setters and the `prepare()` preference refresh can read live
schedule data before preview installation. That call order predates F134, so
describe the actual call order instead of claiming the entire dialog setup
bypasses reads.

## F135 - preserve legacy slot-state values at the Application read boundary - 2026-09-29

Read intensive slot-state overrides through the active `DatabaseSession`
repository, not `ScheduleService`'s compatibility fallback. Carry ordered raw
day/start/state text into the widget so malformed or unknown stored strings
retain their existing map behavior; a successful empty snapshot must still
clear overrides, while unavailable or failed reads leave the current map
untouched. Pin the existing warning prefix in the widget regression test, and
keep the typed save command separate from the lossless read projection.

## F136 - keep schedule assignment reads bounded and display-only - 2026-09-29

Continue the ScheduleWidget read boundary with the testing-assignment display
query. The Qt-free snapshot returns only fields needed by the current view;
one ordered joined repository read supplies special-class details without
per-assignment reads. Preserve raw day/start keys. Keep unresolved special
class rows in the snapshot so the widget can retain its warning and skip
behavior at the UI boundary. Verify fixed query count with a small and larger
assignment set. Keep writes and broader view-model or import-review lifetime
work in separate slices.

## 2026-09-29 — Mid-slice handoff

Reusable lesson: when a substantive slice is paused before acceptance, preserve the working tree and make the handoff state explicit: what is implemented, what verification remains, the commit boundary, and the next authorized step. The current F144 continuation details are kept in latest_session_work.md.

## 2026-09-29 — F144 acceptance

Keep the full acceptance set explicit when a partial checkpoint resumes:
verify the new Application and Platform boundaries, the owning page behavior,
and the prior page/query regressions. A build error on one tree is not product
evidence; a fresh short-path build completed the two blocked Platform
regressions without establishing the cause of the earlier C1083.

## 2026-09-30 — aqt checksum sidecar failures

`ChecksumDownloadFailure` indicates aqt could not retrieve the archive checksum sidecar; it is distinct from downloading an archive whose hash fails validation. Do not infer a metadata parser bug from the archive filename alone. When the exact sidecar status is unknown, bounded retries with delay and a longer request timeout can address transient availability while preserving fail-closed checksum verification. Do not add trusted mirrors or disable hash checks without verifying the exact sidecar and its provenance. Keep equivalent macOS release and baseline install paths consistent.

## 2026-09-30 — F145 Testing Class details update

Keep existing-class detail updates separate from create-time schedule
assignment and delete cascades. Preserve the page's roster-first ordering and
test the partial-save state explicitly: roster data can commit before a later
detail update fails. Verify the Application contract, active-session adapter,
page behavior, and prior read-path regressions together; a focused 10/10 pass
does not imply the full Phase 2 gate is closed.

## 2026-09-30 — F146 Testing Class creation

Keep the optional pending weekday/start-time assignment in the same create
operation as class, details, and room persistence; splitting it would lose the
repository transaction's rollback behavior. On a slot conflict, assert all
four affected tables remain unchanged, not only the class row. Build fixture
fixes in a test-only change should be followed by the complete requested
focused run before acceptance.

## 2026-09-30 — F147 Testing Class deletion

When a successful delete changes selection, discard the deleted record's dirty
editor and roster before rebuilding the list. Otherwise the page's ordinary
selection autosave can create the deleted draft as a new class. Keep the
failure path's draft intact. Test the full cascade, sibling preservation, and
rollback after a late SQL failure; the destructive prompt should disclose all
record categories the repository removes.

## 2026-09-30 — F148 Class Details save parity

For common-input save parity, drive the same live page and persisted database
case on the current tree and pinned baseline. A fake save port proves request
mapping, not persistence parity; assert the saved fields, untouched values,
ordered schedules, and visible success state on both revisions. Keep one
successful save separate from validation and conflict parity.

## 2026-09-30 — F149 Class Details conflict query

Keep conflict data raw at the typed Application boundary, and leave warning
wording in the page. The Platform adapter can preserve existing order while
reading directly from the active repository; the save-time service guard still
matters. When the pinned baseline has a different dialog test API, adapt only
the temporary warning observer and keep the same page input and assertions.
On Windows/MSVC, C1083 can be caused by a test object path beyond CMake's
length limit; the F149 short-path retry reduced the path from 265 to 233
characters and passed without a source change.

## 2026-09-30 — F150 typed validation conversion

When a Qt-free policy returns normalized values, the UI adapter must apply
every persisted normalized field when reconstructing the save model, including
hidden fields restored from storage. Seeding whitespace into hidden notes and
activity fields caught an omission that issue-validation tests could not; keep
that database-backed save assertion with the page parity case.

## 2026-09-30 — F151 validation parity selection

Extend validation parity through live regular and intensive malformed
schedule inputs, end-before-start, and duplicate rows. Legacy `QHash` does not
define duplicate-group order, so compare duplicate membership and visible
row-specific behavior semantically. Keep the fresh Application read for
hidden persisted validation fields as a separate next boundary.

## 2026-09-30 — F152 validation-context read selection

Fetch the hidden persisted fields fresh for every save attempt; the page-load
snapshot may be stale. Preserve teacher ID `-1` as unassigned, `0` as invalid,
and exact notes/activity text for the Qt-free policy. Keep the query context
out of the save request so the save adapter continues to reread and preserve
the latest persisted record. On query failure, retain the existing empty
context fallback and downstream validation/conflict/save order.

## 2026-09-30 — F151 validation parity

Use a valid cross-class conflict as a warning trap while testing invalid
schedule input through the real page. Assert visible validation and unchanged
database state on current and baseline; direct query-count behavior belongs in
the current page tests when the shared baseline constructor offers no query
observer. Compare duplicate groups by membership and row feedback, not by the
legacy `QHash` iteration order.

## 2026-09-30 — F152 fresh validation context

Read hidden persisted validation fields immediately before each save; the
load-time display snapshot can be stale. Keep the context separate from the
save request so persistence still rereads current values and the service's
final validation guard remains active. If preserving the old read-error
fallback, test its continued validation/conflict/save order explicitly.

## 2026-09-30 — F153 teacher sentinel parity

Persisted teacher ID `0` changed after page load now has current/baseline page
coverage. The pinned legacy comparison can use an isolated test-only overlay
when the parity harness did not exist at the baseline; verify the production
page still matches its pinned blob. Use a seeded conflict only as a warning
trap, and keep direct query-count claims in current-only tests.

## 2026-09-30 — F154 Class Notes read boundary

Keep the Class Notes read seam separate from its existing save port. A
page-sized projection with independent class and teacher results preserves
loaded text when teacher lookup fails. Leave display formatting and trimming
in the UI, use the active session without DataService fallback, and keep reads
out of refresh/save. Compare visible load/discard behavior on the pinned
baseline; assert query counts only in current tests.

## 2026-09-30 — F155 Co-Teacher selected-class read

Move the selected assignment and title inputs behind a separate read query,
but keep teacher choices and assignment saving on their existing boundaries.
Refresh the selected-class snapshot after a successful assignment and on
discard. Use baseline parity for visible selection/title behavior and keep
read-count assertions current-only. Historical F123 candidate notes must not
be treated as current production state after commit `9f7e736b`.

## 2026-09-30 — F155 parity fixture and F156 choice read

Seed page-parity teachers with valid catalog grade/level pairs so save reaches
its intended post-save assertions; English-only teacher names are valid when
the optional Korean name is empty. For baseline parity, adapt only test calls
when the pinned API differs, verify baseline production blobs, and avoid
query-count claims. Keep the Co-Teacher choice catalogue as a separate read
from its selected-class snapshot because they have different data and refresh
timing.

## 2026-09-30 — F156 Co-Teacher teacher catalogue

Keep a page read projection Qt-free and include only fields consumed by its
view. Read the teacher list from the active repository without a service or
DataService fallback, leaving display sorting in the UI. Cross the Korean and
English fixture sort orders so tests can detect swapping the two selectors.

## 2026-09-30 — F157 Teacher Profile persistence boundary

Keep persistence adapters honest about which failure path a test reaches:
failure-trigger fixtures must pass normal page validation first. Moving the
page save port to the active-session `TeacherRepository` retained the existing
use case and validation policy, while current page tests and pinned-baseline
public-page parity checked save/reload and invalid-write behavior.

## 2026-09-30 — F158 parity and F159 directory reads

Pinned-baseline checks needed temporary Qt metadata and copied-test API
adaptations. Keep those changes in the disposable baseline checkout, restore
the original CMake hash, and verify production blobs remain pinned. When disk
space is limited, reuse the verified baseline cache only after checking the
overlay; make no query-count claim. Keep Native English and GS Team reads
separate because their records and repositories differ. Give each table its
own typed ID; keep both save paths and the later table model/view conversion
out of the read slice.

## 2026-09-30 — F160 GS Team directory read

Use a distinct int-backed GS Team member ID and read from the active session's
repository. Availability can change during leave confirmation, so perform the
typed read after confirmation; classify an unavailable session as silent
`NotFound` and repository failure as a warning. Keep baseline-only Qt and API
adaptations inside the scratch checkout and verify pinned production blobs.

## 2026-09-30 — F161 directory save validation

Keep validation decisions in Qt-free Application while the feature edge
supplies Qt's normalized comparison key and date-validity fact; do not replace
Qt's Unicode/date behavior with an approximate standard-library algorithm.
Choose Unicode collision fixtures from the supported Qt version itself:
Qt 6.12 keeps U+00DF and `ss` distinct, while U+00C4 and U+00E4 compare equal.
For GS Team, keep English and Korean uniqueness sets separate and allow the
same comparison key across those namespaces.

## 2026-09-30 — F162 GS Team directory save

Keep GS Team validation separate from the Native English save policy even
when both pages share a directory UI. Their identities and uniqueness scopes
differ. On a disk-constrained Windows host, a serial focused build in a
separate scratch volume can provide the requested slice evidence; report that
scope precisely and do not imply a full build.

## 2026-09-30 — F163 ClassesPage class-list read selection

Keep the class-list query distinct from `ClassesNavigationSnapshot`: the
snapshot enriches a list supplied by the feature and does not own list
enumeration. A class ID/name projection can migrate open and post-save list
reads while leaving metadata, editors, and other legacy operations intact.

For page parity, prove a save refresh visibly changes class order and keeps
the selected class; a second read or a rename that stays in the same position
does not demonstrate the ordering behavior. Reconstruct pinned source from its
exact commit and compare production blobs before using a scratch baseline; a
directory name or existing build cache is not proof of provenance.

## 2026-09-30 — F164 ClassesPage section visibility read

Keep the grade lookup for Analytics/Evaluations visibility separate from the
subtitle and navigation metadata reads. They have independent failures and
fallbacks. A missing or failed grade currently leaves both sections visible;
preserve that behavior with a narrow typed class-grade read. A failed optional
read should not trigger a legacy service fallback.

A stub-backed visible-tab parity slot is sufficient to compare the tab rule
against the pinned page behavior; a separate real-database harness timed out on
both builds and established no mismatch. Report those checks separately.

## 2026-09-30 — F165 ClassesPage subtitle read acceptance

Keep class details and optional teacher display fields as separate outcomes:
a failed teacher lookup must leave successful class details available to the
formatter. Keep display formatting and localized fallback choices at the UI
edge. The active-session repository projection only needs class grade, level,
regular meeting day/start time, and the teacher fields used by
`preferredDisplayName()`; do not widen the contract to full class or teacher
records. The exact pinned-baseline page subtitle and all 584 production source
blobs matched without a production overlay.

## 2026-09-30 — F166 roster subtitle acceptance

Keep outer-query unavailability separate from inner class-detail failure: the
roster header falls back to classroom name/ID only for the outer failure, while
the formatter still supplies defaults when its class fields fail. A failed
teacher read must retain successful class fields. The active-session query can
be reused by another display consumer without moving `SidebarNodeNaming`
formatting out of the UI. Exact current and pinned-baseline subtitles matched
with no production overlay.

F167 reuses the existing `ClassDetailsPageReadSnapshot` and query while
replacing its remaining service-backed Platform reads. Keep class details,
teacher display name, and roster count as separate outcomes; do not infer a
memory or query-count improvement because the roster repository count method
currently loads the roster before counting.

## 2026-09-30 - F167 class details display read

Use distinct values for English name, romanization, and preferred name in the
adapter fixture so field mapping is observable. Check baseline visible parity
for successful reads and independent teacher failure; record fallback
differences already present in the current UI separately instead of attributing
them to an adapter-only change. A class-info failure currently falls back to
the selected classroom name on the migrated page but to `Unknown Class • No
Teacher` on the pinned baseline. Keep the class, teacher, and roster-count
failure outcomes independent. The current roster count repository reads the
roster before counting, so make no efficiency claim.

## 2026-09-30 - F168 class details save port

Keep the save adapter on the active session's `ClassInfoRepository` and preserve
validation plus fields the editor does not expose, including schedules. The
current focused save, display, and parity targets passed; the common visible
save case also matched the pinned baseline without production overlays. F169
continues the same bounded boundary migration for the Class Notes save port;
preserve trimming and notes validation while requiring an open session.

## 2026-09-30 - F169 Class Notes save adapter

Keep the UTF-16 request limit at the typed boundary, trim both values after Qt
conversion, and run `ClassInfoValidator::validateNotes` before the repository
write. Requiring the open session removes the legacy service's `DataService`
fallback; retain the existing typed error mapping and repository failure text.
The focused fixture checks exact 10,000-unit requests with edge whitespace and
surrogate pairs, plus failed-write preservation of both stored fields.

## 2026-10-01 - F170 roster read adapter

Keep roster snapshot conversion at the Platform edge and use the open
session's `RosterRepository` directly. Preserve all sparse rows, source order,
column widths, and UTF-16 cell contents; the UI applies its own display row
limit and model normalization. The current `RosterEditorWidget` path already
uses the typed read port, so its load, empty, and failed-read slots provide the
integration regression without broadening this slice into roster save.

## 2026-10-01 - F171 roster save adapter

Move the write to the open session's `RosterRepository`, but keep
`RosterValidator` normalization and validation at the Platform adapter because
the typed request is a raw snapshot. Test rejected input by comparing the full
persisted snapshot before and after; exercise repository rollback with an
injected write failure. F172 is the Co-Teacher assignment save port, whose
direct repository path must retain full class validation and regular/intensive
schedule-conflict checks.

## 2026-10-01 - F172 Co-Teacher assignment save port

ClassInfo's teacher details are joined from the selected teacher record, while
the repository persists the teacher ID, class fields, and schedules. Verify
assignment by checking the selected teacher's joined metadata separately from
the unchanged stored class fields; verify unassignment clears joined metadata.
Keep full validation and both schedule-conflict preflights before the
transactional class-info save. F173 pairs the already-migrated slot-state read
with its save port.

## 2026-10-01 - F173 schedule slot-state save port

After moving a widget save port from a feature service to its session
repository, route the widget test double through that same repository. Keep
legacy service doubles only for legacy callers. The repository fake should
apply override updates and default-state deletions to its stored fixture so
reload assertions observe the committed state; cover preservation of unrelated
overrides and failed-write behavior.

## 2026-10-01 - F174 Speaking Evaluation read port

Keep evaluation names exact at the typed query boundary. The current query
allows whitespace-only names, while the repository rejects them as blank; the
Platform adapter therefore preserves a Technical failure for this case. Cover
the mapping explicitly when replacing service delegation with direct reads.

## 2026-10-01 - F175 ScheduleBuilder source port

When a feature service only forwards to a repository, the Platform adapter can
replace the wrapper with that same session repository call without changing
query rules. Keep repository read metrics observable; the Platform tests assert
them as well as the mapped source order and page integration.

## 2026-10-01 - F176 Sub Prep class-details read port

Use `ClassInfoRepository::loadSubPrepClassDetails()` on the active session and
keep the post-read session check: a repository result is invalid if the session
closes while the read is in progress. Preserve the existing teacher fallback
and bounded UTF-8 mapping. F177 continues the same Sub Prep read boundary with
the schedule-summary port.

## 2026-10-01 - F177 Sub Prep schedule-summary read port

Call `ClassInfoRepository::loadSubPrepClassSummaries()` through the active
session and preserve the existing scope checks, no-read empty scope, projection
order and error classification. F178 advances the Sub Prep output boundary to
the operation-scoped print-source port; keep its missing-teacher omission and
zero-count fallback for roster-read failures.

## 2026-10-01 - F178 Sub Prep print-source port

Use the active session's class-info, teacher, and roster repositories directly.
Preserve the selected schedule scope, teacher caching/order, missing-teacher
omission, and count-zero fallback only for roster-count failures. F179 moves the
larger roster-output source; pass the remaining row, cell, and text-byte budgets
to the repository and keep failed reads from returning a partial package.

## 2026-10-01 - F196 Current Campus preference adapter

When a `DataService` object remains available after its database session closes,
test the v2 adapter against the session itself and verify it does not fall back
through the compatibility facade. Keep each preference adapter's own defaults,
coercion, and error behavior while migrating persistence to
`SettingsRepository`; do not broaden a one-key preference slice into the
separate aggregate Personal Details writer.

## 2026-10-01 - F197 visibility preference adapter

When a typed preference save returns `void` and the legacy adapter suppresses
repository failures, test an injected open-session write failure explicitly:
the call remains silent and the previous stored value remains intact. Pair that
with a closed-session case where `DataService` still exists to prove the adapter
checks the active session directly.

F198 is the Personal Details aggregate save boundary. Keep its nine settings in
one `SettingsRepository::saveSettings()` transaction; preserve rollback and
signature conversion as one adapter operation.

## 2026-10-01 - F198 Personal Details aggregate save

Keep the Personal Details nine-key write atomic through the active session's
`SettingsRepository`. A closed-session test should retain a live `DataService`
object and verify the whole seeded bundle after reopening, so an accidental
compatibility-facade fallback cannot hide behind the service's lifetime.

## 2026-10-01 - F199 Personal Display Name read port

Pair the `myInfo/name` read port with the aggregate Personal Details writer
that already owns that key. Verify the typed read port sees the aggregate
writer's value, including UTF-8 and whitespace, while keeping callers unchanged.

## 2026-10-01 - F200 Personal Signature preferences reader

The typed signature-preferences reader consumes three keys owned by the F198
aggregate writer. Keep missing/invalid defaults and mode/font conversion at
the Qt adapter edge; retain the compatibility reader's warning and default
result when repository reads fail.

## 2026-10-01 - F201 Personal Signature Image reader

Keep image lookup, Base64 decoding, and signature-image preparation together
at the adapter boundary. When moving reads to the active session repository,
preserve the warning and empty-result behavior for read errors, plus the
existing empty behavior for missing or corrupt stored images.

## 2026-10-01 - F202 Class Visibility preference

Preserve this preference's special default behavior: missing, invalid, or
failed reads attempt to persist `active_schedule`, while a valid unrecognized
string defaults in memory without being rewritten. The void save port keeps
write failures silent; test a rejected write and the prior value explicitly.
SQLite represented a written invalid `QVariant` as a valid empty string in the
focused path, so tests distinguish missing-key materialization from valid
unsupported values instead of treating the empty string as an invalid variant.

## 2026-10-01 - F203 Evaluation Default Policy preference

When choosing between a one-key typed preference and a multi-format palette
adapter, continue with the smaller typed policy boundary first. Preserve the
distinction between missing/read-error values, which attempt default
materialization, and valid unrecognized values, which default only in memory.

## 2026-10-01 - F204 Class Day Filter Reset policy

Independent scans compared the adjacent one-key class-navigation policy with
the multi-format Custom Color Palette adapter. Continue the small typed
preference sequence; keep the palette's legacy payload formats and broad caller
surface for a later bounded slice.

## 2026-10-01 - F205 Class Selection Reset policy

Complete the neighboring class-navigation reset preferences together while
keeping the typed policy adapters separate. Preserve the difference between
page-leave clearing and application-close retention; the existing Classes Page
tests exercise both policies and their interaction with day-filter state.

## 2026-10-01 - F206 Custom Color Palette adapter

After closing the adjacent class-navigation preference pair, continue with
the existing typed palette boundary. Keep its 16-color normalization and
legacy payload decoding in the Qt adapter, preserve warning behavior for read
and write failures, and migrate only persistence ownership; callers and the
typed ColorUtils contract already use the adapter.

When a UI-shared header consumes a persistence adapter, keep database headers
out of that header: forward-declare `DatabaseSession` and move repository calls
to a `.cpp` owned by the existing QtSql-enabled feature target. This preserves
the UI compile boundary without widening its Qt dependencies.

When a preference reader falls back from a primary key to a legacy key, keep
the legacy value as the successful result even if best-effort migration of that
value to the primary key fails. Check session openness before trying either
key so the compatibility facade cannot supply a closed-session fallback.

## 2026-10-01 - F210 recent workspace history policy

Move deterministic mutation policy behind a Qt-free Application use case while
keeping path normalization, UTF-8 conversion, persistence, database-directory
memory, and menu updates at the controller boundary. Test alias cleanup and
`lastPath` behavior independently, and retain the controller lifecycle test as
an integration check.

## 2026-10-01 - F211 Speaking Evaluation read adapter boundary

The Speaking Evaluation read adapter already has a typed application query;
moving its repository and Qt conversion implementation out of the shared
header is a focused structural slice after F209 moved the save adapter. Keep
query identity validation, canonical IDs, UTF-16 row ordering, active-session
errors, and blank-grid behavior intact.

## 2026-10-01 - F212 upcoming birthday bucketing

Move deterministic date parsing, occurrence selection, and week-bucket policy
into the Qt-free Application layer. Keep locale-aware name ordering and
presentation at the feature boundary, and preserve Sunday week endings, year
rollover, the invalid-reference-date empty result, and the existing February
29 fallback. Add exact UTF-16 display-name preservation to the app-less tests
when feature inputs cross into the standard-library contract.

## 2026-10-01 - F213 default evaluation selection

When a feature path already has typed Application read ports and a Qt-free
selection rule, complete that path end to end before taking an adjacent
controller-policy candidate. Keep calendar schedule/date calculations and
localized display labels at the feature boundary; move only row-content policy
and data reads behind the existing contracts. Preserve the rule that any
non-whitespace evaluation cell makes the current term populated.

## 2026-10-01 - F214 class day-filter policy

Move matching and active-schedule visibility decisions behind the Qt-free
Application boundary while keeping weekday-string adaptation and presentation
in the feature. Preserve trimmed case-folded matching, weekend aliases, the
selected regular/intensive schedule, OR matching, and the rule that
ActiveSchedule excludes classes without entries even when the selected-day set
is empty.

## 2026-10-01 - F215 automatic update startup eligibility

Keep lifecycle and filesystem effects in the controller, and make eligibility
a deterministic Application decision. Preserve the order: pass lifecycle
guards, run one-time cleanup, check configuration/preference/URL, then mark the
one-shot state only when dispatching a forced check. A disabled preference can
be retried after the preference changes. `UpdateService` configuration is
immutable, so cover missing-URL eligibility in the app-less matrix and verify
controller non-dispatch without adding mutable configuration solely for a
same-controller recovery test. Re-read the preference before automatic
prompting.

## 2026-10-01 - F216 skipped update-version policy

Keep numeric parsing and settings/dialog effects at the Qt controller boundary
while moving clear/keep and automatic-prompt suppression decisions into
Application. Preserve the legacy two-part comparison: parsed numeric ordering
decides whether a stored skip is stale, while literal version text decides
whether it matches the latest release. Leading-zero values can parse as the
same version without being exact text matches.

## 2026-10-01 - F217 roster score import

Use the existing typed read query at the feature boundary, derive score rows in
a Qt-free Application use case, and keep table-column discovery and model
updates in the widget. Share the existing Qt-compatible UTF-16 trim policy
instead of maintaining a second whitespace list; retain UI pair matching and
last-write-wins behavior for duplicate imported names.

## 2026-10-01 - F218 Speaking Evaluation page reads

For a page migration, query, port, and navigation-model tests do not prove that
the widget wires selected schedule and visibility preferences into the model.
Keep a page-level integration case that exercises both filter dimensions,
class ordering, and selected-class retention through the migrated read path.

## 2026-10-01 - F219 Schedule Import state preflight

Use the shared Application state validator after the dialog has complete
decisions and current snapshots. Keep the dialog's earlier, more specific
status message and detailed overlap-warning list; the repository must repeat
state validation at apply time because the review snapshot can become stale.
Cover ambiguous targeted Skip alongside unique and targetless Skip so review
readiness matches the repository's exact-match rule.

## 2026-10-02 - F220 Schedule Import plan eligibility

Give intrinsic plan checks a Qt-free Application contract while keeping legacy
plan conversion and localized errors at the feature boundary. Preserve the
validator's existing first-error sequence and keep review-decision validation
as its own tested contract. Add a multi-candidate assertion to prove all
candidate basics are checked before any meeting-pattern or color rule.

## 2026-10-02 - F221 Schedule Import snapshot reads

Use the existing typed schedule-state snapshot as the payload for an
Application read query and active-session Platform adapter. The dialog refresh
also uses this one typed snapshot for projection and conflict labels, avoiding
mixed-source reads while it evaluates the user's resolutions. Preserve class
order by mapping aggregate class details back to the active repository's class
list. Missing sessions and read failures surface explicitly without legacy
fallback, and the repository remains authoritative if state changes after
review.

## 2026-10-02 - F222 Schedule Import resolution choices

Keep control construction in the feature UI while moving existing teacher and
class choice reads onto the typed snapshot. Preserve option ordering and
eligibility behavior, and keep schedule preview and apply-time validation in
their current owners so the read-boundary migration stays isolated.

## 2026-10-02 - F223 Schedule Import matching projection

Reuse the existing Qt-free matching projection for review preparation and
adapt Qt text only at the feature boundary. Preserve the legacy simplified,
case-folded keys and ID ordering while sourcing teacher/class state from one
typed snapshot; include class room number in that snapshot because room
matching is part of the current preview behavior. Keep workbook parsing and
repository apply validation with their existing owners.
When a warning depends on projected choices, assert the actual initial target
IDs before checking the warning; a fake preview can otherwise mask a mismatch
between snapshot matching and conflict reporting.

## 2026-10-02 - F224 Schedule Import review readiness

Let the typed snapshot query own review-time session and repository readiness.
Keep the legacy service lookup for the write operation only, and test a closed
session through the typed failure message, absence of controls, and absence of
legacy preview calls.

## 2026-10-02 - F225 Schedule Import apply contract

Validate action/target-ID shape before dispatching an application write port,
while leaving target existence and current-state validation to the repository
transaction. Test both invalid shape rejection without a port call and valid
existing-teacher dispatch.

## 2026-10-02 - F226 Schedule Import review readiness

Compose the existing decision and state validators behind one Application
boundary, preserving decision-first evaluation. Return both typed decisions
and optional state errors so the UI can keep conflict detail and message
priority without re-reading the snapshot.

## 2026-10-02 - F227 Schedule Import proposed summary

Build review counts from the compact decisions already produced during a
refresh plus scalar diagnostic and cleared-schedule counts. Reconstructing the
full apply request for summary text needlessly copies candidate and time data.
Keep localized wording in the dialog and the applied-result summary on the
apply path.

## 2026-10-02 - F228 Schedule Import cleared-schedule count

Move the derived count onto the typed snapshot and decision boundary, while
preserving the feature's full-parse integer ID matching for selected targets.
Keep zero when a refresh has no snapshot or intensive classes are retained;
assert the failed-refresh summary count so this fallback remains visible in
coverage.

## 2026-10-02 - F229 Schedule Import preview projection

Use Application projection membership and preservation markers to select
review-preview schedules, but retain the dialog's control order and per-control
Skip rows. The projector coalesces by ClassId and orders references for apply;
that order is not the established UI order. Preserve targeted Skip times from
the snapshot when another action shares the ID, and test order, targetless
Skip, incomplete actions, and intensive preservation.

## 2026-10-02 - F230 Schedule Import typed apply handoff

When an Application apply request already exists, the dialog should build and
pass that typed contract directly instead of routing through a legacy plan.
Keep confirmation timing, detailed user-facing policy errors, and legacy
conversion at the persistence boundary under explicit parity coverage.

## 2026-10-02 - F231 Schedule Import decision projection

Derive the compact review-decision request from the typed apply request once
in Application and reuse it for apply eligibility, readiness, and summary.
Keep UI ordering and the class skip cascade visible before projection; test
Unicode conversion, typed targets, and normalized/empty rooms at the boundary.
For the persistence follow-up, confirm dependency direction before routing the
Application contract into repository code; keep fresh-state checks and
transaction guarantees at the active-session write boundary.

## 2026-10-02 - F232 Schedule Import session-bound apply

Keep the typed v2 write path on the active session's repository and out of the
legacy service fallback. Preserve the single repository transaction core and
its fresh-state checks for both typed and v1 plan callers; put any interim
request adapter at the repository boundary rather than duplicating writes or
extending the legacy service with the Application DTO.

## 2026-10-02 - F233 Schedule Import shared apply core accepted

Use the existing typed ApplyRequest as the shared repository-core input and
keep ScheduleImportPlan only at the v1 edge. This removes the v2 plan
round-trip without introducing a second persistence command; both entrypoints
preserve their validated results while sharing fresh-state and transactional
behavior. Direct typed requests need their own malformed-index and
action-target rejection coverage because they bypass the legacy plan edge.

## 2026-10-02 - F234 shared typed apply validation accepted

Keep the same typed ApplyRequest validation at both the UseCase and direct
repository boundary. Validate plan eligibility first, then reject invalid
intensive-mode enum values even on normal schedules, then check teacher target
shape. This closes a boundary mismatch while preserving diagnostic ordering,
localized edge messages, and the v1 ScheduleImportPlan validator.

## 2026-10-02 - F235 exact typed target IDs

Keep typed teacher and class IDs intact through fresh-state comparison. Do not
parse and reserialize the IDs before validation: the typed value `01` must not
select the persisted canonical ID `1`. Convert only at the SQL adapter after
state validation, and cover both teacher and class targets before writes.

## 2026-10-02 - F236 structured Schedule Import failures accepted

Carry structured policy, teacher-target, and fresh-state validation failures
through the typed Repository and Platform path so the dialog can use its
existing issue formatters. Keep SQL/transaction failures message-only and
preserve the legacy plan API. Keep overlap start/end context in its typed
error. A dialog result code alone does not prove it stayed open; show it and
assert visibility before and after the failure.

## 2026-10-02 - F237 Speaking Evaluation validation accepted

Share Qt-free normalization and validation between the save use case and page
feedback, preserving baseline issue locations/messages, focus behavior, and
the questionable Korean-name-length option. Compare both flag settings with
the legacy validator, and test that declining the confirmation preserves saved
data.

## 2026-10-02 - F238 initial setup recovery accepted

Address the explicitly named backup/recovery gap with a Qt-free contract for
preserving, finalizing, and restoring an initial-setup profile replacement.
Keep file operations in Platform and test failure ordering, original-profile
survival, UI messages, and recent-file behavior. Defer Teacher Import typed
apply, whose current Application use case already runs in the repository and
whose production integration is a separate boundary migration.

## 2026-10-02 - F241 custom-column name policy accepted; F242 selected

Move custom-column name admission into the Qt-free Application layer while
keeping exact Qt Unicode comparison at its adapter boundary. Preserve
`QString::simplified()` across all UTF-16 code units and copy code units without
standard-string BOM interpretation; that same lossless adapter protects the
adjacent row move/removal contracts. The next bounded Gate 1 gap is
custom-column removal eligibility, with mutation and layout remaining at the
model/widget boundary.

## 2026-10-02 - F242 custom-column removal eligibility accepted; F243 selected

Reuse F241's normalization and injected Qt case-insensitive comparison for
custom-column removal eligibility, while keeping actual mutation, notifications,
validation, dirty state, and widget effects at their current owners. The paired
scan also found transfer-row mapping/admission; choose the narrower adjacent
custom-column append operation for F243, reusing F241 name admission and leaving
model signals and widget width/layout/selection/autosave at the existing
boundaries.

## 2026-10-02 - F243 custom-column append accepted; F244 selected and started

Reuse F241 admission to append a normalized roster column and one empty cell to
each existing row; preserve width metadata in the Qt-free operation. Keep model
signals and widget layout, selection, and autosave at their current owners. The
paired scan found target-side student transfer preparation as the next cohesive
Application boundary, with source removal and the atomic two-roster save left
in the widget workflow. F244 implementation has started, with independent
acceptance pending.

## 2026-10-02 - F244 roster transfer preparation accepted; F245 selected

Keep roster transfer preparation in Application as a compact mapped-row result
with typed rejection and first-empty-slot decision; leave actual target mutation
and the widget's atomic two-roster workflow in their existing owners. For F245,
move the shared first-unused Korean-name suffix suggestion to Application for
both roster and Speaking Evaluation models. Prefer this reusable cross-feature
rule over the widget-only same-grade target filter; keep duplicate-pair
grouping and the suffix choice UI separate.

## 2026-10-02 - F245 Korean-name suffix policy accepted; F246 selected

Commit `99c95146` moves first-unused suffix selection into a Qt-free
Application policy for both roster and Speaking Evaluation. Project the exact
legacy `StudentNameUtils::baseKoreanName()` and `koreanNameSuffix()` results
through the UI boundary: the validation normalizer does not match the legacy
regular-expression behavior for all Unicode whitespace and malformed UTF-16.
Keep duplicate grouping and suffix UI behavior in place. Move the lossless
QString/UTF-16 adapter to neutral `src/ui/shared` so Speaking Evaluation does
not depend on Roster UI. Fresh independent verification passed four focused
CTest entries and one-owner validation for 1,196 handwritten files.

For F246, select the same-grade transfer-target eligibility filter deferred in
the plan. This is the smaller next boundary: class/roster service access,
display labels, sorting, fullness, menus, and the transfer transaction remain
in the widget; Application receives current and candidate class IDs/grades and
returns eligibility only. The paired scans compared this with extracting
Speaking Evaluation roster-name import planning; keep that as a later option.

## 2026-10-02 - F246 transfer-target eligibility accepted; F247 selected

Keep candidate class-info lookups behind a separate Qt-free ID guard so invalid
and same-class IDs do not trigger service reads. Compare source and candidate
grades only after Qt-compatible trimming, with exact case-sensitive equality.
The widget still owns class/roster reads and errors, labels, ordering, fullness,
menus, and the atomic transfer/save workflow. Independent verification passed
the app-less policy and two focused roster/page targets; it also found the
current UI tests do not exercise eligible-target enumeration for a nonempty
grade. Direct contract coverage does exercise ID and grade decisions.

For F247, extract Speaking Evaluation roster-name import planning, leaving data
reads, warnings, per-cell editability and change application at the page edge.
Preserve first matching case-insensitive roster columns, trimmed legacy pair
keys, source order, duplicate filtering, blank-name-row eligibility, and
unchanged-cell omission. Add both policy cases and a page-level import-action
regression because no dedicated coverage currently exercises this path.

## 2026-10-02 - F247 roster-name import planning accepted; F248 selected

Commit `7021e657` moves deterministic Speaking Evaluation roster-name matching
and blank-row assignments into Qt-free Application. Keep first-match column
lookup, page/model editability checks and change application in the UI adapter.
Preserve the legacy trimmed U+001F pair key, including collisions; the query
uses the same Qt-compatible whitespace helper established by earlier slices.
The page regression verifies mixed-case duplicate headers, imports in source
order, preserves unrelated notes, and reports both successful and repeated
imports. Independent verification passed the standalone planner and page-save
CTest entries with one-owner validation for 1,200 handwritten files. Current
model flags make both name cells editable, so the adapter's partial-editability
branch is preserved but not exercised through the page test.

For F248, extract interactive duplicate-peer row lookup shared by RosterModel
and SpeakingEvalModel. Reuse F247's trimmed UTF-16 pair-key rule where clean,
but preserve the legacy U+001F collision behavior, selected-row exclusion, and
candidate order. Keep column choice and duplicate prompts/actions in their
current owners. Prefer this cross-model boundary over the paired scan's AI
batch eligibility candidate.

## 2026-10-02 - F248 duplicate-peer lookup accepted; F249 selected

The shared lookup can reuse F247's pair-key contract while model adapters retain
their own column selection and UI actions. Independent tests verified ordered
peers and collision compatibility. The next paired scans diverged between a
roster first-empty query and AI-batch eligibility; choose the pure student
eligibility decision with its existing dialog boundary and leave review status,
comment application, and the roster query separate.

## 2026-10-02 - F249 AI-batch eligibility accepted; F250 selected

The dialog can keep localization and row presentation while Application returns
a typed first-failure eligibility reason from simple facts. The two clipboard
failures reproduce as environment clipboard errors but lack a pre-slice baseline
comparison, so record them as unresolved environment limits. For the next
slice, choose the documented shared first-empty roster-row query to remove the
duplicate model/transfer decision; defer the more involved AI comment-review
policy until its legacy length thresholds have a clear shared source.

## 2026-10-02 - F250 row availability accepted; F251 selected

Share row occupancy and first-empty ordering once so RosterModel and transfer
preparation use the same trim rule. Reuse the existing Qt-compatible UTF-16
boundary; do not keep the now-unused model helper. The next AI comment-review
policy should take normalized UTF-16 length as input, retain empty parser status
behavior and warning order, and avoid duplicating the legacy maximum constant.

## 2026-10-02 - F251 AI comment quality accepted; F252 selected

F251 centralizes the preferred 420-character threshold with the prompt builder
and keeps the 450-character hard maximum sourced from the existing domain
constant. Independent x64 Debug verification passed the app-less policy and
three focused dialog functions; prompt wording remained unchanged. For F252,
extract the accepted-comment planning loop, including exact no-op filtering and
overwrite counting, while leaving QString normalization, localized confirmation,
and page/table mutation at the UI edge. Paired scans also proposed moving the
checkbox default rule; defer that presentation-only policy.

## 2026-10-02 - F252 accepted comment plan accepted; F253 selected

F252 moves checked/valid report filtering, unchanged-comment removal, ordered
assignments, and overwrite counting into a Qt-free Application policy. Keep
QString simplification, localized confirmation, accepted-dialog state, and
table mutation at the UI edge. Independent fresh x64 Debug verification passed
the app-less planner and the overwrite confirmation and existing apply-flow
dialog slots. For F253, share the exactly duplicated Did Well/Needs Improvement
section splitter between the AI batch dialog and private-notes editor. Keep its
legacy malformed-input fallback and section whitespace intact; leave joining,
bullet editing, and AI response parsing for separate slices.

## 2026-10-07 - Page and QML projection coverage lessons

When a page-level writer stub reports success, assert the artifact the page
actually owns. The Sub Prep page test verifies its information sheet; roster
PDF rendering remains lower-level coverage because the test stub writes no
roster PDF. Navigation availability guards belong before dirty-page leave
confirmation; the Classes route now has focused coverage for both an empty
open workspace and an unavailable database. Calendar month-grid projection can
be tested by enumerating instantiated QML cells and comparing their event rows
with `CalendarEventModel`; no separate legacy month grid was found, so the
coverage records the current projection contract rather than a historical-view
comparison.

## 2026-10-08 - F384 Calendar Preferences restoration accepted

F384 verifies that Restore Term Defaults stages the selected year's schedules in
the Calendar Preferences editors, checks linked schedules, and disables the
dependent Middle School controls without writing preferences or emitting the
calendar-change signal. Pin the academic year and seed both schools so date and
week expectations remain deterministic; saving is a separate action.
Independent current and pinned-baseline verification passed.

The user requested a stop immediately after the F384 acceptance commit. Phase 2
remains open, and no next slice is selected.


## 2026-10-08 - Staff Directory closed-session navigation gate

When a workspace is closed, each Staff Directory route must be rejected before prompting a dirty page to close. F386 covers both Native English and GS Team routes and checks prompt absence, destination-page absence, and preservation of the current dirty page. This supplements tests for open-session routing and sessions that close during confirmation.


## 2026-10-08 - F387 Calendar Preferences event-reset parity

F387 covers unavailable-session rejection, cancel behavior, failure after the destructive prompt, and successful reset with Calendar cache refresh. Its focused target passes on the pinned F386 production source with only the test source and CMake registration overlaid. Repeated canonical transcripts match; no production files changed.


## 2026-10-08 - F388 class route availability parity

F388 covers Class Details, Class Notes, and student Evaluation navigation with
the database open and closed. Closed-session routes preserve dirty Teacher Info
state without prompting; open-session routes navigate to the requested class
section after one Discard confirmation. The evaluation route selects Winter.
The focused Windows x64 Debug CTest passed 1/1, and direct QtTest emitted six
canonical route/session observations. No production sources changed.


## 2026-10-08 - F389 My Info route gate selected

F389 covers the My Workspace root and its Information, Schedule, and Calendar
route keys with open and closed database sessions. Closed-session dispatch must
preserve the dirty current page without prompting; open-session dispatch must
confirm once and select the requested tab. The second-last Batch 13 position
also triggered bounded Batch 14 discovery: Teacher, Campus Directory, and
Document Catalog route parity are the next candidates; the details are in the
Phase 2 progress log.


## 2026-10-08 - F389 My Info route gate accepted

F389 is accepted on the F388 source pin. Independent focused Windows x64 Debug
build and CTest passed, and direct QtTest verified all eight route/session
rows. Closed-session dispatch preserves the dirty Teacher Info page without
prompting, warning, page creation, or activation; open-session dispatch
selects the requested My Workspace tab after one Discard confirmation. The
canonical transcript SHA-256 is
`4d725cd4826c2913dbe97d49b1d10a4173d09c5f9a488a7634119096fe7e8c7d`.

The user supplied seven MSVC errors in the Teacher Profile Edit persistence
target. Commit `0b128601` uses the shared string-backed `TeacherId` and keeps
canonical positive integer conversion at the platform and UI boundaries. It
updates two existing test sources after the first production repair exposed
stale integer ID usage. The named target and standard all-target build passed
independent build-only verification with zero errors; no test binaries or CTest
ran. After the fix commit, F390 Sub Prep route gate parity became the current
slice. Its matrix covers the sidebar root and supported Notes destination with
open and closed sessions; Batch 14 remained inactive until Batch 13 completed.


## 2026-10-08 - F390 Sub Prep route gate accepted

F390 is accepted on the Teacher Profile Edit build-repair commit. The focused
Sub Prep route test passed 1/1 CTest, and independent direct QtTest verified
four route/session rows with byte-identical JSONL transcripts. Closed-session
root and Notes routes preserve dirty Teacher Info without creating Sub Prep or
prompting. Open routes select Important Information or Notes after one Discard
confirmation. The canonical transcript SHA-256 is
`FF1CCDB4ED143C6B4955254A115EA7DF3AFD000878D5EE6CBAEF4F67F3689179`.

F390 completes Batch 13. Batch 14 is active with F391 Teacher route closed-
session leave-confirmation gate parity selected/current; F392 Campus Directory
and F393 Document Catalog/PDF Viewer route parity remain provisional.


## 2026-10-08 - F391 Teacher route gate selected

The current Teacher route reads the profile before asking the dirty current
page to close. The missing parity case is a valid Teacher route with a closed
database and unsaved Teacher Info; it should return before prompting and
preserve the current profile and notes. Existing tests already cover the open
route and invalid IDs. The two regular-Teacher sidebar origins share this
controller branch; Staff Directory routes remain separate. F392 is Batch 14's
second-last slice, so Batch 15 discovery must begin when F392 starts.


## 2026-10-08 - F391 Teacher route gate accepted

F391 adds the missing closed-session case for a valid Teacher route while a
dirty Teacher Info page is current. The focused target build and CTest passed;
independent direct invocation of the new QtTest case and `git diff --check`
also passed. The case confirms the selected profile and exact dirty notes stay
visible with no prompt or captured Qt warning. Batch 14 now has F392 Campus
Directory route parity selected/current and F393 Document Catalog/PDF Viewer
route parity provisional. Batch 15 bounded discovery starts with F392.


## 2026-10-08 - F392 Campus Directory route parity selected

F392 covers Campus Directory root and section navigation. Local source has no
database-session availability gate: navigation from another page confirms
before entering, each child route selects its matching section, and navigation
within Campus Dashboard keeps the current page without a leave prompt. The
acceptance matrix includes root and child cancellation, the five section
destinations, and root/section actions while the dashboard is already dirty.
Batch 15 discovery at this second-last Batch 14 slice found two distinct
candidates: F394 Classes landing open-session confirmation parity and F395
Campus Dashboard typed save boundary. Both activate after F393; the bounded
discovery record is in the Phase 2 progress log.


## 2026-10-08 - F392 Campus Directory route parity accepted

The focused Campus route target and CTest passed, and independent direct
QtTest verified all ten route rows. Root and child navigation from a dirty
page follows the leave confirmation; the root selects Information, each of
the five child routes selects its matching section, and root/child actions
within Campus Dashboard do not prompt to leave. Batch 14 advances to F393
Document Catalog/PDF Viewer route parity. Batch 15 candidates F394 and F395
remain provisional until F393 completes.


## 2026-10-08 - F393 Document Catalog route parity accepted

F393 adds a controller-level test that clicks a real document leaf in the
Sidebar, then checks cancellation, successful PDF loading, and missing-resource
behavior from a dirty page. The controller resolves content before asking to
leave, so a missing PDF does not discard the current edits; cancellation also
releases the preflight resource lease. The focused target build and CTest passed,
and direct QtTest invocation of all three cases passed. Controller warning
capture was empty; the offscreen size-hints and font-directory setup warnings
occurred before the capture scope. No full suite ran.


## 2026-10-08 - F394 Classes landing route parity selected

F393 completes Batch 14. Batch 15 is active with F394 Classes landing
open-session confirmation parity selected/current and F395 Campus Dashboard
typed save boundary provisional. F394 is the second-last slice in this batch,
so Batch 16 discovery begins as F394 work starts.


## 2026-10-08 - F394 Classes landing route parity accepted

F394 adds open-session confirmation coverage from dirty Teacher Info to the
real Classes landing route. Cancel preserves the source form and does not
create Classes; Discard lands on the empty Classes page without persisting the
draft. Its focused target build, CTest, and two direct QtTest invocations
passed.


## 2026-10-08 - F395 Campus Dashboard typed save boundary accepted

F395 shares a Qt-free CampusDashboardCampusSnapshot between selected-campus
reads and a typed save port, with a Platform adapter delegating to the
existing repository. Save failures retain the dirty draft and stop campus
switch or New Campus transitions. Focused Application, Platform, and page
builds and CTests passed 3/3; direct cases covered the value contract,
canonical repository round-trip, error mapping, and page behavior.


## 2026-10-08 - F396 Staff Directory open-session dirty-exit parity accepted

F396 verifies both production Staff Directory Sidebar routes from dirty Teacher
Info with the workspace open. Real route payloads pass through
NavigationController. Cancel preserves the complete dirty form and does not
create the destination; Discard loads the requested seeded directory without
persisting the edit. The focused build and CTest passed, as did all four direct
QtTest cases. The production controller already met the behavior, so this
slice required test and CMake registration changes only. The next selected
slice is F397; Batch 17 discovery is due when it starts.


## 2026-10-08 - F397 Document Catalog retranslation accepted

F397 showed that MainWindow retranslated catalog labels but preserved only
top-level Sidebar expansion. The shared Sidebar snapshot now captures full
stable key paths recursively and restores all keyed open/closed states across
the rebuild. Tests cover a synthetic deeper folder with an expanded descendant
under a collapsed ancestor and a real MainWindow English/Korean action path
using the shipped catalog. Selection/page state remained unchanged, and no PDF
content was requested or loaded. Focused Sidebar and MainWindow CTests passed
2/2; the two direct QtTest cases passed. Batch 16 advances to F398; Batch 17
discovery is recorded and activates after F398.

### F398 same-path workspace reopen - 2026-10-08

Same-path selection preserves the ordinary successful-open transition: the
coordinator invokes its gateway and clears selection, the FileController
reorders recent history, and MainWindow returns to My Workspace/Schedule. This
decision preserves the existing Phase 2 parity behavior. For regression tests,
establish the initial active workspace
through FileController/coordinator. Opening only through the legacy
ApplicationServices facade leaves the controller’s separate WorkspaceState
closed and does not exercise replacement.

## 2026-10-08 - F399 Close File no-workspace parity accepted

The production Close File QAction applies the current page's dirty decision
before attempting workspace close. A real MainWindow test now covers Cancel
preserving the dirty draft and active UI, then Discard closing through the
workspace coordinator and showing Campus Dashboard Information. In an empty
workspace, entity-dependent database actions may already be disabled; snapshot
their initial state when checking Cancel, while requiring core file actions to
start enabled and all database-backed actions to be disabled after close. The
production route already met the contract, so this slice added test coverage
only.

## 2026-10-08 - F400 Open File dirty-page gate accepted

The production Open File action asks the current page before opening the file
chooser. Page Cancel returns before the chooser and preserves the dirty draft.
Discard reloads the saved page data before the chooser; if the chooser is then
cancelled, the current workspace remains open and the discarded draft stays
discarded. The real MainWindow QAction test covers both decision boundaries with
injected prompt and file-dialog services. This records current behavior and
does not claim legacy parity for draft restoration after chooser cancellation.

## 2026-10-08 - F401 Recent menu selection and missing-path pruning accepted

F401 adds real MainWindow coverage for opening a different existing workspace
from the Recent menu and for selecting a missing recent path. The successful
selection activates the chosen workspace and moves it to the front of recent
history without invoking the Open File chooser. A missing selection warns,
prunes recent and matching last-file history, refreshes the menu, and preserves
the active database session and page. FileController already implemented these
behaviors, so the slice changes tests only. The missing-path case uses a clean
page; this slice does not change the existing dirty-page prompt order. The
focused Ninja fallback target built and CTest passed 1/1 (1.02 s); no full suite
ran.


## 2026-10-08 - F402 MainWindow application-exit confirmation accepted

A real visible MainWindow test exercises the application-exit QCloseEvent via
`QWidget::close()` with an open workspace and dirty Details draft. Cancel rejects
the close and preserves the exact session/path, page, tab, Sidebar selection,
draft, and dirty state; Discard accepts a second close and restores the saved
draft. No production change was needed. The focused Ninja/MSVC/Qt 6.12 target
built and filtered CTest passed 1/1 (0.34 s). No full suite ran; the standard
Visual Studio tree remains blocked before source compilation by the existing
FileTracker/CommonApplicationData issue. Batch 17 advances to F403 after the
F402 commit; the ten Batch 18 candidates activate after F403.


## 2026-10-08 - F403 Dynamic Teacher Sidebar leaf navigation accepted

A real MainWindow integration test uses the production startup refresh to
create Co-Teachers and Korean Teachers leaves, clicks each target, and checks
the emitted teacher ID and stable route keys. For both groups, Cancel preserves
the source Teacher Info page and exact dirty draft; Discard loads the target,
cleans the page, and leaves source persistence unchanged. The workspace session
and path remain active. No production changes were needed. The focused
Ninja/MSVC/Qt 6.12 target built and CTest passed 1/1 (0.52 s); independent
filtered CTest passed 1/1. No full suite ran. F403 is ready to commit; F404 is
the first Batch 18 candidate and activates after the F403 commit.


## 2026-10-08 - F404 Save choice on Open/Close File accepted

F404 adds real MainWindow action coverage for the Save choice on both Open File
and Close File. Open→Save→chooser cancel persists the Details draft, clears
dirty state, and preserves the same workspace/session/path/page/tab/Sidebar.
Close→Save persists the draft, closes the workspace, routes to Campus Dashboard
Information, disables database-backed actions, and remains persisted after the
workspace is reopened. No production changes were needed. Both focused
Ninja/MSVC/Qt 6.12 targets built under VS 18 x64, and filtered CTest passed 2/2.
No full suite ran. Batch 18 continues with F405 queued after the F404 commit.

## 2026-10-08 - F405 MainWindow Save As and Export action integration accepted

F405 is accepted and ready to commit. The target built under VS 18 x64 with
Ninja/MSVC; CTest simple-name filter passed 1/1. Batch 18 remains active, with
F406 queued after the F405 commit.

## 2026-10-08 - F405 committed; F406 Manage Campuses transition selected

F405 is committed as 07dc5864. Its target built under VS 18 x64 with
Ninja/MSVC; CTest simple-name filter passed 1/1.

F406 is selected/current in Batch 18. It covers Cancel and Discard from dirty My
Workspace Details through the real Manage Campuses QAction in an admin MainWindow.
Cancel preserves page/tab, draft, Sidebar, session/path and blocks the transition.
Discard must reach Campus Dashboard Information with matching Sidebar selection,
restore the persisted draft cleanly, and preserve session/path/actions. Set the
Dashboard to a different section before Discard: the action changes Sidebar
selection to `campus_information` but does not reset an existing page section;
`CampusDashboardPage::showInformation()` may be needed. This matrix is provisional;
F406 is not accepted. F407-F413 remain queued.

## 2026-10-08 - F406 Manage Campuses QAction accepted

F406 is accepted and ready to commit. With the real Manage Campuses QAction,
Cancel preserves the dirty Details state and stops the transition; Discard from a
different Dashboard section reaches Campus Dashboard Information, restores the
persisted draft cleanly, and preserves the open session/path/actions. MainWindow
now calls `CampusDashboardPage::showInformation()` after showing the reused
Dashboard, fixing the Sidebar/page mismatch. The focused
`ClassMngrMainWindowManageCampusesParityTests` target built under VS 18 x64 with
Ninja/MSVC; simple-name filtered CTest passed 1/1 with both cases. F407 is queued
after the F406 commit; F408-F413 follow.

## 2026-10-08 - F407 Schedule handoff accepted / ready to commit

F406 is committed as 28b27998. F407 is implementation complete and accepted,
ready to commit; F408 is queued after its commit and F409-F413 follow. Real
Testing Classes/back actions cover standalone Schedule and My Workspace →
Schedule, returning to the correct source page/tab and preserving session/path/
Sidebar. The cell-dialog producer forwards the empty choice list’s natural
non-positive ID plus a specific day/time, and class creation persists the slot.
Cancel preserves dirty Testing Classes state; Discard completes the
source-dependent return. The cell-dialog route now uses `QTest::mouseClick` on the rendered table
viewport, covering the actual `QTableWidget` `cellClicked` connection and
downstream handoff. Focused target
`ClassMngrMainWindowScheduleTestingClassesHandoffParityTests` built with
Ninja/MSVC; CTest passed 1/1 (1.54 s) on 2026-10-08.

## 2026-10-08 - F410 committed / F411 discovery started

F407 is committed as `f639fbd3`; F408 Dynamic Teacher Sidebar
selection/state after retranslation is committed as `9f92b78d`; F409 My
Workspace Sidebar root producer-to-handler integration is committed as
`ee319df2`. Sidebar
restoration now uses the saved stable key path plus teacher ID to restore the
exact duplicate occurrence, falling back to ID-based selection if that
occurrence no longer exists. The focused MainWindow test switches
English↔Korean from both Co-Teachers and Campus Staff → Korean Teachers and
verifies the occurrence, selected ID, expansion, no route event, Teacher Info
identity, dirty manual-save draft and no prompt, delete action, database
session/path, and dynamic teacher display. It also verifies translated group
and internet-type labels while keeping the combo’s stable data and user draft
intact. The focused target built under VS 18 x64; filtered CTest passed 1/1
(1.21 s).

F409 My Workspace Sidebar root producer-to-handler integration is implemented
and accepted. Existing route coverage synthesized `NavigationData` and called
the controller directly, while Sidebar structure coverage selected the root
programmatically; neither proved a real root click reached MainWindow’s
connected handler. The new focused test clicks the rendered root and passively
checks its production payload, then verifies the current My Workspace page,
Schedule tab, root selection, unchanged page instance, and open database
session/path. It built under VS 18 x64 and filtered CTest passed 1/1 (0.37 s).
A possible dirty-cancel selection mismatch remains source-inferred and outside
F409.

F410 Classes Sidebar root integration is implemented and accepted. The new
MainWindow test clicks the rendered Classes root, checks its actual payload
(`Page`, displayed label, `classes` key/route, class ID -1), and verifies the
Classes page, root selection, and unchanged open database session/path. The
first run exposed a production teardown lifetime defect: QObject-owned pages
kept non-owning service pointers but were destroyed after MainWindow's service
member. `MainWindow::~MainWindow()` now deletes `m_pages` while
`ApplicationServices` is alive, preserving normal page hide/teardown behavior.
The focused target built under VS 18 x64 and independent CTest passed 1/1
(0.32 s) after the fix. F410 is committed as `14723973`.

F411 Sub Prep Sidebar root integration is implemented and accepted. The new
MainWindow test clicks the rendered root, verifies its actual `Page` payload,
displayed label, stable `sub_prep` key/route and class ID -1, then checks the
Sub Prep page at Important Information, root selection, and unchanged open
database session/path. Existing route-gate tests retain closed-session and
dirty-page coverage. The focused target rebuilt under VS 18 x64; independent
CTest passed 1/1 (0.41 s), and the direct executable exited normally. F412-F413
remain queued in Batch 18; discover Batch 19 when F412 starts.

## 2026-10-08 - F411 committed / F412 selected; Batch 19 discovery completed

F411 Sub Prep Sidebar root integration is committed as `3e5dbea4`. Its focused
VS 18 x64 Debug target rebuilt and independent CTest passed 1/1 (0.41 s); the
direct executable exited normally. F412 Campus Sidebar root plus section
producer integration is now selected/current. Batch 19 discovery began before
F412 implementation, following the second-last-slice trigger in
`00-Start-Here.md`. Two independent bounded reviews produced eight reconciled
provisional candidates for after F413; they do not establish Phase 2 exhaustion.
The user-modified `latest_session_work.md` and untracked `%SystemDrive%/`
artifact remain untouched.

## 2026-10-08 - F412 Campus Sidebar integration accepted / ready to commit

F412 Campus Sidebar root and section producer-to-handler integration is
implemented and independently accepted. The real MainWindow test QTest-clicks
the rendered root and each of its five sections, verifies each emitted payload,
current Campus Dashboard section, selected Sidebar path, and unchanged open
session/path. The VS 18 x64 Debug target rebuilt; independent filtered CTest
passed 1/1 (0.42 s), with no lingering process. No production change or runtime
defect was needed. F413 Initial Setup success navigation remains queued.

## 2026-10-08 - F412 committed / F413 selected-current

F412 Campus Sidebar root and section integration is committed as `76661791`.
It adds real MainWindow clicks for the Campus root and all five sections, with
payload, page, selection, and session/path assertions; independent focused
CTest passed 1/1 (0.42 s). F413 Initial Setup success navigation from the
empty-state button is now selected/current. Its acceptance discovery is
underway; Batch 19 remains provisional until the final Batch 18 slice completes.

## 2026-10-08 - F413 Initial Setup handoff accepted / ready to commit

F413 exercises the real empty-state setup button in MainWindow, the forwarded
BasePage/PageManager signals, and the accepted InitialSetupWizard result. It
verifies the new profile/session, My Workspace Schedule, `my_workspace`
Sidebar selection, and hidden no-database banner. The VS 18 x64 Debug target
rebuilt; independent filtered CTest passed 1/1 (0.46 s), with no lingering
process. No production change or runtime defect was needed. Batch 19 remains
provisional until F413 is committed and Batch 18 closes.

## 2026-10-08 - F413 committed / Batch 18 complete / F414 selected-current

F413 Initial Setup empty-state handoff is committed as `5d8a941a`, completing
Batch 18. Its focused VS 18 x64 Debug target and independent registered CTest
passed 1/1 (0.46 s). Batch 19 is active with F414 Teacher profile save
preserving the selected duplicate Sidebar occurrence selected/current; F415-F421
follow in discovery order.

## 2026-10-08 - F414 Teacher profile save occurrence restoration accepted

The focused MainWindow regression reproduced the selected duplicate shifting
from Campus Staff → Korean Teachers to Co-Teachers after a successful manual
profile save. The fix captures the selected key path before the Sidebar refresh
and restores it when the saved teacher remains selected. The test verifies the
updated duplicate labels, persisted profile, clean same page, no extra route or
prompt, and stable database session/path. The focused VS 18 x64 Debug target
rebuilt; independent filtered CTest passed 1/1 (1.52 s), and direct QtTest
passed 5/5. No LNK4006 warnings occurred. F414 is accepted and ready to commit;
F415 remains queued until that commit.

## 2026-10-08 - F414 committed / F415 selected-current

F414 Teacher profile save preserving the selected duplicate Sidebar occurrence
is committed as `6fb39b2b`. Its focused target rebuilt, the exact filtered
CTest passed 1/1 (1.52 s), direct QtTest passed 5/5, and no LNK4006 warnings
occurred. Batch 19 continues with F415 Campus Dashboard page-tab-to-Sidebar
synchronization selected/current; source discovery and acceptance definition
are underway. F416-F421 remain queued.

## 2026-10-08 - F415 Campus Dashboard tab handoff accepted / ready to commit

The existing MainWindow/open-session test now QTest-clicks the actual Campus
Dashboard tabs for Information, Address, Directions, Housing, and Maps without
adding a test-side connection. Each `sectionChanged` key matched the page and
Sidebar selection; no extra route event fired, and the same page and database
session/path remained active. The focused VS 18 x64 Debug target rebuilt;
independent filtered CTest passed 1/1 (0.46 s). No LNK4006 warnings occurred.
No production change was needed. F415 is accepted and ready to commit; F416
remains queued until the commit.

## 2026-10-08 - F415 committed / F416 selected-current

F415 Campus Dashboard page-tab-to-Sidebar synchronization is committed as
`f4bc5282`. Its focused target rebuilt and its independent filtered CTest
passed 1/1 (0.46 s), with no LNK4006 warnings and no production change. Batch
19 continues with F416 Document Catalog rendered Sidebar leaf through
MainWindow and viewer selected/current; bounded source discovery is underway.
F417-F421 remain queued.

## 2026-10-09 - F416 Document Catalog MainWindow viewer integration accepted

F416 adds a test-only MainWindow click on a rendered Document Catalog leaf,
covering the production route connection through a Ready PDF viewer. The test
checks the viewer is not instantiated before the click, route and selected
keys, resource reference/path, print/save capability, and absence of
navigation-time modal dialogs or Qt warnings. Independent focused CTest passed
1/1. Keep F417 queued: the user requested stopping after the F416 commit.

## 2026-10-09 - F417 Staff Directory MainWindow integration accepted

The user resumed Phase 2 after F416 and requested commit-per-slice continuation.
F417 adds real MainWindow coverage for both rendered Staff Directory leaves
through the production Sidebar-to-NavigationController connection. It verifies
route payloads, selected keys, current pages, sorted rows, and unchanged
workspace session/path; all four existing open-session cancel/discard cases
remain intact. Executor and independent builds succeeded, and the exact
filtered CTest passed 1/1 for each; no production change was needed. The
acceptance matrix and details are in the Phase 2 progress log. The pre-existing
modification to `latest_session_work.md` and untracked `%SystemDrive%/` entry
remain preserved. F417 is accepted and committed as `d04d9bb0`.

## 2026-10-09 - F417 committed / F418 selected-current

F417's seven-file slice is committed as `d04d9bb0`. It adds real MainWindow
coverage for both rendered Staff Directory leaves and retains all four
open-session cancel/discard cases. Executor and independent focused CTest runs
passed 1/1 each. F418 Schedule Import through MainWindow apply and Sidebar
refresh is now selected/current; F419-F421 remain queued.

## 2026-10-09 - F418 Schedule Import MainWindow integration accepted

F418 extends the real MainWindow Schedule target to exercise the rendered
Schedule Import action through the production dialogs, apply use case, stale
Schedule-page refresh, and teacher Sidebar refresh. The test checks the
persisted Korean teacher/class and Monday/Friday times, matching teacher ID in
the Korean Teachers group, refreshed visible schedule, expected prompts, and
unchanged active page, Sidebar selection, session, and workspace path. The
executor and independent Tester builds succeeded after elevated retries for
the known Visual Studio `ZERO_CHECK` FileTracker access error; both focused
CTest runs passed 1/1, and the independent QtTest run passed 7/7 including
init/cleanup. Qt emitted only missing-font-directory and offscreen
`propagateSizeHints()` notices; the project fonts loaded and no test process
remained. No production change was required, and no full suite ran. F418 is
accepted and ready to commit; F419-F421 remain queued until its commit.

## 2026-10-09 - F418 committed / F419 selected-current

F418 Schedule Import through MainWindow apply and Sidebar refresh is committed
as `ea755736`. The six-file commit contains the real MainWindow import
integration and acceptance records. Executor and independent focused CTest
runs passed 1/1; direct QtTest passed 7/7 including init/cleanup. F419
MainWindow Print/Save Current Page As action capability and enabled state is
selected/current; F420-F421 remain queued.

## 2026-10-09 - F419 MainWindow output action state accepted

F419 extends the real Document Catalog MainWindow integration to check the
Print, Save As, and Print / Export actions from the unsupported Campus
Dashboard through a Ready PDF and back after the viewer is released. The
post-navigation state confirms no database is open. Existing route, resource,
no-modal, and warning assertions remain. Executor and independent focused
CTest runs passed 1/1 each; QtTest passed 3/3 including init/cleanup. The
executor build reported `LNK4075`; its elevated retry succeeded, while the
independent elevated retry had no build warnings. Qt's missing `lib/fonts`
directory notice occurred at runtime, but repository fonts loaded. No
production change was needed; no process remained and no full suite ran. F419
is accepted and ready to commit; F420-F421 remain queued until its commit.

## 2026-10-09 - F419 committed / F420 selected-current

F419 MainWindow Print/Save Current Page As action capability and enabled state
is committed as `e2ad222b`. Its six-file commit covers disabled actions on
Campus Dashboard, enabled actions for the Ready PDF, and disabled actions
after document release, including the no-database end state. Executor and
independent focused CTest runs passed 1/1; QtTest passed 3/3. F420
Class/Schedule save signal to Sidebar action-state refresh is selected/current;
F421 remains queued.


## 2026-10-09 - F420 Class and Schedule save refresh accepted

F420 proves that successful saves through Classes Details and the workspace
Schedule Editor refresh the real Sidebar class actions in MainWindow. Both
action pairs transition from disabled to enabled after the production page
signal; the workspace remains open and the saved class details persist.
Executor and independent focused CTest runs passed 1/1, and direct QtTest
passed 9/9 each. The independent first run caught an overlength fixture
teacher name before either save route ran; after correcting it and retaining
the service error, both routes passed. No production change was needed. Qt
font-directory and offscreen sizing notices remained non-failing; no full suite
ran. F420 is accepted and ready to commit; F421 remains queued.


## 2026-10-09 - F420 committed / F421 selected-current

F420 is committed as 4c10f0d1. Its two MainWindow save paths independently passed focused CTest and QtTest, confirming class action refresh, persistence, and session continuity. F421 Useful Links URL handoff is selected/current. Qt supports capturing the real HTTPS openUrl request with a scoped URL handler; the test will also preserve the no-navigation and Sidebar current-item behavior.


## 2026-10-09 - F421 Useful Links URL handoff accepted

F421 covers all seven rendered HTTPS leaves through the MainWindow Sidebar and captures the real Qt openUrl handoff without launching a browser. It verifies exact destinations, no navigation, unchanged page, cleared selection, and current root/leaf keys. The initial runtime exposed an invalid setUrlHandler method signature and a CTest filter mismatch; both were corrected. Executor and independent CTest passed 1/1, and independent QtTest passed 4/4. Only Qt's system-font-directory notice remained; repository fonts loaded. No production change or full-suite run was needed. F421 is accepted and ready to commit.


## 2026-10-09 - F421 committed / F422 selected-current

F421 Useful Links URL handoff is committed as 27c064e3. The focused target and independent CTest passed 1/1; independent QtTest passed 4/4 for the seven real Sidebar URL handoffs. F422 is selected next: verify both standalone and workspace Schedule pages become stale after a Testing Classes save, then refresh through normal page activation. The test must avoid the Back route that independently marks one page stale.

## 2026-10-09 - F422 Testing Classes schedule refresh accepted

A real Testing Classes rename/save marks both distinct Schedule views stale, and normal page activation refreshes each Testing-mode model to the updated class name. The focused build, exact filtered CTest (1/1), and direct QtTest (10/10) passed independently. No production code changed. The fixture must use the slot's HH:mm storage format; diagnostics must not evaluate expected::error() on success.

## 2026-10-09 - F422 committed / F423 selected

F422 is committed as 08f44ac6. Its real Testing Classes save signal marked both distinct Schedule views stale; normal activation refreshed each Testing-mode view to the renamed class. Independent focused CTest and direct QtTest passed. For F423, the test must observe the already-open Classes page change before clicking its Sidebar route again: that route reloads from the persisted mode and could mask a missing live MainWindow signal connection.

## 2026-10-09 - F423 My Schedule mode handoff accepted

The integration test changes My Workspace Schedule to Intensive through its real button and observes the already-loaded Classes page update before routing to Classes again. This catches the missing-connection case that a route reload could mask by reading the saved preference. Executor and independent focused CTest passed; direct target and selected-case QtTest passed. No production code changed.

## 2026-10-09 - F423 committed / F424 selected

F423 is committed as 34966439. The test proved My Workspace Schedule's Intensive action updated an already-loaded Classes page before route reload; this avoids a false pass from stored-preference initialization. F424 targets the Sidebar Add Class QAction's MainWindow/controller connection and resulting selected Details record, beyond the existing signal-only Sidebar test.

## 2026-10-09 - F424 Add Class context-menu handoff accepted

The test now follows the Sidebar tree context-menu connection into the MainWindow SidebarController and verifies the created class opens in Details. Offscreen QtTest did not produce a context-menu event from a right-button mouse click alone; sending QContextMenuEvent through the viewport exercised the production connection. The existing signal-only test remains useful but cannot prove the handler path. Independent focused checks passed; no production code changed.

## 2026-10-09 - F424 committed / F425 selected

F424 is committed as 3342963b. Offscreen QtTest did not produce a context-menu event from a right-click alone; a viewport QContextMenuEvent exercised the production tree connection and Add Class QAction. F425 will trigger the ActionRegistry Upcoming Birthdays QAction and inspect current-date entries across all three staff directories; direct controller tests do not cover its QAction connection.

## 2026-10-09 - F425 Upcoming Birthdays QAction accepted

The real MainWindow action opens the modal with birthday entries and directory details from all three staff sources, while leaving page, Sidebar, database session/path, prompt queues, and dismissal preference unchanged. Existing controller tests did not cover the QAction connection. The MainWindow case reuses the established teacher-directory persistence path; the first fixture seed attempt did not populate it correctly. Independent focused CTest and QtTest passed. No production code changed.

## 2026-10-09 - F425 committed / F426 selected

F425 is committed as 37588279. The ActionRegistry Upcoming Birthdays QAction displayed seeded current-date entries across three directories; no controller shortcut was used. F426 will isolate the Import Classes QAction-to-file-picker connection with a canceled FakeFileDialogService request, since existing direct-controller tests already cover import apply behavior.

## 2026-10-09 - F426 Class Transfer import QAction accepted

The real Import Classes QAction reaches the file-dialog boundary with the ClassTransfer purpose, active database directory, and JSON filter. A scripted cancellation leaves the workspace unchanged. Existing controller tests already cover successful apply, so this slice isolates the missing QAction-to-controller connection. Independent focused CTest and QtTest passed; no production code changed.

## 2026-10-09 - F426 committed / F427 selected

F426 Class Transfer import QAction is committed as 249d57b11ce327923a6416d72e53e034b53cda3c. Executor and independent exact CTest passed 1/1 each; executor target QtTest passed 6/6 and independent selected-case QtTest passed 3/3. The real action reached the scoped file-dialog boundary and cancellation preserved workspace state. F427 was the current Schedule page Save As/PDF action, distinct from the
FileController profile Save As action that writes .tps files. Batch 21 reviews
started with F427 and surfaced ten provisional candidates after F428; F429 is next.

## 2026-10-09 - F427 Schedule Save As/PDF accepted

The real MainWindow saveCurrentPageAs QAction opens SchedulePrintDialog and the test clicks its Save As button. The fake picker request and real PDF are verified, including QPdfDocument validity and stable page/session/path/prompt state. Executor and independent focused CTest both passed 1/1; direct QtTest passed 5/5 and the selected case passed 3/3. Independent CMake regeneration took 220.7 seconds, then the exact Debug CTest passed; no focused process remains. No production change or full-suite run was needed. F427 is accepted and ready to commit.

Batch 21 was discovered while F427, the second-last known Batch 20 candidate, was current. Two bounded reviews surfaced ten provisional candidates after F428; F429 is recommended next once Batch 20 completes.

## 2026-10-09 - F427 committed / F428 selected

F427 Schedule Save As/PDF output is committed as bbdf10e85ba2c5a61066ed6fd32a7c8993c0f04e. Executor and independent focused CTest passed 1/1 each; direct target QtTest passed 5/5 and the new case passed 3/3. The real MainWindow action and SchedulePrintDialog button produced a valid PDF without changing page/session/path/prompt state. F428 is selected next: explicit cancellation of the separate database profile Save As action.

## 2026-10-09 - F428 canceled profile Save As accepted

The MainWindow cancellation case explicitly queues std::nullopt and triggers the real saveAsFile QAction. It verifies the .tps request and unchanged session/path/page/Sidebar/recent-file settings, with no destination or prompts. Executor and independent focused CTest passed 1/1; target QtTest passed 6/6 and the selected case 3/3. No production code changed; no full suite ran. F428 is accepted and ready to commit.

## 2026-10-09 - F428 committed / F429 selected

F428 canceled database profile Save As is committed as b82bddaa59c57d88f773370e94b3163df9055f33. Executor and independent exact CTest passed 1/1; direct target QtTest passed 6/6 and the selected case passed 3/3. Explicit cancellation preserved the open profile and settings. F429 Document Catalog PDF viewer Save As through MainWindow is selected from Batch 21.

## 2026-10-09 - F429 Document Catalog PDF Save As accepted

The new MainWindow test follows the rendered document leaf to a ready PdfViewerPage, triggers the real saveCurrentPageAs QAction, and verifies the GeneratedPdf save request, byte-identical catalog PDF copy, and QPdfDocument validity. Viewer/content/path/page/Sidebar state remains stable, no database is open, and no external URL is launched. Executor and independent CTest passed 1/1; target QtTest passed 4/4 and the selected case 3/3. One include was fixed and a nonfatal LNK4075 remained. No full suite ran. F429 is accepted and ready to commit.



## 2026-10-09 - F429 committed / F430 selected

F429 Document Catalog viewer Save As is committed as 19f6024d113dda41e7e7a2b41acf27961160462b with the six approved slice paths. Batch 21 remains active, with F430 Schedule Print QAction selected/current.

## 2026-10-09 - F430 acceptance matrix recorded

F430 covers the real MainWindow Print QAction through the Schedule print-options dialog. The test will inspect the Print-mode dialog and reject it before the print service or native printer UI, then assert stable workspace state and no picker or prompt. The focused target and CTest are recorded in the Phase 2 progress log; successful physical printing is outside this slice.



## 2026-10-09 - F430 Schedule Print QAction accepted

The real MainWindow Print QAction entered the Schedule Print options dialog and the test rejected it before printer service/native printer UI. Executor and independent Tester each passed the focused CTest 1/1, direct target QtTest 7/7, and selected case 3/3. Workspace state, action state, and empty picker/prompt queues were verified; no production code or full-suite run. F430 is accepted and ready to commit. F431 is next after the commit.



## 2026-10-09 - F430 committed / F431 selected


F430 is committed as aa7fca3761143a1ee0dce403995063799343b699 with exactly the six approved slice paths. The branch is ahead 15. Executor and independent focused CTest passed 1/1; direct QtTest passed 7/7; the selected case passed 3/3. F431 Import Teachers QAction through MainWindow, including the page-leave gate, is selected from Batch 21.



## 2026-10-09 - F431 acceptance matrix recorded


The real MainWindow Import Teachers QAction is the target. Reuse the existing My Workspace Details dirty-draft fixture to cover both page-leave choices: Cancel preserves the unsaved draft and blocks TeacherImportDialog; Discard restores the saved draft and enters TeacherImportDialog, then the test rejects it before file selection or apply. The acceptance matrix is in the Phase 2 progress log. Implementation has not started.



## 2026-10-09 - F431 Import Teachers QAction accepted


The real MainWindow QAction now has Cancel and Discard gate coverage. Cancel preserved a dirty Details draft and blocked TeacherImportDialog. Discard restored the persisted name and reached the actual dialog, which was rejected before file selection or apply. Executor and independent Tester each passed focused CTest 1/1, target QtTest 6/6, and each new case 3/3. No production/CMake change or full-suite run. F431 is accepted and ready to commit; F432 is next after commit.



## 2026-10-09 - F431 committed / F432 selected


F431 is committed as 2ac08388cf8b28258cefd03fa42de25f43c45891 with the six approved slice paths. Executor and independent focused CTest passed 1/1; target QtTest passed 6/6; each new case passed 3/3. F432 Export Classes QAction through its selection dialog and JSON picker is selected next in Batch 21.



## 2026-10-09 - F432 acceptance matrix recorded


F432 covers the real Export Classes QAction through the class selection dialog and JSON save picker. Seed an assigned teacher/class before MainWindow startup to enable the QAction, select the seeded ID in the actual dialog, then explicitly cancel the fake JSON picker. This verifies request metadata and silent cancellation without duplicating lower-level export serialization coverage. Implementation has not started.



## 2026-10-09 - F432 Export Classes QAction accepted


The real MainWindow Export Classes QAction opened the class selection dialog, selected the seeded teacher-assigned class, and reached the fake JSON save picker. Explicit null cancellation preserved the class/table and workspace state without writing a JSON file. Independent review found classTimes missing from the first snapshot assertions; the executor added size/day/startTime/endTime comparisons and final independent verification passed. Executor and independent focused CTest passed 1/1, target QtTest 7/7, selected case 3/3. No production/CMake change or full-suite run. F432 is accepted; F433 is next after commit.



## 2026-10-09 - F432 committed / F433 selected


F432 is committed as 4bfe3dc27c1c3adbc2ca3824d290cffec7d805c0 with the six approved slice paths. Executor and independent focused CTest passed 1/1, direct target QtTest 7/7, and selected case 3/3 after adding the full classTimes snapshot comparison. F433 New Teacher menu QAction is selected next in Batch 21.



## 2026-10-09 - F433 acceptance matrix recorded


The New Teacher menu QAction currently attempts a blank teacher create, receives required-name validation failure, and shows an Add Teacher warning before any row insert or navigation. F433 will characterize that real MainWindow action path without changing the blank-draft contract; F285 remains deferred pending clarification. The matrix is recorded in the Phase 2 progress log before implementation.



## 2026-10-09 - F433 New Teacher QAction accepted


The real New Teacher QAction currently attempts a blank create and captures the expected Add Teacher warning containing teacher.name.required, with no database row or navigation. This is characterization only; F285 remains deferred. Executor and independent CTest passed 1/1, target QtTest 8/8, selected case 3/3. No production/CMake change or full-suite run. F433 is accepted; F434 is next after commit.

## 2026-10-09 - F433 committed; F434 selected

F433 committed as b60c8025c937d8080395479b86dc30b84c75f160 (Phase2 - Cover New Teacher QAction validation warning (F433)); branch is ahead by 18. The commit contains the approved test plus five Phase 2 documentation paths. F434 Delete Teacher QAction confirmation through MainWindow is accepted and ready to commit; F435-F438 remain provisional. latest_session_work.md and unrelated %SystemDrive%/ remain outside slice commits.


## 2026-10-09 - F434 selection and acceptance matrix

Read-only discovery traced Delete Teacher QAction through SidebarController, the real RecordSelectionDialog, destructive confirmation, and repository deletion. F434 will automate the real chooser to select one seeded teacher and reject the fake destructive confirmation, then assert prompt details, retained teacher/survivor data, sidebar state, and stable MyWorkspace/session state. Target: ClassMngrMainWindowTeacherSidebarNavigationParityTests; exact CTest target matches. This covers the MainWindow action path; direct-controller tests already cover lower-level branches. Matrix is recorded before implementation.


## 2026-10-09 - F434 Delete Teacher QAction cancellation accepted

The real MainWindow action and chooser reached the destructive Delete Teacher confirmation; rejecting it preserved the target and survivor records, sidebar rows, MyWorkspace page/widget/selection, session, and path, with no extra prompt. The test includes a five-second modal watchdog and claims cancellation only. Executor and independent Tester passed focused target build, exact Debug CTest 1/1, direct target QtTest 6/6, and selected case 3/3. Executor used Ninja Debug after VS FileTracker errors; independent VS target build passed without retry. No production/CMake change or full-suite run. F434 is committed as 3fb5241347d14c2d9dfa94b5a25a6742d6ba28df; branch is ahead by 19.


## 2026-10-09 - F434 committed; F435 selected

F434 committed as 3fb5241347d14c2d9dfa94b5a25a6742d6ba28df (Phase2 - Cover Delete Teacher QAction cancellation (F434)); branch is ahead by 19. F435 Empty-state Open/New Profile button handoff through Banner, PageManager, and MainWindow is accepted and ready to commit. F436 Invalid UTF-8 document resource references is next after commit; F437-F438 remain provisional. latest_session_work.md and unrelated %SystemDrive%/ remain outside slice commits.


## 2026-10-09 - F435 Open/New Profile matrix

Explorer traced BasePage banner signals through PageManager to MainWindow QAction triggers and FileController. F435 will add separate Open and New success cases to the existing empty-state target. Open loads a seeded temporary profile through the TeacherProfile open picker; New creates a profile at a unique nonexistent temporary path through the TeacherProfile save picker. Both verify signal/action/picker handoff and loaded MyWorkspace Schedule state; New stays separate from Initial Setup. Build target and exact CTest: ClassMngrMainWindowInitialSetupEmptyStateNavigationTests. Matrix was recorded before implementation.


## 2026-10-09 - F435 banner Open/New Profile accepted

The actual empty-state Open and New Profile buttons traversed BasePage, PageManager, the matching MainWindow QAction, and the expected TeacherProfile file picker. Open consumed a seeded temp profile; New created/opened one at a fresh nonexistent temp path. Both reached MyWorkspace Schedule, selected the expected sidebar route, hid the banner, and avoided Initial Setup/prompt/opposite picker. F413 remains separate. Executor and independent Tester passed focused build, exact Debug CTest 1/1, full target QtTest 5/5, and each new case 3/3. No production/CMake change or full-suite run. F435 is committed as 998565115361bdd301f1d06ecc4beb2da7a519b9; branch is ahead by 20.


## 2026-10-09 - F435 committed; F436 selected

F435 committed as 998565115361bdd301f1d06ecc4beb2da7a519b9 (Phase2 - Cover Empty-state Open/New Profile button handoff (F435)); branch is ahead by 20. F436 Invalid UTF-8 document resource references is accepted and ready to commit. F437 is next after commit; F438 remains provisional. latest_session_work.md and unrelated %SystemDrive%/ remain outside slice commits.


## 2026-10-09 - F436 invalid UTF-8 matrix

Explorer and independent contract review support treating invalid UTF-8 as malformed input: InvalidInput, nonrecoverable, while NotFound remains for valid missing resources. Strict validation belongs at the resource-port boundary before path conversion and lease acquisition, covering primary and optional export references. Tests will preserve raw malformed bytes and assert the pack stays unmounted. Exact target/CTest: ClassMngrNextPlatformDocumentContentResourcePortTests. Matrix was recorded before implementation.


## 2026-10-09 - F436 invalid UTF-8 accepted

The port now rejects malformed UTF-8 before path conversion and resource-pack acquisition as nonrecoverable InvalidInput for both primary and export references. The focused test covers invalid continuation, truncation, overlong, surrogate, out-of-range, and standalone continuation bytes, with valid missing-resource behavior still NotFound. Executor and independent Tester passed the Ninja target build, exact CTest 1/1, full target QtTest 9/9, and selected case 3/3. No full-suite run. F436 is committed as c19e247f8066b546bfb71e9177e2413123309112; branch is ahead by 21.


## 2026-10-09 - F436 committed; F437 selected

F436 committed as c19e247f8066b546bfb71e9177e2413123309112 (Phase2 - Reject invalid UTF-8 document references (F436)); branch is ahead by 21. F437 Report worker event-post failure with a zero-capacity queue is accepted and ready to commit. F438 Optional typed occurrence IDs during repeat-series creation is selected/current, with read-only context discovery underway. Batch 21 remains active through F438; Batch 22 remains inactive.


## 2026-10-09 - F437 report worker event-post failure matrix

A zero-capacity ReportJobEventQueue rejects the worker terminal Failed event with Conflict. The bounded case uses an immediate failing callback, then checks lastResult preserves the post error, queue is empty, worker exits and joins, and coordinator remains Running without a pumped event. This follows the analogous Import worker case. Target/CTest: ClassMngrNextPlatformQtJobWorkerTests. Matrix was recorded before implementation; final implementation and independent verification passed. Batch 22 discovery is documented and inactive until Batch 21 completes.


## 2026-10-09 - Batch 22 discovery at F437

Two independent bounded reviews surfaced eight provisional MainWindow/worker integration candidates after F436, with evidence and targets recorded in the Phase 2 progress log. Batch 21 remains active with F438 current; Batch 22 activates after F438 commits. Same-path open remains unselected pending contract definition; successful New Teacher remains deferred under F285.


## 2026-10-09 - F437 report worker terminal-post rejection accepted

An immediate report failure with zero queue capacity rejects the terminal Failed event as Conflict. Worker lastResult retains the post error, the worker exits and joins, the queue is empty, pump consumes zero, and coordinator remains Running because no event was delivered. Executor and independent Tester passed the final Ninja build, exact CTest 1/1, target QtTest 12/12, and selected case 3/3. VS FileTracker blocked rebuilds in the other tree; final Ninja build had no warnings. No production/CMake change or full-suite run. F437 is committed as f355a1aa65d63bbf18ddeda3e8cfa7ff76c985d7; branch is ahead by 22.


## 2026-10-09 - F437 committed; F438 selected

F437 committed as f355a1aa65d63bbf18ddeda3e8cfa7ff76c985d7 (Phase2 - Cover Report worker terminal event-post failure (F437)); branch is ahead by 22. F438 Optional typed occurrence IDs during repeat-series creation is current; create-only acceptance matrix is recorded and implementation is pending. Batch 22 is discovered with eight provisional candidates and remains inactive until F438 commits.


## 2026-10-09 - F438 occurrence-ID matrix

A present CalendarEventId means update in the generic save request; repeat-series creation is create-only. The normal planner clears seed IDs, and calendar import rejects supplied IDs. F438 will reject every present occurrence ID as nonrecoverable InvalidInput before persistence, protecting an existing row and preventing partial series inserts. An integration test will seed a row, submit its ID on a later series occurrence, and verify the request fails, stored row remains unchanged, and count is stable; ID-less series creation stays covered. Target/CTest: ClassMngrNextPlatformApplicationServicesCalendarEventPortTests. Matrix recorded before implementation. Batch 22 remains inactive until F438 commits.


## 2026-10-09 - F438 repeat-series create-only IDs accepted

Series creation now rejects every occurrence with a supplied CalendarEventId as nonrecoverable InvalidInput during request validation, before persistence. A focused integration regression seeds a standalone event and supplies its ID on the second occurrence; both request and adapter return the validation error, all stored row fields and repeatSeriesId are unchanged, and row count remains one. Existing ID-less generated series creation remains covered. Executor and independent Tester passed the Ninja Debug target build, exact CTest 1/1, target QtTest 55/55, and selected case 3/3, with no compiler warnings. No full suite ran. F438 is accepted and ready to commit; after that commit Batch 21 completes and Batch 22 starts at F439 Delete Teacher confirmation success.


## 2026-10-09 - F438 committed; F439 selected

F438 committed as 0c2ceca6136a962f41130f52975bbab7315cd465 (Phase2 - Reject existing IDs in repeat-series creation (F438)); branch is ahead by 23. The exact seven approved paths were committed after the progress-log header path was corrected; diff checks passed. latest_session_work.md and unrelated %SystemDrive%/ remain outside the commit. Batch 21 is complete. Batch 22 is active with F439 Accept Delete Teacher confirmation success selected/current; bounded context discovery is underway. The remaining six Batch 22 candidates are provisional.


## 2026-10-09 - F439 MainWindow Delete Teacher success matrix

F439 will add a focused MainWindow success case beside F434 in mainwindow_teacher_sidebar_navigation_parity_tests.cpp. It will begin on MyWorkspace with no teacher sidebar row selected, create two unassigned teachers, trigger the real Delete Teacher QAction, select the target in the actual chooser using the established QTimer + five-second watchdog, and script PromptChoice::Destructive. Acceptance checks the confirmation request is correct and consumed, no warning is shown, only the target teacher disappears from repository/sidebar, the survivor is unchanged, and MyWorkspace page/widget plus session/path stay active. It does not exercise teacher deletion while a TeacherInfo page is active or class-assignment cleanup. Target/CTest: ClassMngrMainWindowTeacherSidebarNavigationParityTests. Matrix is recorded before implementation; no implementation has started.


## 2026-10-09 - F439 MainWindow Delete Teacher accepted

The F439 success case reuses the real MainWindow QAction and chooser. The target teacher was deleted from the repository and sidebar after the destructive confirmation; the unassigned survivor snapshot and leaf remained unchanged. No teacher row was selected, and MyWorkspace, the page/widget, service, database session, and path remained stable. The prompt was consumed with no extra warning/message. Executor and independent Tester passed the Ninja Debug target build, exact CTest 1/1, full target QtTest 7/7, and selected case 3/3. No compiler warnings; Qt offscreen/font notices only. No full suite or production/CMake change. F439 is accepted and ready to commit; F440 Export Classes JSON completion follows after commit.


## 2026-10-09 - F439 committed; F440 selected

F439 committed as a0c50d2dd37de11a4bfa91cfea044822cba7269f (Phase2 - Cover Delete Teacher QAction confirmation success (F439)); branch is ahead by 24. Only the test file and six approved documentation paths were committed; diff checks passed. latest_session_work.md and unrelated %SystemDrive%/ stay excluded. Batch 22 is active with F440 successful Export Classes JSON output selected/current for bounded discovery; F441-F446 remain provisional.


## 2026-10-09 - F440 successful Export Classes JSON matrix

F440 will add a success case beside F432’s picker cancellation in mainwindow_close_file_parity_tests.cpp. Trigger the real QAction and reuse the observer with its five-second modal watchdog; select exactly one of two seeded classes, then return a temporary .json path from FakeFileDialogService. Verify the saved JSON envelope (ClassMngr Classes, version 1, valid UTC export timestamp), one selected class with its class info and linked exported teacher, and absence of the unselected class. Assert the success information prompt carries count/path and no warning, while MyWorkspace, database session/path, and persisted classes stay unchanged. Avoid asserting a fixed timestamp or serializing DB IDs into generated keys; codec tests already cover full payload parity. Exact target/CTest: ClassMngrMainWindowCloseFileParityTests. Matrix recorded before implementation.


## 2026-10-09 - F440 Export Classes JSON success accepted

The F440 case exercised the real QAction, actual ClassExportDialog and JSON save path. It listed two classes, selected only the target, and verified the saved package metadata, UTC timestamp, target class information, linked teacher profile, and absence of the unselected class. The Export Classes information prompt reported count and output path without warnings. Database class names/count/rows/info, MyWorkspace and page identity, session/path remained unchanged. Executor and independent Tester passed target build, exact CTest 1/1, QtTest 9/9, and selected case 3/3. No compiler warnings; Qt font/offscreen notices only. No full suite or production/CMake change. F440 is accepted and ready to commit; F441 Import Classes apply follows.


## 2026-10-09 - F440 committed; F441 selected

F440 committed as 01d1559caae48eddcda739f4ea6f30dba8667e24 (Phase2 - Cover Export Classes JSON output (F440)); branch is ahead by 25. The exact six approved paths were committed; source changes were test-only and diff checks passed. latest_session_work.md and unrelated %SystemDrive%/ remain excluded. Batch 22 is active with F441 Import Classes QAction apply selected/current for bounded context discovery; F442-F446 remain provisional.


## 2026-10-09 - F441 Import Classes success matrix

F441 will exercise the real MainWindow Import Classes QAction through picker, review, and apply. Use an empty destination and one valid package class with no package teachers/empty teacher_ref; this keeps Import Teachers and replacement/update behavior out of scope. Script the JSON file picker and accept the real ClassImportDialog after confirming Create is selected, using timer automation plus a five-second watchdog. Assert one class with expected ClassInfo, no teachers and teacherId -1, no replacement/skip, an information summary for Created 1/replaced 0/skipped 0, and no warning. The Classes page should show the imported class selected in Details with the Classes sidebar route, while session/path remain stable. Target/CTest: ClassMngrMainWindowCloseFileParityTests. Matrix is recorded before implementation.


## 2026-10-09 - F441 Import Classes MainWindow apply accepted

The F441 test imported a valid one-class, no-teacher package through the actual QAction, JSON picker, review dialog, Create selection, and Apply button. The empty destination now contains one class with its complete expected ClassInfo and no teachers; the class remains unassigned. The exact completion summary is Created 1/replaced 0/skipped 0. The Classes page shows the imported ID in Details, sidebar route is classes, and session/path remain stable. Executor and independent Tester passed the focused target build, CTest 1/1, QtTest 10/10, and selected case 3/3. No compiler warnings; Qt font/offscreen notices only. No full suite or production/CMake change. F441 is accepted and ready to commit; F442 Import Teachers QAction apply follows.


## 2026-10-09 - F441 committed; F442 selected

F441 committed as 75a559ae7bad175cba106d0377bcf59c506cd247 (Phase2 - Cover Import Classes QAction apply success (F441)); branch is ahead by 26. The exact six approved paths were committed; source change was test-only and diff checks passed. latest_session_work.md and unrelated %SystemDrive%/ remain excluded. Batch 22 is active with F442 Import Teachers QAction apply selected/current for bounded context discovery; F443-F446 remain provisional.


## 2026-10-09 - F442 Import Teachers QAction apply matrix

F442 will use the known checked-in sectioned_review.xlsx fixture and an empty destination so the path exercises only creates, not matching/update behavior. Trigger the actual MainWindow Import Teachers QAction, browse via the fake file picker, wait for async validation, set M1 candidate 0 / M2 None / H1 All, then accept the real TeacherImportDialog. The fixture plan imports Korean Hong and Park, Native English Alex, and GS Taylor (2/1/1). Assert one browse request, enabled Valid File state, created counts with zero updated/unchanged, refreshed Korean sidebar entries, exact information summary/no warning or date confirmation, and stable MyWorkspace/session/path. Use the 15-second bounded modal/workbook-validation watchdog. Target/CTest: ClassMngrMainWindowManageCampusesParityTests. Matrix recorded before implementation.


## 2026-10-09 - F442 Import Teachers success accepted; pause after commit

F442 drove the real Import Teachers QAction, Browse picker, asynchronous workbook validation, review choices, and apply path from a fresh database. The checked-in workbook with M1 candidate 0 / M2 None / H1 All created Hong and Park, Alex, and Taylor (2 Korean, 1 Native English, 1 GS), with zero updates/unchanged; the Korean sidebar refreshed and the exact category summary was shown. No warning or date-confirmation prompt appeared, and MyWorkspace/session/path remained stable. Executor and independent Tester passed target build, exact CTest 1/1, target QtTest 7/7, and selected case 3/3. No compiler warnings; Qt font/offscreen notices only. No full suite or production/CMake change. F442 is accepted and ready to commit. Per user request, pause after its commit; F443 Delete Class QAction is next on resume and has not started.


## 2026-10-09 - F442 committed; user resumed at F443

F442 committed as 8cde28263d3c89cc5f3f610d031a1a536ad183d4 (Phase2 - Cover Import Teachers QAction apply success (F442)); branch is ahead by 27. Only the six approved paths were committed; source change was test-only and diff checks passed. latest_session_work.md and unrelated %SystemDrive%/ remain excluded. After the user-requested pause, work resumed at F443 Delete Class QAction; bounded discovery is underway. Batch 22 remains active and F444-F446 provisional.


## 2026-10-09 - F443 Delete Class MainWindow success matrix

F443 will add the missing real MainWindow QAction success route in mainwindow_schedule_testing_classes_handoff_parity_tests.cpp. Start with two unassigned classes, target selected/open in a clean Classes Details page, and target ClassInfo plus a schedule row. Drive the real class chooser with a five-second watchdog and accept the Delete Class confirmation. Verify target/class-info/schedule removal, sibling data preservation and fallback to the sibling in Details, Classes sidebar route, stable session/path, and no warning or unsaved prompt. Keep teacher assignments out of scope. The lower-level controller suite already covers cancellation and success/fallback. Target/CTest: ClassMngrMainWindowScheduleTestingClassesHandoffParityTests. Matrix recorded before implementation.


## 2026-10-09 - F443 Delete Class MainWindow success accepted

F443 committed as `1c03b326567cf52d808bc4c54b7a5e77021bb7bf`; branch is ahead by 28.

The real MainWindow Delete Class QAction and chooser deleted the target after the destructive prompt. Its class, info, and schedule rows were removed; the sibling's info stayed unchanged and it became active in Details. The Classes page/sidebar route and database session/path remained stable, with no warning or unsaved-change prompt. Executor and independent Tester passed target build, exact CTest 1/1, direct target QtTest 12 passes (10 cases plus setup/cleanup), and selected case 3/3. No compiler warnings; Qt font/offscreen notices only. No full suite or production/CMake change. F443 is accepted and ready to commit; F444 successful Open File QAction replacement follows.

## 2026-10-09 - F444 Open File replacement acceptance review

F444 committed as `8a21da618870ba4308415aaf5927fa1393af6e97`; branch is ahead by 29.

Two independent reviews confirmed F444 covers successful replacement of an already-open clean profile with a different profile, which is distinct from F435's empty-state banner handoff and the existing Open File cancellation tests. Acceptance is a real QAction and Teacher Profile picker transition from seeded profile A, initially on My Workspace Schedule, to distinct profile B, verifying the chooser request, target path and persisted data, My Workspace Schedule and Sidebar selection, and no warning or unsaved prompt. The existing `DatabaseSession` is reused in place, so pointer identity is not an assertion. Same-path opening, dirty replacement choices, and load failures stay out of scope. The focused class/target is `MainWindowOpenFileParityTests` / `ClassMngrMainWindowOpenFileParityTests`.

F444 is accepted and ready to commit. The focused target build passed without compiler warnings; the selected QtTest slot passed 3/3 functions (setup, test, cleanup), and exact filtered CTest passed 1/1. The final source review approved the Schedule starting state and assertions. Independent Tester could not inspect or run commands because process creation failed with `helper_unknown_error: setup refresh had errors`. One missing Qt font-directory runtime warning; no full suite.

## 2026-10-09 - F445 Save QAction persistence acceptance review

F445 committed as `d9180f1c465976dfdd707382a1e615108aed9387`; branch is ahead by 30.

F445 covers the MainWindow Save QAction commit effect, a distinct integration from service-level persistence coverage. Acceptance is a clean, file-backed profile already open on My Workspace Schedule with a current path, an explicit active-connection transaction containing a distinctive setting value, and the real Save QAction. Verify the value after persistence through a fresh connection/reopen, with path/page/sidebar stable and no Save As picker, warning, or unsaved prompt. An explicit transaction is necessary because an ordinary setting write auto-commits. Save As and commit-failure/error-reporting behavior stay out of scope. The local source map identifies `MainWindowSaveAsExportParityTests` / `ClassMngrMainWindowSaveAsExportParityTests`; the separate scope review used a default-branch snapshot after local process setup failed. F445 is accepted and ready to commit; F446 remains provisional.

F445 verification passed: target build without compiler warnings, selected QtTest 3/3 functions, full target QtTest 8/8 functions, and exact filtered CTest 1/1. Independent source review approved the transaction, real QAction, fresh-connection readback, and UI-state assertions. `SettingsService::save()` stalled before the action, so the fixture stages the unique setting with a prepared query on the same active connection; Save commits it and fresh `ApplicationServices` reads it. One missing Qt font-directory runtime warning; no full suite or production/CMake changes.

## 2026-10-09 - F446 Close-confirmation Save acceptance review

F446 committed as `4b34a3b8a15a062377a245607228097fb43ee46b`; branch is ahead by 31.

F446 is distinct from F445: it tests the Save decision inside `MainWindow::closeEvent()`, not the File → Save QAction. In the existing Manual Save My Details fixture, persist a known baseline personal name, edit it to a distinct draft name, script the fake unsaved-changes prompt as Save, and close the window. Verify the choice was issued, close was accepted with the window hidden and no second prompt, then reopen via fresh `ApplicationServices` and verify the exact draft replaced the baseline. Manual mode avoids autosave races. The current Cancel/Discard case remains; Save As, save failure, File → Save details, and unrelated fields are out of scope. Local source evidence maps the close path through PageManager to current-page `saveChanges()` and My Details `saveMyInfoInternal()`; target is `MainWindowExitConfirmationParityTests` / `ClassMngrMainWindowExitConfirmationParityTests`. A second scope review relied on the supplied plan excerpt due local source-access failure. F446 is accepted and ready to commit.

F446 verification passed: target build without compiler warnings, selected QtTest 3 pass incidents (setup, slot, cleanup), full target QtTest 4 pass incidents (two slots, setup, cleanup), and exact filtered CTest 1/1. Independent source re-review approved the final test. One Qt font-directory warning; no full suite or production/CMake changes. The first wrapped CTest regex invocation matched no tests because cmd.exe retained quotes; rerunning with the regex passed 1/1.

## 2026-10-09 - F447 New Class QAction acceptance review

F447 committed as `d5b130bd0558146185ebf4cdeab885bde6956fec`; branch is ahead by 32.

Two independent local reviews identified the top-level New Class QAction as a distinct MainWindow wiring gap. Existing context-menu coverage tests the same `addClass()` handler through a different Sidebar action. Acceptance is a real `window.actions().newClass` trigger from a clean open profile, followed by verification that one new persisted class is selected on Details, Classes is selected in the Sidebar, the session/path stay stable, and no warning or prompt appears. Failure paths, dirty-page confirmation, no-database behavior, and unrelated fields are excluded. Focused class/target: `MainWindowClassesSidebarRootNavigationTests` / `ClassMngrMainWindowClassesSidebarRootNavigationTests`. F447 is committed as `d5b130bd0558146185ebf4cdeab885bde6956fec`; creation/read failures remain deferred under F298.

F447 verification passed: target build without compiler warnings, selected QtTest 3 pass incidents (setup, slot, cleanup), full target QtTest 5 pass incidents (three slots, setup, cleanup), and exact filtered CTest 1/1. Independent source review approved the action wiring, count and persisted-ID assertion, Details route, and Sidebar/session/path checks. One missing Qt font-directory warning and offscreen `raise()`/keyboard-grab notices occurred; tests passed. No full project suite or production/CMake changes.

## 2026-10-09 - F448 New File QAction open-profile acceptance review

Two independent local reviews found that F435 exercises New File only from the no-database state, while F444 replaces an open profile through Open File. The MainWindow New File QAction has no successful replacement test for an already-open profile; lower-level lifecycle tests do not cover this UI route.

The acceptance matrix starts with a clean file-backed profile A on My Workspace Schedule, selects a unique nonexistent `.tps` destination B, and triggers the real `window.actions().newFile`. It verifies the picker uses the active profile directory, B is created and active with an open session, A remains on disk with its seeded name and campus unchanged, the Schedule/Sidebar route is correct, and no prompt, warning, or modal remains. Session pointer identity is excluded. Dirty-page choices, cancellation, existing-target overwrite, Initial Setup, and failures remain out of scope. Focused class/target: `MainWindowCloseFileParityTests` / `ClassMngrMainWindowCloseFileParityTests`. Batch 24 is active; matrix defined before implementation.

## 2026-10-09 - F448 New File QAction success accepted

The F448 test opens a clean profile A, triggers the real New File QAction, and creates B at a unique destination. A’s seeded name and campus remain unchanged when reopened through a fresh service, while B is still active and open on My Workspace Schedule with the `my_workspace` Sidebar selection. Picker metadata and the absence of prompts or modal warnings are asserted.

The `ClassMngrMainWindowCloseFileParityTests` target built successfully; the selected slot passed 3 incidents (setup, test, cleanup), filtered CTest passed 1/1, and `git diff --check` was clean. The independent source reviewer approved the revised assertions but could not launch tests because of `helper_unknown_error: setup refresh had errors`; the executor ran the updated build and focused verification. One Qt font-directory warning appeared. No full suite or production/CMake change. F448 is accepted; commit metadata and next-slice discovery are recorded below.

## 2026-10-09 - F448 committed; F449 discovery started

F448 committed as `f825a388db1897bc42cacf43c488850fa48a9de8` (`Phase2 - Cover New File QAction open-profile success (F448)`); the branch is ahead by 33. Exactly the six approved paths were committed, the diff check was clean, and `agent_docs/latest_session_work.md` plus `%SystemDrive%/` remained outside. Batch 24 is complete. Batch 25 is active; two independent read-only reviews selected the Exit QAction close-confirmation handoff as F449 (acceptance matrix follows).

## 2026-10-09 - F449 Exit QAction close-confirmation acceptance review

Two independent read-only reviews found that `ActionRegistry::exitApp` is created and added to the File menu but has no project-level signal connection. Existing close-confirmation tests exercise `window.close()` directly and do not reference `exitApp`, so the menu action does not reach the guarded close path.

Acceptance: Seed a file-backed profile with a persisted baseline personal name, open it cleanly in Manual Save mode, and change the name editor to a distinct unsaved draft. Script the close choices as Cancel then Discard, and trigger the real `window.actions().exitApp` each time. After Cancel, verify the window remains visible, the draft remains unsaved, the active session/path remain, and the close prompt was recorded. After Discard, verify the window is hidden, the second prompt was consumed, and fresh-service readback still returns the baseline name.

Source map: `ActionRegistry::exitApp` -> File menu -> `MainWindow::close()` -> `MainWindow::closeEvent()` -> `confirmCurrentPageCanLeave(true)`. Production connection belongs in MainWindow action/controller wiring. Focused class/target/CTest: `MainWindowExitConfirmationParityTests` / `ClassMngrMainWindowExitConfirmationParityTests`. Exclude F446 Save behavior, OS shutdown, and multi-window behavior. Matrix recorded before implementation. F449 acceptance is recorded; commit metadata and next-slice discovery follow.

## 2026-10-09 - F449 Exit QAction success accepted

The File menu Exit QAction now calls `MainWindow::close()`, so it follows the existing `closeEvent` unsaved-changes guard. The new test drives Cancel then Discard through the QAction: Cancel keeps the dirty draft, active session/path, page, and window; Discard hides the window, and a fresh-service readback confirms the persisted baseline remains unchanged. Existing direct close tests and F446 Save coverage remain intact.

Verification: Ninja target build passed; the exact QtTest slot passed 3 incidents (setup, test, cleanup); full target QtTest passed 5 incidents (three test slots plus setup/cleanup); filtered CTest passed 1/1; `git diff --check` is clean. An MSBuild attempt hit an environment FileTracker access-denied error, so verification used the existing Ninja build with initialized MSVC. Independent Tester reran the slot, full target, and filtered CTest successfully and approved the source scope. Qt reported the known missing-font-directory warning and LF-to-CRLF notices; no whitespace errors. No full project suite or CMake changes. F449 acceptance is recorded; commit metadata and next-slice discovery follow.

## 2026-10-09 - F449 committed; F450 discovery started

F449 committed as `4e42a5a9cbe5c261fb78b2e39a98f81c93ef0372` (`Phase2 - Cover Exit QAction close-confirmation handoff (F449)`); the branch is ahead by 34. Exactly the seven approved paths were committed and the diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 25 is complete; Batch 26 is active with bounded read-only QAction coverage discovery underway. No F450 candidate is selected yet.

## 2026-10-09 - F450 Undo QAction focused-editor acceptance review

Two independent read-only reviews found no direct MainWindow Edit QAction coverage. Existing local QUndoStack/component tests do not exercise ActionRegistry wiring or focus-based dispatch through the MainWindow. Undo is a bounded, deterministic path through `EditController` and a personal-details `QLineEdit`.

Acceptance: Seed a file-backed profile with a persisted baseline personal name, start MainWindow on My Workspace Details, focus the name editor, and change the text with keyboard events (`Ctrl+A` plus QTest key typing) so QLineEdit records undo history. Do not call `setText` for the draft. Verify the real `window.actions().undo` is enabled, trigger it, and verify the editor returns to the baseline.

Source map: `ActionRegistry::undo` -> Edit menu -> `MainWindow::connectControllers()` / `EditController::connectActions()` -> focus-based `EditController::undo()` dispatch -> `QLineEdit::undo()`. Dedicated test/target/CTest: `mainwindow_edit_action_parity_tests.cpp` / `MainWindowEditActionParityTests` / `ClassMngrMainWindowEditActionParityTests`, registered in `cmake/tests/pages_and_output.cmake`. Exclude Redo, clipboard actions, read-only widgets, and other editor types. Matrix recorded before implementation; Batch 26 is active.

## 2026-10-09 - F450 Undo QAction success accepted

The dedicated MainWindow test seeds a file-backed baseline personal name, opens My Workspace Details, focuses the name editor, enters a draft through Ctrl+A and QTest keyboard input, confirms the real Undo QAction is enabled, triggers it, and verifies the baseline value is restored. The test covers EditController focus dispatch to QLineEdit undo; no repository write is expected from this local editor operation. Independent source review approved the test and target registration.

Verification: the Ninja target build passed (the independent rerun reported no work to do); the exact QtTest slot and full target each passed 3 incidents (setup, test, cleanup) with zero failures; filtered CTest passed 1/1 using the generated registration `ClassMngrMainWindowEditActionParityTests`; `git diff --check` is clean. Both direct test runs emitted the existing Qt missing-font-directory warning, and bundled Inter/Pretendard fonts loaded. Initial MSVC attempts needed CMake regeneration and `VsDevCmd` initialization; the final executor and independent runs passed with that environment. No full project suite or production-code changes. F450 is accepted; commit metadata and next-slice discovery follow.

## 2026-10-09 - F450 committed; F451 discovery started

F450 committed as `e39852c8a9cef33c80d684dd0e63e18d19a6acf7` (`Phase2 - Cover Undo QAction focused-editor dispatch (F450)`); the branch is ahead by 35. Exactly the seven approved files were committed and the commit diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 26 is complete. Batch 27 is active; two independent read-only reviews are beginning bounded MainWindow QAction coverage discovery for F451. No candidate is selected yet.

## 2026-10-09 - F451 Redo QAction focused-editor acceptance review

Two independent reviews found no direct MainWindow Redo QAction coverage. Existing update-controller tests do not exercise Edit action focus dispatch, and the committed F450 test verifies only Undo. Redo is a bounded continuation of the same focused QLineEdit path in EditController.

Acceptance: In the file-backed profile and My Workspace Details setup used by `MainWindowEditActionParityTests`, focus the personal-name editor, and create a distinct draft using keyboard input (`Ctrl+A` and QTest key typing). Trigger the real Undo QAction and verify the baseline value returns. Then verify the real Redo QAction is available, trigger it, and verify the draft value returns while the editor remains focused. Keep the operation local to the editor; do not call `setText` for the draft.

Source map: `ActionRegistry::redo` -> Edit menu -> MainWindow/EditController wiring -> focus-based dispatch -> `QLineEdit::redo()`. Test class/target/CTest remain `MainWindowEditActionParityTests` / `ClassMngrMainWindowEditActionParityTests`, already registered in `cmake/tests/pages_and_output.cmake`; add the focused Redo behavior to `mainwindow_edit_action_parity_tests.cpp`. Exclude Cut/Copy/Paste, read-only widgets, other editor types, and update-controller behavior. Matrix recorded before implementation; Batch 27 is active.

## 2026-10-09 - F451 Redo QAction success accepted

The existing focused MainWindow edit-action case now covers the paired path: after keyboard-entering a distinct draft in the focused personal-name QLineEdit, it triggers the real Undo QAction and verifies the persisted baseline value returns, then triggers the real Redo QAction and verifies the draft returns while the editor retains focus. The test slot is `undoAndRedoActionsRestoreAndReapplyPersonalNameInFocusedLineEdit`; this extends the F450 focused-editor test without production or CMake changes.

Verification: the Ninja target build passed; the exact slot passed 3 incidents (setup, test, cleanup) with zero failures, and verbose output confirmed the slot ran; full target QtTest passed 3 incidents; filtered CTest `ClassMngrMainWindowEditActionParityTests` passed 1/1; `git diff --check` is clean. The direct runs emitted Qt’s known missing-font-directory warning; bundled Inter and Pretendard fonts loaded. No full project suite. F451 is accepted; commit metadata and next-slice discovery follow.

## 2026-10-09 - F451 committed; F452 discovery started

F451 committed as `3137d522796365e81d4c3f99e341aacaf83cc392` (`Phase2 - Cover Redo QAction focused-editor dispatch (F451)`); the branch is ahead by 36. Exactly the six approved files were committed and the commit diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 27 is complete. Batch 28 is active; two independent read-only reviews are beginning bounded QAction coverage discovery for F452. No candidate is selected yet.

## 2026-10-09 - F452 Paste QAction focused-editor acceptance review

Two independent reviews found no direct MainWindow Cut/Copy/Paste QAction tests. Paste is selected for its distinct clipboard-dependent enablement path and dispatch to the focused editor.

Acceptance: In `MainWindowEditActionParityTests`, snapshot the current clipboard MIME data and restore it during cleanup, including early assertion exits. Open the file-backed profile on My Workspace Details and focus the personal-name QLineEdit. Clear clipboard content and verify the real Paste QAction is disabled; then set non-empty text, wait for the QAction to enable, select the editor content using Ctrl+A, trigger the actual Paste QAction, and verify the editor contains the clipboard text and retains focus. Avoid `setText` for the edit operation.

Source map: ActionRegistry::paste -> Edit menu -> MainWindow/EditController wiring -> clipboard-change/focus action-state refresh -> focused QLineEdit::paste(). Test class/target/CTest remain `MainWindowEditActionParityTests` / `ClassMngrMainWindowEditActionParityTests`, already registered in `cmake/tests/pages_and_output.cmake`; add coverage in `mainwindow_edit_action_parity_tests.cpp`. Exclude Cut/Copy, read-only gating, non-text clipboard formats, and other editor types. Matrix recorded before implementation; Batch 28 is active.

## 2026-10-09 - F452 Paste QAction success accepted

Added `pasteActionReplacesSelectedPersonalNameFromClipboard()` to the existing MainWindow edit-action target. The test snapshots clipboard MIME formats and payloads in an RAII restorer, opens a file-backed profile on My Workspace Details, focuses the personal-name QLineEdit, verifies Paste is disabled with empty clipboard content, sets clipboard text and waits for the action to enable, selects the existing name with Ctrl+A, triggers the actual Paste QAction, and verifies replacement text and retained focus. The clipboard is restored on normal and assertion-failure exits. No production or CMake changes.

Verification: Ninja target build passed; the exact slot passed 3 incidents (setup, test, cleanup) with zero failures; full target QtTest passed 4 incidents; filtered CTest `ClassMngrMainWindowEditActionParityTests` passed 1/1; `git diff --check` is clean. Independent verification captured the selected-slot output in a temp log after initializing VS 18 and Qt 6.12 paths. Qt emitted the known missing-font-directory warning; bundled Inter and Pretendard fonts loaded. No full project suite. F452 is accepted; commit metadata and next-slice discovery follow.

## 2026-10-09 - F452 committed; F453 discovery started

F452 committed as `249dd38b8b3ee2223292c99ec740f2470e20e5e9` (`Phase2 - Cover Paste QAction focused-editor dispatch (F452)`); the branch is ahead by 37. Exactly the six approved paths were committed and the commit diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 28 is complete. Batch 29 is active; two independent read-only reviews are beginning bounded QAction coverage discovery for F453. No candidate is selected yet.

## 2026-10-09 - F453 Cut QAction focused-editor acceptance review

Two independent reviews found no MainWindow Cut or Copy QAction coverage after the Undo/Redo/Paste cases. Select Cut as a focused text-edit action with a direct clipboard result.

Acceptance: Add a dedicated Cut slot to `MainWindowEditActionParityTests`, reusing the file-backed personal-name editor setup and `ClipboardMimeDataRestorer`. Open My Workspace Details, focus the personal-name QLineEdit, seed a distinct sentinel clipboard value, select the existing name with Ctrl+A and verify it is the selected text. Confirm the real Cut QAction is enabled, trigger it, and verify the editor is empty, clipboard text now equals the selected baseline name, focus remains in the editor, and the active workspace stays open without a prompt. Do not use `setText` for the edit.

Source map: ActionRegistry::cut -> Edit menu -> MainWindow/EditController wiring -> focused-widget dispatch with editable-text guard -> QLineEdit::cut(). Test class/target/CTest remain `MainWindowEditActionParityTests` / `ClassMngrMainWindowEditActionParityTests`, already registered in `cmake/tests/pages_and_output.cmake`; add the focused Cut case to `mainwindow_edit_action_parity_tests.cpp`. Exclude Copy, read-only gating, other editor types, persistence assertions, and non-text clipboard formats. Matrix recorded before implementation; Batch 29 is active.

## 2026-10-09 - F453 Cut QAction success accepted

Added `cutActionCopiesSelectedPersonalNameFromFocusedEditor()` to the existing MainWindow edit-action test. It seeds a file-backed baseline, snapshots/restores clipboard MIME data, focuses the personal-name QLineEdit in My Workspace Details, puts sentinel text on the clipboard, selects the baseline using Ctrl+A, and triggers the real enabled Cut QAction. Assertions verify the editor clears, the clipboard now contains the selected name, focus remains, the workspace stays open on Details, and no modal prompt appears. Manual Save mode keeps the intentionally empty field local during the test. No `setText` editing, production or CMake changes.

Verification: Ninja target build passed; the exact Cut slot passed 3 incidents (setup, test, cleanup) with zero failures; full target QtTest passed 5 incidents; filtered CTest `ClassMngrMainWindowEditActionParityTests` passed 1/1; `git diff --check` is clean. Independent rerun used the CTest offscreen Qt environment and captured both QtTest logs. The first direct launch without Qt runtime settings failed to start; after setting VS 18/Qt 6.12 paths and offscreen mode, the selected slot and full target passed. Qt emitted the known missing-font-directory warning; bundled Inter and Pretendard fonts loaded. No full project suite. F453 is accepted; commit metadata and next-slice discovery follow.

## 2026-10-09 - F453 committed; F454 discovery started

F453 committed as `4e9b3d6d79aff3c43b943aec8e165a6ab16ca8a9` (`Phase2 - Cover Cut QAction focused-editor dispatch (F453)`); the branch is ahead by 38. Exactly the six approved paths were committed and the commit diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 29 is complete. Batch 30 is active; two independent read-only reviews are beginning bounded QAction coverage discovery for F454. No candidate is selected yet.

## 2026-10-09 - F454 Copy QAction focused-editor acceptance review

Two independent reviews found no direct MainWindow Copy QAction coverage after Undo/Redo, Paste, and Cut. Copy is a bounded focused-widget action that should update the clipboard while preserving editor and workspace state.

Acceptance: Add a dedicated Copy slot to `MainWindowEditActionParityTests`, reusing the file-backed personal-name editor setup and `ClipboardMimeDataRestorer`. Open My Workspace Details, focus the personal-name QLineEdit, seed a sentinel clipboard value, select the baseline name using Ctrl+A, and verify the selection. Confirm the real Copy QAction is enabled, trigger it, then verify clipboard text equals the selected name while editor text, editor focus, active workspace/path, and Details route remain unchanged with no modal prompt. Restore clipboard MIME data on all exits. Exclude empty selection behavior, keyboard-shortcut dispatch, other editor types, and production/CMake changes.

Source map: ActionRegistry::copy -> Edit menu -> MainWindow/EditController wiring -> focused-widget dispatch -> QLineEdit::copy(). Test class/target/CTest remain `MainWindowEditActionParityTests` / `ClassMngrMainWindowEditActionParityTests`, already registered in `cmake/tests/pages_and_output.cmake`; add the focused Copy case in `mainwindow_edit_action_parity_tests.cpp`. Matrix recorded before implementation; Batch 30 is active.

## 2026-10-09 - F454 Copy QAction success accepted

Added `copyActionCopiesSelectedPersonalNameFromFocusedEditor()` to the existing MainWindow edit-action test. It snapshots clipboard MIME formats/payloads, opens a file-backed profile on My Workspace Details, focuses the personal-name QLineEdit, seeds sentinel clipboard text, selects the baseline with Ctrl+A, and triggers the real enabled Copy QAction. Assertions verify the clipboard receives the selection, the editor value/focus and selection stay unchanged, the workspace/path and Details route remain active, and no modal prompt appears. The test explicitly restores clipboard data and verifies the original formats and payload bytes; the RAII destructor remains a fallback on early assertion returns. No production or CMake changes.

Verification: Ninja target build passed; the exact Copy slot passed 3 incidents (setup, test, cleanup) with zero failures; full target QtTest passed 6 incidents; filtered CTest `ClassMngrMainWindowEditActionParityTests` passed 1/1; `git diff --check` is clean. Independent rerun confirmed the selection and clipboard restoration assertions. Qt emitted the known missing-font-directory warning; bundled fonts loaded. No full project suite. F454 is accepted; commit metadata and next-slice discovery follow.

## 2026-10-09 - F454 committed; F455 discovery started

F454 committed as `dd73b6d9c536809f05d38487ecca4bc873b9ea86` (`Phase2 - Cover Copy QAction focused-editor dispatch (F454)`); the branch is ahead by 39. Exactly the six approved paths were committed and the commit diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 30 is complete. Batch 31 is active; two independent read-only reviews are beginning bounded QAction coverage discovery for F455. No candidate is selected yet.

## 2026-10-09 - F455 About QAction modal-handoff acceptance review

Independent Help-action reviews found no About QAction test. Select a bounded modal-handoff case that verifies the actual Help QAction opens the parented AboutDialog.

Acceptance: In a dedicated offscreen `MainWindowAboutActionParityTests` target, isolate `CLASSMNGR_SETTINGS_ROOT` in a temporary directory because DialogShell persists geometry. Construct and show MainWindow without an update controller; verify the real About QAction exists and is enabled. Queue a zero-delay timer before triggering it; while the QAction’s modal `exec()` is active, capture that `QApplication::activeModalWidget()` is an AboutDialog, it is visible/modal and parented to the MainWindow, then close it so the trigger returns. Afterward verify the observations, MainWindow remains visible, and no modal widget remains.

Source map: ActionRegistry::about -> Help menu -> MainWindow action connection -> `AboutDialog dialog(this); dialog.exec()`. Test file/target/CTest: `mainwindow_about_action_parity_tests.cpp` / `MainWindowAboutActionParityTests` / `ClassMngrMainWindowAboutActionParityTests`, registered in `cmake/tests/pages_and_output.cmake` with the neighboring MainWindow Qt/resource setup. Exclude license buttons/content, external links, and Check for Updates/network/download behavior. Matrix recorded before implementation; Batch 31 is active.

## 2026-10-09 - F455 About QAction modal handoff accepted

Added `aboutActionShowsModalAboutDialogAndReturnsToMainWindow()` and the dedicated `MainWindowAboutActionParity` target/`ClassMngrMainWindowAboutActionParityTests` registration. The test isolates settings in QTemporaryDir, creates a MainWindow without an update controller, and triggers the actual enabled About QAction. A zero-delay timer captures the visible modal AboutDialog, confirms it is modal and parented to MainWindow, then closes it. A mismatch branch closes any active modal and child AboutDialog so the test reaches its assertions instead of hanging. After the trigger, assertions verify the observations, MainWindow visibility, and no remaining modal. No production changes.

Verification: Ninja target build passed; the exact QtTest slot and full target each passed 3 incidents (setup, test, cleanup) with zero failures; filtered CTest `ClassMngrMainWindowAboutActionParityTests` passed 1/1; `git diff --check` is clean. Independent rerun confirmed the exact slot and modal assertions. An initial direct shell launch hung during QApplication startup due to missing Qt runtime settings; the final run used VS 18/Qt 6.12 and offscreen settings. CMake emitted pre-existing missing-Vulkan and object-path warnings; Qt emitted the known missing-font-directory warning while bundled Inter/Pretendard fonts loaded. No full project suite. F455 is accepted; commit metadata and next-slice discovery follow.

## 2026-10-09 - F455 committed; F456 discovery started

F455 committed as `76c67663cda5604353b4ba25e54c97160f6daf90` (`Phase2 - Cover About QAction modal handoff (F455)`); the branch is ahead by 40. Exactly the seven approved paths were committed and the commit diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 31 is complete. Batch 32 is active; two independent read-only reviews are beginning bounded QAction coverage discovery for F456. No candidate is selected yet.

## 2026-10-09 - F456 Check for Updates QAction acceptance scope

Cover the real enabled `window.actions().checkForUpdates` QAction and its handoff to a visible, manual `UpdateDialog`. Inject an empty `UpdateConfiguration` so checking fails deterministically before network access. Assert exactly one `GitHub releases API URL is not configured.` failure, a visible MainWindow-parented dialog with `automaticUpdatePrompt == false`, the expected failure title/details and enabled “Try Again” button, and that MainWindow remains visible. Isolate startup cleanup by setting `TMP`, `TEMP`, and `TMPDIR` to a dedicated CTest build-directory root and asserting `QStandardPaths::TempLocation` resolves there before triggering the action; create that private root before constructing the settings `QTemporaryDir`. No production changes or successful online update behavior are in scope.

## 2026-10-09 - F456 Check for Updates QAction accepted

Added `mainwindow_check_for_updates_action_parity_tests.cpp` and the dedicated `MainWindowCheckForUpdatesActionParity` CTest target, including keyboard resources/translations and the isolated temp environment. The actual QAction opened the manual update dialog; the test observed one configuration failure, verified the failure UI and retry action, and confirmed MainWindow remained visible. Because the service configuration has no API URL, the action makes no network request; startup cleanup resolves only inside the test-specific temp root. No production changes.

Verification: the Ninja target build passed under Visual Studio 18 with Qt 6.12; focused CTest `ClassMngrMainWindowCheckForUpdatesActionParityTests` passed 1/1; direct full-target and selected-slot QtTest runs each passed 3/0; an independent tester reran CTest and confirmed temp isolation and action-path assertions. Qt emitted the existing missing-font-directory and offscreen size/raise warnings while bundled fonts loaded. No full project suite. F456 is accepted and ready to commit; Batch 32 remains active until commit.

## 2026-10-09 - F456 committed; F457 discovery started

F456 committed as `d90def93f7947d0f031dc6f37a8a4a491f4d3718` (`Phase2 - Cover Check for Updates QAction manual handoff (F456)`); the branch was ahead by 41. Exactly the seven approved paths were committed and the cached diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 32 is complete. Batch 33 is active with F457 selected for implementation.

## 2026-10-09 - F457 Font Size QAction accepted

The next bounded QAction gap was the MainWindow Font Size option. Existing tests cover persistence and visual effects by calling `OptionState::set(...)`, but do not trigger MainWindow's Font Size QAction. The new offscreen parity target isolates settings, uses an English/Normal baseline, disables recent-database loading, then triggers Large and checks state, saved preference, FontManager offset, and QApplication font size. An RAII guard restores the original app font and static offset after MainWindow destruction, including assertion-return paths. No production behavior changed.

Verification passed with VS18/Qt6.12/Ninja: focused target build succeeded; focused CTest passed 1/1; direct QtTest and independent rerun passed setup/test/cleanup (3/0 each); independent focused CTest passed 1/1; `git diff --check` is clean. Reconfigure warnings noted pre-existing long object paths and missing optional Vulkan headers; Qt's bundled `lib/fonts` path warning remained while Inter/Pretendard loaded. No full suite was run. F457 is accepted; commit transition follows.

## 2026-10-09 - F457 committed; F458 discovery started

F457, `Phase2 - Cover Font Size QAction application handoff (F457)`, committed as `719efeacb6e5a8533f2dd45f6fb0fdfe152c0714` on `Qt-Rewrite` (42 commits ahead of origin). Exactly seven scoped paths were committed and the cached diff check was clean. Batch 33 is complete; Batch 34 is active with F458 Theme QAction coverage selected for implementation. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain excluded user changes.

## 2026-10-09 - F458 Theme QAction accepted

Theme was the next uncovered MainWindow option QAction with a direct visible effect; existing tests used `themeState->set(...)` but did not exercise the QAction-to-ThemeController path. The new test isolates settings, injects a Light ThemeService, triggers Dark, and checks action/state/persistence plus service theme, application Window palette, and MainWindow theme property. It then triggers Light and verifies the restored state and presentation. A cleanup guard now captures the application palette/stylesheet before applying Light and restores them while MainWindow and its service remain alive, including assertion-return paths. Save mode remains an alternative with broader autosave/timer effects. No production changes.

Verification passed with VS18/Qt6.12/Ninja: focused target build succeeded; focused CTest passed 1/1; independent direct QtTest and CTest reruns passed (3/0 incidents and 1/1 CTest); `git diff --check` and untracked-source whitespace checks are clean. CMake reported unrelated long object paths during reconfigure; Qt's missing bundled `lib/fonts` directory warning remained while packaged Inter/Pretendard loaded. No full suite. F458 is accepted; commit transition follows.

## 2026-10-09 - F458 committed; F459 discovery started

F458, `Phase2 - Cover Theme QAction application handoff (F458)`, committed as `2ec351a805af2064a865c34b76b46f60df80acdc` on `Qt-Rewrite` (43 commits ahead of origin). Exactly seven scoped paths were committed and the cached diff check was clean. Batch 34 is complete; Batch 35 is active with F459 Document Viewer Background QAction coverage selected for implementation. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain excluded user changes.

## 2026-10-09 - F459 Document Viewer Background QAction accepted

The next gap was the MainWindow Document Viewer Background option. Existing tests exercise preference persistence through `OptionState::set(...)` but do not trigger the MainWindow QAction. The new offscreen test creates a blank viewer via PageManager, triggers Black, and checks the actual action/state, persisted value, viewer/viewport properties, and palette color before restoring Default. This gives a direct presentation assertion without loading a PDF or creating a database. Save Mode remains uncovered but crosses into autosave state and timer behavior. No production changes.

Verification passed with VS18/Qt6.12/Ninja: focused target build succeeded; focused CTest passed 1/1; independent direct QtTest and CTest reruns passed (3/0 incidents and 1/1 CTest); `git diff --check` and untracked-source whitespace/conflict checks are clean. CMake reported unrelated long object paths during reconfigure; Qt's missing bundled `lib/fonts` warning remained while packaged Inter/Pretendard loaded. No full suite. F459 is accepted and ready to commit; Batch 35 remains active until commit.

## 2026-10-09 - F459 committed; F460 discovery complete

F459 committed as `cba31503f0bb5accf68de2032131db984403deff` (`Phase2 - Cover Document Viewer Background QAction parity (F459)`); `Qt-Rewrite` is 44 commits ahead of origin. Exactly seven scoped paths were committed and the cached diff check was clean. Batch 35 is complete. Two independent read-only reviews identified Page Spacing QAction parity as the next bounded gap: existing coverage sets the option directly and does not trigger the MainWindow action. Batch 36 is active with F460 selected. The pre-existing changes to `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain excluded.

## 2026-10-09 - F460 Document Viewer Page Spacing QAction acceptance

Acceptance: Add a dedicated offscreen MainWindow parity target that isolates settings, disables recent-database loading, and creates a blank viewer using `pageManager()->ensurePdfViewerPage()`. Trigger the actual Large then Small Page Spacing actions. Check that each is enabled and selected, that the option state and persisted preference match Large/Small, and that the public `QPdfView::pageSpacing()` accessor reports 32/8 pixels. Use RAII to restore Small while MainWindow remains alive even if a QtTest assertion returns early. No PDF load, database, network, autosave timer, production change, or user-file access.

Source map: ActionRegistry Page Spacing options -> Documents menu -> MainWindow action connection -> PageManager -> `PdfViewerPage::setDocumentPageSpacing()` -> `QPdfView::setPageSpacing()`. The focused test will be registered in `cmake/tests/pages_and_output.cmake`. Batch 36/F460 is selected; implementation, independent verification, and commit are pending.

## 2026-10-09 - F460 Document Viewer Page Spacing QAction accepted

Implementation added a dedicated `MainWindowDocumentViewerPageSpacingActionParity` target and test. The test creates a blank PDF viewer, triggers Large then Small through the actual MainWindow actions, and checks action selection, option state, persisted values, and the public QPdfView spacing values (32 and 8 pixels). A scoped restorer changes the option back to Small while MainWindow and the viewer still exist, including QtTest early-return paths. Settings are temporary and recent-database loading is disabled; no PDF is opened and there is no database, network, or autosave activity. No production code changed.

Verification passed: VS18/Qt6.12/Ninja target build; focused CTest 1/1; independent direct QtTest setup/test/cleanup 3 passed, 0 failed; independent CTest rerun 1/1. Qt logged the existing missing bundled `lib/fonts` path while packaged Inter/Pretendard loaded. No full suite. F460 is accepted and ready to commit; Batch 36 is active pending commit.

## 2026-10-09 - F460 committed; F461 discovery started

F460 committed as `fba46915d76b05aab53de85760a3f857dc4ed2a2` (`Phase2 - Cover Document Viewer Page Spacing QAction parity (F460)`). The commit contains exactly seven scoped paths and its cached diff check was clean. `Qt-Rewrite` is 45 commits ahead of origin. Batch 36 is complete; Batch 37 is active with two independent read-only reviews beginning F461 coverage discovery. No candidate is selected yet. The pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/` changes remain excluded.

## 2026-10-09 - F461 Sidebar Overflow Tooltips QAction selected

Two independent read-only reviews compared the remaining MainWindow action gaps. Select Sidebar Show Tooltips: its standalone test only covers option persistence, while the actual MainWindow connection updates tooltip text on overflowing tree items. The separate Save Mode path reaches autosave coordinators and has broader timer/state setup.

Acceptance: In a dedicated offscreen MainWindow target, isolate settings, disable recent-database loading, add a synthetic teacher with a very long display name, narrow the Sidebar/tree, then show the window and process events. Trigger the actual `showSidebarTooltips` action false→true→false. Verify action/check and persisted preference state plus the overflow item tooltip (full name when enabled, empty when disabled). A scoped guard ends false while MainWindow remains alive. No DB, network, autosave timer, or production changes. Batch 37/F461 is selected; implementation and verification are pending.

## 2026-10-09 - F461 Sidebar Overflow Tooltips QAction accepted

Implementation adds a dedicated offscreen `MainWindowSidebarOverflowTooltipsActionParity` test. With temporary settings and recent-database loading disabled, it inserts a synthetic long-name teacher, constrains the Sidebar and tree, and confirms measured text exceeds available viewport width. The actual enabled/checkable `showSidebarTooltips` action is driven false→true→false; assertions cover action and persisted state and the item tooltip changing from empty to the full teacher name and back. An RAII restorer ends false while MainWindow remains alive even when a QtTest assertion exits early. No production code, database, network, or autosave activity.

Verification passed: VS18/Qt6.12/Ninja target build; focused CTest 1/1; independent direct QtTest setup/test/cleanup 3 passed, 0 failed; independent CTest rerun 1/1. Qt logged the missing bundled `lib/fonts` path while packaged Inter/Pretendard loaded. No full suite. F461 is accepted and ready to commit; Batch 37 is active pending commit.

## 2026-10-09 - F461 committed; F462 Save Mode selected

F461 committed as `e590ea773d4b6b1d415d248c9c3f5068094d5d4c` (`Phase2 - Cover Sidebar Overflow Tooltips QAction parity (F461)`). The commit contains exactly seven scoped paths and its cached diff check was clean. `Qt-Rewrite` is 46 commits ahead of origin. Batch 37 is complete; Batch 38 is active. Two independent reviews compared Save Mode with remaining Sidebar Marquee and automatic-update actions. Save Mode is selected because its downstream coordinator has a direct mode accessor and a clean page can be prepared without activation or database reads; Marquee depends on hover and timer timing. The pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/` changes remain excluded.

Acceptance: In a dedicated offscreen MainWindow target, isolate settings and disable recent-database loading. Ensure a `TestingClassesPage` without activating it, then inspect its direct-child `AutosaveCoordinator`. Trigger the actual Manual and Automatic actions, asserting action/option/persisted state, coordinator mode, and zero `saveRequested` emissions while the page is clean. A scoped guard restores Automatic while MainWindow is alive, including QtTest early returns. No database, network, user file, or dirty-page autosave. Batch 38/F462 is selected; implementation and verification are pending.

## 2026-10-09 - F462 Save Mode QAction accepted

Implementation adds a dedicated offscreen Save Mode MainWindow parity target. It creates a `TestingClassesPage` through PageManager without activating it, then inspects its direct-child `AutosaveCoordinator`. With temporary settings and recent-database loading disabled, it triggers the real Manual and Automatic actions and checks QAction/OptionState/persisted values, coordinator mode, and clean page/coordinator state. A `saveRequested` spy remains at zero after both changes. The RAII guard restores Automatic while MainWindow is alive, including early assertion returns. No production code, database, network, or user-file access.

Verification passed: VS18/Qt6.12/Ninja target build; focused CTest 1/1; independent direct QtTest setup/test/cleanup 3 passed, 0 failed; independent CTest rerun 1/1. Qt logged the missing bundled `lib/fonts` path while packaged Inter/Pretendard loaded. No full suite. F462 is accepted and ready to commit; Batch 38 is active pending commit.

## 2026-10-09 - F462 committed; F463 Automatic Update Preference selected

F462 committed as `ad0d4de70aa5d984dd50eb0da13c1e3506e2be70` (`Phase2 - Cover Save Mode QAction application handoff (F462)`). The commit contains exactly seven scoped paths and its cached diff check was clean. `Qt-Rewrite` is 47 commits ahead of origin. Batch 38 is complete; Batch 39 is active. Two independent read-only reviews compared Sidebar Marquee with the Automatic Update preference action. Select the latter: its preference-port value is directly observable, while Marquee needs hover/timer rendering and has no public enabled-state getter. The pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/` changes remain excluded.

Acceptance: In a dedicated offscreen MainWindow test, isolate settings, use English, and disable recent-database loading. Do not inject an UpdateController. Trigger the actual `automaticallyCheckForUpdates` QAction off then on, verifying check state and `SettingsManagerAutomaticUpdatePreferencesPort::read()` after each change. A scoped guard restores the original preference while MainWindow remains alive, including QtTest early-return paths. No network/startup cleanup, database, or user-file activity. Batch 39/F463 is selected; implementation and verification are pending.

## 2026-10-09 - F463 Automatic Update Preference QAction accepted

Implementation adds a dedicated offscreen MainWindow parity target for the Automatic Update preference. The test isolates settings and temporary paths, uses English, disables recent-database loading, and passes a null UpdateController. It triggers the actual `automaticallyCheckForUpdates` QAction off and on, checking both QAction state and `SettingsManagerAutomaticUpdatePreferencesPort::read()`. The guard restores the original setting while MainWindow remains alive. With no controller attached, no startup update cleanup or network request occurs.

Verification passed: VS18/Qt6.12/Ninja focused build; focused CTest 1/1; direct and independent QtTest setup/test/cleanup 3 passed, 0 failed; independent CTest 1/1; `git diff --check` clean. Initial CMake regeneration exceeded a 180-second wrapper timeout; no build process remained, and a sequential retry passed after CMake configuration completed in about 244 seconds. Qt logged the missing bundled `lib/fonts` path while packaged Inter/Pretendard loaded. No full suite. F463 is accepted and ready to commit; Batch 39 is active pending commit.

## 2026-10-09 - F463 committed; F464 AI Comment Voice selected

F463 committed as `e0771d5a3b87f75f6385bff23dd869e24e237242` (`Phase2 - Cover Automatic Update Preference QAction parity (F463)`). The commit contains exactly seven scoped paths and its cached diff check was clean. `Qt-Rewrite` is 48 commits ahead of origin. Batch 39 is complete; Batch 40 is active. Two independent read-only reviews compared Sidebar Marquee with AI Comment Voice. Select the latter because generated prompt text is a deterministic downstream observable, while marquee needs hover and timer-driven rendering. The pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/` changes remain excluded.

Acceptance: In a dedicated offscreen MainWindow target, isolate settings and disable recent-database loading. Trigger the real Third Person voice QAction and verify action state plus `SettingsManagerAiCommentVoicePreferencesPort::read()`. Construct an AI batch dialog with a synthetic eligible grade-4 report, click its Create Prompt button, and verify the generated prompt contains parent/guardian and they/their wording. Do not copy or open it. A scoped guard restores the original voice while MainWindow remains alive. No database, network, timer, or user-file activity. Batch 40/F464 is selected; implementation and verification are pending.

## 2026-10-09 - F464 AI Comment Voice QAction accepted

The new offscreen MainWindow test triggers the real Third Person AI Comment Voice preference action and checks the selected action, `OptionState`, and preference port. A synthetic eligible grade-4 report is passed to `SpeakingEvalAiBatchDialog`; clicking Create Prompt generates text that contains the parent/guardian and they/their instructions. Copy and Copy/Open are not invoked. Settings are temporary and recent-database loading is disabled. The scoped restorer restores and syncs the original voice while MainWindow remains alive, including early QtTest returns. No production code or external activity.

Verification passed: VS18/Qt6.12/Ninja target build; focused CTest 1/1; independent direct QtTest setup/test/cleanup 3 passed, 0 failed; independent CTest 1/1; whitespace checks clean. CMake logged the unavailable optional Vulkan headers; Qt logged the missing bundled `lib/fonts` path while packaged Inter/Pretendard loaded. No full suite. F464 is accepted and ready to commit; Batch 40 is active pending commit.

## 2026-10-09 - F464 committed; F465 AI Comment Provider selected

F464 committed as `27d30bbd8452f0d350e968ef462af83ae340cdd3` (`Phase2 - Cover AI Comment Voice QAction prompt handoff (F464)`). The commit contains exactly seven scoped paths and its cached diff check was clean. `Qt-Rewrite` is 49 commits ahead of origin. Batch 40 is complete; Batch 41 is active. Two independent read-only reviews compared AI Comment Provider with Sidebar Marquee and the macOS-only PowerPoint notice. Select the built-in provider action because the batch dialog's provider-specific button label is a stable observable and requires no hover/timer behavior. The pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/` changes remain excluded.

Acceptance: In a dedicated offscreen MainWindow test, isolate settings, use English, and disable recent-database loading. Trigger the actual Gemini provider action and check selection plus `SettingsManagerAiCommentProviderPreferencesPort::read()`. Construct the AI batch dialog and verify the “Copy Prompt and Open Gemini” button label without clicking it. Do not exercise Custom Website or external behavior. A scoped guard restores and syncs the original provider while MainWindow remains alive. No database, network, timer, or user-file activity. Batch 41/F465 is selected; implementation and verification are pending.

## 2026-10-09 - F465 AI Comment Provider QAction accepted

The new offscreen MainWindow target starts with temporary ChatGPT settings, triggers the real Gemini preference QAction, and verifies QAction state and the synced provider port. It then creates the AI batch dialog and checks its Copy/Open button label contains Gemini without clicking it. The scope-bound restorer triggers and syncs the original provider while MainWindow remains alive, including QtTest early returns. No external clipboard/browser behavior occurs.

Verification passed: VS18/Qt6.12/Ninja target build; focused CTest 1/1; independent direct QtTest setup/test/cleanup 3 passed, 0 failed; independent CTest 1/1; whitespace checks clean. CMake logged unavailable optional Vulkan headers and long object paths; Qt logged the missing bundled `lib/fonts` path while packaged Inter/Pretendard loaded. No full suite. F465 is accepted and ready to commit; Batch 41 is active pending commit.

## 2026-10-09 - F465 committed; F466 Custom Website provider selected

F465 committed as `599916bada7a2a59ae041dc80d59cba2187cf3be` (`Phase2 - Cover AI Comment Provider QAction dialog handoff (F465)`). The commit contains exactly seven scoped paths and its cached diff check was clean. `Qt-Rewrite` is 50 commits ahead of origin. Batch 41 is complete; Batch 42 is active. Reviews identified Sidebar Marquee and a macOS-only notice gap; source inspection also found the uncovered Custom Website provider action, whose input dialog leads to a clear persisted URL and provider selection. Select this modal handoff rather than timer-driven marquee rendering. The pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/` changes remain excluded.

Acceptance: In a dedicated offscreen MainWindow target, isolate settings, use English, and disable recent-database loading. Trigger the actual Custom Website provider QAction; a zero-delay timer captures the active QInputDialog, enters a valid test URL, and accepts it. Verify the modal was observed, the provider is CustomWebsite, and the URL port stores the entered URL. Close unexpected modals on failure to prevent hangs. A scoped guard restores the original provider and URL without another prompt while MainWindow is alive. Do not click Copy/Open or perform network access. Batch 42/F466 is selected; implementation and verification are pending.

## 2026-10-09 - F466 Custom Website provider QAction accepted

The dedicated offscreen MainWindow test triggers the actual Custom Website provider action. A zero-delay timer captures its `QInputDialog`, supplies `https://example.test/`, and accepts it. The test verifies the modal closed, the provider state is CustomWebsite, and the URL preference matches. Unexpected modals close on the failure path. A scoped restorer restores the original provider and URL while MainWindow is alive. The test never clicks Copy/Open; no browser or network call occurs. Settings are temporary and recent-database loading is disabled.

Verification passed: VS18/Qt6.12/Ninja build; focused CTest 1/1; independent direct QtTest setup/test/cleanup 3 passed, 0 failed; independent CTest 1/1; `git diff --check` clean. CMake logged unavailable optional Vulkan headers and used bundled zlib fallback; Qt logged the missing `lib/fonts` path and offscreen size-hint warnings while packaged Inter/Pretendard loaded. No full suite. F466 is accepted and ready to commit; Batch 42 is active pending commit.

## 2026-10-09 - F466 committed; F467 Sidebar Marquee selected

F466 committed as `0abbaf9238a25407f3c8a6c88a07adb4e93aa238` (`Phase2 - Cover Custom Website provider QAction modal handoff (F466)`). The commit contains exactly seven scoped paths; `Qt-Rewrite` is 51 commits ahead of origin. Batch 42 is complete. The pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/` changes remain excluded.

Discovery left the portable Sidebar Marquee QAction as the remaining MainWindow option gap; the PowerPoint notice is macOS-only. F467 extends the already registered Sidebar Overflow parity target and adds a read-only delegate enabled-state query. Acceptance verifies the actual `animateSidebarText` QAction updates delegate state and the persisted preference across both transitions. A scope-bound restorer returns the original state while MainWindow remains alive. This covers configured handoff, not hover, timer, or rendered animation. Batch 43/F467 is selected; implementation and verification are pending.

## 2026-10-09 - F467 Sidebar Marquee QAction parity accepted

F467 adds a read-only delegate enabled-state query and extends the existing Sidebar Overflow parity test. It triggers the actual `animateSidebarText` QAction in both directions and checks action state, delegate state, and the persisted preference each time. The test starts with the setting disabled in its temporary settings root, disables recent-database loading, and restores the original action state with RAII before MainWindow destruction. The scope covers configuration handoff; it does not test hover, timer movement, or visible animation. No CMake change.

The focused target build passed; focused CTest passed 1/1 and an independent CTest rerun passed 1/1. The direct executable exited 0 from the build directory with the offscreen environment, but emitted no QtTest summary, so no count is claimed. `git diff --check` passed and independent review accepted the source/test lifecycle. No full suite. F467 is accepted and ready to commit; Batch 43 remains active pending commit.

## 2026-10-09 - F467 committed; F468 language preference selected

F467 committed as `0384c3768c18009689f918f456ae932c0c1d5a89` (`Phase2 - Cover Sidebar Marquee QAction parity (F467)`). Its commit contains exactly seven scoped paths and a clean cached diff check; `Qt-Rewrite` is 52 commits ahead of origin. Batch 43 is complete. The pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/` entries remain excluded.

Independent plan and code scans agree on F468: the existing retranslation test drives real Korean and English language QActions and checks locale/UI changes, but does not read back the typed persisted preference or selected action. Extend those same transitions with preference, OptionState, and QAction assertions. A scoped restorer returns to the English baseline through the action and syncs while MainWindow remains alive. Temporary settings and disabled recent-database loading remain in place. No production or CMake change. Batch 44/F468 is selected; implementation and verification are pending.

## 2026-10-09 - F468 Document Catalog Language QAction parity accepted

F468 extends the existing MainWindow document-catalog retranslation parity test. The actual Korean and English language QAction transitions now verify `languageState->current()`, selected/unselected actions, and the typed persisted language preference after syncing. The fixture starts from English under temporary settings and disables recent-database loading. A scoped guard restores English through the QAction only when needed and syncs before MainWindow destruction on assertion exits. No production or CMake change.

Focused build passed; CTest passed 1/1; direct offscreen QtTest passed 4/0/0; `git diff --check` passed. Independent Tester confirmed the test scope, fixture/restorer order, and reran CTest 1/1 and direct QtTest 4/0/0. Qt warned about a missing configured font directory, while packaged Inter/Pretendard fonts loaded. No full suite. F468 is accepted and ready to commit; Batch 44 remains active pending commit.

## 2026-10-09 - F468 committed; F469 Page Spacing variants selected

F468 committed as `219ca8fb64b50247c24a722f7a091d75a2a6d86e` (`Phase2 - Cover Document Catalog Language Preference QAction parity (F468)`). Its commit contains six scoped paths and a clean cached diff check; `Qt-Rewrite` is 53 commits ahead of origin. Batch 44 is complete. The existing `agent_docs/latest_session_work.md` and `%SystemDrive%/` changes remain excluded.

F469 extends the existing Document Viewer Page Spacing parity test. F460 covers Small/Large; the `DocumentPageSpacing` enum also includes None and Medium, with viewer mappings 0 px and 16 px. Drive those actual QActions and assert OptionState, checked actions, persisted setting, and `QPdfView::pageSpacing()` while retaining the existing assertions. Keep temporary settings, disable recent-database loading, and sync the Small-state restorer on assertion exits. The blank PDF viewer is constructed without loading a PDF. No production or CMake change. This deterministic viewer-state slice was selected over System Default language. Batch 45/F469 is selected; implementation and verification are pending.

## 2026-10-09 - F469 implementation and focused verification

Extended the existing page-spacing parity test to trigger None and Medium and assert the QAction group state, `OptionState`, persisted integer values, and viewer spacing (0 px and 16 px). Existing Small/Large checks remain. The scoped Small restorer triggers only when needed and syncs settings on normal and assertion-return exits. The test uses temporary settings, disables recent-database loading, and does not load a PDF. No production or CMake changes.

Focused target build passed; focused CTest passed 1/1; direct offscreen QtTest passed 3/0/0. `git diff --check` passed. Qt warned that its optional configured fonts directory is missing; packaged Inter/Pretendard fonts loaded. An initial plain-shell build lacked MSVC headers; the focused build succeeded under the VS18 developer environment. Independent review is pending; F469 remains active in Batch 45.

Independent review accepted F469: the Tester confirmed the test scope and restorer, reran focused CTest (1/1), and passed `git diff --check`. The independent direct offscreen invocation exited 0 without a QtTest summary, so it provides no count; the executor invocation reported 3/0/0. No full suite. F469 is independently verified and ready to commit; Batch 45 remains active pending commit.


## 2026-10-09 - F469 committed; F470 Background White selected

F469, `Phase2 - Cover Document Viewer Page Spacing None and Medium QAction parity (F469)`, committed as `4044c80a` on `Qt-Rewrite`. The commit contains the six reviewed test and Phase 2 tracking paths. Batch 45 is complete. The only post-commit worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

Two independent F470 scans selected the omitted White action in the registered Document Viewer Background parity test. Extend the blank-viewer test to trigger White and assert `OptionState`, exclusive Default/White/Black QAction checks, persisted value `1`, viewer/viewport properties `white`, and the viewer `QPalette::Dark` color. Retain Black and Default behavior and cleanup. Use temporary settings, disable recent-database loading, and sync the Default restorer on assertion exits; no PDF load, production change, or CMake change is needed. F470 is selected for implementation; Batch 46 is active.

## 2026-10-09 - F470 implementation and focused verification

Extended the registered Document Viewer Background parity test to cover the White QAction before the existing Black and Default transitions. It checks `OptionState`, exclusive Default/White/Black action state, persisted value 1, viewer and viewport `pdfViewerBackground` properties, and `QPalette::Dark == white`. Existing Black and Default assertions remain. The scoped Default restorer triggers only when needed and syncs on normal and assertion-return paths. No production or CMake change.

The focused target built under VS18; focused CTest passed 1/1; direct offscreen QtTest passed 3/0/0; `git diff --check` passed. Qt reported a missing optional configured fonts directory while packaged Inter/Pretendard fonts loaded. Independent review is pending; Batch 46 remains active.

Independent review accepted F470. The Tester confirmed the action, persistence, viewer-property, palette, and restorer assertions, and independently passed focused CTest 1/1 and `git diff HEAD --check`. Its direct offscreen invocation exited 0 without a summary, so no independent case count is claimed; the executor run reported 3/0/0. No full suite. F470 is independently verified and ready to commit; Batch 46 remains active pending commit.


## 2026-10-09 - F470 committed; F471 Font Size matrix selected

F470, `Phase2 - Cover Document Viewer Background White QAction parity (F470)`, committed as `738f40f1` on `Qt-Rewrite`. The commit contains the six scoped test and Phase 2 tracking paths. Batch 46 is complete; the only post-commit worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

Two independent F471 scans selected the missing Font Size actions in the existing registered parity test. It currently starts at Normal and exercises Large only. Extend it to trigger Small, retain Large, trigger Extra Large, and return through the Normal action. At each transition assert `OptionState`, exclusive action checks, the typed persisted preference after sync, `FontManager::sizeOffset()`, and runtime-relative application point size (`FontManager::getPlatformFontSize() + offset`). Keep English and temporary settings, disable recent-database loading, and restore the original application font/offset plus the Normal preference on assertion exits. Use dynamic base-size assertions, not hard-coded visual sizes. No production or CMake change. F471 is selected; Batch 47 is active.

## 2026-10-09 - F471 implementation and focused verification

Extended the existing Font Size QAction parity test to cover Small, Large, Extra Large, and Normal. Each action transition checks OptionState, exclusive checks, typed persisted preference after sync, FontManager offset, and QApplication point size relative to the runtime platform base. The scoped restorer preserves the original application font and offset and syncs the Normal preference on normal or assertion-return exits.

The focused target built under VS18 and CTest passed 1/1. Direct offscreen QtTest exited 0 without a summary, so no direct count is claimed. `git diff --check` passed. Independent review is pending; Batch 47 remains active.

Independent review accepted F471. The Tester confirmed the four QAction transitions and cleanup, and independently passed focused CTest 1/1 and `git diff --check`. The direct offscreen invocation exited 0 without output, so no case count is claimed; the executor direct invocation also exited 0 without a summary. No full suite. F471 is independently verified and ready to commit; Batch 47 remains active pending commit.


## 2026-10-09 - F471 committed; F472 System Default Theme selected

F471, `Phase2 - Cover Small and Extra Large Font Size QAction parity (F471)`, committed as `0d972b53` on `Qt-Rewrite`. The commit contains the six reviewed test and Phase 2 tracking paths. Batch 47 is complete; the only post-commit worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

Two independent F472 scans selected the omitted System Default Theme action in the existing MainWindow Theme parity test. Extend its Light → Dark → System Default → Light flow to assert the three exclusive actions, `OptionState`, typed persisted preference after sync, and the effective resolved theme from `ThemeService::currentTheme()` plus the window theme property. Compute the expected Light/Dark result from `QApplication::styleHints()->colorScheme()` (Unknown resolves Light); do not assume a host scheme. Restore Light while MainWindow remains alive and sync settings, retaining the existing palette/stylesheet cleanup. Keep temporary settings, English, and recent-database loading disabled; no production or CMake change. F472 is selected; Batch 48 is active.

## 2026-10-09 - F472 implementation and focused verification

Extended the existing theme parity test with the System Default QAction while retaining Dark and Light transitions. The test checks all three actions’ exclusive group, selected checks, OptionState, and typed persisted preference after sync. It computes the resolved theme from `QApplication::styleHints()->colorScheme()` (Unknown resolves to Light) and verifies ThemeService, application palette, and the window theme property. The scoped restorer returns to Light while MainWindow lives, syncs settings, and restores the original palette and stylesheet. No production or CMake change.

The VS2026 x64 focused build passed; focused CTest passed 1/1; direct offscreen QtTest passed 3/0/0; `git diff --check` passed. Independent review is pending; Batch 48 remains active.

Independent review accepted F472. The Tester confirmed all action transitions and the dynamic color-scheme expectation, and independently passed focused CTest 1/1, direct offscreen QtTest 3/0/0, and `git diff --check`. Qt warned about a missing optional configured fonts directory; packaged Inter/Pretendard fonts loaded. No full suite. F472 is independently verified and ready to commit; Batch 48 remains active pending commit.


## 2026-10-09 - F472 committed; F473 Claude Provider selected

F472, `Phase2 - Cover System Default Theme QAction parity (F472)`, committed as `88fd2088` on `Qt-Rewrite`. The commit contains the six reviewed test and Phase 2 tracking paths. Batch 48 is complete; the only post-commit worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

Two independent F473 scans nominated System Default Language and Claude AI Provider. Select Claude in the existing MainWindow provider parity test: it already triggers Gemini and checks the local batch-dialog provider label, while Claude’s QAction, saved preference value, and consumer label have stable local behavior. System Default Language remains locale-dependent and was set aside during F469. Extend the test to trigger the actual Claude action after Gemini, assert provider state and exclusive ChatGPT/Gemini/Claude checks, typed persisted preference after sync, and a newly constructed batch dialog label identifying Claude. Retain Gemini checks. Keep temporary settings, English, and recent-database loading disabled; do not click the open button or access a browser/network. Reuse the existing restorer to return to ChatGPT and sync. No production or CMake change. F473 is selected; Batch 49 is active.

## 2026-10-09 - F473 implementation and focused verification

Extended the existing Gemini provider QAction test with Claude. It checks actual selected/exclusive actions, typed saved preference after sync, and the batch dialog’s “Copy Prompt and Open Claude” label while retaining Gemini coverage. The test uses temporary settings, English, and disabled recent database loading. It does not click the open button or access a browser/network. The existing restorer returns to ChatGPT and syncs. No production or CMake change.

The VS18 x64 focused build passed and focused CTest passed 1/1. Direct offscreen QtTest exited 0 without a summary; no direct count is claimed. `git diff --check` passed. Independent review is pending; Batch 49 remains active.

Independent review accepted F473. The Tester confirmed Gemini/Claude action state, persistence, dialog labels, fixture, and restorer, and independently passed focused CTest 1/1 and `git diff --check`. The existing executable was newer than source; its direct offscreen run exited 0 without output, so no count is claimed. No full suite or network behavior. F473 is independently verified and ready to commit; Batch 49 remains active pending commit.


## 2026-10-09 - F473 committed; F474 Microsoft Copilot selected

F473, `Phase2 - Cover Claude AI Comment Provider QAction parity (F473)`, committed as `438f541a` on `Qt-Rewrite`. The commit contains the six reviewed test and Phase 2 tracking paths. Batch 49 is complete; the only post-commit worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

Two independent F474 scans found provider action gaps: the code scan nominated Microsoft Copilot; the plan scan noted ChatGPT’s consumer label/transition is not asserted. Select Copilot because the existing provider test already verifies ChatGPT baseline/restoration and Gemini/Claude action handoffs, while Copilot has no actual selected-action or consumer-label coverage. Extend `mainwindow_ai_comment_provider_action_parity_tests.cpp` to trigger Copilot after Claude, assert state and exclusivity across ChatGPT/Gemini/Claude/Copilot/CustomWebsite, typed persisted MicrosoftCopilot preference after sync, and a new batch dialog label containing “Microsoft Copilot.” Retain Gemini and Claude checks. Use temporary settings, English, and disabled recent-database loading; do not click the open button or access browser/network. Reuse the ChatGPT restorer and sync. No production or CMake change. F474 is selected; Batch 50 is active.

## 2026-10-09 - F474 implementation and focused verification

Extended the existing provider action test with Microsoft Copilot after Gemini and Claude. It verifies action exclusivity across all five providers, persisted typed Copilot preference after sync, and the batch dialog’s “Microsoft Copilot” label. Gemini and Claude checks remain. The test uses temporary settings, English, and disabled recent database loading; it does not click the open button or access a browser/network. The existing guard restores ChatGPT and syncs. No production or CMake change.

The VS18 focused build passed and focused CTest passed 1/1. Direct offscreen QtTest exited 0 without output, so no direct count is claimed. `git diff --check` passed. Independent review is pending; Batch 50 remains active.

Independent review accepted F474. The Tester confirmed all provider transitions, exclusivity, typed persistence, Copilot label, and ChatGPT restoration, and independently passed focused CTest 1/1 and `git diff --check`. The direct offscreen invocation exited 0 without output; no direct count is claimed. No full suite or network behavior. F474 is independently verified and ready to commit; Batch 50 remains active pending commit.


## 2026-10-09 - F474 committed; F475 Custom Website label selected

F474, `Phase2 - Cover Microsoft Copilot AI Comment Provider QAction parity (F474)`, committed as `f102e7ad` on `Qt-Rewrite`. The commit contains the six reviewed test and Phase 2 tracking paths. Batch 50 is complete; the only post-commit worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

Two independent F475 scans found missing batch-dialog label coverage: one nominated ChatGPT at the existing default baseline, and the other nominated Custom Website after its modal action flow. Select Custom Website because the existing isolated test verifies the unique URL-entry action, saved provider, and URL; asserting its local consumer label closes the end-to-end handoff without host or network dependence. Extend `mainwindow_custom_website_ai_comment_provider_action_parity_tests.cpp` to construct a batch dialog with synthetic report data after the action saves the custom URL, then assert `speakingEvalAiBatchCopyOpen` contains “Custom AI Website.” Retain provider/URL persistence assertions and the ChatGPT/URL restorer. Do not click the button or access a browser/network. Use temporary settings, English, and disabled recent-database loading; no production or CMake change. F475 is selected; Batch 51 is active.

## 2026-10-09 - F475 implementation and focused verification

Extended the existing Custom Website modal/action parity test to construct a batch dialog after saving the custom provider and URL, then assert its “Custom AI Website” button label. Existing preference/URL persistence and ChatGPT/URL restoration checks remain. The test does not click the button or access browser/network behavior. No production or CMake change.

The VS18 focused target build passed and focused CTest passed 1/1. Direct offscreen QtTest exited 0 without a summary; no count is claimed. `git diff --check` passed. Git emitted its LF-to-CRLF normalization notice for the test file. Independent review is pending; Batch 51 remains active.

Independent review accepted F475. The Tester confirmed the custom URL modal, persistence, batch dialog label, fixture, and restorer, and independently passed focused CTest 1/1 and `git diff --check`. Its direct invocation exited 0 without a summary, so no direct count is claimed; the existing executable was newer than source. No full suite or network behavior. F475 is independently verified and ready to commit; Batch 51 remains active pending commit.

F475, `Phase2 - Cover Custom Website batch dialog label (F475)`, committed as `643d9674` on `Qt-Rewrite` with six scoped paths; Batch 51 is complete. The only remaining worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

Independent F476 reviews found both a missing ChatGPT consumer label and an untested Direct to Student voice handoff. Choose the voice slice because it verifies a complete local transition from QAction through persisted preference into generated prompt text. Preserve the ChatGPT label gap for a later slice if still needed.

F476 implementation now covers both voice choices through the real QAction, persisted preference, and local prompt consumer. The focused target build, CTest (1/1), and diff check passed; independent verification remains pending.

Independent review accepted F476. Focused CTest passed 1/1, the direct offscreen executable exited 0 without a summary, and `git diff --check` passed. The Tester confirmed prompt behavior and cleanup. The executor's successful focused build is the build evidence; no full suite.

F476 committed as `9e58daae` with exactly six scoped paths; Batch 52 is complete. The only remaining worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`. F477 discovery begins after this commit.

Select F477 for the deferred ChatGPT batch-dialog label gap. Independent plan and code scans found the same missing consumer assertion; extend the existing registered provider parity test through the real ChatGPT QAction, persisted preference, and fresh local dialog label. The other provider labels remain covered.

F477 implementation completes the provider label matrix for ChatGPT through its QAction, saved preference, and fresh local batch dialog. The executor's focused build, CTest (1/1), and diff check passed; independent verification remains pending.

Independent review accepted F477. CTest passed 1/1; the direct offscreen executable exited 0 without a summary; `git diff --check` passed. The Tester confirmed provider action/persistence, label, and cleanup. The focused artifacts postdate the source; no full suite.

F477 committed as `9a4c9e85` with exactly six scoped paths; Batch 53 is complete. The only remaining worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`. F478 discovery begins after this commit.

F478 follows the provider-label matrix into the separate single-student report dialog, whose main provider button had only a Gemini assertion. The plan scan found F285/F298 still contract-dependent; the code scan found a bounded local label consumer, so this testable gap was selected. Keep the modal preview popup for a later focused slice.

F478 implementation covers the main single-student report dialog provider label across all five providers while preserving batch-dialog coverage. Both focused builds, CTests (2/2), direct QtTests (3/0 each), and diff check passed; independent review remains pending.

Independent review accepted F478. Both focused CTests passed 2/2, direct QtTests passed 3/0 each, and diff check passed. The Tester confirmed persisted provider labels and cleanup for all five providers. Only optional Qt font/offscreen warnings and Git line-ending notices were reported; no full suite.
