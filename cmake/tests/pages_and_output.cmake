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
    NAME TypedSignatureRenderer
    SOURCES
        tests/typed_signature_renderer_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

qt_add_executable(ClassMngrClassesPageTests
        tests/classes_page_tests.cpp
        tests/schedule_widget_test_stubs.cpp
        src/core/utils/colorutils.cpp
        src/core/utils/sidebar_node_naming.cpp
        src/domain/models/classroom.cpp
        src/domain/models/roster.cpp
        src/features/classes/class_navigation_preferences.cpp
        src/features/classes/class_navigation_preferences.h
        src/features/classes/config/class_info_config.cpp
        src/features/classes/models/class_tab_navigation_model.cpp
        src/features/classes/models/class_tab_navigation_model.h
        src/features/classes/ui/class_co_teacher_page.cpp
        src/features/classes/ui/class_co_teacher_page.h
        src/features/classes/ui/class_details_page.cpp
        src/features/classes/ui/class_details_page.h
        src/features/classes/ui/classes_page.cpp
        src/features/classes/ui/classes_page.h
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
        src/features/roster/ui/roster_table_view.cpp
        src/features/schedule/schedule_display_mode_preferences.cpp
        src/features/schedule/schedule_display_mode_preferences.h
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
            ClassMngrEngine
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

    qt_add_executable(ClassMngrScheduleImportDialogTests
        tests/schedule_import_dialog_tests.cpp
        tests/schedule_widget_test_stubs.cpp
        src/core/settingsmanager.cpp
        src/core/utils/colorutils.cpp
        src/domain/models/classroom.cpp
        src/features/calendar/calendar_workbook_reader.cpp
        src/features/classes/config/class_info_config.cpp
        src/features/schedule/schedule_display_mode_preferences.cpp
        src/features/schedule/schedule_display_mode_preferences.h
        src/features/schedule/schedule_settings_preferences.cpp
        src/features/schedule/schedule_settings_preferences.h
        src/features/schedule/import/schedule_workbook_parser.cpp
        src/features/schedule/ui/schedule_editor_dialog.h
        src/features/schedule/ui/schedule_import_dialog.cpp
        src/features/schedule/ui/schedule_import_dialog.h
        src/features/schedule/ui/schedule_import_dialog_shared.cpp
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

    target_link_libraries(ClassMngrScheduleImportDialogTests
        PRIVATE
            ClassMngrEngine
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
        tests/schedule_widget_test_stubs.cpp
        src/core/utils/sidebar_node_naming.cpp
        src/domain/models/classroom.cpp
        src/features/campus/data/campus_json_codec.cpp
        src/features/campus/data/campus_json_repository.cpp
        src/features/classes/config/class_info_config.cpp
        src/features/classes/models/class_tab_navigation_model.cpp
        src/features/classes/models/class_tab_navigation_model.h
        src/features/schedule/schedule_display_mode_preferences.cpp
        src/features/schedule/schedule_display_mode_preferences.h
        src/features/schedule/schedule_settings_preferences.cpp
        src/features/schedule/schedule_settings_preferences.h
        src/features/schedule/ui/schedule_editor_dialog.h
        src/features/schedule/ui/schedule_print_dialog.h
        src/features/sub_prep/ui/sub_prep_class_information_model.cpp
        src/features/sub_prep/ui/sub_prep_page.cpp
        src/features/sub_prep/ui/sub_prep_page_class_information.cpp
        src/features/sub_prep/ui/sub_prep_page.h
        src/features/sub_prep/ui/sub_prep_page_settings.cpp
        src/features/sub_prep/ui/sub_prep_page_ui.cpp
        src/features/sub_prep/ui/sub_prep_print_dialog.cpp
        src/features/sub_prep/ui/sub_prep_print_dialog.h
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
        src/features/schedule/schedule_display_mode_preferences.cpp
        src/features/schedule/schedule_display_mode_preferences.h
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
            ClassMngrEngine
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
            ClassMngrEngine
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
