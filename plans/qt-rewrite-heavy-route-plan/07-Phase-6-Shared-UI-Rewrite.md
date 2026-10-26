# Phase 6 — Shared UI Rewrite

Read [00-Start-Here.md](00-Start-Here.md) first for the overall plan, workflow, and phase sequence.

## Mandatory route for new and resumed work

- Use the Heavy route for every task, slice, follow-up, and reopened gate in this phase. `Default route: Heavy` is a binding instruction.
- At each start or resume, explicitly select Heavy and reread this phase plan and [00-Start-Here.md](00-Start-Here.md). Follow the active `AGENTS.md` Heavy-route instructions and `~/.codex/codex_workflow/heavy_route.md`, including their delegation, verification, and deployment-state requirements.
- A new session, handoff, or context reset does not change the route. Do not continue under Light or Medium. If a required Heavy-route step blocks progress, report the blocker before implementation instead of silently switching routes.

## Build and test verification

- Use `build/windows-x64-debug` as the standard local Windows x64 Debug
  build folder for slice work. Reuse this same configured folder across
  slices and phases; do not create slice-, task-, or reviewer-specific build
  folders. Build the affected targets and their dependencies, and let the
  build system recompile changed or out-of-date inputs.
- If a fresh build is needed, empty the applicable standard build folder
  before configuring and building in it. For Windows x64 Debug, keep using
  `build/windows-x64-debug`; do not create a new folder for the fresh build.
  Apply the same rule when changing the build system, toolchain, or
  dependency configuration; when stale artifacts could explain a result;
  or when a phase gate explicitly requires clean-checkout evidence.
- A gate requiring another platform or configuration must use that CMake
  preset's standard `build/<preset-name>` folder and reuse it for that
  preset, rather than creating a slice-specific folder.
- Keep any phase-specific full builds, test suites, platform matrices, and
  packaging checks required by an exit gate. Record whether verification
  used an incremental or fresh build, which targets were built, and which
  tests ran. Describe focused results as focused; do not report them as a
  full-suite pass.

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phase 5
- Blocks: Feature migration and visual parity
- Owner: Unassigned
- Last updated: 2026-10-26
- Current note: Phase 6 remains active on the Heavy route. F1514–F1527 are
  accepted; F1527 verified shell appearance and labels in English and Korean
  under both supported themes. Batch 1 is ready to commit and push. F1528 —
  bounded review-window model — is the discovered Batch 2 requirement for
  §6.7, and starts after that push. Staff Directory remains the representative
  model/view consumer; its full feature migration is Phase 7C. Window/splitter
  preference lifecycle behavior remains deferred under F1524 because there
  are no active callers.

### Slice batches

Use the shared [current slice-batch tracker](Current-Slice-Batches.md) for live work when Phase 6 is active. It contains only the active
batch and the next batch once discovery starts; completed batches are
not archived there.

## Progress log

Use this section only when a decision, acceptance result, verification
finding, blocker, or exit-gate detail needs a durable record. Routine
slice chronology and closed batch history do not need to be archived;
keep current status in this plan and live batches in the shared tracker.

### F1514 — complete — 2026-10-26

`PageDescriptor` now carries validated stable identity, label, optional icon
resource ID, required permission IDs, factory, resource policy, and initial-data
policy. `PageHost` retains and exposes descriptors without constructing pages;
the Next bootstrap registers its placeholder through this API. The old
string/factory registration path was removed, and the focused lifecycle and
failure tests now use descriptors. Permission and policy values remain
metadata; their enforcement belongs to navigation/application slices.

The incremental macOS Debug build of `ClassMngrNextPageHostTests` and
`ClassMngrNextBootstrapTests` passed. Focused CTest passed 2/2, and
`git diff --check` passed. No full suite or cross-platform build ran.

### F1515 — complete — 2026-10-26

