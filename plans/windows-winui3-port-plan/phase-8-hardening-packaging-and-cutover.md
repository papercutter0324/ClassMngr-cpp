# Phase 8 — Hardening, Packaging, and Cutover

> Progress is tracked in [00-START-HERE.md](00-START-HERE.md).

## Goal

Prove the WinUI 3 product is supportable, package it through the established
release channel, cut Windows over safely, and retain a tested rollback path.

## Implementation Sequence

1. Run unit, engine, integration, UI, visual, database, import/export, updater,
   installer, and long-session soak tests on supported x64 hardware.
   Keep x86 Debug/Release builds, engine tests, feature integration tests, and
   staged application smoke tests green as a required compatibility lane.
2. Profile cold/warm startup, first navigation, resize, large-grid scrolling,
   PDF use, report generation, working/private memory, GPU memory, handles, and
   package footprint against approved budgets.
3. Validate signing, SmartScreen workflow, Inno Setup install/upgrade/uninstall,
   file associations, settings migration, crash recovery, and updates from the
   final Qt Windows release.
4. Validate self-contained Windows App SDK servicing and document how runtime
   updates are delivered with ClassMngr releases.
5. Run structured keyboard, Korean IME, DPI, text scaling, high contrast,
   automation, and Narrator checks for critical workflows.
6. Produce at least two release candidates that install beside an isolated Qt
   build and operate only on copied databases during beta and rollback tests.
7. Switch the public `ClassMngr.exe`, installer, updater metadata, and release
   workflows to the WinUI target only after every applicable parity row passes.
8. Remove Windows Qt deployment, QML, plugin, translation, and Qt license
   payloads only after the supported rollback artifact is archived and tested.
9. Keep macOS/Linux Qt source, resources, tests, and release workflows intact.
10. Record final architecture, dependency, license, support-floor, and
    known-limitations reviews.
11. Record whether verified user demand justifies promoting x86 from supported
    build output to a signed installer/update artifact. Promotion requires
    architecture-specific performance, dependency, installation, upgrade, and
    rollback evidence; it is not implied by build support.

## Completion Criteria

- The Windows executable and installer contain no Qt runtime or generated QML
  dependency.
- All applicable parity workflows pass on Windows x64, including Korean input,
  keyboard navigation, output, updates, PowerPoint, and failure recovery.
- Windows x86 Debug and Release configurations compile, test, and launch from
  architecture-correct self-contained stages with no mixed-bitness or Qt
  dependency.
- The 200 MiB steady-state memory target is measured on x64 and x86, and x86
  worst-case peak usage remains within its separately approved address-space
  budget.
- `.tps` databases and supported exports round-trip with macOS/Linux Qt.
- Startup, interaction, memory, accessibility, reliability, and package-size
  budgets are met.
- Installer upgrades from the final Qt release and rollback to a supported
  artifact are proven.
- The macOS and Linux Qt products remain green and releasable.

## Exit Gate

The WinUI 3 build is the supported Windows release, public update and installer
channels point to it, rollback evidence exists, and the retired Qt Windows
payload is no longer part of current Windows packaging.
