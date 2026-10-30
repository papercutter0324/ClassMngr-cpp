include_guard(GLOBAL)

# Source manifests and test target SOURCES are the ownership record. The globs
# below are inventory-only: they detect handwritten files that have not been
# assigned, without discovering target sources. Build-tree generated files are
# outside these source roots; src/core/build_info.h.in is a configure_file input
# and is intentionally excluded by its .in suffix.
function(_classmngr_record_source_owner target source)
    if("${source}" MATCHES "^\\$<")
        return()
    endif()

    if(IS_ABSOLUTE "${source}")
        set(source_path "${source}")
    else()
        set(source_path "${PROJECT_SOURCE_DIR}/${source}")
    endif()

    cmake_path(NORMAL_PATH source_path OUTPUT_VARIABLE normalized_source)
    file(TO_CMAKE_PATH "${normalized_source}" normalized_source)

    if(NOT EXISTS "${normalized_source}")
        message(FATAL_ERROR
            "Source ownership for ${target} names a missing file: "
            "${normalized_source}"
        )
    endif()

    set_property(GLOBAL APPEND PROPERTY CLASSMNGR_OWNED_SOURCE_PATHS
        "${normalized_source}"
    )
    set_property(GLOBAL APPEND PROPERTY CLASSMNGR_OWNED_SOURCE_TARGETS
        "${target}"
    )
endfunction()

function(_classmngr_record_manifest target)
    foreach(source IN LISTS ARGN)
        _classmngr_record_source_owner("${target}" "${source}")
    endforeach()
endfunction()

