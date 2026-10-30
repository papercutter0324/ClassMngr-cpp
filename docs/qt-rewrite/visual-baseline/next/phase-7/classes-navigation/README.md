# Classes navigator captures

Source commit: `368fcee8`.

The eight PNGs and [`manifest.json`](manifest.json) were copied from the
standard `build/macos-clang-debug/classes-navigator-visual-capture/` output.
They use Fusion at 1068×768, offscreen, with synthetic page data. English and
Korean captures cover light and dark themes at 6 and 96 classes. The manifest
records use of the compiled production Korean catalog.

| Locale/theme | 6 classes | 96 classes |
|---|---|---|
| English / light | [PNG](classes_en_US_light_small.png) | [PNG](classes_en_US_light_96.png) |
| English / dark | [PNG](classes_en_US_dark_small.png) | [PNG](classes_en_US_dark_96.png) |
| Korean / light | [PNG](classes_ko_KR_light_small.png) | [PNG](classes_ko_KR_light_96.png) |
| Korean / dark | [PNG](classes_ko_KR_dark_small.png) | [PNG](classes_ko_KR_dark_96.png) |

## Scope and navigation

Next `Classes` is a page-scoped navigator. It always shows grade groups and
All, including when there are six or fewer classes. It retains legacy class
ordering, labels, and disambiguation; offers Regular/Intensive schedule source,
weekday/weekend filtering, and All Classes/Active Schedule visibility; and
uses typed `ClassId` selection with an independent last selection for each
grade group and All. `My Classes` remains a separate route.

The page reads bounded class IDs/names and navigation summaries from the active
workspace through a path-scoped `WorkspaceStore` action and bounded
`ClassRepository`/`ClassInfoRepository` APIs. The Classes navigator’s Platform
adapter contains no SQL; it maps bounded repository results. Limits are 4,096
classes, 14 schedule rows per source/class, and bounded class,
summary, and schedule-token text. Over-limit reads fail instead of truncating.
Testing classes are excluded. Class details/editors, student counts,
create/delete, co-teachers, notes, analytics, rosters, evaluations, testing
workflows, and section deep links are outside this candidate.

## Model, lifecycle, and memory

The 96-class captures report three grade groups plus All, one page, one list
model/view, 266 total page widgets, zero per-class widgets, and one shared
selected class editor. Suspension clears model rows while retaining the
single shared editor and its fixed page structure. The manifest records the
SHA-256 of each generated image.
Suspension clears model rows; the page and its 41 fixed widgets remain. Active
whole-process observations range from 57,622,528 to 65,159,168 working-set
bytes and from 20,087,648 to 24,970,104 private-usage bytes. These observations
include the process and are not page allocations or a memory budget. Manifest
callback counts describe injected action calls, not SQL query counts.

## Legacy references and visual limits

The existing release references are [English/light](../../../release/large-classes-visual-states/english-light/classes-entry.png),
[English/dark](../../../release/large-classes-visual-states/english-dark/classes-entry.png),
[Korean/light](../../../release/large-classes-visual-states/korean-light/classes-entry.png),
and [Korean/dark](../../../release/large-classes-visual-states/korean-dark/classes-entry.png).
The original Next captures in this directory are synthetic page-only
navigation states. They remain useful for the small and 96-row model/widget
counts, but they do not match the Legacy shell captures. The new
[matched Classes entry captures](paired-full-shell/README.md) use the same
tracked 96-class workspace, class ID 25, locale, theme, and 800×600 viewport.
The Legacy images are existing full-application captures. Each Next image
shows the complete captured `ApplicationShell` window with one test-registered
Classes route; it does not include the production navigation tree. The route
label uses the production localized `QObject::tr("Classes")` string. The
manifest records fixture provenance and SHA-256 hashes. These pairs address
the missing matched populated-entry captures; they do not claim pixel parity
or cover other Classes states. The Korean Legacy references retain some
English labels. Empty/dialog/error full-window captures, the 7A screenshot,
memory evidence, and the Phase 7 exit gates remain open.

## Verification

An independent repaired-tree incremental build passed for the affected Data,
Next, bootstrap, navigator page/Store/capture, My Classes, PageHost, ordering,
repository, route, translation, and resource targets. The focused CTest
selection passed 15/15 in 20.20 seconds:

```sh
ctest --test-dir build/macos-clang-debug --output-on-failure -R '^(ClassMngrTestingClassRepositoryTests|ClassMngrClassRouteAvailabilityParityTests|ClassMngrClassesPageF1019NavigationSelectionTests|ClassMngrNextBootstrapTests|ClassMngrNextPageHostTests|ClassMngrNextNavigationTests|NextStartupWorkspaceDependencyBoundary|ClassMngrNextMyClassesPageTests|ClassMngrNextClassesNavigatorPageTests|ClassMngrNextClassesNavigatorVisualCapture|ClassMngrNextClassesNavigatorWorkspaceStoreTests|ClassMngrNextApplicationClassTabNavigationOrderingPolicyTests|ClassMngrNextApplicationTestingClassesOrderingPolicyTests|ClassMngrNextApplicationClassesNavigationSnapshotTests|ClassMngrNextApplicationClassesListReadQueryTests)$'
```

Both active-workspace lifecycle selectors also passed directly with
`QT_QPA_PLATFORM=offscreen` (four QtTest results including setup/cleanup).
Resource inventory passed with 187 direct resources and 7 generated runtime
inputs; `git diff --check` passed.

The pre-repair broad legacy
`ClassMngrClassesPageTests::navigationReleaseCachesSnapshotAndEditorsAcrossReentry`
timed out after 300,010 ms with 19 passes. No cause or baseline was established,
and it was not repeated. The optional adjacent
`ClassMngrNextPlatformApplicationServicesClassesListReadPortTests` target did
not link because of unresolved `FeatureService`, `ClassService::classes`, and
`DocumentCatalog` symbols; it did not run and was not rerun. The focused suite
uses the standalone Next WorkspaceStore path. No full-suite or Windows result
is claimed.
