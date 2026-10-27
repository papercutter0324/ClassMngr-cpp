include_guard(GLOBAL)

set(CLASSMNGR_NEXT_RESOURCE_INVENTORY
    "${PROJECT_SOURCE_DIR}/docs/qt-rewrite/phase-4-resource-inventory.json"
)
set(CLASSMNGR_NEXT_RESOURCE_BAKE_SCRIPT
    "${PROJECT_SOURCE_DIR}/scripts/phase4/bake_resources.py"
)
set(CLASSMNGR_NEXT_RESOURCE_CATALOG_SCRIPT
    "${PROJECT_SOURCE_DIR}/scripts/phase4/generate_resource_catalog.py"
)
set(CLASSMNGR_NEXT_RESOURCE_ROOT
    "${CMAKE_CURRENT_BINARY_DIR}/generated/ClassMngrNext/Resources"
)
set(CLASSMNGR_NEXT_RESOURCE_CATALOG
    "${CMAKE_CURRENT_BINARY_DIR}/generated/ClassMngrNext/resource-catalog.json"
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

file(READ "${CLASSMNGR_NEXT_RESOURCE_INVENTORY}" _classmngr_next_resource_inventory_json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CLASSMNGR_NEXT_RESOURCE_INVENTORY}"
)

set(_classmngr_next_resource_inputs)
set(_classmngr_next_translation_sources)
foreach(_classmngr_next_collection IN ITEMS
        document_catalog_metadata
        document_bodies
        resources
        excluded_resources
        build_inputs
        calendar_qml_sources)
    string(JSON _classmngr_next_entry_count
        LENGTH "${_classmngr_next_resource_inventory_json}"
        "${_classmngr_next_collection}"
    )
    if(_classmngr_next_entry_count GREATER 0)
        math(EXPR _classmngr_next_last_entry "${_classmngr_next_entry_count} - 1")
        foreach(_classmngr_next_entry_index RANGE 0 ${_classmngr_next_last_entry})
            string(JSON _classmngr_next_source_path
                GET "${_classmngr_next_resource_inventory_json}"
                "${_classmngr_next_collection}"
                ${_classmngr_next_entry_index}
                source_path
            )
            list(APPEND _classmngr_next_resource_inputs
                "${PROJECT_SOURCE_DIR}/${_classmngr_next_source_path}"
            )

            if(_classmngr_next_collection STREQUAL "resources")
                string(JSON _classmngr_next_resource_kind
                    GET "${_classmngr_next_resource_inventory_json}"
                    "${_classmngr_next_collection}"
                    ${_classmngr_next_entry_index}
                    resource_kind
                )
                if(_classmngr_next_resource_kind STREQUAL "translation_source_for_generated_qm")
                    list(APPEND _classmngr_next_translation_sources
                        "${PROJECT_SOURCE_DIR}/${_classmngr_next_source_path}"
                    )
                endif()
            endif()
        endforeach()
    endif()
endforeach()
list(REMOVE_DUPLICATES _classmngr_next_resource_inputs)
list(REMOVE_DUPLICATES _classmngr_next_translation_sources)

set(_classmngr_next_resource_manifest
    "${CLASSMNGR_NEXT_RESOURCE_ROOT}/resource-manifest.json"
)
add_custom_command(
    OUTPUT "${_classmngr_next_resource_manifest}"
    COMMAND "${Python3_EXECUTABLE}"
        "${CLASSMNGR_NEXT_RESOURCE_BAKE_SCRIPT}"
        "${CLASSMNGR_NEXT_RESOURCE_ROOT}"
        --inventory "${CLASSMNGR_NEXT_RESOURCE_INVENTORY}"
        --source-root "${PROJECT_SOURCE_DIR}"
    DEPENDS
        "${CLASSMNGR_NEXT_RESOURCE_BAKE_SCRIPT}"
        "${CLASSMNGR_NEXT_RESOURCE_INVENTORY}"
        ${_classmngr_next_resource_inputs}
    COMMENT "Baking ClassMngrNext direct resources"
    VERBATIM
)

