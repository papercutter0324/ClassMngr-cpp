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

function(classmngr_next_add_mode_combo_arrow_resources target)
    qt_add_resources("${target}" classmngr_next_mode_combo_arrows
        PREFIX "/"
        BASE "${PROJECT_SOURCE_DIR}/resources"
        FILES
            "${PROJECT_SOURCE_DIR}/resources/assets/icons/combo_arrow_light.svg"
            "${PROJECT_SOURCE_DIR}/resources/assets/icons/combo_arrow_dark.svg"
    )
endfunction()

function(classmngr_next_add_shared_widget_icon_resources target)
    qt_add_resources("${target}" classmngr_next_shared_widget_icons
        PREFIX "/"
        BASE "${PROJECT_SOURCE_DIR}/resources"
        FILES
            "${PROJECT_SOURCE_DIR}/resources/assets/icons/keyboard_light.svg"
            "${PROJECT_SOURCE_DIR}/resources/assets/icons/keyboard_dark.svg"
            "${PROJECT_SOURCE_DIR}/resources/assets/icons/spin_up_light.svg"
            "${PROJECT_SOURCE_DIR}/resources/assets/icons/spin_up_dark.svg"
    )
endfunction()

classmngr_next_add_calendar_qml_resources(ClassMngrNext)
classmngr_next_add_mode_combo_arrow_resources(ClassMngrNext)
classmngr_next_add_shared_widget_icon_resources(ClassMngrNext)

get_target_property(_classmngr_next_resource_link_dependencies
    ClassMngrNext
    LINK_LIBRARIES
)
list(REMOVE_DUPLICATES _classmngr_next_resource_link_dependencies)
if(NOT "${_classmngr_next_resource_link_dependencies}"
   STREQUAL "Qt6::Core;$<TARGET_OBJECTS:ClassMngrNextWorkspaceProfilerCompat>;$<TARGET_OBJECTS:ClassMngrNextSubPrepProfilerCompat>;Qt6::Widgets;ClassMngrNext::Application;ClassMngrClassTransferSupport;ClassMngrNext::Resources;ClassMngrNextWorkspace;ClassMngrNextScheduleOutput;ClassMngrNextSubPrepOutput;ClassMngrNextRosterOutput;ClassMngrNextSpeakingEvaluationOutput;ClassMngrNextCalendarUi;ClassMngrQtZipArchiveReadAdapter;ClassMngrNextDocumentCatalog;ClassMngrQtPdfPrint;ClassMngrQtPdfGeneration;Qt6::PdfWidgets;ClassMngrNextStartupUpdates;ClassMngrTeacherImportParser;ClassMngrScheduleImportParser;Qt6::Concurrent;Qt6::Network")
    message(FATAL_ERROR
        "ClassMngrNext resource embedding must retain its Widgets application "
        "and Resources links plus the narrow workspace, Schedule and Sub Prep output, "
        "Speaking Evaluation output, Calendar UI, "
        "document catalog, PDF viewer and print adapters, and "
        "startup-update adapters with Qt Network; found "
        "[${_classmngr_next_resource_link_dependencies}]"
    )
endif()

qt_finalize_target(ClassMngrNext)