`PageHost` now tracks `Declared`, `Created`, `Activated`, `Suspended`, and
`Released` states and exposes state queries plus transition notifications.
Creation remains lazy; `Created` is observed during the activation attempt,
before `onActivating()`. Reentrant host mutations from transition observers are
rejected. Failed target activation cleans up the target and records `Released`
while preserving the previous active page. Released descriptors remain
registered and can be recreated.

Independent review caught a test-fixture lifetime error that caused a
`SIGTRAP`: a retained probe page outlived its stack-owned probe. The fixture now
destroys the host before the probes and explicitly releases the final page.
The incremental macOS Debug build passed; both focused CTests passed 2/2, the
PageHost CTest passed ten consecutive Executor runs and one independent run,
and `git diff --check` passed. No full suite or cross-platform build ran.

### F1516 — complete — 2026-10-26

`NavigationModel` now uses ordered group and page nodes; page leaves reference
`PageId` and obtain labels, icons, and permission requirements from
`PageDescriptor`. Workspace visibility and explicit permission grants are
separate. The initial placeholder route is `campus_information` with a closed
workspace and `my_workspace` with an open workspace. The shell commits location,
selection, and page-ID history only after guard approval and successful
`PageHost` activation. Back/forward, branch truncation, rejected navigation,
and activation-failure rollback are covered.

The shell loads a descriptor-supplied icon through `ResourceLoader`; current
legacy routes remain icon-free because the existing sidebar defines no node
icons. Useful Links retains its seven static labels as inert placeholders; URL
actions, dynamic document nodes, dynamic teacher rows, and feature loading are
deferred to later migration slices. Missing icon resources currently render no
icon; this failure path was reviewed but not separately induced in a test.

The incremental macOS Debug build covered the navigation, PageHost, and
bootstrap targets. Focused CTest passed 3/3, and `git diff --check` passed.
No full suite or cross-platform build ran.

### F1517 — complete — 2026-10-26

The application-layer `CommandBus` now provides synchronous typed handler
registration and dispatch keyed by C++ command type. It reports missing,
duplicate, invalid-handler, and unavailable bus errors separately from the
handler's unchanged `Domain::Result<void>`. Availability is queried and
rechecked on dispatch; checked state and NavigationModel remain outside the
bus. The bus and focused test target have no Qt Widgets dependency.

Independent macOS Debug build and CTest passed for
`ClassMngrNextCommandBusTests` and `ClassMngrNextApplicationRuntimeTests` (2/2).
Coverage includes availability changing between query and dispatch, with the
handler kept uncalled. `git diff --check` passed. No full suite or
cross-platform build ran.

### F1518 — complete — 2026-10-26

Back/Forward enabled state now derives from both history position and current
visibility/permission of each history target. The shell presents the model's
value state and refreshes it after navigation or access-context changes. An
inaccessible target disables the action; a rejected history move preserves
the current/selected route, history, and active page. This covers existing
shell navigation controls; feature menu/action migration remains later work.

Independent incremental macOS Debug build and CTest passed for Navigation,
PageHost, and Bootstrap (3/3); `git diff --check` passed. No full suite or
cross-platform build ran.

### F1519 — complete — 2026-10-26

Added a Qt-free `UserPromptPort` with typed message, confirmation,
unsaved-change, and named-action values. Accepted, declined, dismissed,
Save/Discard/Cancel, and stable action IDs are semantic responses; presenter
failures remain separate `Domain::Result` errors. A Next Qt presenter maps the
contracts to dialogs and preserves default/Escape behavior, while a separate
automation API exposes stable prompt IDs and value-only snapshots/commands.
Legacy prompt callers and their workflow adapters remain unchanged.

Independent macOS Debug build covered the prompt contract, presenter, Next
application, Bootstrap, Navigation, and PageHost targets. Focused CTest passed
5/5; coverage includes destructive styling independent from semantic outcome,
Escape mapping, dismissal, unsaved-change choices, named-action IDs, default
actions, and automation snapshots/capture. `git diff --check` passed. No full
suite, cross-platform build, or native window-manager presentation test ran.

