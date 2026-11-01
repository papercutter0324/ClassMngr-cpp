# Initial Setup Wizard Visual Evidence — Phase 7

This package retains 28 Legacy/Next screenshots for seven visible setup
stages. Each route was captured in English/light and Korean/dark, giving 14
matched view-state pairs. The existing personal-details package already has
all four locale/theme combinations and remains at
[`../initial-setup-personal-details/README.md`](../initial-setup-personal-details/README.md).

| Stage | English / light | Korean / dark |
|---|---|---|
| Teacher import step | [Legacy page](legacy-initial-setup-teacher-import-en_US-light.png) / [Next section](next-initial-setup-teacher-import-en_US-light.png) | [Legacy page](legacy-initial-setup-teacher-import-ko_KR-dark.png) / [Next section](next-initial-setup-teacher-import-ko_KR-dark.png) |
| Teacher import dialog | [Legacy dialog](legacy-initial-setup-teacher-import-dialog-en_US-light.png) / [Next dialog](next-initial-setup-teacher-import-dialog-en_US-light.png) | [Legacy dialog](legacy-initial-setup-teacher-import-dialog-ko_KR-dark.png) / [Next dialog](next-initial-setup-teacher-import-dialog-ko_KR-dark.png) |
| Teacher entry | [Legacy page](legacy-initial-setup-teacher-entry-en_US-light.png) / [Next section](next-initial-setup-teacher-entry-en_US-light.png) | [Legacy page](legacy-initial-setup-teacher-entry-ko_KR-dark.png) / [Next section](next-initial-setup-teacher-entry-ko_KR-dark.png) |
| Class details | [Legacy page](legacy-initial-setup-class-details-en_US-light.png) / [Next section](next-initial-setup-class-details-en_US-light.png) | [Legacy page](legacy-initial-setup-class-details-ko_KR-dark.png) / [Next section](next-initial-setup-class-details-ko_KR-dark.png) |
| Class times | [Legacy page](legacy-initial-setup-class-times-en_US-light.png) / [Next section](next-initial-setup-class-times-en_US-light.png) | [Legacy page](legacy-initial-setup-class-times-ko_KR-dark.png) / [Next section](next-initial-setup-class-times-ko_KR-dark.png) |
| Schedule import | [Legacy page](legacy-initial-setup-schedule-import-en_US-light.png) / [Next section](next-initial-setup-schedule-import-en_US-light.png) | [Legacy page](legacy-initial-setup-schedule-import-ko_KR-dark.png) / [Next section](next-initial-setup-schedule-import-ko_KR-dark.png) |
| Finish | [Legacy completion page](legacy-initial-setup-finish-en_US-light.png) / [Next completed state](next-initial-setup-finish-en_US-light.png) | [Legacy completion page](legacy-initial-setup-finish-ko_KR-dark.png) / [Next completed state](next-initial-setup-finish-ko_KR-dark.png) |

## Capture provenance

Legacy uses `InitialSetupWizard` with a temporary `.tps` database. Next uses
`ApplicationBootstrap` with a profile in `QTemporaryDir`. Both routes use only
the synthetic values below; the importer dialogs show an empty workbook
selection and do not load or apply a workbook.

- Teacher: Alex Kim / 김선생님, preferred spelling “Kim Seonsaeng”, room 101.
- Class: E4, Theseus, Reading Explorer 1, essay book 4A, color `#123456`.
- Regular time: Wednesday, 4:00 PM–4:55 PM.
- The Next screenshot label replaces the random temporary profile path with
  `setup-profile.tps`. The temporary workspaces are removed when the tests
  exit.
- No input workbook or retained fixture file is used for these captures.

The complete reproducibility and image-integrity record is in
[`capture-metadata.json`](capture-metadata.json), including each PNG’s pixel
dimensions, byte size, and SHA-256.

## Visual review

- Legacy presents teacher import and teacher entry as separate wizard pages.
  Next puts manual teacher entry and an “Import Teachers from Workbook” action
  in one Teacher Information section. The two page captures show those entry
  surfaces; the separate blank-workbook modal pair records the importer UI.
  Neither route loads a teacher workbook, so workbook preview and apply states
  are outside this package.
- Both class-details states display the same synthetic teacher, grade, level,
  color, and book selections. Legacy calls the screen “Create Your First
  Class”; Next labels the section “Class Details” within the Initial Setup page.
- Both class-times states display Wednesday 4:00–4:55 PM. Legacy uses a full
  wizard page. Next keeps the schedule editor in a scrollable Initial Setup
  section with the application navigation visible.
- Legacy schedule import is its own wizard page, before personal details and
  class creation. Next shows Optional Schedule Import after saving class
  details and times, with “Import Schedule” and “Skip Schedule Import and
  Finish Setup” actions. The screenshots show these controls without opening
  an import workbook.
- Legacy has a dedicated “Setup complete” wizard page and Finish button. Next
  completes setup in place and shows a “Setup is complete” status with the
  Create Profile action; it has no separate completion page.
- Next translates the selected Korean locale, including its shell navigation
  and setup labels. Legacy has no Korean catalog for these wizard pages and
  retains English text in the Korean/dark captures.

The image pairs have matching viewport dimensions within each stage: 1024×720
for the setup pages, 1080×720 for class times, 1240×720 for schedule import,
and 460×680 for the teacher importer dialogs. The wider class-times view fits
Legacy’s 1040-pixel minimum; the schedule-import width keeps Next’s action row
fully visible without horizontal scrolling. These fixed-size Qt offscreen
renders support visual review; they do not establish pixel parity or native
desktop-window parity.

## Reproduction

From the repository root on macOS:

```sh
cmake --build build/macos-clang-debug \
  --target ClassMngrInitialSetupWizardTests ClassMngrNextBootstrapTests

ctest --test-dir build/macos-clang-debug --output-on-failure \
  -R 'ClassMngrInitialSetupWizardVisualCapture|ClassMngrNextInitialSetupVisualCapture'
```

The three focused capture tests passed on macOS 27.0.1, arm64, Qt 6.12.0, and
Apple LLVM 17.0.0. The Next capture test also emits the two matching
personal-details states into the build capture directory; this package omits
those duplicates because the sibling package retains all four combinations.
