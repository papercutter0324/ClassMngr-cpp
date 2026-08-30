qt_add_executable(ClassMngrAcademicCalendarTests
        tests/academic_calendar_tests.cpp
        src/features/calendar/academic_calendar_schedule.cpp
        src/features/calendar/ui/academic_calendar_provider.cpp
    )

    target_compile_features(ClassMngrAcademicCalendarTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrAcademicCalendarTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrAcademicCalendarTests
        PRIVATE
            ClassMngrEngine
            Qt6::Core
            Qt6::Gui
            Qt6::Sql
            Qt6::Test
    )

    add_test(
        NAME ClassMngrAcademicCalendarTests
        COMMAND ClassMngrAcademicCalendarTests
    )

    qt_add_executable(ClassMngrCampusMapTests
        tests/campus_map_tests.cpp
        src/features/campus/data/campus_json_codec.cpp
        src/features/campus/data/campus_json_repository.cpp
        src/features/campus/ui/campus_map_preview.cpp
    )

    file(GLOB_RECURSE CLASSMNGR_CAMPUS_MAP_IMAGES CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/resources/assets/campuses/*.png"
        "${PROJECT_SOURCE_DIR}/resources/assets/campuses/*.jpg"
        "${PROJECT_SOURCE_DIR}/resources/assets/campuses/*.jpeg"
    )

    qt_add_resources(ClassMngrCampusMapTests campus_map_test_resources
        PREFIX "/"
        BASE "${PROJECT_SOURCE_DIR}/resources"
        FILES
            ${CLASSMNGR_CAMPUS_MAP_IMAGES}
    )

    target_compile_features(ClassMngrCampusMapTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrCampusMapTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrCampusMapTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrCampusMapTests
        COMMAND ClassMngrCampusMapTests
    )

    set_tests_properties(
        ClassMngrCampusMapTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(ClassMngrCampusDashboardPageTests
        tests/campus_dashboard_page_tests.cpp
        src/core/fontmanager.cpp
        src/core/resource_packs/resource_pack_manager.cpp
        src/core/settingsmanager.cpp
        src/core/updater/version.cpp
        src/features/campus/data/campus_json_codec.cpp
        src/features/campus/data/campus_json_repository.cpp
        src/features/campus/ui/campus_dashboard_page.cpp
        src/features/campus/ui/campus_dashboard_page_address.cpp
        src/features/campus/ui/campus_dashboard_page_address_ui.cpp
        src/features/campus/ui/campus_dashboard_page_data.cpp
        src/features/campus/ui/campus_dashboard_page_detail.cpp
        src/features/campus/ui/campus_dashboard_page_directions_ui.cpp
        src/features/campus/ui/campus_dashboard_page_form.cpp
        src/features/campus/ui/campus_dashboard_page_housing.cpp
        src/features/campus/ui/campus_dashboard_page_information_ui.cpp
        src/features/campus/ui/campus_dashboard_page_map_ui.cpp
        src/features/campus/ui/campus_dashboard_page_ui.cpp
        src/features/campus/ui/campus_map_preview.cpp
        src/ui/shared/pages/basepage.cpp
        src/ui/shared/widgets/no_wheel_combobox.cpp
        src/ui/shared/widgets/sectioncards/class_info_section_card.cpp
    )

    target_compile_features(ClassMngrCampusDashboardPageTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrCampusDashboardPageTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_compile_definitions(ClassMngrCampusDashboardPageTests
        PRIVATE
            CLASSMNGR_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )

    target_link_libraries(ClassMngrCampusDashboardPageTests
        PRIVATE
            ClassMngrEngine
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrCampusDashboardPageTests
        COMMAND ClassMngrCampusDashboardPageTests
    )

    set_tests_properties(
        ClassMngrCampusDashboardPageTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    classmngr_add_qt_test(
        NAME MemoryUsage
        OFFSCREEN
        SOURCES
            tests/memory_usage_tests.cpp
        LIBRARIES
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    qt_add_executable(ClassMngrSignatureImageProcessorTests
        tests/signature_image_processor_tests.cpp
        src/features/my_info/data/signature_image_processor.cpp
    )

    target_compile_features(ClassMngrSignatureImageProcessorTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSignatureImageProcessorTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrSignatureImageProcessorTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
    )

    add_test(
        NAME ClassMngrSignatureImageProcessorTests
        COMMAND ClassMngrSignatureImageProcessorTests
    )

    qt_add_executable(ClassMngrSpeakingEvalReportWidgetTests
        tests/speaking_eval_report_widget_tests.cpp
        src/core/fontmanager.cpp
        src/features/speaking_eval/services/speaking_eval_report_data_assembler.cpp
        src/features/speaking_eval/ui/speaking_eval_comment_edit.cpp
        src/features/speaking_eval/ui/speaking_eval_report_assets_p.cpp
        src/features/speaking_eval/ui/speaking_eval_report_widget.cpp
        src/features/speaking_eval/ui/speaking_eval_report_widget_interaction.cpp
        src/features/speaking_eval/ui/speaking_eval_report_widget_rendering.cpp
    )

    target_compile_features(ClassMngrSpeakingEvalReportWidgetTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSpeakingEvalReportWidgetTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrSpeakingEvalReportWidgetTests
        PRIVATE
            ClassMngrEngine
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    qt_add_resources(ClassMngrSpeakingEvalReportWidgetTests report_test_resources
        PREFIX "/"
        BASE "${PROJECT_SOURCE_DIR}/resources"
        FILES
            resources/assets/fonts/Inter.ttc
            resources/assets/fonts/JustAnotherHand-Regular.ttf
            resources/assets/fonts/PretendardVariable.ttf
            resources/assets/templates/speaking-eval/advanced-background.png
            resources/assets/templates/speaking-eval/advanced-grey-a.png
            resources/assets/templates/speaking-eval/advanced-grey-aplus.png
            resources/assets/templates/speaking-eval/advanced-grey-b.png
            resources/assets/templates/speaking-eval/advanced-grey-bplus.png
            resources/assets/templates/speaking-eval/advanced-grey-c.png
            resources/assets/templates/speaking-eval/advanced-manifest.json
            resources/assets/templates/speaking-eval/advanced-sprites.png
            resources/assets/templates/speaking-eval/advanced-yellow-a.png
            resources/assets/templates/speaking-eval/advanced-yellow-aplus.png
            resources/assets/templates/speaking-eval/advanced-yellow-b.png
            resources/assets/templates/speaking-eval/advanced-yellow-bplus.png
            resources/assets/templates/speaking-eval/advanced-yellow-c.png
            resources/assets/templates/speaking-eval/standard-background.png
            resources/assets/templates/speaking-eval/standard-manifest.json
            resources/assets/templates/speaking-eval/standard-sprites.png
            resources/assets/templates/speaking-eval/standard-yellow-a.png
            resources/assets/templates/speaking-eval/standard-yellow-aplus.png
            resources/assets/templates/speaking-eval/standard-yellow-b.png
            resources/assets/templates/speaking-eval/standard-yellow-bplus.png
            resources/assets/templates/speaking-eval/standard-yellow-c.png
    )

    add_test(
        NAME ClassMngrSpeakingEvalReportWidgetTests
        COMMAND ClassMngrSpeakingEvalReportWidgetTests
    )

    qt_add_executable(ClassMngrSpeakingEvalBatchReportServiceTests
        tests/speaking_eval_batch_report_service_tests.cpp
        src/core/fontmanager.cpp
        src/core/resource_packs/resource_pack_manager.cpp
        src/core/settingsmanager.cpp
        src/core/updater/version.cpp
        src/core/zip_archive_writer.cpp
        src/features/speaking_eval/services/speaking_eval_ai_prompt.cpp
        src/features/speaking_eval/services/speaking_eval_batch_report_service.cpp
        src/features/speaking_eval/services/speaking_eval_powerpoint_job_model.cpp
        src/features/speaking_eval/services/speaking_eval_powerpoint_job_model.h
        src/features/speaking_eval/services/speaking_eval_powerpoint_scripts.cpp
        src/features/speaking_eval/services/speaking_eval_powerpoint_scripts.h
        src/features/speaking_eval/services/speaking_eval_powerpoint_workspace.cpp
        src/features/speaking_eval/services/speaking_eval_powerpoint_workspace.h
        src/features/speaking_eval/ui/speaking_eval_ai_batch_dialog.cpp
        src/features/speaking_eval/ui/speaking_eval_comment_edit.cpp
        src/features/speaking_eval/ui/speaking_eval_delegate.cpp
        src/features/speaking_eval/ui/speaking_eval_model.cpp
        src/features/speaking_eval/ui/speaking_eval_report_assets_p.cpp
        src/features/speaking_eval/ui/speaking_eval_private_notes_editor.cpp
        src/features/speaking_eval/ui/speaking_eval_notes_dialog.cpp
        src/features/speaking_eval/ui/speaking_eval_report_dialog.cpp
        src/features/speaking_eval/ui/speaking_eval_report_widget.cpp
        src/features/speaking_eval/ui/speaking_eval_report_widget_interaction.cpp
        src/features/speaking_eval/ui/speaking_eval_report_widget_rendering.cpp
        src/features/speaking_eval/ui/speaking_eval_table_view.cpp
        src/ui/shared/printing/pdf_print_dialog.cpp
        src/ui/shared/printing/pdf_print_dialog_pages.cpp
        src/ui/shared/printing/pdf_print_dialog_printer.cpp
        src/ui/shared/printing/pdf_print_dialog_printing.cpp
        src/ui/shared/printing/pdf_print_dialog_widgets.cpp
        src/ui/shared/printing/pdf_print_service.cpp
        src/ui/shared/styles/role_style_registry.cpp
        src/ui/shared/state/ai_comment_options.cpp
        src/ui/shared/widgets/no_wheel_combobox.cpp
    )

    target_compile_features(ClassMngrSpeakingEvalBatchReportServiceTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSpeakingEvalBatchReportServiceTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_compile_definitions(ClassMngrSpeakingEvalBatchReportServiceTests
        PRIVATE
            CLASSMNGR_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )

    target_link_libraries(ClassMngrSpeakingEvalBatchReportServiceTests
        PRIVATE
            ClassMngrEngine
            Qt6::Core
            Qt6::Gui
            Qt6::Pdf
            Qt6::PdfWidgets
            Qt6::PrintSupport
            Qt6::Test
            Qt6::Widgets
            ZLIB::ZLIB
    )

    qt_add_resources(ClassMngrSpeakingEvalBatchReportServiceTests batch_report_test_resources
        PREFIX "/"
        BASE "${PROJECT_SOURCE_DIR}/resources"
        FILES
            resources/assets/fonts/Inter.ttc
            resources/assets/fonts/JustAnotherHand-Regular.ttf
            resources/assets/fonts/PretendardVariable.ttf
            resources/assets/templates/speaking-eval/advanced-background.png
            resources/assets/templates/speaking-eval/advanced-grey-a.png
            resources/assets/templates/speaking-eval/advanced-grey-aplus.png
            resources/assets/templates/speaking-eval/advanced-grey-b.png
            resources/assets/templates/speaking-eval/advanced-grey-bplus.png
            resources/assets/templates/speaking-eval/advanced-grey-c.png
            resources/assets/templates/speaking-eval/advanced-manifest.json
            resources/assets/templates/speaking-eval/advanced-sprites.png
            resources/assets/templates/speaking-eval/advanced-yellow-a.png
            resources/assets/templates/speaking-eval/advanced-yellow-aplus.png
            resources/assets/templates/speaking-eval/advanced-yellow-b.png
            resources/assets/templates/speaking-eval/advanced-yellow-bplus.png
            resources/assets/templates/speaking-eval/advanced-yellow-c.png
            resources/assets/templates/speaking-eval/standard-background.png
            resources/assets/templates/speaking-eval/standard-manifest.json
            resources/assets/templates/speaking-eval/standard-sprites.png
            resources/assets/templates/speaking-eval/standard-yellow-a.png
            resources/assets/templates/speaking-eval/standard-yellow-aplus.png
            resources/assets/templates/speaking-eval/standard-yellow-b.png
            resources/assets/templates/speaking-eval/standard-yellow-bplus.png
            resources/assets/templates/speaking-eval/standard-yellow-c.png
            "resources/assets/documents/Speaking Evaluations/SpeakingEvaluationTemplate-Full.pptx"
            "resources/assets/documents/Speaking Evaluations/SpeakingEvaluationTemplate_Advanced-Full.pptx"
    )

    add_test(
        NAME ClassMngrSpeakingEvalBatchReportServiceTests
        COMMAND ClassMngrSpeakingEvalBatchReportServiceTests
    )

    qt_add_executable(ClassMngrStartupPerformanceTests
        tests/startup_performance_tests.cpp
    )

    add_dependencies(
        ClassMngrStartupPerformanceTests
        ${CLASSMNGR_QT_DESKTOP_TARGET}
    )

    target_compile_features(ClassMngrStartupPerformanceTests
        PRIVATE
            cxx_std_23
    )

    target_link_libraries(ClassMngrStartupPerformanceTests
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    add_test(
        NAME ClassMngrStartupPerformanceTests
        COMMAND ClassMngrStartupPerformanceTests
    )

    qt_add_executable(ClassMngrDatabaseFileFormatTests
        tests/database_file_format_tests.cpp
        src/app/startup_database_path.cpp
        src/core/database_file_format.cpp
    )

    target_compile_features(ClassMngrDatabaseFileFormatTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrDatabaseFileFormatTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrDatabaseFileFormatTests
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    add_test(
        NAME ClassMngrDatabaseFileFormatTests
        COMMAND ClassMngrDatabaseFileFormatTests
    )

    set_tests_properties(
        ClassMngrStartupPerformanceTests
        PROPERTIES
        ENVIRONMENT
            "CLASSMNGR_TEST_APP_PATH=$<TARGET_FILE:${CLASSMNGR_QT_DESKTOP_TARGET}>"
    )

    if(WIN32)
        set_tests_properties(
            ClassMngrUpdaterTests
            ClassMngrResourcePackTests
            ClassMngrFontManagerTests
            ClassMngrTeacherInfoPageTests
            ClassMngrTeacherInfoSectionTests
            ClassMngrIntensiveSlotStateRepositoryTests
            ClassMngrTestingBlockRepositoryTests
            ClassMngrTestingClassRepositoryTests
            ClassMngrCalendarEventRepositoryTests
            ClassMngrCalendarEventCacheTests
            ClassMngrDataServiceLifecycleTests
            ClassMngrCalendarImportTests
            ClassMngrScheduleImportTests
            ClassMngrTeacherImportTests
            ClassMngrTeacherImportDialogTests
            ClassMngrStaffDirectoryPageTests
            ClassMngrClassTabNavigationModelTests
            ClassMngrSubPrepClassInformationModelTests
            ClassMngrScheduleWidgetTests
            ClassMngrTestingClassesPageTests
            ClassMngrScheduleImportDialogTests
            ClassMngrSubPrepPageTests
            ClassMngrNavigationTabWidgetTests
            ClassMngrSidebarStructureTests
            ClassMngrScheduleBuilderTests
            ClassMngrSchedulePrintModelTests
            ClassMngrSchedulePrintPdfTests
            ClassMngrSubPrepPrintPdfTests
            ClassMngrRosterModelTests
            ClassMngrRosterTemplatePrintServiceTests
            ClassMngrLanguageServiceTests
            ClassMngrAcademicCalendarTests
            ClassMngrCampusMapTests
            ClassMngrSpeakingEvalReportWidgetTests
            ClassMngrSpeakingEvalBatchReportServiceTests
            ClassMngrStartupPerformanceTests
            PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )
    endif()
