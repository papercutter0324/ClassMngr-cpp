include_guard(GLOBAL)

# Keep the next-generation layer and feature boundaries separate from the
# existing v1 targets. These interface targets do not compile production
# sources; header-only contracts may be attached as interface sources.
set(_classmngr_next_layer_targets
    Domain
    Application
    Persistence
    Resources
    Platform
    UiShared
)

set(_classmngr_next_feature_targets
    Calendar
    Campus
    Classes
    Documents
    MyInfo
    Roster
    Schedule
    Setup
    SpeakingEvaluation
    SubPrep
    Teacher
)

set(CLASSMNGR_NEXT_DOMAIN_SOURCES
    src/next/domain/domain_types.h
    src/next/domain/operation_result.h
    src/next/domain/schedule_time.h
)

set(CLASSMNGR_NEXT_APPLICATION_SOURCES
    src/next/application/academic_calendar_schedule_preferences.h
    src/next/application/calendar_event_delete_port.h
    src/next/application/calendar_event_delete_all_port.h
    src/next/application/calendar_event_edit_draft.h
    src/next/application/calendar_event_import_plan.h
    src/next/application/calendar_event_import_campus_code_query_port.h
    src/next/application/calendar_page_campus_directory_query_port.h
    src/next/application/my_info_campus_directory_query_port.h
    src/next/application/calendar_event_import_signature_query_port.h
    src/next/application/calendar_event_import_save_port.h
    src/next/application/calendar_event_save_port.h
    src/next/application/calendar_event_series_create_port.h
    src/next/application/calendar_event_series_edit_port.h
    src/next/application/calendar_event_series_delete_port.h
    src/next/application/calendar_event_display_preferences.h
    src/next/application/calendar_event_type_color_preferences.h
    src/next/application/custom_color_palette_preferences.h
    src/next/application/calendar_first_day_of_week_preferences.h
    src/next/application/calendar_event_query_port.h
    src/next/application/calendar_event_projection.h
    src/next/application/sub_prep_calendar_event_intervals_query.h
    src/next/application/campus_directory_projection.h
    src/next/application/class_summary_projection.h
    src/next/application/class_notes_save_port.h
    src/next/application/sub_prep_schedule_summary_query.h
    src/next/application/sub_prep_campus_directory_query_port.h
    src/next/application/sub_prep_print_source_query.h
    src/next/application/sub_prep_roster_output_source_query.h
    src/next/application/sub_prep_class_details_query.h
    src/next/application/sub_prep_class_information_state.h
    src/next/application/class_transfer_projection.h
    src/next/application/document_catalog_projection.h
    src/next/application/document_catalog_use_case.h
    src/next/application/document_content_session.h
    src/next/application/excel_import_timeout_preferences.h
    src/next/application/automatic_update_preferences.h
    src/next/application/evaluation_default_policy_preferences.h
    src/next/application/ai_comment_custom_website_port.h
    src/next/application/ai_comment_provider_preferences.h
    src/next/application/ai_comment_voice_preferences.h
    src/next/application/font_size_preferences.h
    src/next/application/document_viewer_background_preferences.h
    src/next/application/document_page_spacing_preferences.h
    src/next/application/save_mode_preferences.h
    src/next/application/theme_preferences_port.h
    src/next/application/dialog_geometry_preferences_port.h
    src/next/application/file_dialog_directory_preferences.h
    src/next/application/language_preferences_port.h
    src/next/application/upcoming_birthday_dismissal_port.h
    src/next/application/class_day_filter_reset_policy.h
    src/next/application/class_selection_reset_policy.h
    src/next/application/class_visibility_preferences.h
    src/next/application/current_campus_preferences.h
    src/next/application/last_database_directory_port.h
    src/next/application/last_selected_campus_port.h
    src/next/application/middle_school_analytics_preferences.h
    src/next/application/personal_display_name_preferences.h
    src/next/application/personal_details_save.h
    src/next/application/personal_signature_image.h
    src/next/application/personal_signature_preferences.h
    src/next/application/sub_prep_preferences.h
    src/next/application/sub_prep_personal_zoom_preferences.h
    src/next/application/skipped_update_version_preferences.h
    src/next/application/powerpoint_data_access_notice_preferences.h
    src/next/application/import_job_coordinator.h
    src/next/application/import_review_session.h
    src/next/application/import_job_state.h
    src/next/application/report_job_coordinator.h
    src/next/application/report_job_state.h
    src/next/application/recent_workspace_history.h
    src/next/application/selection_state.h
    src/next/application/schedule_display_preferences.h
    src/next/application/schedule_display_mode_preferences.h
    src/next/application/schedule_import_overlap_projection.h
    src/next/application/schedule_import_state_validation.h
    src/next/application/schedule_import_matching_projection.h
    src/next/application/sidebar_display_preferences.h
    src/next/application/schedule_view_projection.h
    src/next/application/user_preferences_state.h
    src/next/application/workspace_contracts.h
    src/next/application/workspace_coordinator.h
    src/next/application/workspace_state.h
    src/next/application/workspace_use_case.h
)

