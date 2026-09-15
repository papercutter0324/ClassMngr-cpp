# Phase 8 — Platform and Output Adapters

## Status

- Status: Not started
- Default route: Heavy
- Depends on: Phases 2, 3, and 7
- Blocks: Cross-platform parity and final release
- Owner: Unassigned
- Last updated: 2026-09-15
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

Isolate:

- Page layout.
- Resource lookup.
- Font embedding.
- Image loading.
- PDF page creation.
- Print preview.
- Printer selection.
- Output file handling.

Avoid retaining source documents, rendered pages, images, and output buffers simultaneously.

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

- Do not duplicate business rules in platform implementations.
- Do not leave PowerPoint or PDF objects owned by widgets indefinitely.
- Do not make resource updates a hidden part of application updates.
- Do not rewrite output formats and UI behavior simultaneously without parity fixtures.
