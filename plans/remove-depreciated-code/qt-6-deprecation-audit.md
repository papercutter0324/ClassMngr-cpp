# Qt 6.11/6.12 Deprecated and Legacy API Audit

Status: Formal Qt 6 deprecation migration complete; legacy cleanup follow-up remains.

This document records the Qt API audit that should be used as input for a future cleanup plan. The requested directory name is retained as `remove-depreciated-code`, while the Qt terminology below uses the correct spelling, `deprecated`.

## Scope and baseline

- Project: ClassMngr-cpp Qt desktop application.
- Qt baseline declared by the project: `Qt 6.11.1` in `CMakeLists.txt:193`.
- Local SDKs inspected: Qt `6.11.1`, `6.11.2`, and `6.12.0`.
- Validation headers and documentation: installed Qt `6.12.0`.
- Project setup is already using modern CMake helpers: `qt_standard_project_setup()` and `qt_add_executable()`.
- The project uses Qt Concurrent, Core, Gui, Pdf, PdfWidgets, PrintSupport, Qml, Quick, QuickControls2, QuickWidgets, Sql, Network, and LinguistTools. No Qt 5 compatibility, Qt Charts, or Qt Data Visualization modules were found.
- QML files reviewed are `src/features/calendar/ui/qml/EventCalendar.qml` and `src/features/calendar/ui/qml/MonthGridDelegate.qml`.

## Executive summary

Three formal Qt deprecation groups require migration:

1. Two `QDateTime` calls still pass `Qt::UTC` as the old `Qt::TimeSpec` form. The overloads were deprecated in Qt 6.9 and fail when deprecated APIs through Qt 6.12 are disabled.
2. Two `QMouseEvent::pos()` calls use the API deprecated since Qt 6.0.
3. Seven test calls use the deprecated fixed-argument `QMetaObject::invokeMethod` overloads through `Q_ARG(...)`.

The codebase also contains supported but legacy or discouraged patterns: four string-based `SIGNAL`/`SLOT` connections and multiple synchronous `QDialog::exec()` calls. `QQuickWidget` is current Qt API, but its rendering trade-offs should remain documented rather than treated as a deprecation.

## Formal deprecations to remove

### 1. Migrate `QDateTime` timezone overloads

Qt deprecated the `Qt::TimeSpec` timezone overloads in Qt 6.9. The replacement is to pass a `QTimeZone`, normally `QTimeZone::UTC`, or to omit the timezone argument where the default is correct.

Production location:

- `src/data/repositories/class_transfer_repository.cpp:97`
  - Current form: `QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(count), Qt::UTC)`.
  - Planned replacement: `QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(count), QTimeZone::UTC)`.
  - Add the appropriate `QTimeZone` include if it is not already provided transitively.

Test location:

- `tests/windows/windows_qt_visual_capture_tests.cpp:313`
  - Current form: `QDateTime(QDate(2026, 8, 28), QTime(9, 0), Qt::UTC)`.
  - Planned replacement: use the corresponding `QTimeZone::UTC` constructor overload.

Acceptance criteria:

- The production and test call sites no longer select a `Qt::TimeSpec` overload.
- Date/time values remain UTC and existing repository and visual-capture tests retain their behavior.
- The production target compiles with `QT_DISABLE_DEPRECATED_UP_TO=0x060C00`.

