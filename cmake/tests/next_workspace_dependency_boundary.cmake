if(NOT DEFINED REPORT_FILE OR NOT EXISTS "${REPORT_FILE}")
    message(FATAL_ERROR "The generated Next workspace dependency report is missing.")
endif()

file(READ "${REPORT_FILE}" _report)
string(REPLACE "\n" ";" _report_lines "${_report}")
foreach(_line IN LISTS _report_lines)
    if(_line MATCHES "^NEXT_LINKS=(.*)$")
        set(_next_links "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^NEXT_WORKSPACE_LINKS=(.*)$")
        set(_next_workspace_links "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^WORKSPACE_STORE_LINKS=(.*)$")
        set(_workspace_store_links "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^NEXT_WORKSPACE_SOURCES=(.*)$")
        set(_next_workspace_sources "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^LEGACY_SOURCES=(.*)$")
        set(_legacy_sources "${CMAKE_MATCH_1}")
    endif()
endforeach()

set(_expected_next_links
    "Qt6::Core|Qt6::Widgets|ClassMngrNext::Application|ClassMngrNext::Resources|ClassMngrNextWorkspace"
)
if(NOT _next_links STREQUAL _expected_next_links)
    message(FATAL_ERROR
        "Unexpected direct ClassMngrNext dependencies: [${_next_links}]"
    )
endif()

set(_expected_workspace_links
    "ClassMngrBuildSettings|ClassMngrNext::Application|ClassMngrWorkspaceStore|ClassMngrDomain|Qt6::Sql"
)
if(NOT _next_workspace_links STREQUAL _expected_workspace_links)
    message(FATAL_ERROR
        "Unexpected direct ClassMngrNextWorkspace dependencies: [${_next_workspace_links}]"
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
        Qt6::Network
        ClassMngrNext::Platform
        ClassMngrRuntime
        ClassMngrCore
        ClassMngrAppServices
        ApplicationServices)
    foreach(_dependency_set IN ITEMS
            _next_links
            _next_workspace_links
            _workspace_store_links)
        string(FIND "${${_dependency_set}}" "${_forbidden_dependency}" _dependency_index)
        if(NOT _dependency_index EQUAL -1)
            message(FATAL_ERROR
                "${_forbidden_dependency} entered ${_dependency_set}: "
                "[${${_dependency_set}}]"
            )
        endif()
    endforeach()
endforeach()

foreach(_required_workspace_source IN ITEMS
        workspace_store_gateway.cpp
        next_startup_profiler_inactive_workspace_compat.cpp)
    string(FIND "${_next_workspace_sources}" "${_required_workspace_source}" _workspace_source_index)
    if(_workspace_source_index EQUAL -1)
        message(FATAL_ERROR
            "ClassMngrNextWorkspace is missing ${_required_workspace_source}: "
            "[${_next_workspace_sources}]"
        )
    endif()
endforeach()

string(FIND "${_legacy_sources}" "next_startup_profiler_inactive_workspace_compat.cpp" _legacy_compat_index)
if(NOT _legacy_compat_index EQUAL -1)
    message(FATAL_ERROR
        "Next-only inactive-profiler compatibility code entered ClassMngr: "
        "[${_legacy_sources}]"
    )
endif()
