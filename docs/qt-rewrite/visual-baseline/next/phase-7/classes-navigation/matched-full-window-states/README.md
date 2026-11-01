# Matched Classes full-window states

These captures pair the production Legacy and Next Classes routes at 800×600.
They cover an empty list, delete confirmation for selected class ID 25, and a
Classes list-read error. Each pair uses `en_US`/light or `ko_KR`/dark. Legacy
opens the temporary workspace in `MainWindow` and follows its sidebar Classes
route; Next opens that same workspace through production `ApplicationBootstrap`
and its `ApplicationShell` navigation tree.

The empty v6 fixture was created by Legacy `ApplicationServices` with no class
rows. The two populated states use temporary copies of the tracked 96-class
fixture at
[`generated-large-classes-visual.tps`](../../../../release/large-classes-visual-states/generated-large-classes-visual.tps).
For the read-error state, the capture drops `testing_classes` from that copy;
reopening the latest-v6 workspace leaves the table absent, so the production
Classes list read fails. Each Legacy/Next pair uses the same temporary file and
SHA-256; the temporary fixtures are discarded after capture.

For modal states, the offscreen capture combines the shell grab with the
visible top-level `QMessageBox` grab at its shell-relative position. The Legacy
read-error warning is presented from the `PromptRequest` emitted by production
Classes navigation. Next shows that error inline without a modal. The pairs
match semantic states only; the navigation and page presentations differ, so
they do not claim pixel parity. See the
[`capture manifest`](matched-full-window-state-captures.json) for fixture,
modal, and image hashes.

In the Korean error pair, Legacy's catalog lacks the warning strings, so the
capture records its emitted English `PromptRequest`; Next translates the
heading but leaves technical details in English.

| State | Locale/theme | Legacy | Next |
|---|---|---|---|
| Empty | `en_US` / light | [PNG](empty/en_US-light/legacy.png) | [PNG](empty/en_US-light/next.png) |
| Empty | `ko_KR` / dark | [PNG](empty/ko_KR-dark/legacy.png) | [PNG](empty/ko_KR-dark/next.png) |
| Delete confirmation | `en_US` / light | [PNG](delete-confirmation/en_US-light/legacy.png) | [PNG](delete-confirmation/en_US-light/next.png) |
| Delete confirmation | `ko_KR` / dark | [PNG](delete-confirmation/ko_KR-dark/legacy.png) | [PNG](delete-confirmation/ko_KR-dark/next.png) |
| List-read error | `en_US` / light | [PNG](list-read-error/en_US-light/legacy.png) | [PNG](list-read-error/en_US-light/next.png) |
| List-read error | `ko_KR` / dark | [PNG](list-read-error/ko_KR-dark/legacy.png) | [PNG](list-read-error/ko_KR-dark/next.png) |

## Capture target

The focused target and CTest are registered in `cmake/tests/next.cmake`:

```sh
cmake --build build/macos-clang-debug --target ClassMngrNextClassesFullWindowStateVisualCapture -j2
ctest --test-dir build/macos-clang-debug --output-on-failure -R '^ClassMngrNextClassesFullWindowStateVisualCapture$'
```

CTest sets `QT_QPA_PLATFORM=offscreen` and `QT_SCALE_FACTOR=1`.
CTest writes generated captures to
`build/macos-clang-debug/classes-full-window-state-visual-capture/`. These
tracked images are the retained evidence; this README records the capture
scope and does not report a test result.
