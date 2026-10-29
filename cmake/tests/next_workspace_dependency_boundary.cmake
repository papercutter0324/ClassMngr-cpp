if(NOT DEFINED REPORT_FILE OR NOT EXISTS "${REPORT_FILE}")
    message(FATAL_ERROR "The generated Next workspace dependency report is missing.")
endif()

file(READ "${REPORT_FILE}" _report)
string(REPLACE "\n" ";" _report_lines "${_report}")
foreach(_line IN LISTS _report_lines)
    if(_line MATCHES "^NEXT_LINKS=(.*)$")
        set(_next_links "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^NEXT_SPEAKING_EVALUATION_OUTPUT_LINKS=(.*)$")
        set(_next_speaking_evaluation_output_links "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^NEXT_STARTUP_UPDATE_LINKS=(.*)$")
        set(_next_startup_update_links "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^UPDATER_LINKS=(.*)$")
        set(_updater_links "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^NEXT_WORKSPACE_LINKS=(.*)$")
        set(_next_workspace_links "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^NEXT_ROSTER_OUTPUT_LINKS=(.*)$")
        set(_next_roster_output_links "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^ROSTER_RENDERER_LINKS=(.*)$")
        set(_roster_renderer_links "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^NEXT_SCHEDULE_OUTPUT_LINKS=(.*)$")
        set(_next_schedule_output_links "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^NEXT_CALENDAR_UI_DEPENDENCIES=(.*)$")
        set(_next_calendar_ui_dependencies "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^NEXT_SCHEDULE_OUTPUT_SOURCES=(.*)$")
        set(_next_schedule_output_sources "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^NEXT_SCHEDULE_OUTPUT_TYPE=(.*)$")
        set(_next_schedule_output_type "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^WORKSPACE_STORE_LINKS=(.*)$")
        set(_workspace_store_links "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^NEXT_WORKSPACE_SOURCES=(.*)$")
        set(_next_workspace_sources "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^NEXT_WORKSPACE_TYPE=(.*)$")
        set(_next_workspace_type "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^NEXT_WORKSPACE_PROFILER_COMPAT_SOURCES=(.*)$")
        set(_next_workspace_profiler_compat_sources "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^NEXT_WORKSPACE_PROFILER_COMPAT_TYPE=(.*)$")
        set(_next_workspace_profiler_compat_type "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^NEXT_WORKSPACE_DOMAIN_LINKS=(.*)$")
        set(_next_workspace_domain_links "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^NEXT_WORKSPACE_DOMAIN_SOURCES=(.*)$")
        set(_next_workspace_domain_sources "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^NEXT_WORKSPACE_DOMAIN_TYPE=(.*)$")
        set(_next_workspace_domain_type "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^DOCUMENT_CATALOG_TEST_LINKS=(.*)$")
        set(_document_catalog_test_links "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^BOOTSTRAP_TEST_LINKS=(.*)$")
        set(_bootstrap_test_links "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^DOCUMENT_CATALOG_LIBRARY_LINKS=(.*)$")
        set(_document_catalog_library_links "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^LEGACY_SOURCES=(.*)$")
        set(_legacy_sources "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^LEGACY_LINKS=(.*)$")
        set(_legacy_links "${CMAKE_MATCH_1}")
    endif()
endforeach()

set(_expected_next_links
    "Qt6::Core|$<TARGET_OBJECTS:ClassMngrNextWorkspaceProfilerCompat>|Qt6::Widgets|ClassMngrNext::Application|ClassMngrClassTransferSupport|ClassMngrNext::Resources|ClassMngrNextWorkspace|ClassMngrNextScheduleOutput|ClassMngrNextRosterOutput|ClassMngrNextSpeakingEvaluationOutput|ClassMngrNextCalendarUi|ClassMngrNextDocumentCatalog|ClassMngrQtPdfPrint|Qt6::PdfWidgets|ClassMngrNextStartupUpdates|ClassMngrTeacherImportParser|ClassMngrScheduleImportParser|Qt6::Concurrent|Qt6::Network"
)
if(NOT _next_links STREQUAL _expected_next_links)
    message(FATAL_ERROR
        "Unexpected direct ClassMngrNext dependencies: [${_next_links}]"
    )
