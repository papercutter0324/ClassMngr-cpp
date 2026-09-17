# Project Structure

## Directory Layout

- `src/main.cpp`: executable entry point.
- `src/next/main.cpp`: entry point for the parallel `ClassMngrNext` console
  bootstrap.
- `src/app`: main-window wiring, controllers, and application feature
  services.
- `src/core`: application settings, language/theme services, networking,
  update/resource-pack infrastructure, utilities, and common result types.
- `src/data`: database session/schema/transaction code and repositories.
- `src/domain`: domain models, import rules, and validation.
- `src/features`: calendar, campus, classes, documents, my-info, roster,
  schedule, setup, speaking evaluation, substitute preparation, and teacher
  modules; each may contain data, services, and UI.
- `src/ui/shared`: reusable actions, components, dialogs, pages, printing,
  state, styles, validation, and widgets.
- `resources`: fonts, icons, styles, translations, documents, templates,
  campus/files/images assets, and resource-pack inputs.
- `tests`: focused Qt test sources and fixtures.
- `cmake`: source/resource/deployment/test and platform build fragments.
- `cmake/production_sources.cmake`: explicit production, executable, and QML
  source manifests.
- `cmake/source_ownership.cmake`: configure-time handwritten-source ownership
  validation against production and test target lists.
- `cmake/next.cmake`: parallel target and interface-boundary definitions,
  configure-time dependency checks, and CTest launch check.
- `scripts/phase0`: Phase 0 platform route-matrix runners.
- `docs`, `plans`, `BUILDING.md`: project references, rewrite planning, and
  build/release guidance.

## Modules and Responsibilities

The application layer coordinates startup, navigation, editing, theme/language
settings, and updates. Core provides cross-cutting services. Data owns the
SQLite session, schema management, transactions, and repositories. Domain
holds framework-light records and business validation. Feature modules combine
feature-specific models, services, import/export logic, and pages. Shared UI
contains reusable Qt widgets and presentation infrastructure.

## Main Interfaces and Integration Boundaries

`ApplicationServices` owns the data service and exposes settings, teacher,
class, schedule, calendar, roster, speaking-evaluation, theme, and document
services. `DatabaseSession` owns the `QSqlDatabase` connection and repository
instances. `ClassMngrBuildSettings` supplies `Qt6::Core`, C++23, common source
and generated include directories, and `CLASSMNGR_SOURCE_DIR`. The six legacy
production object targets link their measured additional Qt modules; CMake
aggregates them into the existing `ClassMngrRuntime`, which remains the legacy
application structure. The runtime and macOS `ClassMngrTestRuntime` retain the
full legacy module union, including `Qt6::QuickControls2` for the QML/resource
graph. In parallel, `cmake/next.cmake`
defines the `ClassMngrNext` executable from `src/next/main.cpp` and source-free
layer/feature interface targets with `ClassMngrNext::<Name>` aliases. Its
configure-time checks enforce the dependency graph. The console bootstrap
links Qt Core only and remains independent of the placeholder targets.
(application_services.h, database_session.h, cmake/sources.cmake,
cmake/next.cmake). See the Phase 1 plan for measured per-target dependency
ownership. `src/core/build_info.h.in` is configured separately; the source
ownership inventory checks handwritten `src/` and `tests/` files against
explicit target owners and skips the Apple-only PowerPoint notice test off
Apple.

## Tests and Supporting Assets

Legacy tests are declared as Qt executables and registered with CTest through
the `cmake/tests/*.cmake` fragments. Shared schedule-widget stubs are compiled
once for five test consumers; the `ResourcePackManager` fake is separate for
four consumers because the Classes page test uses the real manager. The
parallel `ClassMngrNextLaunch` check is registered in `cmake/next.cmake`.
Fixtures cover workspaces, imports, resource packs, and other focused feature
cases. Resource and visual-baseline material is kept under `resources` and
`docs/qt-rewrite`.