### F1520 — complete — 2026-10-26

Added a Qt-free `FileDialogPort` for opening one or many files, saving, and
selecting a directory, plus a Qt presenter that owns widgets, path conversion,
normalization, platform policy, and per-purpose directory persistence. Requests
use UTF-8 values and reuse the existing purpose and preferences port. Initial
directory precedence remains explicit request, remembered directory, then
Documents (Pictures for signature images), with home fallback. The presenter
preserves filters, suggested filename, default suffix, overwrite policy,
optional open-after-save checkbox, and each operation's standard Open/Save/
Choose Directory accept and platform keyboard behavior. Cancellation is a
successful empty selection. Preference-write failures accompany accepted
selections as nonfatal warning values.

Independent macOS Debug build covered the Next application, both file-dialog
test targets, and the QSettings directory-preferences adapter target. Focused
CTest passed 3/3; tests cover current-directory precedence/fallbacks, UTF-8,
filters, selection and cancellation, save options, normalization, persistence
timing, and warning-plus-selection. The adapter target remains independently
covered. `git diff --check` passed. Native OS panels and window-manager keyboard
behavior were not exercised in this offscreen environment; the presenter tests
verify platform routing and Qt dialog policy flags.

### F1521 — complete — 2026-10-26

Added a Qt-free `NotificationPort` with UTF-8 message text and typed
Information/Warning/Error severity, plus a transient Qt presenter owned by the
Next shell. The hidden-when-idle QStatusBar shows one notice at a time, replaces
an active notice, clears explicitly, and expires after a finite five-second
default. Warning and error messages receive translated severity labels. The
service has no action/history/persistence layer; prompts remain responsible
for choices and acknowledgment, and job models continue to own progress.
No feature callers were migrated.

Independent macOS Debug build covered the Next app, notification contract and
presenter, Navigation, and Bootstrap targets. Focused CTest passed 4/4, and
the presenter suite passed five repeated runs. Coverage includes idle
visibility, severity, expiry, replacement, explicit clear, presenter/shell
destruction, invalid requests, and wrong-thread failures. `git diff --check`
passed. Native display and window-manager behavior were not tested.

### F1522 — complete — 2026-10-26

Added a Qt-free `PreferencesModel` around the existing validated
`UserPreferencesState`, with distinct read, in-memory apply, and whole-snapshot
persist ports. It tracks dirty state, skips writes when clean, retains dirty
state after write failure for retry, and notifies subscribers with copies of
the committed snapshot. Failed reads/writes preserve model state. Changed
`apply`/`load`/`persist` operations during synchronous observer delivery return
`Conflict`; an identical apply remains a no-op. Subscription changes during
delivery are safe, and observer exceptions are rethrown after remaining
observers receive the committed value. No storage adapters, theme presenters,
or feature consumers were changed; those remain F1523/F1524 work.

Independent macOS Debug build and focused CTest covered the new Preferences
Model test and existing User Preferences state test (2/2). Coverage includes
load/apply/persist separation, failure atomicity and retry, dirty-state
handling, observer ordering, reentrancy, subscription lifecycle, and exception
visibility. The new Application target remains Qt-free. `git diff --check`
passed. No full suite or cross-platform build ran.

### F1523 — complete — 2026-10-26

Added a Next `ThemePresenter` that applies the existing packaged light/dark
QSS, explicitly maps semantic and persisted theme enums, and retains the active
stylesheet resource until the prior application stylesheet is restored. Startup
applies the stored preference without rewriting it. Live preference changes
load the candidate style, persist first through the existing one-field theme
port, and install the style only after persistence succeeds. Failed resource
loads or writes leave the installed style unchanged. No palette or QSS changes
were made, and the legacy theme controller/service remains unchanged.

SystemDefault resolves `Dark` to the dark stylesheet and all other/unknown
schemes to light when applied. It does not track later OS color-scheme changes,
matching the current Next startup behavior; live legacy parity for this case
remains a later appearance decision.

