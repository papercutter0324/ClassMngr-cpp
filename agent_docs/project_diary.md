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
- Build evidence must be checked against CMake: the current top-level Qt
  requirement is 6.12.0, despite the older 6.11.1 floor stated in
  `BUILDING.md`.
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
