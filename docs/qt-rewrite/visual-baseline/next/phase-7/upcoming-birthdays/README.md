# Upcoming Birthdays captures

Source commit: `a36a1fb5`.

The four images and manifests were copied from the standard
`build/macos-clang-debug/upcoming-birthdays-visual-capture/` output. English/light
and Korean/dark legacy/Next pairs use a 1024×768 viewport; each dialog is
520×560. Main reviewed all four images, and the independent Tester confirmed
layout, content, and empty states. The fixture capture shows one birthday today,
one this week, and two next week, including Korean, Native English, and GS Team
names. The manifests also record empty-schedule assertions for both surfaces.

| Locale/theme | Legacy | Next | Manifest |
|---|---|---|---|
| English / light | [PNG](en_US_light/legacy.png) | [PNG](en_US_light/next.png) | [JSON](en_US_light/manifest.json) |
| Korean / dark | [PNG](ko_KR_dark/legacy.png) | [PNG](ko_KR_dark/next.png) | [JSON](ko_KR_dark/manifest.json) |

The “Don't show again today” checkbox writes today's local ISO date when the
dialog closes with it checked. Legacy has no automatic trigger or reader for
that setting; Next adds neither behavior.

## Verification and limits

The standard incremental `build/macos-clang-debug` build passed focused CTest
7/7. CMake source ownership validated 1,778 handwritten sources; the Next
dependency boundary, resource inventory `--check`, and `git diff --check` passed.
No full suite, Windows run, or Phase 7 exit gate was verified. No dirty-editor
page scenario or separate GS-only failure test was run.

Whole-process macOS snapshots, before dialog display → after dialog cleanup:

| Locale/theme | Working set | Private usage |
|---|---:|---:|
| English / light | about 44.30 → 66.19 MiB | 11.99 → 18.99 MiB |
| Korean / dark | 44.45 → 69.83 MiB | 12.14 → 21.45 MiB |

Memory remains in the process after dialog cleanup. These readings include the
whole process, are not feature-only deltas, and do not establish a memory
budget. The 7A screenshot comparison and memory evidence and the 7A/Phase 7
exit gates remain open.
