# My Classes captures

Source commit: `f7a3d81c`.

The eight images and [`manifest.json`](manifest.json) were copied from the
standard `build/macos-clang-debug/my-classes-visual-capture/` output. The capture uses
the tracked 96-class fixture, Fusion style, an offscreen platform, and a
1024×768 viewport.

| Locale/theme | Legacy | Next |
|---|---|---|
| English / light | [PNG](legacy_en_US_light.png) | [PNG](next_en_US_light.png) |
| English / dark | [PNG](legacy_en_US_dark.png) | [PNG](next_en_US_dark.png) |
| Korean / light | [PNG](legacy_ko_KR_light.png) | [PNG](next_ko_KR_light.png) |
| Korean / dark | [PNG](legacy_ko_KR_dark.png) | [PNG](next_ko_KR_dark.png) |

Main and the independent Tester accepted the matched captures. Grade/class
navigation and the full-width, teacher-first stacked details match the legacy
composition. Visible differences are Next's bold labels and plain values versus
legacy's outlined read-only controls, tighter selector spacing, and more
schedule text visible in Next because the legacy one-line control clips it.
The Korean Next page localizes the room label to “교실.”

The manifest records 96 active summary rows in one Next summary model/list,
one selected detail panel, and one requested class for each class, information,
and roster detail read. Details are released on suspension and summary rows
fall to zero. The fixture reports 69 active Next page widgets, compared with
321 legacy page widgets including 96 class widget shells; these counts describe
this capture fixture.

## Verification and limits

The build command was:

```sh
cmake --build build/macos-clang-debug --target ClassMngrNextMyClassesPageTests ClassMngrNextMyClassesVisualCapture ClassMngrNextBootstrapTests --parallel 8
```

Five focused CTests passed 5/5: `ClassMngrMyClassesPageTests`,
`ClassMngrMyClassesPageTeacherDisplayParityTests`,
`ClassMngrMyClassesPageSummaryParityTests`,
`ClassMngrNextMyClassesPageTests`, and
`ClassMngrNextMyClassesVisualCapture`. The targeted real-store A-to-B
replacement passed in `ClassMngrNextBootstrapTests`, slot
`myClassesReloadsRealWorkspaceDataWhenProfileIsReplacedWhileActive`.
`NextStartupWorkspaceDependencyBoundary` passed 1/1. The resource inventory
`--check` and `git diff --check` passed.

The full `ClassMngrNextBootstrapTests` run reported 66 passes and one failure
at the status-message assertion in
`recentRestoreRemembersDirectoryWhenHistorySaveFails`. That case uses a
recording gateway; this candidate does not modify its code path, and its
baseline is unverified.

For the English/light sequence, process-wide private-working-set samples were
9.56 → 27.69 → 31.05 MiB for legacy and 31.14 → 33.67 → 33.75 MiB for Next
(before, active, suspended). These are sequential process-wide observations;
they do not attribute memory to a page or establish a memory budget. The 7A
screenshot comparison and memory remediation and the 7A/Phase 7 exit gates
remain open.
