classmngr_add_qt_test(
    NAME TeacherImportUiApplyParity
    SOURCES
        tests/teacher_import_ui_apply_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME PageManager
    SOURCES
        tests/pagemanager_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

classmngr_add_qt_test(
    NAME MyWorkspacePage
    SOURCES
        tests/my_workspace_page_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME MyClassesPage
    SOURCES
        tests/my_classes_page_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME MyClassesPageTeacherDisplayParity
    SOURCES
        tests/my_classes_page_teacher_display_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME MyClassesPageSummaryParity
    SOURCES
        tests/my_classes_page_summary_parity_tests.cpp
    LIBRARIES
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME TypedSignatureRenderer
    SOURCES
        tests/typed_signature_renderer_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME ClassDetailsSavePage
    SOURCES
        tests/class_details_save_page_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME ClassDetailsPageSaveParity
    SOURCES
        tests/class_details_page_save_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME InitialSetupWizardClassCreateSaveParity
    SOURCES
        tests/initial_setup_wizard_class_create_save_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME InitialSetupWizardTeacherCreateParity
    SOURCES
        tests/initial_setup_wizard_teacher_create_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

qt_add_resources(
    ClassMngrInitialSetupWizardTeacherCreateParityTests
    initial_setup_teacher_create_parity_keyboard_test_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_resources(
    ClassMngrInitialSetupWizardClassCreateSaveParityTests
    initial_setup_parity_keyboard_test_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

classmngr_add_qt_test(
    NAME ClassNotesPageReadParity
    SOURCES
        tests/class_notes_page_read_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME ClassNotesPageSaveParity
    SOURCES
        tests/class_notes_page_save_parity_tests.cpp
    LIBRARIES
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME CalendarPageEventMutationParity
    SOURCES
        tests/calendar_page_event_mutation_parity_tests.cpp
    LIBRARIES
        Qt6::QuickWidgets
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME CalendarPreferencesRestoreDefaults
    SOURCES
        tests/calendar_preferences_restore_defaults_tests.cpp
    LIBRARIES
        Qt6::QuickWidgets
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

qt_add_resources(
    ClassMngrCalendarPreferencesRestoreDefaultsTests
    calendar_preferences_event_reset_qml_test_resources
    PREFIX "/qt/qml/ClassMngr/Calendar"
    BASE "${PROJECT_SOURCE_DIR}/src/features/calendar/ui/qml"
    FILES
        "${PROJECT_SOURCE_DIR}/src/features/calendar/ui/qml/EventCalendar.qml"
        "${PROJECT_SOURCE_DIR}/src/features/calendar/ui/qml/MonthGridDelegate.qml"
)

qt_add_resources(
    ClassMngrCalendarPreferencesRestoreDefaultsTests
    calendar_preferences_event_reset_keyboard_test_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_resources(
    ClassMngrCalendarPageEventMutationParityTests
    calendar_page_event_mutation_qml_test_resources
    PREFIX "/qt/qml/ClassMngr/Calendar"
    BASE "${PROJECT_SOURCE_DIR}/src/features/calendar/ui/qml"
    FILES
        "${PROJECT_SOURCE_DIR}/src/features/calendar/ui/qml/EventCalendar.qml"
        "${PROJECT_SOURCE_DIR}/src/features/calendar/ui/qml/MonthGridDelegate.qml"
)

qt_add_resources(
    ClassMngrCalendarPageEventMutationParityTests
    calendar_page_event_mutation_keyboard_test_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

classmngr_add_qt_test(
    NAME ClassCoTeacherPageReadParity
    SOURCES
        tests/class_co_teacher_page_read_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME TeacherInfoPagePersistenceParity
    SOURCES
        tests/teacher_info_page_persistence_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NavigationTeacherRead
    SOURCES
        tests/navigation_teacher_read_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NavigationTeacherReadParity
    SOURCES
        tests/navigation_teacher_read_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NavigationRosterSessionParity
    SOURCES
        tests/navigation_roster_session_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME StaffDirectoryClosedSessionNavigationParity
    SOURCES
        tests/staff_directory_closed_session_navigation_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME StaffDirectoryOpenSessionNavigationParity
    SOURCES
        tests/staff_directory_open_session_navigation_parity_tests.cpp
    LIBRARIES
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

qt_add_resources(
    ClassMngrStaffDirectoryOpenSessionNavigationParityTests
    staff_directory_open_session_navigation_parity_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_translations(
    TARGETS ClassMngrStaffDirectoryOpenSessionNavigationParityTests
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

classmngr_add_qt_test(
    NAME ClassRouteAvailabilityParity
    SOURCES
        tests/class_route_availability_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrtemplatesResourcePack
    OFFSCREEN
)

qt_add_resources(
    ClassMngrClassRouteAvailabilityParityTests
    class_route_availability_parity_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

classmngr_add_qt_test(
    NAME MyInfoRouteNavigationParity
    SOURCES
        tests/my_info_route_navigation_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME SubPrepRouteGateParity
    SOURCES
        tests/sub_prep_route_gate_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

qt_add_resources(
    ClassMngrSubPrepRouteGateParityTests
    sub_prep_route_gate_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_resources(
    ClassMngrMyInfoRouteNavigationParityTests
    my_info_route_navigation_parity_qml_resources
    PREFIX "/qt/qml/ClassMngr/Calendar"
    BASE "${PROJECT_SOURCE_DIR}/src/features/calendar/ui/qml"
    FILES
        "${PROJECT_SOURCE_DIR}/src/features/calendar/ui/qml/EventCalendar.qml"
        "${PROJECT_SOURCE_DIR}/src/features/calendar/ui/qml/MonthGridDelegate.qml"
)

qt_add_resources(
    ClassMngrMyInfoRouteNavigationParityTests
    my_info_route_navigation_parity_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_resources(
    ClassMngrNavigationRosterSessionParityTests
    navigation_roster_session_parity_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

classmngr_add_qt_test(
    NAME CampusRouteNavigationParity
    SOURCES
        tests/campus_route_navigation_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

qt_add_resources(
    ClassMngrCampusRouteNavigationParityTests
    campus_route_navigation_parity_qml_resources
    PREFIX "/qt/qml/ClassMngr/Calendar"
    BASE "${PROJECT_SOURCE_DIR}/src/features/calendar/ui/qml"
    FILES
        "${PROJECT_SOURCE_DIR}/src/features/calendar/ui/qml/EventCalendar.qml"
        "${PROJECT_SOURCE_DIR}/src/features/calendar/ui/qml/MonthGridDelegate.qml"
)

qt_add_resources(
    ClassMngrCampusRouteNavigationParityTests
    campus_route_navigation_parity_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

set(document_catalog_navigation_missing_pack_dir
    "${CMAKE_CURRENT_BINARY_DIR}/document-catalog-navigation-missing-resource-pack"
)
file(MAKE_DIRECTORY "${document_catalog_navigation_missing_pack_dir}")
set(document_catalog_navigation_missing_pack_qrc
    "${CMAKE_CURRENT_BINARY_DIR}/document-catalog-navigation-missing-resource-pack.qrc"
)
file(TO_CMAKE_PATH
    "${PROJECT_SOURCE_DIR}/resources/assets/documents/documents.json"
    document_catalog_navigation_catalog_path
)
file(WRITE "${document_catalog_navigation_missing_pack_qrc}"
    "<RCC version=\"1.0\">\n"
    "  <qresource prefix=\"/resource-packs/documents\">\n"
    "    <file alias=\"documents.json\">${document_catalog_navigation_catalog_path}</file>\n"
    "  </qresource>\n"
    "</RCC>\n"
)
qt_add_binary_resources(
    ClassMngrDocumentCatalogNavigationMissingResourcePack
    "${document_catalog_navigation_missing_pack_qrc}"
    DESTINATION
        "${document_catalog_navigation_missing_pack_dir}/documents.rcc"
)

classmngr_add_qt_test(
    NAME DocumentCatalogNavigationParity
    SOURCES
        tests/document_catalog_navigation_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrdocumentsResourcePack
        ClassMngrDocumentCatalogNavigationMissingResourcePack
    OFFSCREEN
)
target_compile_definitions(ClassMngrDocumentCatalogNavigationParityTests
    PRIVATE
        CLASSMNGR_TEST_DOCUMENT_BASELINE_PACK_DIR="${CLASSMNGR_RESOURCE_PACK_OUTPUT_DIR}"
        CLASSMNGR_TEST_DOCUMENT_MISSING_RESOURCE_PACK_DIR="${document_catalog_navigation_missing_pack_dir}"
)

qt_add_resources(
    ClassMngrDocumentCatalogNavigationParityTests
    document_catalog_navigation_parity_qml_resources
    PREFIX "/qt/qml/ClassMngr/Calendar"
    BASE "${PROJECT_SOURCE_DIR}/src/features/calendar/ui/qml"
    FILES
        "${PROJECT_SOURCE_DIR}/src/features/calendar/ui/qml/EventCalendar.qml"
        "${PROJECT_SOURCE_DIR}/src/features/calendar/ui/qml/MonthGridDelegate.qml"
)

qt_add_resources(
    ClassMngrDocumentCatalogNavigationParityTests
    document_catalog_navigation_parity_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

classmngr_add_qt_test(
    NAME SidebarClassDeleteParity
    SOURCES
        tests/sidebar_class_delete_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME SidebarTeacherDeleteParity
    SOURCES
        tests/sidebar_teacher_delete_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME ScheduleTestingLayoutClearParity
    SOURCES
        tests/schedule_testing_layout_clear_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME MainWindowDocumentCatalogRetranslationParity
    SOURCES
        tests/mainwindow_document_catalog_retranslation_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

qt_add_resources(
    ClassMngrMainWindowDocumentCatalogRetranslationParityTests
    mainwindow_document_catalog_retranslation_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_translations(
    TARGETS ClassMngrMainWindowDocumentCatalogRetranslationParityTests
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

classmngr_add_qt_test(
    NAME MainWindowRecentWorkspaceReopenParity
    SOURCES
        tests/mainwindow_recent_workspace_reopen_parity_tests.cpp
    LIBRARIES
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

qt_add_resources(
    ClassMngrMainWindowRecentWorkspaceReopenParityTests
    mainwindow_recent_workspace_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_translations(
    TARGETS ClassMngrMainWindowRecentWorkspaceReopenParityTests
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

classmngr_add_qt_test(
    NAME MainWindowMyWorkspaceSidebarRootNavigation
    SOURCES
        tests/mainwindow_my_workspace_sidebar_root_navigation_tests.cpp
    LIBRARIES
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

qt_add_resources(
    ClassMngrMainWindowMyWorkspaceSidebarRootNavigationTests
    mainwindow_my_workspace_sidebar_root_navigation_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_translations(
    TARGETS ClassMngrMainWindowMyWorkspaceSidebarRootNavigationTests
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

classmngr_add_qt_test(
    NAME MainWindowClassesSidebarRootNavigation
    SOURCES
        tests/mainwindow_classes_sidebar_root_navigation_tests.cpp
    LIBRARIES
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

qt_add_resources(
    ClassMngrMainWindowClassesSidebarRootNavigationTests
    mainwindow_classes_sidebar_root_navigation_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_translations(
    TARGETS ClassMngrMainWindowClassesSidebarRootNavigationTests
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

classmngr_add_qt_test(
    NAME MainWindowSubPrepSidebarRootNavigation
    SOURCES
        tests/mainwindow_subprep_sidebar_root_navigation_tests.cpp
    LIBRARIES
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

qt_add_resources(
    ClassMngrMainWindowSubPrepSidebarRootNavigationTests
    mainwindow_subprep_sidebar_root_navigation_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_translations(
    TARGETS ClassMngrMainWindowSubPrepSidebarRootNavigationTests
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

classmngr_add_qt_test(
    NAME MainWindowInitialSetupEmptyStateNavigation
    SOURCES
        tests/mainwindow_initial_setup_empty_state_navigation_tests.cpp
    LIBRARIES
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

qt_add_resources(
    ClassMngrMainWindowInitialSetupEmptyStateNavigationTests
    mainwindow_initial_setup_empty_state_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_translations(
    TARGETS ClassMngrMainWindowInitialSetupEmptyStateNavigationTests
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

classmngr_add_qt_test(
    NAME MainWindowCampusSidebarNavigation
    SOURCES
        tests/mainwindow_campus_sidebar_navigation_tests.cpp
    LIBRARIES
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

qt_add_resources(
    ClassMngrMainWindowCampusSidebarNavigationTests
    mainwindow_campus_sidebar_navigation_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_translations(
    TARGETS ClassMngrMainWindowCampusSidebarNavigationTests
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

classmngr_add_qt_test(
    NAME MainWindowCloseFileParity
    SOURCES
        tests/mainwindow_close_file_parity_tests.cpp
    LIBRARIES
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

qt_add_resources(
    ClassMngrMainWindowCloseFileParityTests
    mainwindow_close_file_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_translations(
    TARGETS ClassMngrMainWindowCloseFileParityTests
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

classmngr_add_qt_test(
    NAME MainWindowOpenFileParity
    SOURCES
        tests/mainwindow_open_file_parity_tests.cpp
    LIBRARIES
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

qt_add_resources(
    ClassMngrMainWindowOpenFileParityTests
    mainwindow_open_file_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_translations(
    TARGETS ClassMngrMainWindowOpenFileParityTests
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

classmngr_add_qt_test(
    NAME MainWindowExitConfirmationParity
    SOURCES
        tests/mainwindow_exit_confirmation_parity_tests.cpp
    LIBRARIES
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME MainWindowEditActionParity
    SOURCES
        tests/mainwindow_edit_action_parity_tests.cpp
    LIBRARIES
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME MainWindowAboutActionParity
    SOURCES
        tests/mainwindow_about_action_parity_tests.cpp
    LIBRARIES
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME MainWindowCheckForUpdatesActionParity
    SOURCES
        tests/mainwindow_check_for_updates_action_parity_tests.cpp
    LIBRARIES
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    ENVIRONMENT
        "TMP=${CMAKE_CURRENT_BINARY_DIR}/mainwindow-check-for-updates-temp"
        "TEMP=${CMAKE_CURRENT_BINARY_DIR}/mainwindow-check-for-updates-temp"
        "TMPDIR=${CMAKE_CURRENT_BINARY_DIR}/mainwindow-check-for-updates-temp"
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME MainWindowFontSizeActionParity
    SOURCES
        tests/mainwindow_font_size_action_parity_tests.cpp
    LIBRARIES
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME MainWindowThemeActionParity
    SOURCES
        tests/mainwindow_theme_action_parity_tests.cpp
    LIBRARIES
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME MainWindowDocumentViewerBackgroundActionParity
    SOURCES
        tests/mainwindow_document_viewer_background_action_parity_tests.cpp
    LIBRARIES
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME MainWindowDocumentViewerPageSpacingActionParity
    SOURCES
        tests/mainwindow_document_viewer_page_spacing_action_parity_tests.cpp
    LIBRARIES
        Qt6::PdfWidgets
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

qt_add_resources(
    ClassMngrMainWindowDocumentViewerPageSpacingActionParityTests
    mainwindow_document_viewer_page_spacing_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_translations(
    TARGETS ClassMngrMainWindowDocumentViewerPageSpacingActionParityTests
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

qt_add_resources(
    ClassMngrMainWindowDocumentViewerBackgroundActionParityTests
    mainwindow_document_viewer_background_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_translations(
    TARGETS ClassMngrMainWindowDocumentViewerBackgroundActionParityTests
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

classmngr_add_qt_test(
    NAME MainWindowSaveAsExportParity
    SOURCES
        tests/mainwindow_save_as_export_parity_tests.cpp
    LIBRARIES
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME MainWindowScheduleTestingClassesHandoffParity
    SOURCES
        tests/mainwindow_schedule_testing_classes_handoff_parity_tests.cpp
    LIBRARIES
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME MainWindowManageCampusesParity
    SOURCES
        tests/mainwindow_manage_campuses_parity_tests.cpp
    LIBRARIES
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME MainWindowTeacherSidebarNavigationParity
    SOURCES
        tests/mainwindow_teacher_sidebar_navigation_parity_tests.cpp
    LIBRARIES
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    DEPENDENCIES
        ClassMngrcampusesResourcePack
        ClassMngrdocumentsResourcePack
    OFFSCREEN
)

qt_add_resources(
    ClassMngrMainWindowExitConfirmationParityTests
    mainwindow_exit_confirmation_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_translations(
    TARGETS ClassMngrMainWindowExitConfirmationParityTests
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

qt_add_resources(
    ClassMngrMainWindowEditActionParityTests
    mainwindow_edit_action_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_translations(
    TARGETS ClassMngrMainWindowEditActionParityTests
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

qt_add_resources(
    ClassMngrMainWindowAboutActionParityTests
    mainwindow_about_action_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_translations(
    TARGETS ClassMngrMainWindowAboutActionParityTests
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

qt_add_resources(
    ClassMngrMainWindowCheckForUpdatesActionParityTests
    mainwindow_check_for_updates_action_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_translations(
    TARGETS ClassMngrMainWindowCheckForUpdatesActionParityTests
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

qt_add_resources(
    ClassMngrMainWindowFontSizeActionParityTests
    mainwindow_font_size_action_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_translations(
    TARGETS ClassMngrMainWindowFontSizeActionParityTests
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

qt_add_resources(
    ClassMngrMainWindowThemeActionParityTests
    mainwindow_theme_action_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_translations(
    TARGETS ClassMngrMainWindowThemeActionParityTests
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

qt_add_resources(
    ClassMngrMainWindowSaveAsExportParityTests
    mainwindow_save_as_export_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_translations(
    TARGETS ClassMngrMainWindowSaveAsExportParityTests
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

qt_add_resources(
    ClassMngrMainWindowScheduleTestingClassesHandoffParityTests
    mainwindow_schedule_testing_classes_handoff_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_translations(
    TARGETS ClassMngrMainWindowScheduleTestingClassesHandoffParityTests
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

qt_add_resources(
    ClassMngrMainWindowManageCampusesParityTests
    mainwindow_manage_campuses_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_translations(
    TARGETS ClassMngrMainWindowManageCampusesParityTests
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

qt_add_resources(
    ClassMngrMainWindowTeacherSidebarNavigationParityTests
    mainwindow_teacher_sidebar_navigation_keyboard_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

qt_add_translations(
    TARGETS ClassMngrMainWindowTeacherSidebarNavigationParityTests
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

classmngr_add_qt_test(
    NAME StaffDirectoryNativeEnglishRead
    SOURCES
        tests/staff_directory_native_english_read_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME StaffDirectoryNativeEnglishReadParity
    SOURCES
        tests/staff_directory_native_english_read_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME StaffDirectoryNativeEnglishSaveParity
    SOURCES
        tests/staff_directory_native_english_save_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME StaffDirectoryGsTeamRead
    SOURCES
        tests/staff_directory_gs_team_read_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME StaffDirectoryGsTeamReadParity
    SOURCES
        tests/staff_directory_gs_team_read_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME StaffDirectoryGsTeamSave
    SOURCES
        tests/staff_directory_gs_team_save_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME StaffDirectoryGsTeamSaveParity
    SOURCES
        tests/staff_directory_gs_team_save_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME RosterEditorWidgetSave
    SOURCES
        tests/roster_editor_widget_save_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME RosterTransferMenu
    SOURCES
        tests/roster_transfer_menu_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME RosterPrintDialog
    SOURCES
        tests/roster_print_dialog_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME SpeakingEvalPageSave
    SOURCES
        tests/speaking_eval_page_save_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME SpeakingEvalPageSaveParity
    SOURCES
        tests/speaking_eval_page_save_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME ClassDetailsPageDisplay
    SOURCES
        tests/class_details_page_display_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME ClassDetailsPageReadParity
    SOURCES
        tests/class_details_page_read_parity_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME ClassAnalyticsPage
    SOURCES
        tests/class_analytics_page_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME ClassAnalyticsPageReadParity
    SOURCES
        tests/class_analytics_page_read_parity_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    ENVIRONMENT
        QT_FORCE_STDERR_LOGGING=1
    OFFSCREEN
)

qt_add_executable(ClassMngrClassesPageTests
        tests/classes_page_tests.cpp
        src/core/utils/colorutils.cpp
        src/core/utils/sidebar_node_naming.cpp
        src/domain/models/classroom.cpp
        src/domain/models/roster.cpp
        src/features/classes/config/class_info_config.cpp
        src/features/classes/models/class_tab_navigation_model.cpp
        src/features/classes/models/class_tab_navigation_model.h
        src/features/classes/ui/class_co_teacher_page.cpp
        src/features/classes/ui/class_co_teacher_page.h
        src/features/classes/ui/class_details_page.cpp
        src/features/classes/ui/class_details_page.h
        src/features/classes/ui/classes_page.cpp
        src/features/classes/ui/classes_page.h
        src/features/classes/ui/classes_page_subtitle_text.h
        src/features/classes/ui/class_notes_page.cpp
        src/features/classes/ui/class_notes_page.h
        src/features/roster/ui/roster_column_layout_controller.cpp
        src/features/roster/ui/roster_editor_widget.cpp
        src/features/roster/ui/roster_editor_widget.h
        src/features/roster/ui/roster_editor_widget_columns.cpp
        src/features/roster/ui/roster_editor_widget_students.cpp
        src/features/roster/ui/roster_editor_widget_transfer.cpp
        src/features/roster/ui/roster_editor_widget_ui.cpp
        src/features/roster/ui/roster_header_view.cpp
        src/features/roster/ui/roster_item_delegate.cpp
        src/features/roster/ui/roster_model.cpp
        src/features/roster/ui/roster_model_columns.cpp
        src/features/roster/ui/roster_model_names.cpp
        src/features/roster/ui/roster_model_rows.cpp
        src/features/roster/ui/roster_model_validation.cpp
        src/ui/shared/qt_text_adapter.h
        src/features/roster/ui/roster_table_view.cpp
        src/features/schedule/schedule_settings_preferences.cpp
        src/features/schedule/schedule_settings_preferences.h
        src/features/schedule/ui/schedule_editor_dialog.h
        src/features/schedule/ui/schedule_print_dialog.h
        src/features/teacher/ui/teacher_model.cpp
        src/ui/shared/input/hangul_composer.cpp
        src/ui/shared/pages/basepage.cpp
        src/ui/shared/pages/basepage.h
        src/ui/shared/widgets/clickable_color_preview.cpp
        src/ui/shared/widgets/no_wheel_combobox.cpp
        src/ui/shared/widgets/navigation_pill_button.cpp
        src/ui/shared/widgets/navigation_pill_button.h
        src/ui/shared/widgets/navigation_pill_style.cpp
        src/ui/shared/widgets/navigation_pill_style.h
        src/ui/shared/widgets/navigation_settings_button.cpp
        src/ui/shared/widgets/navigation_settings_button.h
        src/ui/shared/widgets/navigation_tab_widget.cpp
        src/ui/shared/widgets/navigation_tab_widget.h
        src/ui/shared/widgets/on_screen_keyboard.cpp
        src/ui/shared/widgets/sectioncards/class_info_section_card.cpp
        src/ui/shared/widgets/sectioncards/class_info_section_card.h
        src/ui/shared/widgets/sectioncards/class_time_row.cpp
        src/ui/shared/widgets/sectioncards/class_time_row.h
        src/ui/shared/widgets/sections/class_details_section.cpp
        src/ui/shared/widgets/sections/class_details_section.h
        src/ui/shared/widgets/sections/class_schedule_section.cpp
        src/ui/shared/widgets/sections/class_schedule_section.h
        src/ui/shared/widgets/sections/teacher_info_section.cpp
        src/ui/shared/widgets/sections/teacher_info_section.h
    )

    target_compile_features(ClassMngrClassesPageTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrClassesPageTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    add_dependencies(
        ClassMngrClassesPageTests
        ClassMngrtemplatesResourcePack
    )

    target_compile_definitions(ClassMngrClassesPageTests
        PRIVATE
            CLASSMNGR_TEST_USE_REAL_RESOURCE_PACK_MANAGER
            CLASSMNGR_RESOURCE_PACK_DIR="${CLASSMNGR_RESOURCE_PACK_OUTPUT_DIR}"
            CLASSMNGR_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )

    target_link_libraries(ClassMngrClassesPageTests
        PRIVATE
            ClassMngrScheduleWidgetTestSupport
            Qt6::Core
            Qt6::Gui
            Qt6::PrintSupport
            Qt6::Sql
            Qt6::Widgets
            Qt6::Test
    )

    add_test(
        NAME ClassMngrClassesPageTests
        COMMAND ClassMngrClassesPageTests
    )

    set_tests_properties(
        ClassMngrClassesPageTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_resources(ClassMngrClassesPageTests classes_page_keyboard_test_resources
        PREFIX "/"
        BASE "${PROJECT_SOURCE_DIR}/resources"
        FILES
            resources/assets/icons/keyboard_dark.svg
            resources/assets/icons/keyboard_light.svg
    )

classmngr_add_qt_test(
    NAME RosterEditorWidgetImport
    SOURCES
        tests/roster_editor_widget_import_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

qt_add_executable(ClassMngrScheduleImportDialogTests
        tests/schedule_import_dialog_tests.cpp
        src/core/settingsmanager.cpp
        src/core/utils/colorutils.cpp
        src/domain/models/classroom.cpp
        src/features/calendar/calendar_workbook_reader.cpp
        src/features/classes/config/class_info_config.cpp
        src/features/schedule/schedule_settings_preferences.cpp
        src/features/schedule/schedule_settings_preferences.h
        src/features/schedule/import/schedule_workbook_parser.cpp
        src/features/schedule/ui/schedule_editor_dialog.h
        src/features/schedule/ui/schedule_import_dialog.cpp
        src/features/schedule/ui/schedule_import_dialog.h
        src/features/schedule/ui/schedule_import_dialog_shared.h
        src/features/schedule/ui/schedule_import_review_presentation.cpp
        src/features/schedule/ui/schedule_import_review_presentation.h
        src/features/schedule/ui/schedule_import_resolution_controls.cpp
        src/features/schedule/ui/schedule_import_resolution_controls.h
        src/features/schedule/ui/schedule_import_review_dialog.cpp
        src/features/schedule/ui/schedule_import_review_dialog.h
        src/features/schedule/ui/schedule_print_dialog.h
        src/features/schedule/ui/schedule_time_formatter.cpp
        src/features/schedule/ui/schedule_time_formatter.h
        src/features/schedule/ui/schedule_cell_widget_factory.cpp
        src/features/schedule/ui/schedule_cell_widget_factory.h
        src/features/schedule/ui/schedule_table_renderer.cpp
        src/features/schedule/ui/schedule_table_renderer.h
        src/features/schedule/ui/schedule_widget.cpp
        src/features/schedule/ui/schedule_widget.h
        src/features/schedule/ui/testing_assignment_dialog.cpp
        src/features/schedule/ui/testing_assignment_dialog.h
        src/ui/shared/widgets/no_wheel_combobox.cpp
    )

    target_compile_features(ClassMngrScheduleImportDialogTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrScheduleImportDialogTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_compile_definitions(ClassMngrScheduleImportDialogTests
        PRIVATE
            CLASSMNGR_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )

    target_link_libraries(ClassMngrScheduleImportDialogTests
        PRIVATE
            ClassMngrScheduleWidgetTestSupport
            ClassMngrScheduleWidgetResourcePackTestSupport
            Qt6::Concurrent
            Qt6::Core
            Qt6::Gui
            Qt6::PrintSupport
            Qt6::Sql
            Qt6::Test
            Qt6::Widgets
            ZLIB::ZLIB
    )

    add_test(
        NAME ClassMngrScheduleImportDialogTests
        COMMAND ClassMngrScheduleImportDialogTests
    )

    set_tests_properties(
        ClassMngrScheduleImportDialogTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

qt_add_executable(ClassMngrSubPrepPageTests
        tests/sub_prep_page_tests.cpp
        src/core/utils/sidebar_node_naming.cpp
        src/domain/models/classroom.cpp
        src/features/campus/data/campus_json_codec.cpp
        src/features/campus/data/campus_json_repository.cpp
        src/features/classes/config/class_info_config.cpp
        src/features/classes/models/class_tab_navigation_model.cpp
        src/features/classes/models/class_tab_navigation_model.h
        src/features/schedule/schedule_settings_preferences.cpp
        src/features/schedule/schedule_settings_preferences.h
        src/features/schedule/ui/schedule_editor_dialog.h
        src/features/schedule/ui/schedule_print_dialog.h
        src/features/sub_prep/ui/sub_prep_class_information_model.cpp
        src/features/sub_prep/ui/sub_prep_class_information_list_model.cpp
        src/features/sub_prep/ui/sub_prep_class_information_list_model.h
        src/features/sub_prep/ui/sub_prep_page.cpp
        src/features/sub_prep/ui/sub_prep_page_class_information.cpp
        src/features/sub_prep/ui/sub_prep_page.h
        src/features/sub_prep/ui/sub_prep_page_settings.cpp
        src/features/sub_prep/ui/sub_prep_page_ui.cpp
        src/features/sub_prep/ui/sub_prep_print_dialog.cpp
        src/features/sub_prep/ui/sub_prep_print_dialog.h
        src/features/sub_prep/ui/sub_prep_print_source_mapper.cpp
        src/features/sub_prep/ui/sub_prep_print_source_mapper.h
        src/features/sub_prep/services/sub_prep_package_service.cpp
        src/features/sub_prep/services/sub_prep_package_service.h
        src/features/sub_prep/services/sub_prep_document_model.cpp
        src/features/sub_prep/services/sub_prep_pdf_renderer.cpp
        src/features/sub_prep/services/sub_prep_print_service.cpp
        src/features/sub_prep/services/sub_prep_print_service.h
        src/ui/shared/pages/basepage.cpp
        src/ui/shared/pages/basepage.h
        src/ui/shared/input/hangul_composer.cpp
        src/ui/shared/widgets/sectioncards/class_info_section_card.cpp
        src/ui/shared/widgets/sectioncards/class_info_section_card.h
        src/ui/shared/widgets/navigation_pill_button.cpp
        src/ui/shared/widgets/navigation_pill_button.h
        src/ui/shared/widgets/navigation_pill_style.cpp
        src/ui/shared/widgets/navigation_pill_style.h
        src/ui/shared/widgets/navigation_settings_button.cpp
        src/ui/shared/widgets/navigation_settings_button.h
        src/ui/shared/widgets/navigation_tab_widget.cpp
        src/ui/shared/widgets/navigation_tab_widget.h
        src/ui/shared/widgets/on_screen_keyboard.cpp
        src/features/schedule/ui/schedule_cell_widget_factory.cpp
        src/features/schedule/ui/schedule_cell_widget_factory.h
        src/features/schedule/ui/schedule_table_renderer.cpp
        src/features/schedule/ui/schedule_table_renderer.h
        src/features/schedule/ui/schedule_widget.cpp
        src/features/schedule/ui/schedule_widget.h
        src/features/schedule/ui/testing_assignment_dialog.cpp
        src/features/schedule/ui/testing_assignment_dialog.h
    )

    target_compile_features(ClassMngrSubPrepPageTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSubPrepPageTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_compile_definitions(ClassMngrSubPrepPageTests
        PRIVATE
            CLASSMNGR_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )

    target_link_libraries(ClassMngrSubPrepPageTests
        PRIVATE
            ClassMngrScheduleWidgetTestSupport
            ClassMngrScheduleWidgetResourcePackTestSupport
            Qt6::Core
            Qt6::Gui
            Qt6::Pdf
            Qt6::PrintSupport
            Qt6::Sql
            Qt6::Widgets
            Qt6::Test
    )

    add_test(
        NAME ClassMngrSubPrepPageTests
        COMMAND ClassMngrSubPrepPageTests
    )

    set_tests_properties(
        ClassMngrScheduleWidgetTests
        ClassMngrSubPrepPageTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_resources(ClassMngrSubPrepPageTests sub_prep_keyboard_test_resources
        PREFIX "/"
        BASE "${PROJECT_SOURCE_DIR}/resources"
        FILES
            resources/assets/icons/keyboard_dark.svg
            resources/assets/icons/keyboard_light.svg
    )

    qt_add_executable(ClassMngrNavigationTabWidgetTests
        tests/navigation_tab_widget_tests.cpp
        src/core/fontmanager.cpp
        src/ui/shared/widgets/navigation_pill_button.cpp
        src/ui/shared/widgets/navigation_pill_button.h
        src/ui/shared/widgets/navigation_pill_style.cpp
        src/ui/shared/widgets/navigation_pill_style.h
        src/ui/shared/widgets/navigation_settings_button.cpp
        src/ui/shared/widgets/navigation_settings_button.h
        src/ui/shared/widgets/navigation_tab_widget.cpp
        src/ui/shared/widgets/navigation_tab_widget.h
    )

    target_compile_features(ClassMngrNavigationTabWidgetTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrNavigationTabWidgetTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_compile_definitions(ClassMngrNavigationTabWidgetTests
        PRIVATE
            CLASSMNGR_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )

    target_link_libraries(ClassMngrNavigationTabWidgetTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrNavigationTabWidgetTests
        COMMAND ClassMngrNavigationTabWidgetTests
    )

    set_tests_properties(
        ClassMngrNavigationTabWidgetTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(ClassMngrSidebarStructureTests
        tests/sidebar_structure_tests.cpp
        src/core/fontmanager.cpp
        src/core/resource_packs/resource_pack_manager.cpp
        src/core/updater/version.cpp
        src/features/documents/document_catalog.cpp
        src/ui/shared/widgets/sidebar/sidebar.cpp
        src/ui/shared/widgets/sidebar/sidebar_context_menu.cpp
        src/ui/shared/widgets/sidebar/sidebar_definitions.cpp
        src/ui/shared/widgets/sidebar/sidebar_marquee_delegate.cpp
        src/ui/shared/widgets/sidebar/sidebar_overflow.cpp
        src/ui/shared/widgets/sidebar/sidebar_selection.cpp
        src/ui/shared/widgets/sidebar/sidebar_teachers.cpp
        src/ui/shared/widgets/sidebar/sidebar_tree.cpp
    )

    target_compile_features(ClassMngrSidebarStructureTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSidebarStructureTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_compile_definitions(ClassMngrSidebarStructureTests
        PRIVATE
            CLASSMNGR_TEST_DOCUMENTS_PATH="${PROJECT_SOURCE_DIR}/resources/assets/documents"
    )

    target_link_libraries(ClassMngrSidebarStructureTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrSidebarStructureTests
        COMMAND ClassMngrSidebarStructureTests
    )

    set_tests_properties(
        ClassMngrSidebarStructureTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
