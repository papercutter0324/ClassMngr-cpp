# Shared staff directory captures

Matched captures compare the legacy and Next Native English and GS Team
directory pages. Each directory uses the 120-row fixture at 1024×768, Fusion,
and English/light or Korean/dark. Each image and manifest is copied from its
matching standard `build/macos-clang-debug/shared-staff-directory-capture/`
output directory (`native_en_US_light`, `native_ko_KR_dark`, `gs_en_US_light`,
or `gs_ko_KR_dark`).

| Directory | English / light | Korean / dark |
|---|---|---|
| Native English, legacy | [capture](legacy_native_en_US-light.png) | [capture](legacy_native_ko_KR-dark.png) |
| Native English, Next | [capture](next_native_en_US-light.png) | [capture](next_native_ko_KR-dark.png) |
| GS Team, legacy | [capture](legacy_gs_en_US-light.png) | [capture](legacy_gs_ko_KR-dark.png) |
| GS Team, Next | [capture](next_gs_en_US-light.png) | [capture](next_gs_ko_KR-dark.png) |

Manifests: [Native English, English/light](manifest_native_en_US-light.json),
[Native English, Korean/dark](manifest_native_ko_KR-dark.json),
[GS Team, English/light](manifest_gs_en_US-light.json), and
[GS Team, Korean/dark](manifest_gs_ko_KR-dark.json).

Main and Tester reviewed all eight images. Content and layout align closely;
the remaining visible difference is the Next table bottom edge, about 6 px
lower. These Fusion/offscreen captures do not establish native-window behavior.

Capture assertions check header order, row and column counts, and every
`Qt::EditRole` cell value against direct `WorkspaceStore` snapshot order. They
assert sorting is disabled and table items and cell widgets are zero, and fail
if either surface uses `QTableWidget`. Next keeps 120 compact records for 120
rows rather than allocating one table item per cell. The shared model/view
supports in-place refresh, active-cell editors, and separate typed store
branches.
`refreshFromStore()` is the seam for candidate #19; normal teacher workbook
import is not part of candidate #18.

Offscreen Bootstrap verification passed 3/3 while emitting missing-icon and
unsupported-operation warnings.

The manifests record before-page, visible-page, and after-release process
snapshots. The table shows visible-surface resident/peak MiB from sequential
whole-process macOS snapshots:

| Directory / locale | Legacy resident / peak | Next resident / peak |
|---|---:|---:|
| Native English / English, light | 72.23 / 72.23 | 77.00 / 77.00 |
| Native English / Korean, dark | 75.77 / 75.77 | 79.53 / 79.53 |
| GS Team / English, light | 73.17 / 74.70 | 76.88 / 76.88 |
| GS Team / Korean, dark | 75.66 / 75.66 | 79.48 / 79.48 |

Values are MiB. Each sample covers the whole process, including Qt, the fixture
workspace, and shared runtime state; snapshots are sequential and can vary
slightly between runs. They are not per-page allocations or a memory budget.
The macOS provider does not report handle-count or private-dirty metrics; zero
defaults for those fields in the JSON are unsupported, not measurements. No
full suite, Windows run, or Phase 7 exit gate was verified. The 7A screenshot
comparison and memory evidence and the 7A/Phase 7 exit gates remain open.