function(classmngr_check_source_ownership)
    _classmngr_record_manifest(ClassMngrStudentName
        ${CLASSMNGR_STUDENT_NAME_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrCore ${CLASSMNGR_CORE_SOURCES})
    _classmngr_record_manifest(ClassMngrFontManager
        ${CLASSMNGR_FONT_MANAGER_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrSpeakingEvalReportEngine ${CLASSMNGR_SPEAKING_EVAL_REPORT_ENGINE_SOURCES})
    _classmngr_record_manifest(ClassMngrUpdater ${CLASSMNGR_UPDATER_SOURCES})
    _classmngr_record_manifest(ClassMngrUpdatePresentation
        ${CLASSMNGR_UPDATE_PRESENTATION_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrSettings ${CLASSMNGR_SETTINGS_SOURCES})
    _classmngr_record_manifest(ClassMngrQtPdfPrint
        ${CLASSMNGR_QT_PDF_PRINT_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrWorkspaceStore
        ${CLASSMNGR_WORKSPACE_STORE_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrWorkbookReader
        ${CLASSMNGR_WORKBOOK_READER_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrCalendarImportParser
        ${CLASSMNGR_CALENDAR_IMPORT_PARSER_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrTeacherImportParser
        ${CLASSMNGR_TEACHER_IMPORT_PARSER_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrScheduleImportRules
        ${CLASSMNGR_SCHEDULE_IMPORT_RULES_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrSchedulePrint
        ${CLASSMNGR_SCHEDULE_PRINT_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrScheduleImportParser
        ${CLASSMNGR_SCHEDULE_IMPORT_PARSER_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrData ${CLASSMNGR_DATA_SOURCES})
    _classmngr_record_manifest(ClassMngrDomain ${CLASSMNGR_DOMAIN_SOURCES})
    _classmngr_record_manifest(ClassMngrUiShared ${CLASSMNGR_UI_SHARED_SOURCES})
    _classmngr_record_manifest(ClassMngrOnScreenKeyboard
        ${CLASSMNGR_ON_SCREEN_KEYBOARD_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrFeatures ${CLASSMNGR_FEATURES_SOURCES})
    _classmngr_record_manifest(ClassMngrClassTransferSupport
        "src/features/classes/services/class_transfer_json_codec.cpp"
        "src/features/classes/services/class_transfer_json_codec.h"
        "src/features/classes/config/class_info_config.cpp"
        "src/core/utils/colorutils.cpp"
        "src/core/utils/file_name_utils.cpp"
    )
    _classmngr_record_manifest(ClassMngrSignature ${CLASSMNGR_SIGNATURE_SOURCES})
    _classmngr_record_manifest(ClassMngrAppServices ${CLASSMNGR_APP_SERVICES_SOURCES})
    _classmngr_record_source_owner(ClassMngr "${CLASSMNGR_MAIN_SOURCE}")
    _classmngr_record_source_owner(ClassMngrNext "${CLASSMNGR_NEXT_MAIN_SOURCE}")
    _classmngr_record_manifest(ClassMngrNext
        ${CLASSMNGR_NEXT_BOOTSTRAP_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrNextStartupUpdates
        ${CLASSMNGR_NEXT_STARTUP_UPDATE_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrNextDocumentCatalog
        ${CLASSMNGR_NEXT_DOCUMENT_CATALOG_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrNextWorkspace
        ${CLASSMNGR_NEXT_WORKSPACE_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrNextWorkspaceProfilerCompat
        ${CLASSMNGR_NEXT_WORKSPACE_PROFILER_COMPAT_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrRosterTemplateRenderer ${CLASSMNGR_ROSTER_TEMPLATE_RENDERER_SOURCES})
    _classmngr_record_manifest(ClassMngrNextRosterOutput ${CLASSMNGR_NEXT_ROSTER_OUTPUT_SOURCES})
    _classmngr_record_manifest(ClassMngrNextSpeakingEvaluationOutput ${CLASSMNGR_NEXT_SPEAKING_EVALUATION_OUTPUT_SOURCES})
    _classmngr_record_manifest(ClassMngrNextScheduleOutput
        ${CLASSMNGR_NEXT_SCHEDULE_OUTPUT_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrSubPrepShared
        ${CLASSMNGR_SUB_PREP_SHARED_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrNextSubPrepProfilerCompat
        ${CLASSMNGR_NEXT_SUB_PREP_PROFILER_COMPAT_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrNextCalendarUi
        ${CLASSMNGR_NEXT_CALENDAR_UI_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrNextDomain
        ${CLASSMNGR_NEXT_DOMAIN_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrNextApplication
        ${CLASSMNGR_NEXT_APPLICATION_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrNextPlatform
        ${CLASSMNGR_NEXT_PLATFORM_SOURCES}
    )
    _classmngr_record_manifest(ClassMngrNextResources
        ${CLASSMNGR_NEXT_RESOURCES_SOURCES}
    )
    foreach(source IN LISTS CLASSMNGR_CALENDAR_QML_FILES)
        _classmngr_record_source_owner(ClassMngr "${source}")
    endforeach()

    if(BUILD_TESTING)
        get_property(test_targets DIRECTORY "${PROJECT_SOURCE_DIR}" PROPERTY TESTS)
        get_property(test_support_targets GLOBAL
            PROPERTY CLASSMNGR_TEST_SUPPORT_TARGETS
        )
        list(APPEND test_targets ${test_support_targets})
        list(REMOVE_DUPLICATES test_targets)

        set(test_source_root "${PROJECT_SOURCE_DIR}/tests")
        cmake_path(NORMAL_PATH test_source_root)

        foreach(target IN LISTS test_targets)
            if(NOT TARGET "${target}")
                continue()
            endif()

            get_target_property(target_sources "${target}" SOURCES)
            if(NOT target_sources OR target_sources MATCHES "-NOTFOUND$")
                continue()
            endif()

            foreach(source IN LISTS target_sources)
                if("${source}" MATCHES "^\\$<")
                    continue()
                endif()

                if(IS_ABSOLUTE "${source}")
                    set(source_path "${source}")
                else()
                    set(source_path "${PROJECT_SOURCE_DIR}/${source}")
                endif()
                cmake_path(NORMAL_PATH source_path OUTPUT_VARIABLE normalized_source)
                file(TO_CMAKE_PATH "${normalized_source}" normalized_source)
                cmake_path(IS_PREFIX test_source_root "${normalized_source}"
                    NORMALIZE is_test_source
                )

                if(is_test_source AND EXISTS "${normalized_source}")
                    _classmngr_record_source_owner("${target}" "${normalized_source}")
                endif()
            endforeach()
        endforeach()
    endif()

    set(handwritten_sources)
    foreach(extension IN ITEMS cpp h ui inc qml)
        file(GLOB_RECURSE source_files CONFIGURE_DEPENDS LIST_DIRECTORIES FALSE
            "${PROJECT_SOURCE_DIR}/src/*.${extension}"
        )
        list(APPEND handwritten_sources ${source_files})
    endforeach()

    if(BUILD_TESTING)
        foreach(extension IN ITEMS cpp h)
            file(GLOB_RECURSE test_files CONFIGURE_DEPENDS LIST_DIRECTORIES FALSE
                "${PROJECT_SOURCE_DIR}/tests/*.${extension}"
            )
            list(APPEND test_inventory ${test_files})
        endforeach()

        # This executable is intentionally declared only on Apple platforms.
        if(NOT APPLE)
            list(FILTER test_inventory EXCLUDE REGEX
                "[/\\\\]powerpoint_data_access_notice_tests[.]cpp$"
            )
        endif()

        # This test target is declared only on Linux. Keep its source in the
        # ownership inventory there, while excluding it on platforms that do
        # not compile the Linux-only target.
        if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
            list(FILTER test_inventory EXCLUDE REGEX
                "[/\\\\]process_memory_snapshot_tests[.]cpp$"
            )
        endif()
        list(APPEND handwritten_sources ${test_inventory})
    endif()

    set(normalized_inventory)
    foreach(source IN LISTS handwritten_sources)
        cmake_path(NORMAL_PATH source OUTPUT_VARIABLE normalized_source)
        file(TO_CMAKE_PATH "${normalized_source}" normalized_source)
        list(APPEND normalized_inventory "${normalized_source}")
    endforeach()
    list(REMOVE_DUPLICATES normalized_inventory)
    list(SORT normalized_inventory)

    get_property(owned_paths GLOBAL PROPERTY CLASSMNGR_OWNED_SOURCE_PATHS)
    get_property(owned_targets GLOBAL PROPERTY CLASSMNGR_OWNED_SOURCE_TARGETS)
    list(LENGTH owned_paths owned_path_count)
    list(LENGTH owned_targets owned_target_count)
    if(NOT owned_path_count EQUAL owned_target_count)
        message(FATAL_ERROR
            "Internal source ownership record is inconsistent: "
            "${owned_path_count} paths but ${owned_target_count} owners."
        )
    endif()

    # Build a normalized-path lookup once, preserving first-seen owner order
    # while recording each owner only once per path.
    if(owned_path_count GREATER 0)
        math(EXPR last_owner_index "${owned_path_count} - 1")
        foreach(index RANGE 0 ${last_owner_index})
            list(GET owned_paths ${index} owned_path)
            list(GET owned_targets ${index} owner)
            string(SHA256 owned_path_key "${owned_path}")
            set(owner_index_property
                "CLASSMNGR_SOURCE_OWNERS_${owned_path_key}"
            )
            get_property(owners GLOBAL PROPERTY "${owner_index_property}")
            if(NOT owner IN_LIST owners)
                list(APPEND owners "${owner}")
                set_property(GLOBAL PROPERTY "${owner_index_property}"
                    "${owners}"
                )
            endif()
        endforeach()
    endif()

    set(missing_sources)
    set(duplicate_sources)
    foreach(source IN LISTS normalized_inventory)
        string(SHA256 source_key "${source}")
        set(owner_index_property "CLASSMNGR_SOURCE_OWNERS_${source_key}")
        get_property(owners GLOBAL PROPERTY "${owner_index_property}")

        list(LENGTH owners owner_count)
        if(owner_count EQUAL 0)
            list(APPEND missing_sources "${source}")
        elseif(owner_count GREATER 1)
            string(JOIN ", " owner_list ${owners})
            list(APPEND duplicate_sources "${source} (${owner_list})")
        endif()
    endforeach()

    if(missing_sources OR duplicate_sources)
        set(ownership_error "Handwritten source ownership validation failed.")
        if(missing_sources)
            string(JOIN "\n  " missing_list ${missing_sources})
            string(APPEND ownership_error
                "\nUnassigned source files:\n  ${missing_list}"
            )
        endif()
        if(duplicate_sources)
            string(JOIN "\n  " duplicate_list ${duplicate_sources})
            string(APPEND ownership_error
                "\nFiles assigned to multiple targets:\n  ${duplicate_list}"
            )
        endif()
        message(FATAL_ERROR "${ownership_error}")
    endif()

    list(LENGTH normalized_inventory owned_source_count)
    message(STATUS
        "Validated one explicit target owner for ${owned_source_count} handwritten source files."
    )
endfunction()
