set(_classmngr_next_bootstrap_sources)
foreach(_classmngr_next_bootstrap_source IN LISTS
        CLASSMNGR_NEXT_BOOTSTRAP_SOURCES)
    list(APPEND _classmngr_next_bootstrap_sources
        "${PROJECT_SOURCE_DIR}/${_classmngr_next_bootstrap_source}"
    )
endforeach()

qt_add_executable(ClassMngrNext
    MANUAL_FINALIZATION
    "${PROJECT_SOURCE_DIR}/${CLASSMNGR_NEXT_MAIN_SOURCE}"
    ${_classmngr_next_bootstrap_sources}
    $<TARGET_OBJECTS:ClassMngrOnScreenKeyboard>
)

if(APPLE)
    set(_classmngr_next_macos_icon
        "${PROJECT_SOURCE_DIR}/resources/assets/icons/app_icon.icns"
    )
    set_source_files_properties("${_classmngr_next_macos_icon}"
        PROPERTIES
            MACOSX_PACKAGE_LOCATION Resources
    )
    target_sources(ClassMngrNext
        PRIVATE
            "${_classmngr_next_macos_icon}"
    )
    set_target_properties(ClassMngrNext
        PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_ICON_FILE app_icon.icns
    )
endif()

if(WIN32)
    set(_classmngr_next_windows_rc
        "${CMAKE_CURRENT_BINARY_DIR}/generated/ClassMngrNext.rc"
    )
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
    configure_file(
        "${PROJECT_SOURCE_DIR}/resources/windows/ClassMngrNext.rc.in"
        "${_classmngr_next_windows_rc}"
        @ONLY
    )
    target_sources(ClassMngrNext
        PRIVATE
            "${_classmngr_next_windows_rc}"
    )
endif()

target_compile_features(ClassMngrNext
    PRIVATE
        cxx_std_23
)

# The target used to be declared in the project root. Keep its executable and
# autogen outputs at their established locations while giving Qt's QML scanner
# this directory's narrow SOURCE_DIR.
set_target_properties(ClassMngrNext
    PROPERTIES
        CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
        AUTOGEN_BUILD_DIR "${PROJECT_BINARY_DIR}/ClassMngrNext_autogen"
)

target_compile_definitions(ClassMngrNext
    PRIVATE
        CLASSMNGR_NEXT_VERSION="${PROJECT_VERSION}"
)

target_include_directories(ClassMngrNext
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(ClassMngrNext
    PRIVATE
        "$<TARGET_OBJECTS:ClassMngrNextWorkspaceProfilerCompat>"
        "$<TARGET_OBJECTS:ClassMngrNextSubPrepProfilerCompat>"
        Qt6::Widgets
        ClassMngrNext::Application
        ClassMngrClassTransferSupport
        ClassMngrNext::Resources
        ClassMngrNextWorkspace
        ClassMngrNextScheduleOutput
        ClassMngrNextSubPrepOutput
        ClassMngrNextRosterOutput
        ClassMngrNextSpeakingEvaluationOutput
        ClassMngrNextCalendarUi
        ClassMngrNextDocumentCatalog
        ClassMngrQtPdfPrint
        Qt6::PdfWidgets
        ClassMngrNextStartupUpdates
        ClassMngrTeacherImportParser
        ClassMngrScheduleImportParser
        Qt6::Concurrent
        Qt6::Network
)

get_target_property(_classmngr_next_executable_dependencies
    ClassMngrNext
    LINK_LIBRARIES
)
list(REMOVE_DUPLICATES _classmngr_next_executable_dependencies)
if(NOT "${_classmngr_next_executable_dependencies}"
   STREQUAL "Qt6::Core;$<TARGET_OBJECTS:ClassMngrNextWorkspaceProfilerCompat>;$<TARGET_OBJECTS:ClassMngrNextSubPrepProfilerCompat>;Qt6::Widgets;ClassMngrNext::Application;ClassMngrClassTransferSupport;ClassMngrNext::Resources;ClassMngrNextWorkspace;ClassMngrNextScheduleOutput;ClassMngrNextSubPrepOutput;ClassMngrNextRosterOutput;ClassMngrNextSpeakingEvaluationOutput;ClassMngrNextCalendarUi;ClassMngrNextDocumentCatalog;ClassMngrQtPdfPrint;Qt6::PdfWidgets;ClassMngrNextStartupUpdates;ClassMngrTeacherImportParser;ClassMngrScheduleImportParser;Qt6::Concurrent;Qt6::Network")
    message(FATAL_ERROR
        "ClassMngrNext must link its inactive-profiler object before "
        "the following dependent targets: Qt6::Widgets, "
        "ClassMngrNext::Application, ClassMngrClassTransferSupport, "
        "ClassMngrNext::Resources, "
        "ClassMngrNextWorkspace, ClassMngrNextScheduleOutput, ClassMngrNextSubPrepOutput, ClassMngrNextRosterOutput, "
        "ClassMngrNextSpeakingEvaluationOutput, "
        "ClassMngrNextCalendarUi, "
        "ClassMngrNextDocumentCatalog, "
        "ClassMngrNextStartupUpdates, ClassMngrTeacherImportParser, "
        "ClassMngrScheduleImportParser, "
        "Qt6::Concurrent, and Qt6::Network; found "
        "[${_classmngr_next_executable_dependencies}]"
    )
endif()