endif()

set(_expected_next_speaking_evaluation_output_links
    "ClassMngrSpeakingEvalReportEngine|ClassMngrNext::Resources|ClassMngrWorkspaceStore|Qt6::Sql"
)
if(NOT _next_speaking_evaluation_output_links STREQUAL
   _expected_next_speaking_evaluation_output_links)
    message(FATAL_ERROR
        "Unexpected Next Speaking Evaluation output closure: "
        "[${_next_speaking_evaluation_output_links}]"
    )
endif()

if(NOT _next_roster_output_links STREQUAL "ClassMngrRosterTemplateRenderer|Qt6::Widgets|ClassMngrWorkspaceStore|ClassMngrQtPdfPrint|Qt6::Sql|Qt6::Pdf")
    message(FATAL_ERROR "Unexpected Next Roster output closure: [${_next_roster_output_links}]")
endif()
if(NOT _roster_renderer_links STREQUAL "Qt6::Gui")
    message(FATAL_ERROR "Roster renderer must depend only on Qt GUI: [${_roster_renderer_links}]")
endif()

set(_expected_next_schedule_output_links
    "ClassMngrBuildSettings|ClassMngrNext::Application|ClassMngrSchedulePrint|Qt6::Widgets"
)
if(NOT _next_schedule_output_links STREQUAL _expected_next_schedule_output_links)
    message(FATAL_ERROR
        "Unexpected direct ClassMngrNextScheduleOutput dependencies: "
        "[${_next_schedule_output_links}]"
    )
endif()
if(NOT _next_schedule_output_sources MATCHES
   "qt_schedule_output_adapter\\.cpp")
    message(FATAL_ERROR
        "The narrow Next Schedule output target does not own its Qt adapter: "
        "[${_next_schedule_output_sources}]"
    )
endif()
if(NOT _next_schedule_output_type STREQUAL "STATIC_LIBRARY")
    message(FATAL_ERROR
        "ClassMngrNextScheduleOutput must remain a static library: "
        "[${_next_schedule_output_type}]"
    )
endif()

set(_expected_next_calendar_ui_dependencies
    "ClassMngrNext::Calendar|Qt6::Widgets|Qt6::Qml|Qt6::Quick|Qt6::QuickWidgets|$<LINK_ONLY:ClassMngrBuildSettings>|$<LINK_ONLY:ClassMngrWorkspaceStore>|$<LINK_ONLY:Qt6::Concurrent>|$<LINK_ONLY:ClassMngrCalendarImportParser>|$<LINK_ONLY:Qt6::Network>"
)
if(NOT _next_calendar_ui_dependencies STREQUAL _expected_next_calendar_ui_dependencies)
    message(FATAL_ERROR
        "Unexpected narrow Next Calendar UI dependencies: "
        "[${_next_calendar_ui_dependencies}]"
    )
endif()

set(_expected_next_startup_update_links
    "ClassMngrBuildSettings|ClassMngrNext::Application|ClassMngrUpdater|ClassMngrSettings|Qt6::Network"
)
if(NOT _next_startup_update_links STREQUAL _expected_next_startup_update_links)
    message(FATAL_ERROR
        "Unexpected direct ClassMngrNextStartupUpdates dependencies: "
        "[${_next_startup_update_links}]"
    )
endif()

set(_expected_updater_links
    "ClassMngrBuildSettings|Qt6::Core|Qt6::Network"
)
if(NOT _updater_links STREQUAL _expected_updater_links)
    message(FATAL_ERROR
        "Unexpected direct ClassMngrUpdater dependencies: [${_updater_links}]"
    )
endif()