if(CLASSMNGR_NEXT_RELEASE_PACKAGE)
    install(TARGETS ClassMngrNext
        BUNDLE DESTINATION .
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT Runtime
    )

    if(WIN32)
        install(IMPORTED_RUNTIME_ARTIFACTS Qt6::Concurrent
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            COMPONENT Runtime
        )
    elseif(APPLE)
        install(IMPORTED_RUNTIME_ARTIFACTS Qt6::Concurrent
            LIBRARY DESTINATION "ClassMngr.app/Contents/Frameworks"
            COMPONENT Runtime
            FRAMEWORK DESTINATION "ClassMngr.app/Contents/Frameworks"
            COMPONENT Runtime
        )
    elseif(UNIX)
        install(IMPORTED_RUNTIME_ARTIFACTS Qt6::Concurrent
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            COMPONENT Runtime
        )
    endif()

    if(APPLE)
        set(_classmngr_next_deploy_executable
            "$<TARGET_FILE_NAME:ClassMngrNext>.app"
        )
    else()
        set(_classmngr_next_deploy_executable
            "$<TARGET_FILE:ClassMngrNext>"
        )
    endif()

    set(_classmngr_next_deploy_postamble "")
    set(_classmngr_next_deploy_plugin_options
        "INCLUDE_PLUGINS qsqlite qoffscreen"
    )
    if(APPLE)
        set(_classmngr_next_deploy_postamble [=[
set(CLASSMNGR_NEXT_BAD_SQL_DRIVERS
    libqsqlmimer.dylib
    libqsqlodbc.dylib
    libqsqlpsql.dylib
)
set(CLASSMNGR_NEXT_APP_SQL_DRIVER_DIR
    "${QT_DEPLOY_PREFIX}/ClassMngr.app/Contents/PlugIns/sqldrivers"
)
foreach(driver IN LISTS CLASSMNGR_NEXT_BAD_SQL_DRIVERS)
    if(EXISTS "${CLASSMNGR_NEXT_APP_SQL_DRIVER_DIR}/${driver}")
        file(REMOVE "${CLASSMNGR_NEXT_APP_SQL_DRIVER_DIR}/${driver}")
    endif()
endforeach()
]=])
    endif()
    if(WIN32)
        set(_classmngr_next_deploy_plugin_options "")
        set(_classmngr_next_deploy_postamble
            "qt_deploy_translations(\n"
            "    LOCALES en ko\n"
            "${CLASSMNGR_QT_DEPLOY_TRANSLATION_OPTIONS}"
            ")\n"
        )
    endif()

    qt6_generate_deploy_script(
        TARGET ClassMngrNext
        OUTPUT_SCRIPT classmngr_next_deploy_script
        CONTENT "
${CLASSMNGR_QT_DEPLOY_PREAMBLE}
qt_deploy_qml_imports(TARGET ClassMngrNext PLUGINS_FOUND plugins_found)
qt_deploy_runtime_dependencies(
    EXECUTABLE \"${_classmngr_next_deploy_executable}\"
    ADDITIONAL_MODULES \${plugins_found}
    GENERATE_QT_CONF
    ${_classmngr_next_deploy_plugin_options}
${CLASSMNGR_QT_DEPLOY_OPTIONS}
)
${_classmngr_next_deploy_postamble}
"
    )
    install(SCRIPT "${classmngr_next_deploy_script}" COMPONENT Runtime)
endif()

add_custom_target(ClassMngrNextResourcePackage
    DEPENDS
        ClassMngrNextResourceCatalog
        ${CLASSMNGR_NEXT_TRANSLATION_OUTPUTS}
)
add_dependencies(ClassMngrNextResourcePackage ClassMngrNext)
if(CLASSMNGR_NEXT_RELEASE_PACKAGE AND TARGET ClassMngrInstaller)
    add_dependencies(ClassMngrInstaller ClassMngrNextResourcePackage)
endif()

if(APPLE)
    set(CLASSMNGR_NEXT_RESOURCE_INSTALL_ROOT
        "ClassMngr.app/Contents/Resources"
    )
    if(CLASSMNGR_NEXT_RELEASE_PACKAGE)
        # Keep canonical Next resources separate from Qt's deployed `qml` tree.
        set(CLASSMNGR_NEXT_RESOURCE_INSTALL_ROOT
            "ClassMngr.app/Contents/Resources/ClassMngr"
        )
    endif()
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
    # The executable lives in ClassMngr.app/Contents/MacOS. Release staging
    # keeps canonical resources under Resources/ClassMngr beside Qt's qml tree.
    if(CLASSMNGR_NEXT_RELEASE_PACKAGE)
        set(CLASSMNGR_NEXT_RESOURCE_RELATIVE_ROOT "../Resources/ClassMngr")
    else()
        set(CLASSMNGR_NEXT_RESOURCE_RELATIVE_ROOT "../Resources")
    endif()
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

if(CLASSMNGR_NEXT_RELEASE_PACKAGE)
    set(_classmngr_next_resource_install_exclusions)
else()
    set(_classmngr_next_resource_install_exclusions EXCLUDE_FROM_ALL)
endif()

install(DIRECTORY "${CLASSMNGR_NEXT_RESOURCE_ROOT}/assets/"
    DESTINATION "${CLASSMNGR_NEXT_RESOURCE_INSTALL_ROOT}/assets"
    COMPONENT ClassMngrNextResources
    ${_classmngr_next_resource_install_exclusions}
)
install(FILES ${CLASSMNGR_NEXT_TRANSLATION_OUTPUTS}
    DESTINATION "${CLASSMNGR_NEXT_RESOURCE_INSTALL_ROOT}/assets/translations"
    COMPONENT ClassMngrNextResources
    ${_classmngr_next_resource_install_exclusions}
)
install(FILES "${CLASSMNGR_NEXT_RESOURCE_CATALOG}"
    DESTINATION "${CLASSMNGR_NEXT_RESOURCE_INSTALL_ROOT}"
    COMPONENT ClassMngrNextResources
    ${_classmngr_next_resource_install_exclusions}
)

