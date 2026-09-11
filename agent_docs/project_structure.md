# Project Structure

## Directory Layout

- `src/engine` and `src/engine/include/classmngr/engine` contain the portable engine implementation and public headers.
- `src/core` contains shared application services such as settings, language, theme, updater, resource-pack, and platform support.
- `src/data` contains database sessions, schema support, and repositories; `src/domain` contains models, rules, and validation.
- `src/ui` contains shared Qt pages, widgets, styles, and UI utilities; `src/features` contains feature-specific UI and services; `src/app` contains the application shell and controllers.
- `src/platform/windows/winui` contains the native Windows presentation lane. `cmake` contains target, platform, deployment, resource, and test definitions.
- `tests`, `resources`, `docs`, `plans`, and `scripts` contain tests and fixtures, bundled assets, project documentation, planning records, and validation/build utilities.

## Modules and Responsibilities

- The engine owns reusable domain services, persistence, validation, document/report output, and other logic that must remain usable without Qt.
- The Qt-side object libraries separate core, data, domain, shared UI, features, and application services before they are assembled into the runtime.
- The Qt Classes feature owns class selection, section navigation, and selected-editor-stack composition.
- Platform directories adapt the products to their host UI and deployment environment.

## Main Interfaces and Integration Boundaries

- `ClassMngrEngine` is the portable static-library boundary, with public headers under `classmngr/engine`.
- `ClassMngrQtRuntime` combines Qt-side production objects with the engine and is consumed by the Qt desktop executable and its tests.
- CMake product options select the Qt desktop, the Windows WinUI target, and the Windows Qt transition target; the WinUI target links the engine directly.

## Tests and Supporting Assets

- CTest registers engine tests and Qt application/component tests. Engine test sources are under `tests/engine`; Qt-facing tests are under `tests` and are grouped by CMake test fragments.
- Database, import, resource-pack, speaking-evaluation, and visual-scenario fixtures live under `tests/fixtures` and related test-data directories.
- Release and platform validation scripts live under `scripts`, with additional porting evidence under `docs/porting` and `artifacts`.
