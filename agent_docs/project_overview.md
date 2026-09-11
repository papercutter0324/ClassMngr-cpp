# Project Overview

## Purpose

ClassMngr is a cross-platform desktop application for classroom administration. The documented product areas include classes, schedules, rosters, speaking evaluations and reports, substitute preparation, teacher and staff information, campus information, and academic-calendar events.

## Scope

The application uses the ClassMngr `.tps` database format and can open legacy `.db` databases. The repository contains the retained Qt desktop product and an optional Windows WinUI 3 product/transition lane. Release packaging deploys the runtime and bundled resources needed by supported desktop installs.

## Architecture

CMake configures the Qt-free `ClassMngrEngine` static library before Qt discovery. The Qt product assembles Core, Data, Domain, shared UI, Features, and App services into `ClassMngrQtRuntime` and links that runtime with the engine. The Windows WinUI target is selected independently and also links the engine.

## Main Workflows

- Configure, build, and test with platform-specific CMake presets.
- Run the desktop application against a new or existing database; import schedules and teachers and manage classroom records.
- Produce reports, printable teaching/substitute documents, and other exported output.
- Package release installs with deployed runtime resources; optional update and resource-pack checks use configured HTTPS endpoints.

## Major Decisions

- Keep the engine Qt-free so it can build and be tested in native-only configurations.
- Select the Qt desktop or Windows WinUI product before Qt package discovery.
- Treat the Qt desktop, Windows WinUI transition, and platform-specific validation lanes as separate CMake products sharing the engine.
