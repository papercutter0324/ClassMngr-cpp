# Project Structure

## Directory Layout

- `src/main.cpp`: executable entry point.
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
instances. `ClassMngrBuildSettings` supplies common include paths, C++23
settings, Qt/zlib links, and generated definitions; CMake aggregates the
production object libraries into `ClassMngrRuntime`. (application_services.h,
database_session.h, cmake/sources.cmake)

## Tests and Supporting Assets

Tests are declared as Qt executables and registered with CTest through the
`cmake/tests/*.cmake` fragments. Fixtures cover workspaces, imports, resource
packs, and other focused feature cases. Resource and visual-baseline material
is kept under `resources` and `docs/qt-rewrite`. No build or test was run as
part of this documentation bootstrap.
