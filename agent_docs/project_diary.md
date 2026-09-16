# Project Diary

## Decisions and Lessons

- The repository is intentionally layered: domain models and validation are
  separate from SQLite repositories, feature services, and shared Qt UI. Keep
  new work at the narrowest appropriate layer.
- `ApplicationServices` is the preferred application boundary. `DataService`
  remains as a compatibility facade while callers migrate; UI/controllers
  should not add direct repository usage.
- Feature-scoped assets are produced as standalone RCC resource packs. Do not
  assume every asset belongs in the main executable bundle; follow
  `cmake/resources.cmake` when changing packaging.
- Build evidence must be checked against CMake: the project, BUILDING.md,
  active CI workflows, and release helper now agree on Qt 6.12.0. Keep future
  examples aligned with that minimum.
- The Phase 0 memory contract separates the final `<250 MiB` end-of-rewrite
  normal target from the temporary `<512 MiB` diagnostic ceiling. Legacy heavy
  routes retain both comparisons as trend evidence; do not treat an over-target
  legacy route as a Phase 0 failure.
- When native desktop automation is unavailable, a small in-process Qt
  controller can drive real modal dialogs and capture the packaged offscreen
  state. The Sub Prep route now retains both valid-generation and disabled-OK
  validation references, and the Speaking Evaluation route retains the
  PowerPoint renderer-selection reference without claiming external Office
  automation ran.
- Generated output folders may receive sandbox-only ACLs. Before staging
  retained artifacts, verify the exact path and grant the normal Git identity
  read access only to that generated evidence folder; do not discard the
  generated PDFs.
- For packaged feature visuals, an opt-in environment variable on an existing
  Heavy-route lifecycle keeps the production path unchanged while allowing
  in-process screenshots after real selection and re-entry operations. The
  Classes visual slice uses `CLASSMNGR_STARTUP_CLASSES_VISUAL_OUTPUT_DIR` and
  retains four language/theme variants; it records capture success in the same
  startup profile as the lifecycle assertions.
- For a fast asynchronous UI boundary, capture the loading state immediately
  after the real action and retain the event-loop poll as a fallback. The
  Schedule Import slice uses the existing large-workbook route to retain
  `Loading workbook...`, indeterminate progress, disabled source/load controls,
  and a validated screenshot for both cancel and apply outcomes without
  changing the production path.
- When a loading control is below the visible fold, an evidence-only probe may
  move the existing scroll bar before grabbing the real dialog and restore its
  value afterward. Calendar Import uses this to show `Importing events...` and
  the disabled Import Events button without changing the production UI path or
  the later Preferences reference state.
- A Schedule Import conflict warning is only created when the projected
  preview actually contains overlapping meetings; a large fixture that merely
  contains invalid patterns does not exercise that modal. For an evidence-only
  boundary, make the cancel fixture overlap deterministic day/time slots, poll
  the existing event loop for the asynchronously queued `QMessageBox`, capture
  it, and keep the apply fixture conflict-free so the transaction path remains
  independently measurable.
- The Calendar Import parser-failure slice is not accepted until it has fresh
  Release evidence. On this host, the preset MSBuild tree hit a FileTracker
  `UnauthorizedAccessException`, an ambient Ninja tree could not resolve MSVC
  standard headers, and a correctly initialized Visual Studio shell still did
  not finish CMake generation even with `BUILD_TESTING=ON`. Treat compiler
  probes/optional-component notices or missing executables as an environment
  blocker, not as evidence that the source compiles; preserve the committed
  implementation for the next clean build attempt.
- The Calendar Import parser-failure boundary uses a deterministic 69-byte
  malformed local HTTP response. Its focused Release route must retain the
  complete workflow/metrics/manifest/trace, prove the real parser error,
  re-enable Import Events, preserve the event count, and omit success
  checkpoints. The unchanged success route and the opt-in-cleared full suite
  are separate required checks.
- When the Calendar Import error status is below the Preferences viewport fold,
  the evidence-only probe must move the existing Calendar-tab scroll bar to its
  maximum before grabbing `calendar-import-error.png`, process the view update,
  and restore the prior value. A non-empty screenshot alone is insufficient;
  manually verify that the real error text and re-enabled Import Events control
  are visible.
- Multi-config CMake generators previously exposed the default
  `Debug;Release;MinSizeRel;RelWithDebInfo` set. The source-level configuration
  guard now limits them to `Debug;Release`, while single-config presets retain
  their explicit build type.
- Release presets explicitly set `BUILD_TESTING=OFF`, and test targets without
  a QML module opt out of Qt import scanning. This removed the unnecessary
  `qmlimportscan` target fan-out seen in the Debug tree without changing test
  source or link ownership.
- The tracked `cmake/` tree remains split by source, resources, deployment,
  platform, and test concerns because those boundaries are active. Only the
  empty root-generated `CMakeFiles/` residue was removed; root CMake output is
  ignored, and build/dist artifacts are preserved.
- The Phase 0 platform contract is Windows x64 plus macOS universal. By user
  decision, Windows ARM64 and Linux are unofficial ports deferred to later;
  do not list them as Phase 0 blockers. A Windows x64 per-run validation pass
  is not the full Phase 0 exit gate while macOS evidence is missing.
- The `scripts/phase0/` runner uses existing opt-in packaged Qt routes and a
  fresh caller-selected evidence run directory; its plan mode is non-mutating.
  The validator checks artifacts, JSON/manifests, lifecycle checkpoints and
  memory trends, and can consolidate a macOS run. Use
  `--require-exit-gate` when automation must fail until all supported-platform
  evidence passes. Legacy 250 MiB measurements remain trend-only; 512 MiB is
  a diagnostic ceiling, not a Phase 0 pass criterion.
