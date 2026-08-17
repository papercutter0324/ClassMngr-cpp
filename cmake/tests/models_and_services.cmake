qt_add_executable(ClassMngrBasePageTests
        tests/basepage_tests.cpp
        src/core/fontmanager.cpp
        src/ui/shared/pages/basepage.cpp
    )

    target_compile_features(ClassMngrBasePageTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrBasePageTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrBasePageTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrBasePageTests
        COMMAND ClassMngrBasePageTests
    )

    qt_add_executable(ClassMngrSchedulePrintModelTests
        tests/schedule_print_model_tests.cpp
        src/features/schedule/ui/schedule_time_formatter.cpp
        src/features/schedule/ui/schedule_view_model.cpp
    )

    target_compile_features(ClassMngrSchedulePrintModelTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSchedulePrintModelTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrSchedulePrintModelTests
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    add_test(
        NAME ClassMngrSchedulePrintModelTests
        COMMAND ClassMngrSchedulePrintModelTests
    )

    qt_add_executable(ClassMngrScheduleBuilderTests
        tests/schedule_builder_tests.cpp
        tests/schedule_builder_test_stubs.cpp
        src/features/schedule/ui/schedule_builder.cpp
    )

    target_compile_features(ClassMngrScheduleBuilderTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrScheduleBuilderTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrScheduleBuilderTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Sql
            Qt6::Test
    )

    add_test(
        NAME ClassMngrScheduleBuilderTests
        COMMAND ClassMngrScheduleBuilderTests
    )

    qt_add_executable(ClassMngrSchedulePrintPdfTests
        tests/schedule_print_pdf_tests.cpp
        src/core/fontmanager.cpp
        src/features/schedule/services/schedule_print_service.cpp
        src/features/schedule/ui/schedule_time_formatter.cpp
        src/features/schedule/ui/schedule_view_model.cpp
    )

    target_compile_features(ClassMngrSchedulePrintPdfTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSchedulePrintPdfTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrSchedulePrintPdfTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Pdf
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrSchedulePrintPdfTests
        COMMAND ClassMngrSchedulePrintPdfTests
    )

    qt_add_executable(ClassMngrSubPrepPrintPdfTests
        tests/sub_prep_print_pdf_tests.cpp
        src/core/fontmanager.cpp
        src/features/schedule/ui/schedule_time_formatter.cpp
        src/features/schedule/ui/schedule_view_model.cpp
        src/features/sub_prep/services/sub_prep_document_model.cpp
        src/features/sub_prep/services/sub_prep_pdf_renderer.cpp
        src/features/sub_prep/services/sub_prep_print_service.cpp
    )

    target_compile_features(ClassMngrSubPrepPrintPdfTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSubPrepPrintPdfTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrSubPrepPrintPdfTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Pdf
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrSubPrepPrintPdfTests
        COMMAND ClassMngrSubPrepPrintPdfTests
    )

    set_tests_properties(
        ClassMngrSubPrepPrintPdfTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )

    qt_add_executable(ClassMngrSubPrepPackageServiceTests
        tests/sub_prep_package_service_tests.cpp
        src/core/fontmanager.cpp
        src/core/utils/sidebar_node_naming.cpp
        src/domain/models/classroom.cpp
        src/domain/models/roster.cpp
        src/features/roster/services/roster_template_print_service.cpp
        src/features/schedule/ui/schedule_time_formatter.cpp
        src/features/schedule/ui/schedule_view_model.cpp
        src/features/sub_prep/services/sub_prep_document_model.cpp
        src/features/sub_prep/services/sub_prep_pdf_renderer.cpp
        src/features/sub_prep/services/sub_prep_package_service.cpp
        src/features/sub_prep/services/sub_prep_print_service.cpp
    )

    target_compile_features(ClassMngrSubPrepPackageServiceTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSubPrepPackageServiceTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrSubPrepPackageServiceTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Pdf
            Qt6::PrintSupport
            Qt6::Sql
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrSubPrepPackageServiceTests
        COMMAND ClassMngrSubPrepPackageServiceTests
    )

    set_tests_properties(ClassMngrSubPrepPackageServiceTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )

    qt_add_executable(ClassMngrRosterModelTests
        tests/roster_model_tests.cpp
        src/domain/models/roster.cpp
        src/features/roster/ui/roster_model.cpp
        src/features/roster/ui/roster_model_columns.cpp
        src/features/roster/ui/roster_model_names.cpp
        src/features/roster/ui/roster_model_rows.cpp
        src/features/roster/ui/roster_model_validation.cpp
    )

    target_compile_features(ClassMngrRosterModelTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrRosterModelTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrRosterModelTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
    )

    add_test(
        NAME ClassMngrRosterModelTests
        COMMAND ClassMngrRosterModelTests
    )

    qt_add_executable(ClassMngrRosterTemplatePrintServiceTests
        tests/roster_template_print_service_tests.cpp
        src/core/fontmanager.cpp
        src/domain/models/classroom.cpp
        src/features/roster/services/roster_template_print_service.cpp
    )

    target_compile_features(ClassMngrRosterTemplatePrintServiceTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrRosterTemplatePrintServiceTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_compile_definitions(ClassMngrRosterTemplatePrintServiceTests
        PRIVATE
            CLASSMNGR_TEST_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )

    target_link_libraries(ClassMngrRosterTemplatePrintServiceTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Pdf
            Qt6::Sql
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrRosterTemplatePrintServiceTests
        COMMAND ClassMngrRosterTemplatePrintServiceTests
    )

    set_tests_properties(ClassMngrRosterTemplatePrintServiceTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )

    qt_add_executable(ClassMngrLanguageServiceTests
        tests/language_service_tests.cpp
        src/core/language_service.cpp
        src/core/settingsmanager.cpp
    )

    target_compile_features(ClassMngrLanguageServiceTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrLanguageServiceTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrLanguageServiceTests
        PRIVATE
            Qt6::Core
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrLanguageServiceTests
        COMMAND ClassMngrLanguageServiceTests
    )

    qt_add_executable(ClassMngrAiCommentOptionsTests
        tests/ai_comment_options_tests.cpp
        src/core/settingsmanager.cpp
        src/ui/shared/actions/action_registry.cpp
        src/ui/shared/state/ai_comment_options.cpp
        src/ui/shared/styles/themed_icon_utils.cpp
    )

    target_compile_features(ClassMngrAiCommentOptionsTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrAiCommentOptionsTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrAiCommentOptionsTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrAiCommentOptionsTests
        COMMAND ClassMngrAiCommentOptionsTests
    )

    set_tests_properties(ClassMngrAiCommentOptionsTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    if(APPLE)
        qt_add_executable(ClassMngrPowerPointDataAccessNoticeTests
            tests/powerpoint_data_access_notice_tests.cpp
            src/core/settingsmanager.cpp
            src/ui/shared/actions/action_registry.cpp
            src/ui/shared/styles/themed_icon_utils.cpp
        )

        target_compile_features(ClassMngrPowerPointDataAccessNoticeTests
            PRIVATE
                cxx_std_23
        )

        target_include_directories(ClassMngrPowerPointDataAccessNoticeTests
            PRIVATE
                ${PROJECT_SOURCE_DIR}/src
        )

        target_link_libraries(ClassMngrPowerPointDataAccessNoticeTests
            PRIVATE
                Qt6::Core
                Qt6::Gui
                Qt6::Test
                Qt6::Widgets
        )

        add_test(
            NAME ClassMngrPowerPointDataAccessNoticeTests
            COMMAND ClassMngrPowerPointDataAccessNoticeTests
        )

        set_tests_properties(ClassMngrPowerPointDataAccessNoticeTests
            PROPERTIES
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        )
    endif()

    qt_add_executable(ClassMngrSpeakingAnalyticsTests
        tests/speaking_analytics_tests.cpp
        src/features/classes/services/speaking_analytics.cpp
    )

    target_compile_features(ClassMngrSpeakingAnalyticsTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSpeakingAnalyticsTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrSpeakingAnalyticsTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
    )

    add_test(
        NAME ClassMngrSpeakingAnalyticsTests
        COMMAND ClassMngrSpeakingAnalyticsTests
    )
