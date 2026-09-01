qt_add_executable(ClassMngrClassTransferTests
        tests/class_transfer_tests.cpp
        src/core/utils/sidebar_node_naming.cpp
        src/data/data_service.cpp
        src/data/database/database_schema_manager.cpp
        src/data/database/database_transaction.cpp
        src/data/repositories/calendar_event_repository.cpp
        src/data/repositories/campus_record_repository.cpp
        src/data/repositories/class_info_repository.cpp
        src/data/repositories/class_repository.cpp
        src/data/repositories/class_transfer_repository.cpp
        src/data/repositories/intensive_slot_state_repository.cpp
        src/data/repositories/testing_block_repository.cpp
        src/data/repositories/testing_class_repository.cpp
        src/data/repositories/gs_team_repository.cpp
        src/data/repositories/native_english_teacher_repository.cpp
        src/data/repositories/roster_repository.cpp
        src/data/repositories/schedule_import_repository.cpp
        src/data/repositories/settings_repository.cpp
        src/data/repositories/speaking_eval_repository.cpp
        src/data/repositories/teacher_repository.cpp
        src/data/repositories/teacher_import_repository.cpp
        src/domain/models/classroom.cpp
        src/domain/models/roster.cpp
        src/features/classes/config/class_info_config.cpp
        src/features/classes/services/class_transfer_json_codec.cpp
        src/features/classes/ui/class_export_dialog.cpp
        src/features/classes/ui/class_import_dialog.cpp
    )

    target_compile_features(ClassMngrClassTransferTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrClassTransferTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrClassTransferTests
        PRIVATE
            ClassMngrEngine
            Qt6::Core
            Qt6::Gui
            Qt6::Sql
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrClassTransferTests
        COMMAND ClassMngrClassTransferTests
    )

    qt_add_executable(ClassMngrCalendarImportTests
        tests/calendar_import_tests.cpp
        src/features/calendar/calendar_event_campus_filter.cpp
        src/features/calendar/academic_calendar_event_parser.cpp
        src/features/calendar/calendar_workbook_reader.cpp
    )

    target_compile_features(ClassMngrCalendarImportTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrCalendarImportTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrCalendarImportTests
        PRIVATE
            ClassMngrEngine
            Qt6::Core
            Qt6::Test
            ZLIB::ZLIB
    )

    add_test(
        NAME ClassMngrCalendarImportTests
        COMMAND ClassMngrCalendarImportTests
    )

    qt_add_executable(ClassMngrScheduleImportTests
        tests/schedule_import_tests.cpp
        src/data/database/database_schema_manager.cpp
        src/data/database/database_transaction.cpp
        src/data/repositories/class_info_repository.cpp
        src/data/repositories/class_repository.cpp
        src/data/repositories/schedule_import_repository.cpp
        src/data/repositories/teacher_repository.cpp
        src/domain/models/classroom.cpp
        src/features/calendar/calendar_workbook_reader.cpp
        src/features/classes/config/class_info_config.cpp
        src/features/schedule/import/schedule_workbook_parser.cpp
    )

    target_compile_features(ClassMngrScheduleImportTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrScheduleImportTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrScheduleImportTests
        PRIVATE
            ClassMngrEngine
            Qt6::Core
            Qt6::Sql
            Qt6::Test
            ZLIB::ZLIB
    )

    add_test(
        NAME ClassMngrScheduleImportTests
        COMMAND ClassMngrScheduleImportTests
    )

    qt_add_executable(ClassMngrTeacherImportTests
        tests/teacher_import_tests.cpp
        src/data/database/database_schema_manager.cpp
        src/data/database/database_transaction.cpp
        src/data/repositories/gs_team_repository.cpp
        src/data/repositories/teacher_import_repository.cpp
        src/features/calendar/calendar_workbook_reader.cpp
        src/features/teacher/import/sectioned_contact_list_template.cpp
        src/features/teacher/import/teacher_import_file_validator.cpp
        src/features/teacher/import/teacher_import_template_registry.cpp
    )

    target_compile_features(ClassMngrTeacherImportTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrTeacherImportTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrTeacherImportTests
        PRIVATE
            ClassMngrEngine
            Qt6::Core
            Qt6::Sql
            Qt6::Test
            ZLIB::ZLIB
    )

    add_test(
        NAME ClassMngrTeacherImportTests
        COMMAND ClassMngrTeacherImportTests
    )

    qt_add_executable(ClassMngrTeacherImportDialogTests
        tests/teacher_import_dialog_tests.cpp
        src/core/settingsmanager.cpp
        src/features/calendar/calendar_workbook_reader.cpp
        src/features/teacher/import/sectioned_contact_list_template.cpp
        src/features/teacher/import/teacher_import_file_validator.cpp
        src/features/teacher/import/teacher_import_template_registry.cpp
        src/features/teacher/ui/teacher_import_dialog.cpp
    )

    target_compile_features(ClassMngrTeacherImportDialogTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrTeacherImportDialogTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrTeacherImportDialogTests
        PRIVATE
            Qt6::Concurrent
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
            ZLIB::ZLIB
    )

    add_test(
        NAME ClassMngrTeacherImportDialogTests
        COMMAND ClassMngrTeacherImportDialogTests
    )

    qt_add_executable(ClassMngrStaffDirectoryPageTests
        tests/staff_directory_page_tests.cpp
        tests/staff_directory_page_test_stubs.cpp
        src/core/fontmanager.cpp
        src/ui/shared/input/hangul_composer.cpp
        src/features/teacher/ui/staff_directory_page.cpp
        src/ui/shared/pages/basepage.cpp
        src/ui/shared/widgets/on_screen_keyboard.cpp
    )

    target_compile_features(ClassMngrStaffDirectoryPageTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrStaffDirectoryPageTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrStaffDirectoryPageTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Sql
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrStaffDirectoryPageTests
        COMMAND ClassMngrStaffDirectoryPageTests
    )

    set_tests_properties(
        ClassMngrStaffDirectoryPageTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_resources(ClassMngrStaffDirectoryPageTests staff_directory_keyboard_test_resources
        PREFIX "/"
        BASE "${PROJECT_SOURCE_DIR}/resources"
        FILES
            resources/assets/icons/keyboard_dark.svg
            resources/assets/icons/keyboard_light.svg
    )

    qt_add_executable(ClassMngrClassTabNavigationModelTests
        tests/class_tab_navigation_model_tests.cpp
        src/features/classes/config/class_info_config.cpp
        src/features/classes/models/class_tab_navigation_model.cpp
        src/ui/shared/widgets/sidebar/sidebar_definitions.cpp
    )

    target_compile_features(ClassMngrClassTabNavigationModelTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrClassTabNavigationModelTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrClassTabNavigationModelTests
        PRIVATE
            ClassMngrEngine
            Qt6::Core
            Qt6::Test
    )

    add_test(
        NAME ClassMngrClassTabNavigationModelTests
        COMMAND ClassMngrClassTabNavigationModelTests
    )

    qt_add_executable(ClassMngrSubPrepClassInformationModelTests
        tests/sub_prep_class_information_model_tests.cpp
        src/core/utils/sidebar_node_naming.cpp
        src/domain/models/classroom.cpp
        src/features/classes/config/class_info_config.cpp
        src/features/sub_prep/ui/sub_prep_class_information_model.cpp
    )

    target_compile_features(ClassMngrSubPrepClassInformationModelTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSubPrepClassInformationModelTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrSubPrepClassInformationModelTests
        PRIVATE
            ClassMngrEngine
            Qt6::Core
            Qt6::Test
    )

    add_test(
        NAME ClassMngrSubPrepClassInformationModelTests
        COMMAND ClassMngrSubPrepClassInformationModelTests
    )

    qt_add_executable(ClassMngrScheduleWidgetTests
        tests/schedule_widget_tests.cpp
        tests/schedule_widget_test_stubs.cpp
        src/domain/models/classroom.cpp
        src/features/schedule/schedule_display_mode_preferences.cpp
        src/features/schedule/schedule_display_mode_preferences.h
        src/features/schedule/schedule_settings_preferences.cpp
        src/features/schedule/schedule_settings_preferences.h
        src/features/schedule/ui/schedule_editor_dialog.h
        src/features/schedule/ui/schedule_page.cpp
        src/features/schedule/ui/schedule_page.h
        src/features/schedule/ui/schedule_print_dialog.h
        src/features/schedule/ui/schedule_cell_widget_factory.cpp
        src/features/schedule/ui/schedule_cell_widget_factory.h
        src/features/schedule/ui/schedule_table_renderer.cpp
        src/features/schedule/ui/schedule_table_renderer.h
        src/features/schedule/ui/schedule_widget.cpp
        src/features/schedule/ui/schedule_widget.h
        src/features/schedule/ui/testing_assignment_dialog.cpp
        src/features/schedule/ui/testing_assignment_dialog.h
        src/ui/shared/pages/basepage.cpp
        src/ui/shared/pages/basepage.h
    )

    target_compile_features(ClassMngrScheduleWidgetTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrScheduleWidgetTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrScheduleWidgetTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::PrintSupport
            Qt6::Sql
            Qt6::Widgets
            Qt6::Test
    )

    add_test(
        NAME ClassMngrScheduleWidgetTests
        COMMAND ClassMngrScheduleWidgetTests
    )

    qt_add_executable(ClassMngrTestingClassesPageTests
        tests/testing_classes_page_tests.cpp
        tests/schedule_widget_test_stubs.cpp
        src/core/utils/colorutils.cpp
        src/core/utils/sidebar_node_naming.cpp
        src/domain/models/classroom.cpp
        src/domain/models/roster.cpp
        src/features/classes/ui/testing_classes_page.cpp
        src/features/classes/ui/testing_classes_page.h
        src/features/roster/ui/roster_column_layout_controller.cpp
        src/features/roster/ui/roster_editor_widget.cpp
        src/features/roster/ui/roster_editor_widget.h
        src/features/roster/ui/roster_editor_widget_columns.cpp
        src/features/roster/ui/roster_editor_widget_students.cpp
        src/features/roster/ui/roster_editor_widget_transfer.cpp
        src/features/roster/ui/roster_header_view.cpp
        src/features/roster/ui/roster_item_delegate.cpp
        src/features/roster/ui/roster_model.cpp
        src/features/roster/ui/roster_model_columns.cpp
        src/features/roster/ui/roster_model_names.cpp
        src/features/roster/ui/roster_model_rows.cpp
        src/features/roster/ui/roster_model_validation.cpp
        src/features/roster/ui/roster_table_view.cpp
        src/features/roster/ui/roster_editor_widget_ui.cpp
        src/features/schedule/ui/schedule_editor_dialog.h
        src/features/schedule/ui/schedule_print_dialog.h
        src/ui/shared/pages/basepage.cpp
        src/ui/shared/input/hangul_composer.cpp
        src/ui/shared/widgets/clickable_color_preview.cpp
        src/ui/shared/widgets/no_wheel_combobox.cpp
        src/ui/shared/widgets/on_screen_keyboard.cpp
    )

    qt_add_resources(ClassMngrTestingClassesPageTests keyboard_integration_test_resources
        PREFIX "/"
        BASE "${PROJECT_SOURCE_DIR}/resources"
        FILES
            resources/assets/icons/keyboard_dark.svg
            resources/assets/icons/keyboard_light.svg
    )

    target_compile_features(ClassMngrTestingClassesPageTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrTestingClassesPageTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrTestingClassesPageTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::PrintSupport
            Qt6::Sql
            Qt6::Widgets
            Qt6::Test
    )

    add_test(
        NAME ClassMngrTestingClassesPageTests
        COMMAND ClassMngrTestingClassesPageTests
    )

    set_tests_properties(
        ClassMngrTestingClassesPageTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    classmngr_add_qt_test(
        NAME UpcomingBirthdays
        SOURCES
            tests/upcoming_birthdays_tests.cpp
        LIBRARIES
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
        OFFSCREEN
    )
