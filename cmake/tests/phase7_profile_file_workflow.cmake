include_guard(GLOBAL)

# Capture the visible Legacy and Next profile-file workflow states without
# running the broad application test suite.
add_executable(ClassMngrProfileFileWorkflowVisualCapture
    tests/next/profile_file_workflow_visual_capture.cpp
    ${CLASSMNGR_NEXT_BOOTSTRAP_SOURCES}
    $<TARGET_OBJECTS:ClassMngrSettings>
    $<TARGET_OBJECTS:ClassMngrOnScreenKeyboard>
)
target_compile_features(ClassMngrProfileFileWorkflowVisualCapture
    PRIVATE
        cxx_std_23
)
target_include_directories(ClassMngrProfileFileWorkflowVisualCapture
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)
target_compile_definitions(ClassMngrProfileFileWorkflowVisualCapture
    PRIVATE
        CLASSMNGR_NEXT_VERSION="${PROJECT_VERSION}"
        CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG="${CLASSMNGR_NEXT_RESOURCE_CATALOG}"
        CLASSMNGR_NEXT_TEST_RESOURCE_ROOT="${CLASSMNGR_NEXT_RESOURCE_ROOT}"
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY="${_classmngr_next_translation_output_directory}"
)
target_link_libraries(ClassMngrProfileFileWorkflowVisualCapture
    PRIVATE
        ClassMngrRuntime
        ClassMngrClassTransferSupport
        ClassMngrNextCalendarUi
        ClassMngrSubPrepShared
        ClassMngrNextScheduleOutput
        ClassMngrNextRosterOutput
        ClassMngrNextSpeakingEvaluationOutput
        ClassMngrNext::Resources
        ClassMngrNextWorkspace
        ClassMngrNext::DocumentCatalog
        ClassMngrQtPdfPrint
        ClassMngrNextStartupUpdates
        ClassMngrTeacherImportParser
        ClassMngrScheduleImportParser
        Qt6::Concurrent
        Qt6::Sql
        Qt6::Widgets
)
set_target_properties(ClassMngrProfileFileWorkflowVisualCapture
    PROPERTIES
        AUTOMOC ON
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(TARGET ClassMngrProfileFileWorkflowVisualCapture
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
add_dependencies(ClassMngrProfileFileWorkflowVisualCapture
    ClassMngrNextResourcePackage
    ClassMngrNextTranslationOutputs
)
classmngr_next_add_mode_combo_arrow_resources(
    ClassMngrProfileFileWorkflowVisualCapture
)
classmngr_next_add_calendar_qml_resources(
    ClassMngrProfileFileWorkflowVisualCapture
)
qt_add_resources(ClassMngrProfileFileWorkflowVisualCapture
    profile_file_workflow_dialog_icons
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES ${CLASSMNGR_FILE_DIALOG_ICONS}
)
add_test(
    NAME ClassMngrProfileFileWorkflowVisualCapture
    COMMAND ClassMngrProfileFileWorkflowVisualCapture
        "${PROJECT_BINARY_DIR}/artifacts/next/profile-file-workflows"
)
set_tests_properties(ClassMngrProfileFileWorkflowVisualCapture
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=1"
)
