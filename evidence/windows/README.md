# Windows Phase 7.5 evidence

This directory contains curated, selected Windows evidence for Phase 7.5. Bundle READMEs describe contents, known results and limitations, and omitted source files. Original raw evidence files were copied unchanged.

| Bundle | Selected raw evidence | Recorded outcome |
| --- | ---: | --- |
| [W7.5.1 output-sub-prep](w7.5.1-output-sub-prep/README.md) · [SHA256SUMS.txt](w7.5.1-output-sub-prep/SHA256SUMS.txt) | 44 files, 4,567,109 bytes | Sub Prep pages 1–10 match; pages 11–12 mismatch. All five roster pages match. |
| [W7.5.2 prior current Debug (9f59)](w7.5.2-current-debug/README.md) · [SHA256SUMS.txt](w7.5.2-current-debug/SHA256SUMS.txt) | 14 files, 512,434 bytes | Configuration passed; the all-target Debug build timed out. CTest did not run. |
| [W7.5.2 current-head Debug (4b5f)](w7.5.2-current-debug-head-4b5f/README.md) · [SHA256SUMS.txt](w7.5.2-current-debug-head-4b5f/SHA256SUMS.txt) | 14 files, 2,254,430 bytes | Post-fix all-target Debug build passed; CTest ran 614 tests and failed 39, so the current gate is not green. |
| [W7.5.3 F564/F598 capture](w7.5.3-f564-f598-capture/README.md) · [SHA256SUMS.txt](w7.5.3-f564-f598-capture/SHA256SUMS.txt) | 25 files, 59,263,799 bytes | Capture validation passed. The xperf parser diagnostics and their limits are recorded in the bundle README. |

Each SHA256SUMS.txt covers that bundle's included raw files and README, but does not hash itself.