set(CLASSMNGR_NEXT_PLATFORM_SOURCES
    src/next/platform/legacy_workspace_gateway.h
    src/next/platform/application_services_academic_calendar_schedule_preferences_port.h
    src/next/platform/application_services_document_catalog_port.h
    src/next/platform/application_services_calendar_event_port.h
    src/next/platform/application_services_sub_prep_calendar_event_intervals_port.h
    src/next/platform/calendar_event_import_campus_code_query_adapter.h
    src/next/platform/calendar_page_campus_directory_query_adapter.h
    src/next/platform/my_info_campus_directory_query_adapter.h
    src/next/platform/application_services_calendar_event_import_signature_query_port.h
    src/next/platform/application_services_calendar_event_delete_port.h
    src/next/platform/application_services_calendar_event_delete_all_port.h
    src/next/platform/application_services_calendar_event_import_save_port.h
    src/next/platform/application_services_calendar_event_save_port.h
    src/next/platform/application_services_calendar_event_series_create_port.h
    src/next/platform/application_services_calendar_event_series_edit_port.h
    src/next/platform/application_services_calendar_event_series_delete_port.h
    src/next/platform/application_services_sub_prep_print_source_port.h
    src/next/platform/application_services_sub_prep_roster_output_source_port.h
    src/next/platform/application_services_sub_prep_class_details_port.h
    src/next/platform/application_services_sub_prep_schedule_summary_port.h
    src/next/platform/sub_prep_campus_directory_query_adapter.h
    src/next/platform/application_services_calendar_event_display_preferences_port.h
    src/next/platform/application_services_calendar_event_type_color_preferences_port.h
    src/next/platform/application_services_custom_color_palette_preferences_port.h
    src/next/platform/application_services_calendar_first_day_of_week_preferences_port.h
    src/next/platform/document_content_resource_port.h
    src/next/platform/application_services_workspace_port.h
    src/next/platform/qt_job_worker_lifetime.h
    src/next/platform/qt_job_worker_adapters.h
    src/next/platform/theme_preference_port.h
    src/next/platform/qsettings_file_dialog_directory_preferences_adapter.h
    src/next/platform/language_preference_port.h
    src/next/platform/application_services_schedule_display_preferences_port.h
    src/next/platform/application_services_schedule_display_mode_preferences_port.h
    src/next/platform/application_services_evaluation_default_policy_port.h
    src/next/platform/application_services_class_day_filter_reset_policy_port.h
    src/next/platform/application_services_class_selection_reset_policy_port.h
    src/next/platform/application_services_class_visibility_preferences_port.h
    src/next/platform/application_services_class_notes_save_port.h
    src/next/platform/application_services_current_campus_preferences_port.h
    src/next/platform/application_services_middle_school_analytics_preferences_port.h
    src/next/platform/application_services_personal_display_name_preferences_port.h
    src/next/platform/application_services_personal_details_save_port.h
    src/next/platform/application_services_personal_signature_image_port.h
    src/next/platform/application_services_personal_signature_preferences_port.h
    src/next/platform/application_services_sub_prep_preferences_port.h
    src/next/platform/application_services_sub_prep_personal_zoom_preferences_port.h
    src/next/platform/settings_manager_excel_import_timeout_port.h
    src/next/platform/settings_manager_automatic_update_preferences_port.h
    src/next/platform/settings_manager_ai_comment_custom_website_port.h
    src/next/platform/settings_manager_ai_comment_provider_preferences_port.h
    src/next/platform/settings_manager_ai_comment_voice_preferences_port.h
    src/next/platform/settings_manager_font_size_preferences_port.h
    src/next/platform/settings_manager_document_viewer_background_preferences_port.h
    src/next/platform/settings_manager_document_page_spacing_preferences_port.h
    src/next/platform/settings_manager_save_mode_preferences_port.h
    src/next/platform/settings_manager_theme_preferences_port.h
    src/next/platform/settings_manager_dialog_geometry_preferences_port.h
    src/next/platform/settings_manager_language_preferences_port.h
    src/next/platform/settings_manager_upcoming_birthday_dismissal_port.h
    src/next/platform/settings_manager_skipped_update_version_port.h
    src/next/platform/settings_manager_powerpoint_data_access_notice_port.h
    src/next/platform/settings_manager_recent_workspace_history_port.h
    src/next/platform/settings_manager_sidebar_display_preferences_port.h
    src/next/platform/settings_manager_last_database_directory_port.h
    src/next/platform/settings_manager_last_selected_campus_port.h
)

foreach(_classmngr_next_target IN LISTS
        _classmngr_next_layer_targets
        _classmngr_next_feature_targets)
    add_library(ClassMngrNext${_classmngr_next_target} INTERFACE)
    add_library(ClassMngrNext::${_classmngr_next_target} ALIAS
        ClassMngrNext${_classmngr_next_target})
