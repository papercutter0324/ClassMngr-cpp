include_guard(GLOBAL)

function(classmngr_visit_qt_module_links target report_prefix)
    get_property(_visited GLOBAL PROPERTY "${report_prefix}_VISITED")
    if(target IN_LIST _visited)
        return()
    endif()

    list(APPEND _visited "${target}")
    set_property(GLOBAL PROPERTY "${report_prefix}_VISITED" "${_visited}")

    if(target MATCHES "^Qt6::([A-Za-z0-9_]+)$")
        set_property(GLOBAL APPEND PROPERTY "${report_prefix}_MODULES"
            "${CMAKE_MATCH_1}"
        )
        return()
    endif()

    if(NOT TARGET "${target}")
        return()
    endif()

    get_target_property(_alias_target "${target}" ALIASED_TARGET)
    if(_alias_target)
        set(target "${_alias_target}")
    endif()

    foreach(_link_property IN ITEMS LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
        get_target_property(_dependencies "${target}" "${_link_property}")
        if(NOT _dependencies OR _dependencies MATCHES "-NOTFOUND$")
            continue()
        endif()

        foreach(_dependency IN LISTS _dependencies)
            string(REGEX MATCHALL "Qt6::[A-Za-z0-9_]+" _qt_targets
                "${_dependency}"
            )
            foreach(_qt_target IN LISTS _qt_targets)
                classmngr_visit_qt_module_links(
                    "${_qt_target}" "${report_prefix}"
                )
            endforeach()

            if(TARGET "${_dependency}")
                classmngr_visit_qt_module_links(
                    "${_dependency}" "${report_prefix}"
                )
            endif()
        endforeach()
    endforeach()
endfunction()

function(classmngr_qt_modules_for_target target output_variable)
    string(MAKE_C_IDENTIFIER "classmngr_qt_modules_${target}" _report_prefix)
    set_property(GLOBAL PROPERTY "${_report_prefix}_VISITED" "")
    set_property(GLOBAL PROPERTY "${_report_prefix}_MODULES" "")
    classmngr_visit_qt_module_links("${target}" "${_report_prefix}")
    get_property(_modules GLOBAL PROPERTY "${_report_prefix}_MODULES")
    list(REMOVE_DUPLICATES _modules)
    list(SORT _modules)
    set("${output_variable}" "${_modules}" PARENT_SCOPE)
endfunction()

set(_classmngr_qt_module_report_targets
    ClassMngrNext
    ClassMngr
    ClassMngrRuntime
    ClassMngrCore
    ClassMngrData
    ClassMngrDomain
    ClassMngrUiShared
    ClassMngrFeatures
    ClassMngrAppServices
)

set(_classmngr_qt_module_report
    "{\"schema_version\":1,\"targets\":{"
)
set(_classmngr_first_qt_target TRUE)
foreach(_classmngr_qt_target IN LISTS _classmngr_qt_module_report_targets)
    if(NOT TARGET "${_classmngr_qt_target}")
        continue()
    endif()

    classmngr_qt_modules_for_target(
        "${_classmngr_qt_target}"
        _classmngr_qt_modules
    )
    if(NOT _classmngr_first_qt_target)
        string(APPEND _classmngr_qt_module_report ",")
    endif()
    set(_classmngr_first_qt_target FALSE)

    string(APPEND _classmngr_qt_module_report
        "\"${_classmngr_qt_target}\":["
    )
    set(_classmngr_first_qt_module TRUE)
    foreach(_classmngr_qt_module IN LISTS _classmngr_qt_modules)
        if(NOT _classmngr_first_qt_module)
            string(APPEND _classmngr_qt_module_report ",")
        endif()
        set(_classmngr_first_qt_module FALSE)
        string(APPEND _classmngr_qt_module_report
            "\"Qt6::${_classmngr_qt_module}\""
        )
    endforeach()
    string(APPEND _classmngr_qt_module_report "]")
endforeach()
string(APPEND _classmngr_qt_module_report "}}\n")

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/reports")
file(WRITE
    "${CMAKE_BINARY_DIR}/reports/qt-module-links.json"
    "${_classmngr_qt_module_report}"
)
