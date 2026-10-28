# Classes Details/Times and Co-Teacher

Candidate #25 was accepted at source/test commit `c0cb9c93`.

## Capture set

The paired capture used the tracked [96-class fixture](../../../release/large-classes-visual-states/generated-large-classes-visual.tps), selected class ID `1`, and a 1024×768 viewport. It covers Legacy and Next for Details/Times and Co-Teacher, English (`en_US`) and Korean (`ko_KR`), and light and dark themes: eight pairs and 16 PNGs. The [JSON manifest](classes_details_coteacher_paired_capture.json) records the capture metadata.

| Section | Locale / theme | Legacy | Next |
|---|---|---|---|
| Details/Times | en_US / light | [PNG](classes_details_times_en_US_light_legacy.png) | [PNG](classes_details_times_en_US_light_next.png) |
| Details/Times | en_US / dark | [PNG](classes_details_times_en_US_dark_legacy.png) | [PNG](classes_details_times_en_US_dark_next.png) |
| Details/Times | ko_KR / light | [PNG](classes_details_times_ko_KR_light_legacy.png) | [PNG](classes_details_times_ko_KR_light_next.png) |
| Details/Times | ko_KR / dark | [PNG](classes_details_times_ko_KR_dark_legacy.png) | [PNG](classes_details_times_ko_KR_dark_next.png) |
| Co-Teacher | en_US / light | [PNG](classes_coteacher_en_US_light_legacy.png) | [PNG](classes_coteacher_en_US_light_next.png) |
| Co-Teacher | en_US / dark | [PNG](classes_coteacher_en_US_dark_legacy.png) | [PNG](classes_coteacher_en_US_dark_next.png) |
| Co-Teacher | ko_KR / light | [PNG](classes_coteacher_ko_KR_light_legacy.png) | [PNG](classes_coteacher_ko_KR_light_next.png) |
| Co-Teacher | ko_KR / dark | [PNG](classes_coteacher_ko_KR_dark_legacy.png) | [PNG](classes_coteacher_ko_KR_dark_next.png) |

## Scope and evidence

- Next has 210 page widgets while active and after suspension, one selected editor root with 162 descendants, and zero class-scoped widgets. Class-bound state clears on suspension. Legacy records 1,148 Details widgets, 632 Co-Teacher widgets, and 192 class-scoped widgets. Focused page tests separately verify one reusable editor across 96 classes and lifecycle cleanup.
- Independent Tester inspection covered the English/light and Korean/dark pairs for both sections. Legacy `ClassesPage` and Next `PageHost`/`ClassesNavigatorPage` have substantially different layout and shell scope; these captures do not establish full visual parity. The fixture stores `Level 02`, absent from the selected class's legacy E4 catalog: the Legacy Details selector appears blank while Next shows the stored value. This is a fixture/catalog limitation, not evidence of a parity difference.
- Sequential macOS whole-process snapshots are roughly 93.8–99.2 MB working set and 35.2–39.0 MB private usage. They do not measure page allocation or establish a memory budget.
- The corrected focused CTest selection passed 21/21. The Manual save test passed 3 QtTest results including setup/cleanup; manual and automatic behavior was correct. The `ClassMngrNext`, Bootstrap, PageHost, Classes navigator UI/WorkspaceStore, and related application-contract targets built. The route boundary scan, `NextStartupWorkspaceDependencyBoundary`, resource inventory `--check` (194 runtime records), and `git diff --check` passed.

These captures do not close the 7A screenshot comparison or memory evidence, or the 7A and Phase 7 exit gates. No full-suite, cross-platform, or full visual-parity result is claimed.