if(NOT _document_catalog_test_links STREQUAL "ClassMngrNext::DocumentCatalog")
    message(FATAL_ERROR
        "The document catalog component test must link only its narrow adapter target: "
        "[${_document_catalog_test_links}]"
    )
endif()

set(_expected_bootstrap_test_links
    "ClassMngrClassTransferSupport|ClassMngrNextCalendarUi|ClassMngrNextScheduleOutput|ClassMngrNextRosterOutput|ClassMngrNextSpeakingEvaluationOutput|ClassMngrNext::Resources|ClassMngrNextWorkspace|ClassMngrNext::DocumentCatalog|ClassMngrQtPdfPrint|ClassMngrNextStartupUpdates|ClassMngrTeacherImportParser|ClassMngrScheduleImportParser|Qt6::Concurrent|Qt6::Sql|Qt6::Test|Qt6::PdfWidgets|Qt6::Widgets"
)
if(NOT _bootstrap_test_links STREQUAL _expected_bootstrap_test_links)
    message(FATAL_ERROR
        "Unexpected direct ClassMngrNextBootstrapTests dependencies: "
        "[${_bootstrap_test_links}]"
    )
endif()

set(_expected_document_catalog_library_links
    "Qt6::Core|ClassMngrNext::Application|ClassMngrNext::Resources"
)
if(NOT _document_catalog_library_links STREQUAL _expected_document_catalog_library_links)
    message(FATAL_ERROR
        "Unexpected direct ClassMngrNextDocumentCatalog dependencies: "
        "[${_document_catalog_library_links}]"
    )
endif()

set(_expected_workspace_links
    "ClassMngrSignature|ClassMngrBuildSettings|ClassMngrNext::Application|ClassMngrWorkspaceStore|ClassMngrNextWorkspaceDomain|Qt6::Sql"
)
if(NOT _next_workspace_links STREQUAL _expected_workspace_links)
    message(FATAL_ERROR
        "Unexpected direct ClassMngrNextWorkspace dependencies: [${_next_workspace_links}]"
    )
endif()

if(NOT _next_workspace_type STREQUAL "OBJECT_LIBRARY")
    message(FATAL_ERROR
        "ClassMngrNextWorkspace must supply its profiler hooks as direct objects; "
        "found ${_next_workspace_type}"
    )
endif()
if(NOT _next_workspace_domain_type STREQUAL "STATIC_LIBRARY"
   OR NOT _next_workspace_domain_links STREQUAL "ClassMngrDomain")
    message(FATAL_ERROR
        "ClassMngrNextWorkspaceDomain must be a static archive with only the Domain target link: "
        "type=${_next_workspace_domain_type}, links=[${_next_workspace_domain_links}]"
    )
endif()
string(FIND "${_next_workspace_domain_sources}" "ClassMngrStudentName" _student_name_object_index)
if(_student_name_object_index EQUAL -1)
    message(FATAL_ERROR
        "ClassMngrNextWorkspaceDomain must include the shared StudentNameUtils object: "
        "[${_next_workspace_domain_sources}]"
    )
endif()

foreach(_required_store_dependency IN ITEMS
        ClassMngrSQLiteSnapshot
        Qt6::Gui
        Qt6::Sql)
    string(FIND "${_workspace_store_links}" "${_required_store_dependency}" _store_dependency_index)
    if(_store_dependency_index EQUAL -1)
        message(FATAL_ERROR
            "ClassMngrWorkspaceStore is missing ${_required_store_dependency}: "
            "[${_workspace_store_links}]"
        )
    endif()
endforeach()

foreach(_forbidden_dependency IN ITEMS
        ClassMngrNext::Platform
        ClassMngrRuntime
        ClassMngrLegacyRuntimeClosure
        ClassMngrCore
        ClassMngrAppServices
        ApplicationServices)
    foreach(_dependency_set IN ITEMS
            _next_links
            _next_workspace_links
            _next_workspace_domain_links
            _workspace_store_links
            _next_speaking_evaluation_output_links
            _document_catalog_test_links
            _bootstrap_test_links
            _document_catalog_library_links
            _next_startup_update_links
            _updater_links)
        string(FIND "${${_dependency_set}}" "${_forbidden_dependency}" _dependency_index)
        if(NOT _dependency_index EQUAL -1)
            message(FATAL_ERROR
                "${_forbidden_dependency} entered ${_dependency_set}: "
                "[${${_dependency_set}}]"
            )
        endif()
    endforeach()