endforeach()

target_sources(ClassMngrNextDomain
    INTERFACE
        ${CLASSMNGR_NEXT_DOMAIN_SOURCES}
)
target_include_directories(ClassMngrNextDomain
    INTERFACE
        "${PROJECT_SOURCE_DIR}/src"
)

target_sources(ClassMngrNextApplication
    INTERFACE
        ${CLASSMNGR_NEXT_APPLICATION_SOURCES}
)

target_sources(ClassMngrNextPlatform
    INTERFACE
        ${CLASSMNGR_NEXT_PLATFORM_SOURCES}
)

target_link_libraries(ClassMngrNextApplication
    INTERFACE
        ClassMngrNext::Domain
        ClassMngrNext::Persistence
)

target_link_libraries(ClassMngrNextPersistence
    INTERFACE
        ClassMngrNext::Domain
)

target_link_libraries(ClassMngrNextPlatform
    INTERFACE
        ClassMngrNext::Application
        Qt6::Core
)

target_link_libraries(ClassMngrNextUiShared
    INTERFACE
        ClassMngrNext::Application
        ClassMngrNext::Resources
)

foreach(_classmngr_next_feature IN LISTS _classmngr_next_feature_targets)
    target_link_libraries(ClassMngrNext${_classmngr_next_feature}
        INTERFACE
            ClassMngrNext::Application
            ClassMngrNext::Resources
            ClassMngrNext::UiShared
    )
endforeach()

function(classmngr_next_assert_dependencies target)
    get_target_property(_classmngr_next_actual_dependencies
        "${target}"
        INTERFACE_LINK_LIBRARIES
    )
    if("${_classmngr_next_actual_dependencies}" STREQUAL
       "_classmngr_next_actual_dependencies-NOTFOUND")
        set(_classmngr_next_actual_dependencies)
    endif()

    set(_classmngr_next_expected_dependencies ${ARGN})
    if(NOT "${_classmngr_next_actual_dependencies}" STREQUAL
           "${_classmngr_next_expected_dependencies}")
        message(FATAL_ERROR
            "Unexpected dependencies for ${target}: "
            "expected [${_classmngr_next_expected_dependencies}], "
            "found [${_classmngr_next_actual_dependencies}]"
        )
    endif()
endfunction()

classmngr_next_assert_dependencies(ClassMngrNextDomain)
classmngr_next_assert_dependencies(ClassMngrNextApplication
    ClassMngrNext::Domain
    ClassMngrNext::Persistence
)
classmngr_next_assert_dependencies(ClassMngrNextPersistence
    ClassMngrNext::Domain
)
classmngr_next_assert_dependencies(ClassMngrNextResources)
classmngr_next_assert_dependencies(ClassMngrNextPlatform
    ClassMngrNext::Application
    Qt6::Core
)
classmngr_next_assert_dependencies(ClassMngrNextUiShared
    ClassMngrNext::Application
    ClassMngrNext::Resources
)

foreach(_classmngr_next_feature IN LISTS _classmngr_next_feature_targets)
    classmngr_next_assert_dependencies(ClassMngrNext${_classmngr_next_feature}
        ClassMngrNext::Application
        ClassMngrNext::Resources
        ClassMngrNext::UiShared
    )
endforeach()

qt_add_executable(ClassMngrNext
    "${PROJECT_SOURCE_DIR}/${CLASSMNGR_NEXT_MAIN_SOURCE}"
)

target_compile_features(ClassMngrNext
    PRIVATE
        cxx_std_23
)

set_target_properties(ClassMngrNext
    PROPERTIES
        CXX_EXTENSIONS OFF
)

target_compile_definitions(ClassMngrNext
    PRIVATE
        CLASSMNGR_NEXT_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(ClassMngrNext
    PRIVATE
        Qt6::Core
)

get_target_property(_classmngr_next_executable_dependencies
    ClassMngrNext
    LINK_LIBRARIES
)
list(REMOVE_DUPLICATES _classmngr_next_executable_dependencies)
if(NOT "${_classmngr_next_executable_dependencies}" STREQUAL "Qt6::Core")
    message(FATAL_ERROR
        "ClassMngrNext must link only Qt6::Core; found "
        "[${_classmngr_next_executable_dependencies}]"
    )
endif()

if(BUILD_TESTING)
    add_test(
        NAME ClassMngrNextLaunch
        COMMAND ClassMngrNext
    )

    set_tests_properties(ClassMngrNextLaunch
        PROPERTIES
            PASS_REGULAR_EXPRESSION
                "ClassMngrNext launch version=${PROJECT_VERSION}"
    )
    if(WIN32)
        set_tests_properties(ClassMngrNextLaunch
            PROPERTIES
                ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )
    endif()
endif()
