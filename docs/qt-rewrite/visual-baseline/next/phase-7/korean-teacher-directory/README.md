# Korean teacher sidebar directory captures

Matched legacy and Next sidebar captures use
`tests/fixtures/workspaces/large_startup.sql` (24 Korean teachers), Fusion
style, `QT_QPA_PLATFORM=offscreen`, and a 1024×768 viewport. The Campus Staff
subtree contains 28 items and 12 widgets.
Both trees use Inter 14 pt, 12 px indentation, and 30 px rows, matching the
legacy standard.

| Surface | English / light | Korean / dark |
|---|---|---|
| Legacy | [capture](legacy_en_US-light.png) | [capture](legacy_ko_KR-dark.png) |
| Next | [capture](next_en_US-light.png) | [capture](next_ko_KR-dark.png) |

Main reviewed all four captures. The only expected localized difference is
“My Workspace” text. [English/light manifest](manifest_en_US-light.json) and
[Korean/dark manifest](manifest_ko_KR-dark.json) record fixture, dimensions,
parity metrics, and process snapshots. Sources are the standard outputs under
`build/macos-clang-debug/korean-teacher-directory-visual-capture/` and
`build/macos-clang-debug/korean-teacher-directory-visual-capture-ko/`.

With both surfaces alive, whole-process snapshots were:

| Locale/theme | Resident | Peak resident | Physical footprint | Internal resident |
|---|---:|---:|---:|---:|
| English / light | 74.11 MiB | 74.16 MiB | 30.38 MiB | 29.75 MiB |
| Korean / dark | 79.38 MiB | 79.42 MiB | 32.45 MiB | 31.83 MiB |

These are whole-process measurements, not per-list deltas; they establish no
memory budget and do not close the 7A or Phase 9 memory gates. The 7A screenshot
comparison and 7A/Phase 7 exit gates remain open.