if(CLASSMNGR_NEXT_RELEASE_PACKAGE)
    if(APPLE)
        set(CLASSMNGR_NEXT_LICENSE_INSTALL_ROOT
            "ClassMngr.app/Contents/Resources/licenses"
        )
    else()
        set(CLASSMNGR_NEXT_LICENSE_INSTALL_ROOT "licenses")
    endif()

    install(FILES
        "${PROJECT_SOURCE_DIR}/licenses/fonts/inter/LICENSE.txt"
        DESTINATION "${CLASSMNGR_NEXT_LICENSE_INSTALL_ROOT}/fonts/inter"
        COMPONENT Runtime
    )
    install(FILES
        "${PROJECT_SOURCE_DIR}/licenses/fonts/pretendard/LICENSE.txt"
        DESTINATION "${CLASSMNGR_NEXT_LICENSE_INSTALL_ROOT}/fonts/pretendard"
        COMPONENT Runtime
    )
    install(FILES
        "${PROJECT_SOURCE_DIR}/licenses/fonts/just-another-hand/LICENSE.txt"
        DESTINATION "${CLASSMNGR_NEXT_LICENSE_INSTALL_ROOT}/fonts/just-another-hand"
        COMPONENT Runtime
    )
    install(FILES
        "${PROJECT_SOURCE_DIR}/licenses/fonts/caveat/LICENSE.txt"
        DESTINATION "${CLASSMNGR_NEXT_LICENSE_INSTALL_ROOT}/fonts/caveat"
        COMPONENT Runtime
    )
    install(FILES
        "${PROJECT_SOURCE_DIR}/licenses/fonts/dancing-script/LICENSE.txt"
        DESTINATION "${CLASSMNGR_NEXT_LICENSE_INSTALL_ROOT}/fonts/dancing-script"
        COMPONENT Runtime
    )
    install(FILES
        "${PROJECT_SOURCE_DIR}/licenses/fonts/great-vibes/LICENSE.txt"
        DESTINATION "${CLASSMNGR_NEXT_LICENSE_INSTALL_ROOT}/fonts/great-vibes"
        COMPONENT Runtime
    )
    install(FILES
        "${PROJECT_SOURCE_DIR}/licenses/qt-eventcalendar/BSD-3-Clause.txt"
        DESTINATION "${CLASSMNGR_NEXT_LICENSE_INSTALL_ROOT}/qt-eventcalendar"
        COMPONENT Runtime
    )

    set(_classmngr_next_qt_license_source_dir
        "${PROJECT_SOURCE_DIR}/licenses/qt"
    )
    foreach(_classmngr_next_qt_license IN ITEMS
            LGPL-3.0-only.txt
            GPL-3.0-only.txt
            Qt-GPL-exception-1.0.txt)
        if(NOT EXISTS "${_classmngr_next_qt_license_source_dir}/${_classmngr_next_qt_license}")
            message(FATAL_ERROR
                "Required checked-in Qt license text was not found: "
                "${_classmngr_next_qt_license_source_dir}/${_classmngr_next_qt_license}"
            )
        endif()
    endforeach()

    install(FILES "${_classmngr_next_qt_license_source_dir}/LGPL-3.0-only.txt"
        DESTINATION "${CLASSMNGR_NEXT_LICENSE_INSTALL_ROOT}/qt"
        RENAME Qt-LICENSE.txt
        COMPONENT Runtime
    )
    install(FILES "${_classmngr_next_qt_license_source_dir}/GPL-3.0-only.txt"
        DESTINATION "${CLASSMNGR_NEXT_LICENSE_INSTALL_ROOT}/qt"
        RENAME Qt-GPL3-LICENSE.txt
        COMPONENT Runtime
    )
    install(FILES "${_classmngr_next_qt_license_source_dir}/Qt-GPL-exception-1.0.txt"
        DESTINATION "${CLASSMNGR_NEXT_LICENSE_INSTALL_ROOT}/qt"
        RENAME Qt-GPL3-EXCEPTION.txt
        COMPONENT Runtime
    )

    set(_classmngr_next_qt_sbom_root "${QT6_INSTALL_PREFIX}/sbom")
    foreach(_classmngr_next_qt_component IN ITEMS qtbase qtdeclarative qtpdf)
        file(GLOB _classmngr_next_component_sbom
            LIST_DIRECTORIES FALSE
            "${_classmngr_next_qt_sbom_root}/${_classmngr_next_qt_component}-*.spdx"
            "${_classmngr_next_qt_sbom_root}/${_classmngr_next_qt_component}-*.spdx.json"
            "${_classmngr_next_qt_sbom_root}/${_classmngr_next_qt_component}-*.cdx.json"
        )
        if(NOT _classmngr_next_component_sbom)
            message(FATAL_ERROR
                "Required Qt module SBOM was not found for ${_classmngr_next_qt_component} "
                "under ${_classmngr_next_qt_sbom_root}"
            )
        endif()
        install(FILES ${_classmngr_next_component_sbom}
            DESTINATION "${CLASSMNGR_NEXT_LICENSE_INSTALL_ROOT}/qt/sbom"
            COMPONENT Runtime
        )
    endforeach()
endif()
