qt_add_executable(ClassMngrTeacherInfoSectionTests
        tests/teacher_info_section_tests.cpp
        src/core/fontmanager.cpp
        src/features/teacher/ui/teacher_model.cpp
        src/ui/shared/widgets/no_wheel_combobox.cpp
        src/ui/shared/widgets/sections/teacher_info_section.cpp
    )

    target_compile_features(ClassMngrTeacherInfoSectionTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrTeacherInfoSectionTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_compile_definitions(ClassMngrTeacherInfoSectionTests
        PRIVATE
            CLASSMNGR_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )

    target_link_libraries(ClassMngrTeacherInfoSectionTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrTeacherInfoSectionTests
        COMMAND ClassMngrTeacherInfoSectionTests
    )

    qt_add_executable(ClassMngrTeacherInfoPageTests
        tests/teacher_info_page_tests.cpp
        tests/teacher_info_page_test_stubs.cpp
        src/core/fontmanager.cpp
        src/core/utils/sidebar_node_naming.cpp
        src/ui/shared/input/hangul_composer.cpp
        src/features/teacher/ui/teacher_info_page.cpp
        src/ui/shared/pages/basepage.cpp
        src/ui/shared/pages/page_header.cpp
        src/ui/shared/widgets/no_wheel_combobox.cpp
        src/ui/shared/widgets/on_screen_keyboard.cpp
        src/ui/shared/widgets/sectioncards/teacher_section_card.cpp
    )

    target_compile_features(ClassMngrTeacherInfoPageTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrTeacherInfoPageTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_compile_definitions(ClassMngrTeacherInfoPageTests
        PRIVATE
            CLASSMNGR_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )

    target_link_libraries(ClassMngrTeacherInfoPageTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Sql
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrTeacherInfoPageTests
        COMMAND ClassMngrTeacherInfoPageTests
    )

    set_tests_properties(
        ClassMngrTeacherInfoPageTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_resources(ClassMngrTeacherInfoPageTests teacher_info_keyboard_test_resources
        PREFIX "/"
        BASE "${PROJECT_SOURCE_DIR}/resources"
        FILES
            resources/assets/icons/keyboard_dark.svg
            resources/assets/icons/keyboard_light.svg
    )

    qt_add_executable(ClassMngrIntensiveSlotStateRepositoryTests
        tests/intensive_slot_state_repository_tests.cpp
        src/data/database/sql_query_utils.cpp
        src/data/repositories/intensive_slot_state_repository.cpp
    )

    target_compile_features(ClassMngrIntensiveSlotStateRepositoryTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrIntensiveSlotStateRepositoryTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrIntensiveSlotStateRepositoryTests
        PRIVATE
            Qt6::Core
            Qt6::Sql
            Qt6::Test
    )

    add_test(
        NAME ClassMngrIntensiveSlotStateRepositoryTests
        COMMAND ClassMngrIntensiveSlotStateRepositoryTests
    )

    qt_add_executable(ClassMngrTestingBlockRepositoryTests
        tests/testing_block_repository_tests.cpp
        src/data/database/database_transaction.cpp
        src/data/repositories/testing_block_repository.cpp
    )

    target_compile_features(ClassMngrTestingBlockRepositoryTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrTestingBlockRepositoryTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrTestingBlockRepositoryTests
        PRIVATE
            Qt6::Core
            Qt6::Sql
            Qt6::Test
    )

    add_test(
        NAME ClassMngrTestingBlockRepositoryTests
        COMMAND ClassMngrTestingBlockRepositoryTests
    )

    qt_add_executable(ClassMngrTestingClassRepositoryTests
        tests/testing_class_repository_tests.cpp
        src/data/database/database_transaction.cpp
        src/data/repositories/class_repository.cpp
        src/data/repositories/testing_class_repository.cpp
        src/domain/models/classroom.cpp
    )

    target_compile_features(ClassMngrTestingClassRepositoryTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrTestingClassRepositoryTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrTestingClassRepositoryTests
        PRIVATE
            Qt6::Core
            Qt6::Sql
            Qt6::Test
    )

    add_test(
        NAME ClassMngrTestingClassRepositoryTests
        COMMAND ClassMngrTestingClassRepositoryTests
    )

    qt_add_executable(ClassMngrCalendarEventRepositoryTests
        tests/calendar_event_repository_tests.cpp
        src/data/database/database_schema_manager.cpp
        src/data/database/database_transaction.cpp
        src/data/database/sql_query_utils.cpp
        src/data/repositories/calendar_event_repository.cpp
    )

    target_compile_features(ClassMngrCalendarEventRepositoryTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrCalendarEventRepositoryTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrCalendarEventRepositoryTests
        PRIVATE
            Qt6::Core
            Qt6::Sql
            Qt6::Test
    )

    add_test(
        NAME ClassMngrCalendarEventRepositoryTests
        COMMAND ClassMngrCalendarEventRepositoryTests
    )

    qt_add_executable(ClassMngrCalendarEventCacheTests
        tests/calendar_event_cache_tests.cpp
        src/data/database/database_schema_manager.cpp
        src/data/database/database_transaction.cpp
        src/data/database/sql_query_utils.cpp
        src/data/repositories/calendar_event_repository.cpp
        src/features/calendar/calendar_event_campus_filter.cpp
        src/features/calendar/ui/calendar_event_cache.cpp
        src/features/calendar/ui/calendar_event_model.cpp
    )

    target_compile_features(ClassMngrCalendarEventCacheTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrCalendarEventCacheTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrCalendarEventCacheTests
        PRIVATE
            Qt6::Concurrent
            Qt6::Core
            Qt6::Sql
            Qt6::Test
    )

    add_test(
        NAME ClassMngrCalendarEventCacheTests
        COMMAND ClassMngrCalendarEventCacheTests
    )

    qt_add_executable(ClassMngrDataServiceLifecycleTests
        tests/data_service_lifecycle_tests.cpp
        src/app/services/feature_services.cpp
        src/data/data_service.cpp
        src/data/database/database_session.cpp
        src/data/database/database_schema_manager.cpp
        src/data/database/database_transaction.cpp
        src/data/database/sql_query_utils.cpp
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
    )

    target_compile_features(ClassMngrDataServiceLifecycleTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrDataServiceLifecycleTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrDataServiceLifecycleTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Sql
            Qt6::Test
    )

    add_test(
        NAME ClassMngrDataServiceLifecycleTests
        COMMAND ClassMngrDataServiceLifecycleTests
    )