Independent incremental macOS Debug build and focused CTest covered the theme
presenter, Bootstrap, and existing SettingsManager theme preference adapter
(3/3). Tests cover enum mapping, startup without writes, scheme fallback,
resource-load/write failures, persistence ordering, no-op behavior, and
stylesheet restoration. `git diff --check` passed. No full suite,
cross-platform build, or screenshot comparison ran; appearance evidence remains
F1527.

### F1524 — complete — 2026-10-26

Added validated `FontSize` to `UserPreferencesState`, a Qt-free aggregate
snapshot adapter over the existing typed preference ports, and a
SettingsManager composition for complete startup reads and sequential writes.
The Next bootstrap now loads one `PreferencesModel` for startup language,
theme, and font selection; `ApplicationShell` and `PageHost` pass that same
model through `PageContext` for future page actions. `ThemePresenter` stages
the candidate QSS so observers see a style matching the committed preference,
and restores the old style when persistence fails. Startup font selection
preserves the current locale-dependent font family and size rules and restores
the prior application font during teardown.

Snapshot writes are sequential and non-atomic: an error stops later writes,
leaves earlier writes in place, and keeps the model dirty so a full-snapshot
retry can repair storage. The existing teacher and PowerPoint writers return
`void`, so their write success cannot be checked. Global window/splitter state
remains deferred because the keys have no active callers; current workspace
campus data and per-dialog geometry remain separate. Legacy preference
controllers and feature consumers were not migrated.

Independent incremental macOS Debug build covered `ClassMngrNext` and the
Bootstrap, ThemePresenter, PageHost, Navigation, PreferencesModel,
ApplicationUserPreferences, PreferencesSnapshotAdapter, FontSizePresenter,
and SettingsManagerPreferencesPort tests. Focused CTest passed 9/9. Coverage
includes full-state round trips, partial-write/retry, startup preference
selection, shared page context, font restoration, theme rollback, observer
visibility, and stylesheet resource lifetime. `git diff --check` passed. No
full suite, cross-platform build, or appearance screenshots ran; screenshots
and shell appearance checks remain F1527.

### F1525 — complete — 2026-10-26

Added the shared header-only `EditableTextTableModel<Row>` for rectangular
editable text rows. It centralizes Qt model roles, headers, editability,
checked cell updates, row insertion/removal, and reset notifications while
leaving feature row metadata and persistence outside the shared primitive.
Staff Directory now uses this model base for its existing 5/6-column schemas.
Its persisted IDs, feature queries, save/import rules, native-teacher position
delegate, and dirty/reload state remain in the feature page. Text and combo
editors are still created only for the active cell, and table/model/delegate
ownership remains page-scoped.

The model directly retains supplied row values; it has no paging or backing-
data memory cap. The boundedness evidence here is Qt widget/editor allocation:
the 96-row check confirms widget count remains fixed and no index widgets are
created. Model tests also cover 5/6 columns, display/edit roles, headers,
invalid requests, edit signals, insertion/removal, reset generations, and row
release. The page test covers clear/reload, active-editor disposal, and page
recreation. The existing import refresh-failure test continues to pass.

Independent incremental macOS Debug build covered the new model test, Staff
Directory page test, teacher-import refresh test, and both directory read/save
parity suites. Focused CTest passed 7/7. CMake validated ownership of 1,694
handwritten files, and `git diff --check` passed. A broader CTest selection
also failed in `wizardSuccessTranscript` with macOS
`NSBundle initWithURL: nil URL argument`; that test path is outside the changed
model/view code and is not part of the focused acceptance result. No full suite,
cross-platform build, or screenshot comparison ran; appearance evidence remains
F1527.

### F1526 — complete — 2026-10-26