Reference: [QDateTime obsolete APIs](https://doc.qt.io/qt-6/qdatetime-obsolete.html).

### 2. Replace `QMouseEvent::pos()`

`QMouseEvent::pos()` is deprecated since Qt 6.0. Use `QMouseEvent::position().toPoint()` for widget-local integer coordinates.

Locations:

- `src/ui/shared/widgets/marquee_item_delegate.h:172`
- `src/ui/shared/widgets/sidebar/sidebar_marquee_delegate.cpp:405`

Acceptance criteria:

- Both call sites use `position()` and preserve the existing coordinate semantics.
- Marquee selection and sidebar drag/selection behavior is unchanged.
- Relevant UI tests and the Windows build pass.

Reference: [QMouseEvent obsolete APIs](https://doc.qt.io/qt-6/qmouseevent-obsolete.html).

### 3. Replace fixed-argument `QMetaObject::invokeMethod` overloads

The fixed-argument overloads based on `QGenericArgument` are deprecated. Replace `Q_ARG(...)` arguments with the Qt 6 variadic `invokeMethod` overload and actual typed arguments.

Seven calls require migration:

- `tests/updater_tests.cpp:1053`
- `tests/updater_tests.cpp:1071`
- `tests/teacher_info_page_tests.cpp:222`
- `tests/sidebar_structure_tests.cpp:106`
- `tests/sidebar_structure_tests.cpp:170`
- `tests/testing_classes_page_tests.cpp:217`
- `tests/schedule_widget_tests.cpp:742`

These calls contain ten `Q_ARG(...)` arguments in total. The no-argument call at `tests/updater_tests.cpp:1022` already uses the modern variadic form and should not be changed solely for this audit. Likewise, the dynamic method-name call in `src/app/controllers/edit_controller.cpp:184` has no `Q_ARG` arguments and resolves the modern variadic overload; it is a separate reflection-style cleanup consideration, not a confirmed deprecated overload.

Acceptance criteria:

- No deprecated `QGenericArgument`/`Q_ARG` `invokeMethod` overload remains.
- Invocation connection types, argument types, ordering, and return/error behavior remain unchanged.
- All affected test targets compile and pass.

Reference: [QMetaObject obsolete APIs](https://doc.qt.io/qt-6/qmetaobject-obsolete.html).

## Legacy or discouraged patterns

These are not currently formal Qt 6.11/6.12 deprecations, but they are reasonable follow-up items for the cleanup plan.

### String-based `SIGNAL`/`SLOT` connections

Four connections use the string-based macros:

- Three QML-root signal connections in `src/features/calendar/ui/calendar_page_events.cpp:428`, `:434`, and `:440`.
- One bottom-bar connection in `src/ui/shared/components/bottom_bar_builder.cpp:99`; the callback shape is declared in `src/ui/shared/components/bottom_bar_builder.h:31`.

Prefer typed pointer-to-member connections or lambdas where the sender and receiver types are available. The QML-root connections may need to retain a string/QMetaMethod-based approach because the signal is resolved dynamically across the QML boundary; document that decision in the implementation plan rather than forcing an unsafe conversion.

Reference: [QObject connection APIs](https://doc.qt.io/qt-6/qobject.html).

### Synchronous `QDialog::exec()`

Multiple dialogs use `exec()`, with `src/app/mainwindow.cpp:667` as a representative location. `exec()` remains supported, but Qt documentation recommends asynchronous `open()`/`show()` with the `finished()` signal to avoid nested event loops and re-entrancy problems.

Treat this as a separate behavior-sensitive modernization phase. Each dialog must be converted with an explicit result/continuation strategy, and modal behavior must be verified manually or with UI tests.

Reference: [QDialog documentation](https://doc.qt.io/qt-6/qdialog.html).

### `QQuickWidget`

`QQuickWidget` in `src/features/calendar/ui/calendar_page_events.cpp:391` is current Qt API, not deprecated. It does add an extra render pass and disables the threaded render loop. Keep it unless profiling demonstrates a meaningful performance issue; a future optimization could evaluate `QQuickView` or another window/container arrangement.

Reference: [QQuickWidget documentation](https://doc.qt.io/qt-6/qquickwidget.html).

## Areas checked with no findings

The audit did not find current uses of the following known obsolete/deprecated patterns:

- `QCheckBox::stateChanged` (the Qt 6.9 replacement is `checkStateChanged`).
- Obsolete `QDate` start/end-of-day timezone overloads.
- Old QLocale territory/country APIs.
- Old QMetaType type-ID APIs.
- Obsolete QQuickItem mapping or grab APIs.
- Old QMenu shortcut overloads.
- Old QMessageBox integer-button overloads.
- Obsolete QIcon overloads.
- Deprecated QPixmap cache APIs.
- Deprecated QCryptographicHash character-pointer/length overloads.
- Obsolete `QDataStream::readBytes(char *&, uint &)`.
- `QDir::Modified`, `QQmlFile`, `QQmlPropertyMap`, `Q_ENUMS`, `Q_FLAGS`, and `Qt::NavigationMode` patterns.
- Qt Graphs and Qt WebEngine deprecated APIs.

The `setTextAlignment(Qt::AlignCenter)` calls at `src/features/teacher/ui/staff_directory_page.cpp:166` and `src/features/schedule/ui/schedule_table_renderer.cpp:506` and `:594` resolve the current `Qt::AlignmentFlag` overload; they are not deprecated integer-overload calls.

The reviewed QML uses modern patterns including unversioned imports, required properties, and `pragma ComponentBehavior: Bound`. No obsolete `Qt.include`, string `Qt.atob`/`Qt.btoa`, or `Text.doLayout` usage was found.

## Initial audit validation evidence

- CMake configured successfully against local Qt 6.12.0. Unrelated optional Qt6Positioning/WebEngine plugin warnings were present.
- An isolated build was configured with `QT_DISABLE_DEPRECATED_UP_TO=0x060C00`.
- Production targets compiled until the known `QDateTime` call failed against the Qt 6.12.0 headers. No additional production deprecation compile errors were observed in the scan/build.
- The test targets did not reach translation-unit compilation because the production data target failed first; the test findings are confirmed by the Qt 6.12.0 headers, official obsolete-API documentation, and source inspection.
- No source code was changed as part of the initial audit.

## Implementation evidence (2026-09-02)

- The three formal deprecation groups were migrated in the nine audited source
  and test files: `QTimeZone::UTC` replaces the old UTC time-spec overloads,
  `QMouseEvent::position().toPoint()` replaces `pos()`, and all seven fixed-
  argument `QMetaObject::invokeMethod` calls now use typed variadic arguments.
- The Qt 6.12.0 Debug audit build uses
  `QT_DISABLE_DEPRECATED_UP_TO=0x060C00`. `ClassMngrData`, `ClassMngrUiShared`,
  the five affected Qt test targets, and the enabled Windows visual-capture
  target all compile successfully.
- `ClassMngrUpdaterTests`, `ClassMngrTeacherInfoPageTests`,
  `ClassMngrSidebarStructureTests`, `ClassMngrTestingClassesPageTests`, and
  `ClassMngrScheduleWidgetTests` each passed 1/1 through CTest. Scoped audits
  report no remaining `Q_ARG`, `Qt::UTC`, or audited mouse-position calls.
- `ClassMngrClassTransferTests` requires a matching Qt runtime. Launching the
  Qt 6.12-built executable with the machine-wide Qt 6.11.1 DLL path produces
  the reported `QPdfView::wheelEvent` entry-point error. With Qt 6.12.0 on the
  runtime path, the executable loads; its current run reaches 8 passing and 7
  database-dependent setup failures, so no clean class-transfer runtime pass
  is claimed for this slice.

## Remaining cleanup follow-up

1. Review string-based connections separately, converting only statically type-safe cases and documenting any QML boundary exceptions.
2. Assess `QDialog::exec()` conversions as a behavior-sensitive follow-up, using asynchronous dialog completion where practical.
3. Keep `QQuickWidget` unchanged unless performance profiling justifies a separate rendering architecture task.

## Plan-ready checklist

- [x] Create a focused implementation branch/commit for the three formal deprecation groups.
- [x] Preserve UTC semantics, mouse coordinate semantics, and queued/direct invocation behavior.
- [x] Add or update regression tests for each changed behavior where coverage is missing.
- [ ] Verify a clean build with Qt 6.11.1 and Qt 6.12.0, if both SDKs remain supported.
- [ ] Add a CI compile check with `QT_DISABLE_DEPRECATED_UP_TO=0x060C00` to prevent regression.
- [x] Record remaining legacy patterns and their rationale in the final cleanup plan.
