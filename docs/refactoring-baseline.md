# Refactoring Baseline

This document tracks the behavior and build measurements used by Plan 1. The
measurements are evidence for comparing refactoring changes; they are not hard
performance gates.

## Reproducing a baseline

Run the recorder from a clean checkout. It configures the selected preset,
performs a clean build, runs the complete CTest suite, and writes a JSON report.

Windows x64:

```powershell
python scripts/record_refactoring_baseline.py `
  --configure-preset windows-x64-debug `
  --build-dir build/windows-x64-debug `
  --configuration Debug `
  --output artifacts/baseline/windows-x64-debug.json
```

Linux x64:

```bash
python scripts/record_refactoring_baseline.py \
  --configure-preset linux-gcc-debug \
  --build-dir build/linux-gcc-debug \
  --output artifacts/baseline/linux-gcc-debug.json
```

macOS universal:

```bash
python scripts/record_refactoring_baseline.py \
  --configure-preset macos-clang-debug \
  --build-dir build/macos-clang-debug \
  --output artifacts/baseline/macos-clang-debug.json
```

The recorder defaults to two parallel build jobs so results are repeatable on
developer machines and do not depend on the generator's unlimited default. Use
the same `--parallel` value when comparing two revisions. The report records
configure, clean-build, and test duration; application size; root CMake length;
CTest totals; and how often production source files appear in the compilation
database. The last measurement will show whether later target work actually
stops tests from recompiling production sources. It also fingerprints the source
tree before and afterward and rejects a run if HEAD or source content changes
while it is in progress.

## Characterization coverage

The current test suite already protects the highest-risk behavior named in the
plan:

- SQL lifecycle and repository behavior: `data_service_lifecycle_tests.cpp`,
  `calendar_event_repository_tests.cpp`, `intensive_slot_state_repository_tests.cpp`,
  `testing_block_repository_tests.cpp`, and `testing_class_repository_tests.cpp`.
- Save modes and delayed UI behavior: `teacher_info_page_tests.cpp`,
  `staff_directory_page_tests.cpp`, `classes_page_tests.cpp`, and
  `testing_classes_page_tests.cpp`.
- Dialog results: import, schedule, sub-prep, roster, and speaking-evaluation
  dialog/service tests.
- Import/export naming: `class_transfer_tests.cpp`, `teacher_import_tests.cpp`,
  `schedule_import_tests.cpp`, `speaking_eval_batch_report_service_tests.cpp`,
  and `database_file_format_tests.cpp`.

Add focused tests alongside each consolidation when this coverage does not pin
down the exact behavior being moved.

## Recorded results

| Platform | Commit | Configure | Clean build | Tests | Application size | Report |
| --- | --- | ---: | ---: | --- | ---: | --- |
| Windows 11 x64 Debug | `3a0ae33` | 7.803 s | 120.854 s | 48/49 passed | 89,211,392 bytes | `artifacts/baseline/windows-x64-debug.json` |
| Linux x64 Debug | Pending | Pending | Pending | Pending | Pending | `artifacts/baseline/linux-gcc-debug.json` |
| macOS universal Debug | Pending | Pending | Pending | Pending | Pending | `artifacts/baseline/macos-clang-debug.json` |

Platform results must come from that platform. Do not copy a Windows result into
the Linux or macOS rows.

The Windows run used CMake 4.4.2, Qt 6.11.1, Python 3.14.7, and two build jobs.
HEAD was `3a0ae33`; the working tree also contained the Plan 1 CMake and
characterization changes in this slice. Its exact fingerprint is stored in the
JSON report and was unchanged throughout the run. The one failure is
`ClassMngrClassesPageTests::navigationControlsUsePillsAndPersistScopeSelection`:
with the offscreen platform plugin, the settings button is 26 px high and the
weekend pill is 25 px high. This assertion comes from the concurrent tab-bar
height work in `3a0ae33`; it is recorded here and is not hidden or counted as a
Plan 1 regression.

## Duplication report

The `Duplication report` workflow runs jscpd 4.0.5 with the checked-in
`.jscpd.json` configuration and uploads its JSON output. Its threshold is set so
duplication remains advisory: the report is meant to identify shared policy,
not reward arbitrary helper extraction.
