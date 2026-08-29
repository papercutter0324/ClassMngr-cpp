# Phase 7 — Media, Output, and OS Services

> Progress is tracked in [00-START-HERE.md](00-START-HERE.md).

## Goal

Replace the remaining Qt-dependent document, media, output, update, and Windows
integration paths with reviewed engine and Windows implementations.

## Implementation Sequence

1. Implement bounded image decode, orientation, color handling, thumbnails,
   caching, and cache release using Windows-compatible services.
2. Select and pin a PDF backend after rendering, search, zoom, security,
   maintenance, and license review. WinUI 3 is not itself a PDF parser.
3. Render engine-owned report/page models for preview, PDF, and print. Preserve
   pagination, fonts, Korean text, images, margins, and failure handling.
4. Integrate native print UI and verify cancellation, printer errors, offline
   printers, and output consistency.
5. Move spreadsheet parsing, ZIP/document packaging, and file exports into the
   engine or narrow Windows adapters with atomic writes and cleanup.
6. Preserve the PowerPoint workflow behind an engine automation interface;
   harden timeout, cancellation, privacy notices, and temporary-file cleanup.
7. Port application/resource-pack networking, signature verification, update
   discovery, and installer handoff without changing manifest/signature
   compatibility or rollback policy.
8. Validate file associations, URL launch, clipboard, notifications, and other
   remaining OS services in packaged-test and unpackaged-release contexts as
   applicable.

## Validation

- Golden output fixtures cover PDF, print, reports, spreadsheets, packages,
  Korean text, and representative failure paths.
- Long document sessions keep memory, GPU resources, file handles, and
  temporary files bounded.
- Update and resource-pack signature, rollback, offline, cancellation, and
  partial-download tests pass.
- PDF, image, archive, networking, signature, PowerPoint, and other native
  dependencies build for both x64 and x86; architecture-specific smoke tests
  reject missing or mixed-bitness binaries.
- No Windows release path loads Qt PDF, Qt PrintSupport, Qt Network, QML, or
  another Qt runtime component.

## Exit Gate

PDF viewing, printing, reports, imports/exports, resource packs, application
updates, and PowerPoint automation pass parity and failure-path tests without
Qt in the Windows process or package.