add_custom_target(ClassMngrNextResourceCatalog
    COMMAND "${Python3_EXECUTABLE}"
        "${CLASSMNGR_NEXT_RESOURCE_CATALOG_SCRIPT}"
        --inventory "${CLASSMNGR_NEXT_RESOURCE_INVENTORY}"
        --bake-manifest "${_classmngr_next_resource_manifest}"
        --baked-root "${CLASSMNGR_NEXT_RESOURCE_ROOT}"
        --output "${CLASSMNGR_NEXT_RESOURCE_CATALOG}"
    DEPENDS
        "${CLASSMNGR_NEXT_RESOURCE_CATALOG_SCRIPT}"
        "${CLASSMNGR_NEXT_RESOURCE_INVENTORY}"
        "${_classmngr_next_resource_manifest}"
    COMMENT "Generating ClassMngrNext resource catalog"
    VERBATIM
)

set(_classmngr_next_translation_output_directory
    "${CMAKE_CURRENT_BINARY_DIR}/generated/ClassMngrNext/assets/translations"
)
qt_add_lrelease(
    LRELEASE_TARGET ClassMngrNextTranslationOutputs
    EXCLUDE_FROM_ALL
    NO_GLOBAL_TARGET
    QM_OUTPUT_DIRECTORY "${_classmngr_next_translation_output_directory}"
    QM_FILES_OUTPUT_VARIABLE CLASSMNGR_NEXT_TRANSLATION_OUTPUTS
    TS_FILES ${_classmngr_next_translation_sources}
)

function(classmngr_next_add_calendar_qml_resources target)
    qt_add_resources("${target}" classmngr_next_calendar_qml
        PREFIX "/qt/qml/ClassMngr/Calendar"
        BASE "${PROJECT_SOURCE_DIR}/src/features/calendar/ui/qml"
        FILES
            "${CLASSMNGR_CALENDAR_QML_EVENT_FILE}"
            "${CLASSMNGR_CALENDAR_QML_MONTH_GRID_DELEGATE_FILE}"
    )
endfunction()

classmngr_next_add_calendar_qml_resources(ClassMngrNext)

get_target_property(_classmngr_next_resource_link_dependencies
    ClassMngrNext
    LINK_LIBRARIES
)
list(REMOVE_DUPLICATES _classmngr_next_resource_link_dependencies)
if(NOT "${_classmngr_next_resource_link_dependencies}"
   STREQUAL "Qt6::Core;Qt6::Widgets;ClassMngrNext::Application;ClassMngrNext::Resources;ClassMngrNextWorkspace;ClassMngrNextDocumentCatalog;ClassMngrNextStartupUpdates;ClassMngrTeacherImportParser;ClassMngrScheduleImportParser;Qt6::Concurrent;Qt6::Network")
    message(FATAL_ERROR
        "ClassMngrNext resource embedding must retain its Widgets application "
        "and Resources links plus the narrow workspace, document catalog, and "
        "startup-update adapters with Qt Network; found "
        "[${_classmngr_next_resource_link_dependencies}]"
    )
endif()

add_custom_target(ClassMngrNextResourcePackage
    DEPENDS
        ClassMngrNextResourceCatalog
        ${CLASSMNGR_NEXT_TRANSLATION_OUTPUTS}
)
add_dependencies(ClassMngrNextResourcePackage ClassMngrNext)

if(APPLE)
    set(CLASSMNGR_NEXT_RESOURCE_INSTALL_ROOT
        "ClassMngr.app/Contents/Resources"
    )
elseif(WIN32)
    set(CLASSMNGR_NEXT_RESOURCE_INSTALL_ROOT
        "${CMAKE_INSTALL_BINDIR}/resources"
    )
    cmake_path(NORMAL_PATH CLASSMNGR_NEXT_RESOURCE_INSTALL_ROOT)
