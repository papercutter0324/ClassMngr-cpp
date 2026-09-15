# Phase 6 — Shared UI Rewrite

## Status

- Status: Not started
- Default route: Heavy
- Depends on: Phase 5
- Blocks: Feature migration and visual parity
- Owner: Unassigned
- Last updated: 2026-09-15
- Current note: Replace page/controller ownership with explicit presentation lifecycles.

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
- PreferencesModel.
- ThemePresenter.
- Shared model/view table primitives.
- Shared UI tests and screenshot baselines.

## Exit gate

The v2 shell navigates between placeholder pages with correct labels, icons, permissions, themes, language, and action state without constructing unrelated pages.

At least one representative large table has been migrated to model/view and demonstrates bounded object creation.

## Heavy-route requirements

- Do not preserve BasePage as a universal god-object.
- Do not move hidden global behavior into PageHost.
- Do not let view models become replacement service facades.
- Do not use a table widget for convenience when the data can be represented by a model and delegate.
- Do not change appearance without a captured parity comparison.
