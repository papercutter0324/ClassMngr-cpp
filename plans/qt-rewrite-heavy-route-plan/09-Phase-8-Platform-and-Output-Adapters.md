# Phase 8 — Platform and Output Adapters

## Status

- Status: Not started
- Default route: Heavy
- Depends on: Phases 2, 3, and 7
- Blocks: Cross-platform parity and final release
- Owner: Unassigned
- Last updated: 2026-09-16
- Current note: Keep feature code platform-neutral and isolate external integrations.

## Objective

Move platform-specific behavior and large output workflows behind narrow, testable interfaces.

## Adapter interfaces

Create interfaces for:

- File dialogs.
- Clipboard.
- URL launching.
- Printing.
- PDF generation.
- PowerPoint automation.
- ZIP and package handling.
- Font loading.
- Window state persistence.
- Application update checking.
- Platform memory diagnostics.

Feature and domain code must depend on these interfaces, not directly on Windows, macOS, Linux, or external-application APIs.

## Work packages

### 8.1 File and shell integration

Preserve:

- Open and save dialogs.
- Recent files.
- Clipboard behavior.
- External URL launching.
- Application icon.
- Window geometry and splitter persistence.
- Platform-specific dialog behavior.

Keep the current user-visible behavior while moving implementation behind adapters.

### 8.2 Printing and PDF

Initially retain Qt PDF and printing to preserve output.

The QtPdf viewer is a separate on-demand path. Catalog initialization must not
load a PDF document or render pages. The adapter owns the loaded
`QPdfDocument` for the active viewer session and must close/release it when
the document is replaced, the viewer is closed, or the page is left or
released. Generated and print-output PDFs remain operation-scoped.

Isolate:

- Page layout.
- Resource lookup.
- Font embedding.
- Image loading.
- PDF page creation.
- QtPdf document load, status, page rendering, close, and release.
- Print preview.
- Printer selection.
- Output file handling.

Avoid retaining source documents, rendered pages, images, and output buffers simultaneously.

Sub Prep package generation and PDF/print output must consume the operation-
scoped projection defined by [Sub Prep Class Information and Output Memory
Plan](sub-prep-class-information-memory-plan.md). The adapter boundary must
not require the page to materialize class widgets or retain the page's rich UI
model while output is being generated.

The [Qt Rewrite Memory Hotspot Remediation
Plan](memory-hotspot-remediation-plan.md) extends this rule to speaking
reports and other large output workflows. Output adapters must consume
bounded application projections, render in chunks when necessary, and release
source records, HTML, rendered pages, and final buffers at explicit stage
boundaries.

### 8.3 PowerPoint automation

Preserve the current speaking-evaluation PowerPoint workflow.

Implement:

- Availability detection.
- External application connection.
- Workspace creation.
- Slide generation.
- Failure reporting.
- Cleanup.
- Cancellation.

PowerPoint objects and temporary files must be released even when an operation fails.

### 8.4 Packaging and application updates

The application updater remains a normal application feature.

The update package must contain:

- The executable.
- Required Qt deployment files.
- QML files if retained.
- Canonical resources.
- Required platform plugins.
- Licenses and metadata.

There must be one application update path, not an executable update path plus a resource update path.

### 8.5 Calendar presentation adapter

Keep the QML calendar implementation isolated behind a calendar view interface if it is required for appearance parity.

The adapter must:

- Load QML on calendar entry.
- Avoid creating QML objects during startup.
- Release the QML view when appropriate.
- Expose calendar data through a compact model.

### 8.6 Transfer and package memory

Class-transfer and package operations must follow the [Qt Rewrite Memory
Hotspot Remediation Plan](memory-hotspot-remediation-plan.md):

- write and read transfer data in bounded stages or through a streaming
  device where the format permits;
- avoid retaining raw JSON, parsed documents, domain packages, and preview
  copies simultaneously without a documented reason;
- preview compact summaries before loading full roster/evaluation details;
- transfer operation ownership explicitly into the dialog or job;
- release all temporary data on success, cancellation, validation failure, and
  output failure.

The existing transfer format and user-visible validation behavior remain the
parity contract.

## Deliverables

- Platform adapter interfaces.
- Windows, macOS, and Linux implementations.
- PDF and printing adapters.
- PowerPoint adapter.
- Application-update packaging integration.
- Calendar adapter.
- Adapter tests and failure handling.

## Exit gate

All external integrations work from v2 without platform-specific code leaking into domain, application, or feature logic.

Generated output matches the baseline on every supported platform.

## Heavy-route requirements

- For every Phase 8 slice, use the heavy route: move the behavior behind its
  narrow adapter boundary, verify the full output or platform workflow, and
  remove temporary direct-to-legacy paths after acceptance.
- Do not duplicate business rules in platform implementations.
- Do not leave PowerPoint, QtPdf, or PDF output objects owned by widgets
  indefinitely; active viewer documents are session-scoped.
- Do not make resource updates a hidden part of application updates.
- Do not rewrite output formats and UI behavior simultaneously without parity fixtures.
