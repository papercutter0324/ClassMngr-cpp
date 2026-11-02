# W7.5.1 Windows output-sub-prep evidence

- Task/run identity: W751-WINDOWS-RUN; parent run w751f-20261101T171718Z.
- Route: output-sub-prep, with PDF capture enabled.
- Source revision: 31a71e90f304400fd99b70698f986d64eb7e8a01; the run manifest records a clean checkout.
- Source run root: C:/Users/wfelt/AppData/Local/Temp/w751f-20261101T171718Z/e/w751-31a71e90-windows-output-sub-prep.

## Recorded result and limits

The Release application and Debug startup-test harness builds completed, and the focused route produced both PDFs, workflow data, route metadata and screenshots. The recorded validator exited 1 on semantic parity: generated Sub Prep pages 1–10 match the historical normalized text, while pages 11–12 differ in the assignment of Teachers 22–24 across the page break. The five roster pages match. See windows-environment-and-outcome.md, validation-summary.json and output/sub-prep/sub-prep-output-parity.json for the full recorded result. This bundle does not claim that the route passed.

## Included

The original relative run paths are retained for both raw PDFs, generated fixture snapshot, route/parity JSON, workflow trace and workflow JSON, page and viewer screenshots, generated and historical page 11–12 renders with extracted text, run and environment records, validation summary, and configure/build/install/route/validation logs.

The run records fixture SQL SHA-256 1c5759f9df9f20849939aeb8a3f1932c95f256485cb5eb2bfd0f384a93dab8b4. The tracked [large_startup.sql](../../../tests/fixtures/workspaces/large_startup.sql) has the same SHA-256. The generated database snapshot SHA-256 is 4b190baa546508ec076158ac6f668c11a1fdff6884c77f909068247887431667, and the reference date is 2026-09-14. The generated .tps snapshot is included. Historical page inspection images and text are included; the historical source PDF itself was not retained in that run folder.

## Omitted source files

Build trees and package binaries, CMake inspection build outputs, Edge browser profile data, and inspection helper sources are excluded. The route's expected output/sub-prep/manifest.json was not present in the source run because the semantic assertion failed before it was written; it is recorded as missing by validation. The raw evidence files were copied without edits.

SHA256SUMS.txt covers every included raw evidence file and this README using forward-slash relative paths; it does not hash itself.

