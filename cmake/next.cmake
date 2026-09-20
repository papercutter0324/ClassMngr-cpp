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
)

set(CLASSMNGR_NEXT_APPLICATION_SOURCES
    src/next/application/calendar_event_delete_port.h
    src/next/application/calendar_event_save_port.h
    src/next/application/calendar_event_series_create_port.h
    src/next/application/calendar_event_series_edit_port.h
    src/next/application/calendar_event_series_delete_port.h
    src/next/application/calendar_event_query_port.h
    src/next/application/calendar_event_projection.h
    src/next/application/campus_directory_projection.h
    src/next/application/class_summary_projection.h
    src/next/application/class_transfer_projection.h
    src/next/application/document_catalog_projection.h
    src/next/application/document_catalog_use_case.h
    src/next/application/document_content_session.h
    src/next/application/import_job_coordinator.h
    src/next/application/import_review_session.h
    src/next/application/import_job_state.h
    src/next/application/report_job_coordinator.h
    src/next/application/report_job_state.h
    src/next/application/selection_state.h
    src/next/application/schedule_view_projection.h
    src/next/application/user_preferences_state.h
    src/next/application/workspace_contracts.h
    src/next/application/workspace_coordinator.h
    src/next/application/workspace_state.h
    src/next/application/workspace_use_case.h
)

set(CLASSMNGR_NEXT_PLATFORM_SOURCES
    src/next/platform/legacy_workspace_gateway.h
    src/next/platform/application_services_document_catalog_port.h
    src/next/platform/application_services_calendar_event_port.h
    src/next/platform/application_services_calendar_event_delete_port.h
    src/next/platform/application_services_calendar_event_save_port.h
    src/next/platform/application_services_calendar_event_series_create_port.h
    src/next/platform/application_services_calendar_event_series_edit_port.h
    src/next/platform/application_services_calendar_event_series_delete_port.h
    src/next/platform/document_content_resource_port.h
    src/next/platform/application_services_workspace_port.h
    src/next/platform/qt_job_worker_lifetime.h
    src/next/platform/qt_job_worker_adapters.h
    src/next/platform/theme_preference_port.h
    src/next/platform/language_preference_port.h
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