else()
    if(CMAKE_INSTALL_DATADIR)
        set(_classmngr_next_install_data_dir "${CMAKE_INSTALL_DATADIR}")
    else()
        set(_classmngr_next_install_data_dir "share")
    endif()
    set(CLASSMNGR_NEXT_RESOURCE_INSTALL_ROOT
        "${_classmngr_next_install_data_dir}/ClassMngr"
    )
    cmake_path(NORMAL_PATH CLASSMNGR_NEXT_RESOURCE_INSTALL_ROOT)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include("${CMAKE_CURRENT_LIST_DIR}/next_resource_install_prefix_guard.cmake")
    classmngr_next_add_install_prefix_guard(
        ClassMngrNextResources
        "${CMAKE_INSTALL_BINDIR}"
        "${_classmngr_next_install_data_dir}"
    )
endif()

if(APPLE)
    # The executable lives in ClassMngr.app/Contents/MacOS while the resource
    # component is installed in ClassMngr.app/Contents/Resources.
    set(CLASSMNGR_NEXT_RESOURCE_RELATIVE_ROOT "../Resources")
elseif(WIN32)
    # The Windows preset installs the executable into CMAKE_INSTALL_BINDIR and
    # the resource component into the sibling `resources` directory.
    set(CLASSMNGR_NEXT_RESOURCE_RELATIVE_ROOT "resources")
else()
    if(IS_ABSOLUTE "${CMAKE_INSTALL_BINDIR}")
        cmake_path(NORMAL_PATH CMAKE_INSTALL_BINDIR
            OUTPUT_VARIABLE _classmngr_next_executable_install_directory
        )
    else()
        cmake_path(ABSOLUTE_PATH CMAKE_INSTALL_BINDIR
            BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
            NORMALIZE
            OUTPUT_VARIABLE _classmngr_next_executable_install_directory
        )
    endif()

    if(IS_ABSOLUTE "${CLASSMNGR_NEXT_RESOURCE_INSTALL_ROOT}")
        cmake_path(NORMAL_PATH CLASSMNGR_NEXT_RESOURCE_INSTALL_ROOT
            OUTPUT_VARIABLE _classmngr_next_resource_install_directory
        )
    else()
        cmake_path(ABSOLUTE_PATH CLASSMNGR_NEXT_RESOURCE_INSTALL_ROOT
            BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
            NORMALIZE
            OUTPUT_VARIABLE _classmngr_next_resource_install_directory
        )
    endif()

    file(RELATIVE_PATH CLASSMNGR_NEXT_RESOURCE_RELATIVE_ROOT
        "${_classmngr_next_executable_install_directory}"
        "${_classmngr_next_resource_install_directory}"
    )
endif()

string(REPLACE "\\" "/" CLASSMNGR_NEXT_RESOURCE_RELATIVE_ROOT
    "${CLASSMNGR_NEXT_RESOURCE_RELATIVE_ROOT}"
)
if(CLASSMNGR_NEXT_RESOURCE_RELATIVE_ROOT STREQUAL ""
   OR IS_ABSOLUTE "${CLASSMNGR_NEXT_RESOURCE_RELATIVE_ROOT}")
    message(FATAL_ERROR
        "ClassMngrNext resource root must be relative to the executable install directory"
    )
endif()
target_compile_definitions(ClassMngrNextResources
    INTERFACE
        "CLASSMNGR_NEXT_RESOURCE_RELATIVE_ROOT=\"${CLASSMNGR_NEXT_RESOURCE_RELATIVE_ROOT}\""
)

install(DIRECTORY "${CLASSMNGR_NEXT_RESOURCE_ROOT}/assets/"
    DESTINATION "${CLASSMNGR_NEXT_RESOURCE_INSTALL_ROOT}/assets"
    COMPONENT ClassMngrNextResources
    EXCLUDE_FROM_ALL
)
install(FILES ${CLASSMNGR_NEXT_TRANSLATION_OUTPUTS}
    DESTINATION "${CLASSMNGR_NEXT_RESOURCE_INSTALL_ROOT}/assets/translations"
    COMPONENT ClassMngrNextResources
    EXCLUDE_FROM_ALL
)
install(FILES "${CLASSMNGR_NEXT_RESOURCE_CATALOG}"
    DESTINATION "${CLASSMNGR_NEXT_RESOURCE_INSTALL_ROOT}"
    COMPONENT ClassMngrNextResources
    EXCLUDE_FROM_ALL
)
