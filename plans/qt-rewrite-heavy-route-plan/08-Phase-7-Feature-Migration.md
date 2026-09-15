# Phase 7 — Feature Migration

## Status

- Status: Not started
- Default route: Heavy
- Depends on: Phases 2 through 6
- Blocks: Final parity, memory hardening, and cutover
- Owner: Unassigned
- Last updated: 2026-09-16
- Current note: Migrate complete vertical slices, not isolated UI files.

## Objective

Move every feature from domain behavior through persistence, application use cases, presentation, resources, output, localization, and tests.

## Vertical-slice rule

A feature is not migrated when its page compiles. It is migrated only when:

- Domain behavior is implemented.
- Persistence behavior is implemented.
- Application use cases are used.
- The new page matches the baseline.
- Resource loading is scoped and observable.
- Output behavior matches.
- English and Korean are covered.
- Light and dark themes are covered.
- Existing files and imports work.
- Memory behavior is measured.
- Old call paths are removed.

## Migration order

### 7A — Setup and workspace/file workflows

Migrate:

- Initial setup.
- New/open/save/save-as.
- Recent files.
- Backups and recovery.
- Legacy database import.
- .tps handling.
- Import and export errors.

Break the large setup wizard into small screens driven by application commands and explicit state.

Acceptance:

- New users can complete setup.
- Existing supported workspaces open.
- Failed setup and failed migration preserve recoverability.

### 7B — My Workspace and personal information

Migrate:

- Personal details.
- Signature image.
- Typed signature rendering.
- My Classes.
- Workspace calendar.
- Upcoming events.
- Birthdays.
- Workspace navigation.

Load only the initial workspace data required for the visible page. Calendar and schedule detail must not be fully materialized just because the workspace exists.

### 7C — Teachers and staff

Migrate:

- Teacher information.
- Teacher imports.
- Native-English teacher view.
- GS team.
- Staff directory.
- Birthday views.

Use compact models and shared records. Do not keep duplicate teacher datasets in repository, service, page, and table layers.

### 7D — Classes

Migrate:

- Class list.
- Class details.
- Co-teachers.
- Notes.
- Analytics.
- Testing classes and blocks.
- Speaking-evaluation defaults.
- Class-transfer behavior.

Split the current large class page into section presenters sharing explicit class application state.

### 7E — Schedule and import workflows

Migrate:

- Schedule display.
- Schedule editing.
- Workbook parsing.
- Teacher/class matching.
- Conflict planning.
- Import review.
- Testing assignments.
- Schedule output.
- Schedule printing.

Use streaming or bounded workbook processing. Release parser workbooks, import plans, review data, and temporary render data as soon as their stage completes.

This is a priority memory feature. The schedule view must not recreate a cell-widget-per-entry architecture.

### 7F — Calendar

Migrate:

- Calendar repository and model.
- Workbook import.
- Event filtering.
- Event editing.
- Preferences.
- Month-grid display.

Keep the current QML month grid behind an adapter initially if required for visual parity. Measure its memory cost before choosing a QWidget replacement.

### 7G — Rosters

Migrate:

- Student model.
- Roster editor.
- Headers and delegates.
- Imports.
- Transfers.
- Template selection.
- Printing.

Instantiate editors only for active rows or cells. Load roster designs and print assets on demand.

### 7H — Speaking evaluations

Migrate:

- Evaluation persistence.
- Evaluation grid.
- Private notes.
- Comment editing.
- Batch workflows.
- Analytics.
- Reports.
- AI-assisted comment workflow.
- PDF output.
- PowerPoint automation.
- Export jobs.

Preserve the current behavior of opening the configured external AI website. Do not introduce a hosted AI API as part of this rewrite.

Report images, PowerPoint objects, PDF documents, and temporary workspaces require explicit lifetime and cleanup tests.

### 7I — Campus and documents

Migrate:

- Campus information.
- Directions.
- Address.
- Housing.
- Maps.
- Campus staff.
- Document catalog.
- Document viewer.

Load catalog metadata and localized names initially. Do not load PDF bodies or
render QtPdf pages while building the catalog or starting the application.
When the user opens a document, load only the selected PDF into the active
viewer session; close and release it when the document is replaced, the viewer
is closed, or the page is left, suspended, or released. Reopening may load the
document again. Load campus maps and campus JSON only when opened, and decode
images at a bounded display resolution.

### 7J — Substitute preparation and output

Migrate:

- Substitute-preparation page.
- Class-information model.
- Document package generation.
- PDF rendering.
- Printing.
- Output-folder handling.

Stream or stage large outputs instead of retaining source documents, rendered pages, and final buffers simultaneously.

## Per-feature deliverables

Every subphase must produce:

- Application use cases.
- Persistence integration.
- New presenter/view model.
- New Qt view.
- Resource policy.
- On-demand document-load and viewer-release trace for document features.
- Output adapter usage.
- Localization updates.
- Unit and integration tests.
- Visual parity screenshots.
- Memory measurement.
- Old call-path removal list.

## Phase-level exit gate

Every feature in the preservation matrix works in v2, and no migrated feature requires DataService, old page ownership, resource-pack leases, or direct filesystem path guessing.

## Heavy-route requirements

- Migrate complete vertical slices.
- Do not leave old and new code paths permanently interleaved.
- Do not accept a feature based only on a happy-path screenshot.
- Test large datasets, imports, errors, printing, localization, and repeated navigation.
- Keep the current user workflow unless a deliberate product change is separately approved.
