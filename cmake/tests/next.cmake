include_guard(GLOBAL)

# Keep the cycle-selection rule independent of Qt and the legacy runtime.
add_executable(
    ClassMngrNextApplicationEvaluationDefaultSelectionTests
    tests/next_application_evaluation_default_selection_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationEvaluationDefaultSelectionTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationEvaluationDefaultSelectionTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationEvaluationDefaultSelectionTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationEvaluationDefaultSelectionTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationEvaluationDefaultSelectionTests
    COMMAND ClassMngrNextApplicationEvaluationDefaultSelectionTests
)

classmngr_add_qt_test(
    NAME NextPlatformQSettingsFileDialogDirectoryPreferencesAdapter
    SOURCES
        tests/next_platform_qsettings_file_dialog_directory_preferences_adapter_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextDomainContract
    SOURCES
        tests/next_domain_contract_tests.cpp
    LIBRARIES
        ClassMngrNext::Domain
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationContract
    SOURCES
        tests/next_application_contract_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationState
    SOURCES
        tests/next_application_state_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationWorkspaceCoordinator
    SOURCES
        tests/next_application_workspace_coordinator_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationSelection
    SOURCES
        tests/next_application_selection_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationImportJob
    SOURCES
        tests/next_application_import_job_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationImportJobCoordinator
    SOURCES
        tests/next_application_import_job_coordinator_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationReportJob
    SOURCES
        tests/next_application_report_job_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationReportJobCoordinator
    SOURCES
        tests/next_application_report_job_coordinator_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationImportReview
    SOURCES
        tests/next_application_import_review_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationDocumentCatalog
    SOURCES
        tests/next_application_document_catalog_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationDocumentCatalogUseCase
    SOURCES
        tests/next_application_document_catalog_use_case_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationDocumentContent
    SOURCES
        tests/next_application_document_content_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationClassSummary
    SOURCES
        tests/next_application_class_summary_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationSubPrepScheduleSummaryQuery
    SOURCES
        tests/next_application_sub_prep_schedule_summary_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationSubPrepPrintSourceQuery
    SOURCES
        tests/next_application_sub_prep_print_source_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationClassNotesSavePort
    SOURCES
        tests/next_application_class_notes_save_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationSubPrepRosterOutputSourceQuery
    SOURCES
        tests/next_application_sub_prep_roster_output_source_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationSubPrepClassDetailsQuery
    SOURCES
        tests/next_application_sub_prep_class_details_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationSubPrepClassInformationState
    SOURCES
        tests/next_application_sub_prep_class_information_state_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationClassTransfer
    SOURCES
        tests/next_application_class_transfer_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationScheduleView
    SOURCES
        tests/next_application_schedule_view_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationCalendarEvent
    SOURCES
        tests/next_application_calendar_event_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationCalendarEventImportPlan
    SOURCES
        tests/next_application_calendar_event_import_plan_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationCalendarEventImportUseCase
    SOURCES
        tests/next_application_calendar_event_import_use_case_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

# Exercise the duplicate-key value without Qt or the legacy runtime.
add_executable(
    ClassMngrNextApplicationCalendarEventImportSignatureTests
    tests/next_application_calendar_event_import_signature_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationCalendarEventImportSignatureTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationCalendarEventImportSignatureTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationCalendarEventImportSignatureTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationCalendarEventImportSignatureTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationCalendarEventImportSignatureTests
    COMMAND ClassMngrNextApplicationCalendarEventImportSignatureTests
)

classmngr_add_qt_test(
    NAME NextApplicationScheduleImportStateValidation
    SOURCES
        tests/next_application_schedule_import_state_validation_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationScheduleImportMatchingProjection
    SOURCES
        tests/next_application_schedule_import_matching_projection_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

# Exercise the review decision contract without Qt or the legacy runtime.
add_executable(
    ClassMngrNextApplicationScheduleImportReviewDecisionsTests
    tests/next_application_schedule_import_review_decisions_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationScheduleImportReviewDecisionsTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationScheduleImportReviewDecisionsTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationScheduleImportReviewDecisionsTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationScheduleImportReviewDecisionsTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationScheduleImportReviewDecisionsTests
    COMMAND ClassMngrNextApplicationScheduleImportReviewDecisionsTests
)

# Exercise the Korean teacher sparse update policy without Qt or the legacy runtime.
add_executable(
    ClassMngrNextApplicationKoreanTeacherImportUpdateTests
    tests/next_application_korean_teacher_import_update_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationKoreanTeacherImportUpdateTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationKoreanTeacherImportUpdateTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationKoreanTeacherImportUpdateTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationKoreanTeacherImportUpdateTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationKoreanTeacherImportUpdateTests
    COMMAND ClassMngrNextApplicationKoreanTeacherImportUpdateTests
)

classmngr_add_qt_test(
    NAME CalendarEventImportParity
    SOURCES
        tests/next_feature_calendar_event_import_parity_tests.cpp
    COMPILE_DEFINITIONS
        CLASSMNGR_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Network
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationSubPrepCampusDirectoryQuery
    SOURCES
        tests/next_application_sub_prep_campus_directory_query_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationCalendarEventImportSignatureQueryPort
    SOURCES
        tests/next_application_calendar_event_import_signature_query_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationCalendarEventQueryPort
    SOURCES
        tests/next_application_calendar_event_query_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationCampusDirectory
    SOURCES
        tests/next_application_campus_directory_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationUserPreferences
    SOURCES
        tests/next_application_user_preferences_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerExcelImportTimeoutPort
    SOURCES
        tests/next_platform_settings_manager_excel_import_timeout_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerAutomaticUpdatePreferencesPort
    SOURCES
        tests/next_platform_settings_manager_automatic_update_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerAiCommentCustomWebsitePort
    SOURCES
        tests/next_platform_settings_manager_ai_comment_custom_website_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerAiCommentVoicePreferencesPort
    SOURCES
        tests/next_platform_settings_manager_ai_comment_voice_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerAiCommentProviderPreferencesPort
    SOURCES
        tests/next_platform_settings_manager_ai_comment_provider_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerFontSizePreferencesPort
    SOURCES
        tests/next_platform_settings_manager_font_size_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerDocumentViewerBackgroundPreferencesPort
    SOURCES
        tests/next_platform_settings_manager_document_viewer_background_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerDocumentPageSpacingPreferencesPort
    SOURCES
        tests/next_platform_settings_manager_document_page_spacing_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerSaveModePreferencesPort
    SOURCES
        tests/next_platform_settings_manager_save_mode_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerThemePreferencesPort
    SOURCES
        tests/next_platform_settings_manager_theme_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerDialogGeometryPreferencesPort
    SOURCES
        tests/next_platform_settings_manager_dialog_geometry_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerLanguagePreferencesPort
    SOURCES
        tests/next_platform_settings_manager_language_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerUpcomingBirthdayDismissalPort
    SOURCES
        tests/next_platform_settings_manager_upcoming_birthday_dismissal_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerSkippedUpdateVersionPort
    SOURCES
        tests/next_platform_settings_manager_skipped_update_version_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerPowerPointDataAccessNoticePort
    SOURCES
        tests/next_platform_settings_manager_powerpoint_data_access_notice_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerSidebarDisplayPreferencesPort
    SOURCES
        tests/next_platform_settings_manager_sidebar_display_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerRecentWorkspaceHistoryPort
    SOURCES
        tests/next_platform_settings_manager_recent_workspace_history_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerLastSelectedCampusPort
    SOURCES
        tests/next_platform_settings_manager_last_selected_campus_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerLastDatabaseDirectoryPort
    SOURCES
        tests/next_platform_settings_manager_last_database_directory_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformLegacyWorkspaceGateway
    SOURCES
        tests/next_platform_legacy_workspace_gateway_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesWorkspacePort
    SOURCES
        tests/next_platform_application_services_workspace_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesDocumentCatalogPort
    SOURCES
        tests/next_platform_application_services_document_catalog_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesSubPrepPrintSourcePort
    SOURCES
        tests/next_platform_application_services_sub_prep_print_source_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextApplicationSubPrepCalendarEventIntervalsQuery
    SOURCES
        tests/next_application_sub_prep_calendar_event_intervals_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesClassNotesSavePort
    SOURCES
        tests/next_platform_application_services_class_notes_save_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextFeatureClassNotesPage
    SOURCES
        tests/next_feature_class_notes_page_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextApplicationMyInfoCampusDirectoryQueryPort
    SOURCES
        tests/next_application_my_info_campus_directory_query_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesSubPrepRosterOutputSourcePort
    SOURCES
        tests/next_platform_application_services_sub_prep_roster_output_source_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformSubPrepCampusDirectoryQueryAdapter
    SOURCES
        tests/next_platform_sub_prep_campus_directory_query_adapter_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesCalendarEventPort
    SOURCES
        tests/next_platform_application_services_calendar_event_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesCalendarEventDisplayPreferencesPort
    SOURCES
        tests/next_platform_application_services_calendar_event_display_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesCalendarFirstDayOfWeekPreferencesPort
    SOURCES
        tests/next_platform_application_services_calendar_first_day_of_week_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPort
    SOURCES
        tests/next_platform_application_services_academic_calendar_schedule_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesCalendarEventTypeColorPreferencesPort
    SOURCES
        tests/next_platform_application_services_calendar_event_type_color_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesCustomColorPalettePreferencesPort
    SOURCES
        tests/next_platform_application_services_custom_color_palette_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME ColorUtilsCustomColorPalette
    SOURCES
        tests/colorutils_custom_color_palette_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesCurrentCampusPreferencesPort
    SOURCES
        tests/next_platform_application_services_current_campus_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformCalendarEventImportCampusCodeQuery
    SOURCES
        tests/next_platform_calendar_event_import_campus_code_query_adapter_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformCalendarPageCampusDirectoryQuery
    SOURCES
        tests/next_platform_calendar_page_campus_directory_query_adapter_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformMyInfoCampusDirectoryQueryAdapter
    SOURCES
        tests/next_platform_my_info_campus_directory_query_adapter_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesPersonalDisplayNamePreferencesPort
    SOURCES
        tests/next_platform_application_services_personal_display_name_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesPersonalDetailsSavePort
    SOURCES
        tests/next_platform_application_services_personal_details_save_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesPersonalSignatureImagePort
    SOURCES
        tests/next_platform_application_services_personal_signature_image_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesPersonalSignaturePreferencesPort
    SOURCES
        tests/next_platform_application_services_personal_signature_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesSubPrepPreferencesPort
    SOURCES
        tests/next_platform_application_services_sub_prep_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesSubPrepPersonalZoomPreferencesPort
    SOURCES
        tests/next_platform_application_services_sub_prep_personal_zoom_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesScheduleDisplayPreferencesPort
    SOURCES
        tests/next_platform_application_services_schedule_display_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesScheduleDisplayModePreferencesPort
    SOURCES
        tests/next_platform_application_services_schedule_display_mode_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesEvaluationDefaultPolicyPort
    SOURCES
        tests/next_platform_application_services_evaluation_default_policy_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesClassDayFilterResetPolicyPort
    SOURCES
        tests/next_platform_application_services_class_day_filter_reset_policy_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesClassSelectionResetPolicyPort
    SOURCES
        tests/next_platform_application_services_class_selection_reset_policy_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesClassVisibilityPreferencesPort
    SOURCES
        tests/next_platform_application_services_class_visibility_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesMiddleSchoolAnalyticsPreferencesPort
    SOURCES
        tests/next_platform_application_services_middle_school_analytics_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformDocumentContentResourcePort
    SOURCES
        tests/next_platform_document_content_resource_port_tests.cpp
    COMPILE_DEFINITIONS
        CLASSMNGR_RESOURCE_PACK_DIR="${CLASSMNGR_RESOURCE_PACK_OUTPUT_DIR}"
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    DEPENDENCIES
        ClassMngrdocumentsResourcePack
)

classmngr_add_qt_test(
    NAME NextPlatformQtJobWorker
    SOURCES
        tests/next_platform_qt_job_worker_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME FileControllerWorkspaceLifecycle
    SOURCES
        tests/file_controller_workspace_lifecycle_tests.cpp
    LIBRARIES
        Qt6::Test
    OFFSCREEN
)
