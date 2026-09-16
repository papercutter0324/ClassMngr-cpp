# Project Overview

## Purpose

ClassMngr is a cross-platform desktop application for classroom
administration: classes, schedules, rosters, speaking evaluations and
reports, substitute preparation, teacher/staff details, campus information,
and academic-calendar events. (README.md)

## Scope

The application creates and opens ClassMngr `.tps` databases and can open
legacy `.db` databases. It supports teacher and schedule imports, class
transfers, roster/schedule/report printing or export, English/Korean UI
localization, light/dark themes, and optional application/resource-pack
updates. Its AI-assisted comment flow opens a configured AI website; the
application does not send observations to an AI API. (README.md,
docs/auto_updates.md)

## Architecture

`src/main.cpp` is the Qt application entry point. The source is organized
into application wiring/controllers (`src/app`), shared infrastructure
(`src/core`), SQLite persistence and repositories (`src/data`), domain
models/rules/validation (`src/domain`), feature modules (`src/features`),
and reusable Qt UI (`src/ui/shared`). CMake composes these areas into
`ClassMngrRuntime` and the `ClassMngr` executable. (CMakeLists.txt,
cmake/sources.cmake)

## Main Workflows

- Start the application, create or open a workspace database, and use the
  feature pages for classroom administration.
- Import teachers or schedules, review conflicts/plans, and persist approved
  changes.
- View, print, or export schedules, rosters, substitute-preparation packages,
  and speaking-evaluation reports.
- Select language/theme settings and, when configured, check for signed
  application updates or independently updated resource packs.

## Major Decisions

- `ApplicationServices` exposes narrow feature services; `DataService` is
  a compatibility facade while callers migrate, and UI/controllers are intended
  to use feature services rather than database repositories directly.
- Feature-scoped content is built as standalone Qt RCC resource packs, while
  common assets, styles, translations, and calendar QML are packaged with the
  application. (cmake/resources.cmake)