Accepted the existing Staff Directory as Phase 6's representative model/view
table. Both directory variants use `QTableView` with 6- and 5-column schemas;
the 96-row test confirms widget count does not grow with rows/cells and no
`QTableWidget` or per-cell index widgets are created. Existing editor tests
show text and native-position combo editors are created only for active cells.
F1525's clear/recreation test verifies active-editor disposal and page-owned
model/delegate release. Import refresh failure continues to lock stale rows
until a successful reload. Sorting remains disabled and headers remain
non-clickable/non-movable, preserving current behavior.

Independent incremental macOS Debug build covered
`ClassMngrStaffDirectoryPageTests` and
`ClassMngrTeacherImportUiApplyParityTests`. Focused CTest passed 2/2 for the
Staff Directory page suite and isolated
`TeacherImportUiApplyParityF1285DirectoryReloadFailure` regression. This gate
proves bounded Qt widget/editor allocation for the representative table; it
does not prove bounded backing-row memory or complete Staff Directory feature
migration. The latter remains Phase 7C, and packaged Release memory measurement
remains Phase 9. No full suite or cross-platform build ran.

Paired Batch 2 discovery found F1528, a bounded review-window model, to satisfy
the remaining Phase 6 §6.7 shared-primitive requirement. F1528 is inactive
until Batch 1 is accepted and pushed. Window/splitter keys remain intact and
their lifecycle behavior remains deferred under the F1524 decision because
there are no active callers.

### F1527 — complete — 2026-10-26

Captured and reviewed the Next placeholder shell in English and Korean under
Light and Dark themes at 1024×720 and 1× scale. The baselines and manifest are
in `docs/qt-rewrite/visual-baseline/next/phase-6/f1527/`. The review exposed
missing Korean translation contexts for navigation labels, shell actions, and
placeholder text; those Next contexts are now explicit. It also exposed a
clipped English placeholder description and a tree missing the legacy
sidebar's 2px solid, 8px-radius frame. The description now uses available
content width, and the tree uses the existing theme border colors and frame
dimensions. Typography, margins, spacing, palette, and selection colors remain
unchanged.

The appearance assertions cover translated page and group labels, selected
route/item, focus, placeholder text fit, icon absence, disabled initial
Back/Forward actions, theme stylesheet, and frame width. Independent
incremental macOS Debug build and focused CTest passed 4/4 across Bootstrap,
ThemePresenter, Navigation, and PageHost. `git diff --check`, the resource
inventory check, four-capture manifest checks, and CMake ownership validation
for 1,694 handwritten files passed. No full suite, native-window capture,
cross-platform build, or cross-platform visual parity was claimed.

Paired Batch 2 discovery found F1528, a bounded review-window model, to satisfy
the remaining Phase 6 §6.7 shared-primitive requirement. It starts after Batch
1 is committed and pushed. Window/splitter keys remain intact and their
lifecycle behavior remains deferred under the F1524 decision because there
are no active callers.

## Objective

Create a small, predictable shared UI foundation that preserves the current appearance while eliminating hidden page construction, duplicated state, and widget-heavy data presentation.

## Work packages

### 6.1 PageHost

Replace the current page ownership model with:

- PageDescriptor.
- PageHost.
- NavigationModel.
- Page lifecycle state.

Each PageDescriptor contains:

- Stable page ID.
- Label.
- Icon resource ID.
- Permissions.
- Factory.
- Resource policy.
- Initial data policy.

Registering a page must not construct it.

### 6.2 Page lifecycle

Use:

    Declared → Created → Activated → Suspended → Released

Rules:

- Creation does not load the complete database.
- Activation loads only required page state and resources.
- Suspension releases transient images, documents, editors, and large models.
- A document viewer may retain catalog selection state, but its loaded QtPdf
  document is released when the viewer is closed, replaced, suspended, or
  released.
- Release destroys page-owned resources.
- Persistent state is stored in application state or persistence, not only in widgets.
- A released page can be recreated correctly.

### 6.3 Navigation

Create NavigationModel for:

- Current location.
- Parent/child page relationships.
- Navigation history.
- Sidebar selection.
- Permission filtering.
- Initial page.
- Unsaved-change prompts.