endforeach()

foreach(_speaking_evaluation_forbidden_dependency IN ITEMS
        ClassMngrNextCalendarUi
        ClassMngrCalendarImportParser
        ClassMngrNext::Calendar)
    string(FIND "${_next_speaking_evaluation_output_links}"
        "${_speaking_evaluation_forbidden_dependency}"
        _speaking_evaluation_dependency_index
    )
    if(NOT _speaking_evaluation_dependency_index EQUAL -1)
        message(FATAL_ERROR
            "${_speaking_evaluation_forbidden_dependency} entered the narrow "
            "Next Speaking Evaluation output closure: "
            "[${_next_speaking_evaluation_output_links}]"
        )
    endif()
endforeach()

foreach(_network_forbidden_set IN ITEMS
        _next_workspace_links
        _next_workspace_domain_links
        _workspace_store_links
        _document_catalog_test_links
        _bootstrap_test_links
        _document_catalog_library_links)
    string(FIND "${${_network_forbidden_set}}" "Qt6::Network" _network_dependency_index)
    if(NOT _network_dependency_index EQUAL -1)
        message(FATAL_ERROR
            "Qt6::Network escaped the dedicated updater/startup-update targets into "
            "${_network_forbidden_set}: [${${_network_forbidden_set}}]"
        )
    endif()
endforeach()

foreach(_required_workspace_source IN ITEMS workspace_store_gateway.cpp)
    string(FIND "${_next_workspace_sources}" "${_required_workspace_source}" _workspace_source_index)
    if(_workspace_source_index EQUAL -1)
        message(FATAL_ERROR
            "ClassMngrNextWorkspace is missing ${_required_workspace_source}: "
            "[${_next_workspace_sources}]"
        )
    endif()
endforeach()

if(NOT _next_workspace_profiler_compat_type STREQUAL "OBJECT_LIBRARY"
   OR NOT _next_workspace_profiler_compat_sources MATCHES
      "next_startup_profiler_inactive_workspace_compat\\.cpp")
    message(FATAL_ERROR
        "The inactive-profiler compat source must have one object-library owner: "
        "type=${_next_workspace_profiler_compat_type}, "
        "sources=[${_next_workspace_profiler_compat_sources}]"
    )
endif()
string(FIND "${_next_workspace_sources}"
    "next_startup_profiler_inactive_workspace_compat.cpp"
    _workspace_compat_source_index
)
if(NOT _workspace_compat_source_index EQUAL -1)
    message(FATAL_ERROR
        "The inactive-profiler compat source must not also be owned by "
        "ClassMngrNextWorkspace: [${_next_workspace_sources}]"
    )
endif()

string(FIND "${_legacy_sources}" "next_startup_profiler_inactive_workspace_compat.cpp" _legacy_compat_index)
if(NOT _legacy_compat_index EQUAL -1)
    message(FATAL_ERROR
        "Next-only inactive-profiler compatibility code entered ClassMngr: "
        "[${_legacy_sources}]"
    )
endif()

foreach(_next_only_target IN ITEMS ClassMngrNextWorkspace ClassMngrNextWorkspaceDomain)
    string(FIND "${_legacy_links}|${_legacy_sources}" "${_next_only_target}" _legacy_next_target_index)
    if(NOT _legacy_next_target_index EQUAL -1)
        message(FATAL_ERROR
            "Next-only workspace target ${_next_only_target} entered ClassMngr: "
            "links=[${_legacy_links}], sources=[${_legacy_sources}]"
        )
    endif()
endforeach()
