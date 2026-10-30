# Matched Classes entry captures

This package pairs the Legacy Classes entry image with a Next Classes entry
image for the same tracked workspace, selected class, locale, theme, and
800×600 viewport. Both images show their complete captured window. The Legacy
source is the existing `classes-entry.png` reference. Next captures the
`ApplicationShell` window with one test-registered Classes page; it does not
load the production navigation tree. The Next route label comes from the
production `QObject::tr("Classes")` string and compiled locale catalog.

| Locale | Theme | Legacy | Next |
|---|---|---|---|
| English (`en_US`) | Light | [PNG](en_US-light/legacy_classes_en_US_light.png) | [PNG](en_US-light/next_classes_en_US-light.png) |
| English (`en_US`) | Dark | [PNG](en_US-dark/legacy_classes_en_US_dark.png) | [PNG](en_US-dark/next_classes_en_US-dark.png) |
| Korean (`ko_KR`) | Light | [PNG](ko_KR-light/legacy_classes_ko_KR_light.png) | [PNG](ko_KR-light/next_classes_ko_KR-light.png) |
| Korean (`ko_KR`) | Dark | [PNG](ko_KR-dark/legacy_classes_ko_KR_dark.png) | [PNG](ko_KR-dark/next_classes_ko_KR-dark.png) |

The fixture is
[`generated-large-classes-visual.tps`](../../../../release/large-classes-visual-states/generated-large-classes-visual.tps):
96 regular classes and 24 teachers. Each image shows class ID 25 selected.
The Next capture copies the fixture to a temporary directory before reading
it, leaving the tracked fixture untouched. Legacy images come from the four
`classes-entry.png` references under `release/large-classes-visual-states/`.

[`matched-full-shell-captures.json`](matched-full-shell-captures.json) records
fixture provenance and SHA-256, source Legacy paths, image SHA-256 hashes,
locale, theme, route label, class ID, and exact 800×600 viewport. Next used
Fusion, the packaged locale theme, the production Korean catalog, and scale
factor 1 in the offscreen capture test.

The applications use different navigation hierarchies and Classes
presentations. The Next image includes its entire captured `ApplicationShell`
window but has only the test-registered Classes route, while Legacy is the
existing full-application reference. These captures support visual review and
do not claim pixel parity. They cover only the populated 96-class entry with
class 25 selected; empty, dialog, and error full-window captures remain
pending. The existing focused tests cover selected empty, dialog, and error
behavior, as listed in the preservation matrix. No cross-platform appearance
or memory budget is established.

## Capture command

```sh
cmake --build build/macos-clang-debug --target ClassMngrNextClassesFullShellVisualCapture -j2
ctest --test-dir build/macos-clang-debug --output-on-failure -R '^ClassMngrNextClassesFullShellVisualCapture$'
```

The target writes captures beneath
`build/macos-clang-debug/classes-full-shell-visual-capture/`; the tracked
directory contains the retained PNGs and manifest.