Keep the current labels, hierarchy, icons, and navigation behavior.

### 6.4 Commands and actions

Create CommandBus and typed commands for:

- File operations.
- Editing.
- Printing.
- Class operations.
- Imports and exports.
- Theme and language changes.
- Help and updates.
- Admin and developer actions.

Enabled state must derive from application state and permissions rather than scattered widget callbacks.

### 6.5 Dialog and notification services

Replace direct dialog construction in feature code with:

- DialogService.
- UserPromptService adapter.
- FileDialogService adapter.
- NotificationService.

Preserve wording, buttons, default actions, and keyboard behavior.

Keep the permanent user-prompt contract semantic: showing a message,
confirming a choice, confirming unsaved changes, or choosing a named action.
Do not add active-widget lookup, visibility, `objectName` selection, or
button-click methods to the domain/application prompt contract.

Provide a separate Qt presentation/test adapter for legacy startup and visual
automation where necessary. Its value-only snapshot may expose prompt ID,
title, text, and visibility, while its commands may accept/reject a prompt,
activate its default action, or capture the prompt. It must never return a
`QMessageBox*` or `QWidget*`. Use stable semantic prompt IDs at the adapter
boundary; Qt object names remain implementation details. Keep the adapter
temporary until the corresponding legacy workflow is migrated.

### 6.6 Preferences and theme

Create a typed PreferencesModel.

Separate:

- Reading preferences.
- Applying preferences.
- Persisting preferences.
- Notifying interested pages.

Keep current theme, language, font, window, splitter, campus, and feature options.

### 6.7 Data presentation

Replace large QTableWidget and per-cell-widget structures with:

- QAbstractItemModel.
- QTableView.
- Custom delegates.
- On-demand editors.
- Shared rendering state.
- Bounded or windowed data models.

Prioritize:

- Schedule tables.
- Speaking-evaluation batch views.
- Staff directories.
- Roster editors.
- Any view that creates one Qt object per cell.

Sub Prep class information is a priority model/view slice: its grade/level
navigation must remain lightweight, and class details must be presented in a
reusable selected-class view rather than one widget tree per class. See [Sub
Prep Class Information and Output Memory Plan](sub-prep-class-information-memory-plan.md).

The broader [Qt Rewrite Memory Hotspot Remediation
Plan](memory-hotspot-remediation-plan.md) extends the same presentation rule
to My Classes, Classes, Speaking Evaluation, schedule import review, Staff
Directory, and AI batch review. Shared primitives must support lazy row/page
creation, active-item editors, bounded review windows, and in-place model
refresh without retaining the previous widget tree.

### 6.8 Appearance preservation

Retain the current:

- QSS.
- Palette.
- Typography.
- Iconography.
- Spacing.
- Menu structure.
- Sidebar structure.
- Dialog style.
- Focus and selection states.

Change ownership and loading, not visual design.

## Deliverables

- PageHost.
- NavigationModel.
- PageDescriptor system.
- CommandBus.
- DialogService.
- NotificationService.
- Prompt test/automation adapter with semantic request and response coverage.
- PreferencesModel.
- ThemePresenter.
- Shared model/view table primitives.
- Shared UI tests and screenshot baselines.

## Exit gate

The v2 shell navigates between placeholder pages with correct labels, icons, permissions, themes, language, and action state without constructing unrelated pages.

At least one representative large table has been migrated to model/view and demonstrates bounded object creation.

## Heavy-route requirements

- For every Phase 6 slice, use the heavy route: carry the slice through its
  page, lifecycle, command, and presentation boundaries, verify recreation and
  release, and remove temporary shared-UI bridges after acceptance.
- Do not preserve BasePage as a universal god-object.
- Do not move hidden global behavior into PageHost.
- Do not let view models become replacement service facades.
- Do not use a table widget for convenience when the data can be represented by a model and delegate.
- Do not change appearance without a captured parity comparison.
