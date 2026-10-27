if(NOT DEFINED REPORT_FILE OR NOT EXISTS "${REPORT_FILE}")
    message(FATAL_ERROR "The generated Next workspace dependency report is missing.")
endif()

file(READ "${REPORT_FILE}" _report)
string(REPLACE "\n" ";" _report_lines "${_report}")
foreach(_line IN LISTS _report_lines)
    if(_line MATCHES "^NEXT_LINKS=(.*)$")
        set(_next_links "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^NEXT_STARTUP_UPDATE_LINKS=(.*)$")
        set(_next_startup_update_links "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^UPDATER_LINKS=(.*)$")
        set(_updater_links "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^NEXT_WORKSPACE_LINKS=(.*)$")
        set(_next_workspace_links "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^WORKSPACE_STORE_LINKS=(.*)$")
        set(_workspace_store_links "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^NEXT_WORKSPACE_SOURCES=(.*)$")
        set(_next_workspace_sources "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^NEXT_WORKSPACE_TYPE=(.*)$")
        set(_next_workspace_type "${CMAKE_MATCH_1}")
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
    "Qt6::Core|Qt6::Widgets|ClassMngrNext::Application|ClassMngrNext::Resources|ClassMngrNextWorkspace|ClassMngrNextDocumentCatalog|ClassMngrNextStartupUpdates|ClassMngrTeacherImportParser|Qt6::Concurrent|Qt6::Network"
)
if(NOT _next_links STREQUAL _expected_next_links)
    message(FATAL_ERROR
        "Unexpected direct ClassMngrNext dependencies: [${_next_links}]"
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
    "ClassMngrNext::Resources|ClassMngrNextWorkspace|ClassMngrNext::DocumentCatalog|ClassMngrNextStartupUpdates|ClassMngrTeacherImportParser|Qt6::Concurrent|Qt6::Sql|Qt6::Test|Qt6::Widgets"
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
        Qt6::Pdf
        Qt6::PdfWidgets
        ClassMngrNext::Platform
        ClassMngrRuntime
        ClassMngrCore
        ClassMngrAppServices
        ApplicationServices)
    foreach(_dependency_set IN ITEMS
            _next_links
            _next_workspace_links
            _next_workspace_domain_links
            _workspace_store_links
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

foreach(_next_only_target IN ITEMS ClassMngrNextWorkspace ClassMngrNextWorkspaceDomain)
    string(FIND "${_legacy_links}|${_legacy_sources}" "${_next_only_target}" _legacy_next_target_index)
    if(NOT _legacy_next_target_index EQUAL -1)
        message(FATAL_ERROR
            "Next-only workspace target ${_next_only_target} entered ClassMngr: "
            "links=[${_legacy_links}], sources=[${_legacy_sources}]"
        )
    endif()
endforeach()
