# Qt Rewrite Phase 0 - File and Output Compatibility Matrix

Status: In progress
Snapshot: `75755460`

This matrix distinguishes formats that the application owns from files that it
only imports, stages, or generates. Exact fixture content and byte-level output
comparisons are still open Phase 0 work.

| Format/path | Current behavior | Read/write contract | Current evidence | v2 acceptance |
| --- | --- | --- | --- | --- |
| `.tps` | Native Teacher Profile database; created by New Teacher Profile and used by normal open/save flows | Create, open, save, save-as, recent-file restore, backup, close, export | `database_file_format_tests`, `data_service_lifecycle_tests`, schema/repository tests | Open/save round trip plus representative fixture and backup comparison |
| `.db` | Legacy Teacher Profile input accepted case-insensitively | Open/import legacy database without breaking current workspace; native output normalizes to `.tps` | `database_file_format_tests` covers recognition/path normalization | Legacy fixture import with data/relationships/output comparison |
| Class transfer `.json` | Export selected classes and related teachers, schedules, rosters, evaluations; import uses review/reconciliation dialog | Versioned JSON round trip; conflicts, replacement, teacher matching, atomic failure behavior | `class_transfer_tests`, `tests/fixtures/transfers/conflict_source.json`, and offscreen class-import review capture | Golden package plus conflict/rollback fixtures |
| Teacher workbook `.xlsx` | Teacher and campus staff import workflow | Read workbook, validate, review, apply partial/failed import behavior | `teacher_import_tests`, `teacher_import_dialog_tests` | Small/large workbook and failure fixture |
| Schedule workbook `.xlsx` | Regular/intensive schedule import workflow | Read workbook, select sheet/user/mode, review conflicts, apply or cancel | `schedule_import_tests`, `schedule_import_dialog_tests` | Large schedule and conflict fixture |
| Calendar import | Calendar event import workflow | Read supported calendar input and preserve dates/recurrence/locale behavior | `calendar_import_tests` | Representative calendar fixture and localized output |
| Campus JSON | One JSON document per campus in the installed/editable campus directory | Load catalog, save admin edits atomically, enumerate campuses; maps/images referenced by data | `campus_map_tests`, `campus_dashboard_page_tests` | Fixture catalog plus admin save/reload comparison |
| Documents catalog JSON | `resources/assets/documents/documents.json` indexes localized document names and PDF/PPTX assets | Validate schema, safe relative paths, localized names, load on demand | `document_catalog_tests` | Catalog fixture with missing/corrupt/unsafe entries |
| PDF input/output | PDF viewer opens generated or packaged PDFs; schedule, roster, speaking, and sub-prep workflows save/print PDFs | Catalog metadata is available without loading PDF bodies; QtPdf loads the selected document on request and releases it when the viewer session ends; preserve page selection, orientation, spacing/background preferences, output naming, and print behavior | schedule/roster/sub-prep/speaking report tests and PDF page implementation | Golden PDFs rendered to images, viewer open/close/reopen lifecycle checks, and print-preview screenshots |
| PowerPoint `.pptx` | Speaking evaluation output and packaged lesson/template documents | Preserve Windows/macOS external-app behavior, protected workspace notice, staged assets, generated output | speaking report/batch tests and PowerPoint notice tests | Platform-specific end-to-end fixture and output comparison |
| Backup files | Initial setup and file controller create temporary/backup paths around replacement flows | Never lose existing data during replacement/cancel/failure | `FileController` backup code; failure fixture pending | Failure-injection fixture and filesystem assertions |
| Update artifacts | Application updater downloads release metadata and platform binaries | Keep application update path; resource packs must not acquire a separate update path in v2 | updater tests and current update documentation | Packaged update fixture with resources included in application release |

## Current extension rules

- Native output is `.tps`.
- Legacy input is `.db`.
- Extension checks are case-insensitive.
- Extensionless native output receives `.tps`.
- Unknown input extensions are preserved by `supportedInputPath` rather than
  silently renamed.
- Class transfer files are saved/loaded as `.json`.
- Excel imports require `.xlsx` in the current dialogs.

## Open compatibility questions

1. Which schema versions and real-world legacy `.db` variants still occur in
   installed user data?
2. Which generated outputs require byte equality and which require rendered
   visual equivalence only?
3. Which PowerPoint operations are available on each supported platform and
   what failure message is part of the user-facing contract?
4. How should user-edited campus/document data be staged when resources become
   read-only application assets in v2?
