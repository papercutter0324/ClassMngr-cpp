include_guard(GLOBAL)

# Keep complete-row reordering independent of Qt and the feature runtime.
add_executable(
    ClassMngrNextApplicationRosterRowReorderingTests
    tests/next_application_roster_row_reordering_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationRosterRowReorderingTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationRosterRowReorderingTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationRosterRowReorderingTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationRosterRowReorderingTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationRosterRowReorderingTests
    COMMAND ClassMngrNextApplicationRosterRowReorderingTests
)

# Keep roster row removal and compaction independent of Qt and the feature runtime.
add_executable(
    ClassMngrNextApplicationRosterRowRemovalTests
    tests/next_application_roster_row_removal_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationRosterRowRemovalTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationRosterRowRemovalTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationRosterRowRemovalTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationRosterRowRemovalTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationRosterRowRemovalTests
    COMMAND ClassMngrNextApplicationRosterRowRemovalTests
)

# Keep custom roster-column naming policy independent of Qt.
add_executable(
    ClassMngrNextApplicationRosterCustomColumnNamePolicyTests
    tests/next_application_roster_custom_column_name_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationRosterCustomColumnNamePolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationRosterCustomColumnNamePolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationRosterCustomColumnNamePolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationRosterCustomColumnNamePolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationRosterCustomColumnNamePolicyTests
    COMMAND ClassMngrNextApplicationRosterCustomColumnNamePolicyTests
)

# Keep custom roster-column append and admission independent of Qt.
add_executable(
    ClassMngrNextApplicationRosterCustomColumnAppendTests
    tests/next_application_roster_custom_column_append_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationRosterCustomColumnAppendTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationRosterCustomColumnAppendTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationRosterCustomColumnAppendTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationRosterCustomColumnAppendTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationRosterCustomColumnAppendTests
    COMMAND ClassMngrNextApplicationRosterCustomColumnAppendTests
)

# Keep custom roster-column removal eligibility independent of Qt.
add_executable(
    ClassMngrNextApplicationRosterCustomColumnRemovalPolicyTests
    tests/next_application_roster_custom_column_removal_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationRosterCustomColumnRemovalPolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationRosterCustomColumnRemovalPolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationRosterCustomColumnRemovalPolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationRosterCustomColumnRemovalPolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationRosterCustomColumnRemovalPolicyTests
    COMMAND ClassMngrNextApplicationRosterCustomColumnRemovalPolicyTests
)

# Keep roster row transfer preparation independent of Qt.
add_executable(
    ClassMngrNextApplicationRosterRowTransferPreparationTests
    tests/next_application_roster_row_transfer_preparation_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationRosterRowTransferPreparationTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationRosterRowTransferPreparationTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationRosterRowTransferPreparationTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationRosterRowTransferPreparationTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationRosterRowTransferPreparationTests
    COMMAND ClassMngrNextApplicationRosterRowTransferPreparationTests
)

# Keep roster-row data and first-empty queries independent of Qt.
add_executable(
    ClassMngrNextApplicationRosterRowAvailabilityTests
    tests/next_application_roster_row_availability_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationRosterRowAvailabilityTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationRosterRowAvailabilityTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationRosterRowAvailabilityTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationRosterRowAvailabilityTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationRosterRowAvailabilityTests
    COMMAND ClassMngrNextApplicationRosterRowAvailabilityTests
)

# Keep roster transfer-target eligibility independent of Qt.
add_executable(
    ClassMngrNextApplicationRosterTransferTargetEligibilityTests
    tests/next_application_roster_transfer_target_eligibility_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationRosterTransferTargetEligibilityTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationRosterTransferTargetEligibilityTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationRosterTransferTargetEligibilityTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationRosterTransferTargetEligibilityTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationRosterTransferTargetEligibilityTests
    COMMAND ClassMngrNextApplicationRosterTransferTargetEligibilityTests
)

# Keep Korean student-name suffix selection independent of Qt.
add_executable(
    ClassMngrNextApplicationStudentKoreanNameSuffixSuggestionTests
    tests/next_application_student_korean_name_suffix_suggestion_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationStudentKoreanNameSuffixSuggestionTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationStudentKoreanNameSuffixSuggestionTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationStudentKoreanNameSuffixSuggestionTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationStudentKoreanNameSuffixSuggestionTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationStudentKoreanNameSuffixSuggestionTests
    COMMAND ClassMngrNextApplicationStudentKoreanNameSuffixSuggestionTests
)

# Keep speaking-evaluation roster name import planning independent of Qt.
add_executable(
    ClassMngrNextApplicationSpeakingEvaluationRosterNameImportPlanTests
    tests/next_application_speaking_evaluation_roster_name_import_plan_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationSpeakingEvaluationRosterNameImportPlanTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationSpeakingEvaluationRosterNameImportPlanTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationSpeakingEvaluationRosterNameImportPlanTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationSpeakingEvaluationRosterNameImportPlanTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationSpeakingEvaluationRosterNameImportPlanTests
    COMMAND ClassMngrNextApplicationSpeakingEvaluationRosterNameImportPlanTests
)

# Keep speaking-evaluation score-to-roster assignment independent of Qt.
add_executable(
    ClassMngrNextApplicationSpeakingEvaluationRosterScoreRowAssignmentsTests
    tests/next_application_speaking_evaluation_roster_score_row_assignments_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationSpeakingEvaluationRosterScoreRowAssignmentsTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationSpeakingEvaluationRosterScoreRowAssignmentsTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationSpeakingEvaluationRosterScoreRowAssignmentsTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationSpeakingEvaluationRosterScoreRowAssignmentsTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationSpeakingEvaluationRosterScoreRowAssignmentsTests
    COMMAND ClassMngrNextApplicationSpeakingEvaluationRosterScoreRowAssignmentsTests
)

# Keep student name-pair lookup independent of Qt.
add_executable(
    ClassMngrNextApplicationStudentNamePairLookupTests
    tests/next_application_student_name_pair_lookup_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationStudentNamePairLookupTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationStudentNamePairLookupTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationStudentNamePairLookupTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationStudentNamePairLookupTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationStudentNamePairLookupTests
    COMMAND ClassMngrNextApplicationStudentNamePairLookupTests
)

# Keep speaking-evaluation AI batch eligibility independent of Qt.
add_executable(
    ClassMngrNextApplicationSpeakingEvaluationAiBatchEligibilityTests
    tests/next_application_speaking_evaluation_ai_batch_eligibility_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationSpeakingEvaluationAiBatchEligibilityTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationSpeakingEvaluationAiBatchEligibilityTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationSpeakingEvaluationAiBatchEligibilityTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationSpeakingEvaluationAiBatchEligibilityTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationSpeakingEvaluationAiBatchEligibilityTests
    COMMAND ClassMngrNextApplicationSpeakingEvaluationAiBatchEligibilityTests
)

# Keep speaking-evaluation AI batch comment quality independent of Qt.
add_executable(
    ClassMngrNextApplicationSpeakingEvaluationAiBatchCommentQualityTests
    tests/next_application_speaking_evaluation_ai_batch_comment_quality_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationSpeakingEvaluationAiBatchCommentQualityTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationSpeakingEvaluationAiBatchCommentQualityTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationSpeakingEvaluationAiBatchCommentQualityTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationSpeakingEvaluationAiBatchCommentQualityTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationSpeakingEvaluationAiBatchCommentQualityTests
    COMMAND ClassMngrNextApplicationSpeakingEvaluationAiBatchCommentQualityTests
)

# Keep accepted speaking-evaluation AI batch comment planning independent of Qt.
add_executable(
    ClassMngrNextApplicationSpeakingEvaluationAiBatchAcceptedCommentPlanTests
    tests/next_application_speaking_evaluation_ai_batch_accepted_comment_plan_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationSpeakingEvaluationAiBatchAcceptedCommentPlanTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationSpeakingEvaluationAiBatchAcceptedCommentPlanTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationSpeakingEvaluationAiBatchAcceptedCommentPlanTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationSpeakingEvaluationAiBatchAcceptedCommentPlanTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationSpeakingEvaluationAiBatchAcceptedCommentPlanTests
    COMMAND ClassMngrNextApplicationSpeakingEvaluationAiBatchAcceptedCommentPlanTests
)

# Keep stored speaking-evaluation private-note splitting independent of Qt.
add_executable(
    ClassMngrNextApplicationSpeakingEvaluationPrivateNotesSplitTests
    tests/next_application_speaking_evaluation_private_notes_split_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationSpeakingEvaluationPrivateNotesSplitTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationSpeakingEvaluationPrivateNotesSplitTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationSpeakingEvaluationPrivateNotesSplitTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationSpeakingEvaluationPrivateNotesSplitTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationSpeakingEvaluationPrivateNotesSplitTests
    COMMAND ClassMngrNextApplicationSpeakingEvaluationPrivateNotesSplitTests
)

# Keep the cycle-selection rule independent of Qt and the legacy runtime.
add_executable(
    ClassMngrNextApplicationEvaluationDefaultSelectionTests
    tests/next_application_evaluation_default_selection_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationEvaluationDefaultSelectionTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationEvaluationDefaultSelectionTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationEvaluationDefaultSelectionTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationEvaluationDefaultSelectionTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationEvaluationDefaultSelectionTests
    COMMAND ClassMngrNextApplicationEvaluationDefaultSelectionTests
)

# Keep class day matching independent of Qt and the feature runtime.
add_executable(
    ClassMngrNextApplicationClassDayFilterPolicyTests
    tests/next_application_class_day_filter_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationClassDayFilterPolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationClassDayFilterPolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationClassDayFilterPolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationClassDayFilterPolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationClassDayFilterPolicyTests
    COMMAND ClassMngrNextApplicationClassDayFilterPolicyTests
)

# Keep automatic-update startup eligibility app-less and Qt-free.
add_executable(
    ClassMngrNextApplicationAutomaticUpdateStartupEligibilityTests
    tests/next_application_automatic_update_startup_eligibility_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationAutomaticUpdateStartupEligibilityTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationAutomaticUpdateStartupEligibilityTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationAutomaticUpdateStartupEligibilityTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationAutomaticUpdateStartupEligibilityTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationAutomaticUpdateStartupEligibilityTests
    COMMAND ClassMngrNextApplicationAutomaticUpdateStartupEligibilityTests
)

# Keep skipped-update version decisions independent of Qt and the runtime.
add_executable(
    ClassMngrNextApplicationSkippedUpdateVersionPolicyTests
    tests/next_application_skipped_update_version_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationSkippedUpdateVersionPolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationSkippedUpdateVersionPolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationSkippedUpdateVersionPolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationSkippedUpdateVersionPolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationSkippedUpdateVersionPolicyTests
    COMMAND ClassMngrNextApplicationSkippedUpdateVersionPolicyTests
)

# Keep initial setup replacement decisions independent of Qt and the runtime.
add_executable(
    ClassMngrNextApplicationInitialSetupLifecycleTests
    tests/next_application_initial_setup_lifecycle_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationInitialSetupLifecycleTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationInitialSetupLifecycleTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationInitialSetupLifecycleTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationInitialSetupLifecycleTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationInitialSetupLifecycleTests
    COMMAND ClassMngrNextApplicationInitialSetupLifecycleTests
)

classmngr_add_qt_test(
    NAME NextPlatformInitialSetupLifecycleAdapter
    SOURCES
        tests/next_platform_initial_setup_lifecycle_adapter_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME UpdateControllerAutomaticStartup
    SOURCES
        tests/update_controller_automatic_startup_tests.cpp
    LIBRARIES
        Qt6::Test
    OFFSCREEN
    ENVIRONMENT
        "TMP=${CMAKE_CURRENT_BINARY_DIR}/update-controller-automatic-startup-temp"
        "TEMP=${CMAKE_CURRENT_BINARY_DIR}/update-controller-automatic-startup-temp"
)

classmngr_add_qt_test(
    NAME NextPlatformQSettingsFileDialogDirectoryPreferencesAdapter
    SOURCES
        tests/next_platform_qsettings_file_dialog_directory_preferences_adapter_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextDomainContract
    SOURCES
        tests/next_domain_contract_tests.cpp
    LIBRARIES
        ClassMngrNext::Domain
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationContract
    SOURCES
        tests/next_application_contract_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationState
    SOURCES
        tests/next_application_state_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationWorkspaceCoordinator
    SOURCES
        tests/next_application_workspace_coordinator_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationRecentWorkspaceHistoryUseCase
    SOURCES
        tests/next_application_recent_workspace_history_use_case_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationUpcomingBirthdayScheduleUseCase
    SOURCES
        tests/next_application_upcoming_birthday_schedule_use_case_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationSelection
    SOURCES
        tests/next_application_selection_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationImportJob
    SOURCES
        tests/next_application_import_job_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationImportJobCoordinator
    SOURCES
        tests/next_application_import_job_coordinator_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationReportJob
    SOURCES
        tests/next_application_report_job_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationReportJobCoordinator
    SOURCES
        tests/next_application_report_job_coordinator_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationImportReview
    SOURCES
        tests/next_application_import_review_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationDocumentCatalog
    SOURCES
        tests/next_application_document_catalog_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationDocumentCatalogUseCase
    SOURCES
        tests/next_application_document_catalog_use_case_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationDocumentContent
    SOURCES
        tests/next_application_document_content_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationClassSummary
    SOURCES
        tests/next_application_class_summary_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationSubPrepScheduleSummaryQuery
    SOURCES
        tests/next_application_sub_prep_schedule_summary_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationSubPrepPrintSourceQuery
    SOURCES
        tests/next_application_sub_prep_print_source_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationClassCoTeacherPageReadQuery
    SOURCES
        tests/next_application_class_co_teacher_page_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationClassCoTeacherTeacherChoicesReadQuery
    SOURCES
        tests/next_application_class_co_teacher_teacher_choices_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationClassNotesPageReadQuery
    SOURCES
        tests/next_application_class_notes_page_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationSelectedClassGradeReadQuery
    SOURCES
        tests/next_application_selected_class_grade_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationSelectedClassSubtitleReadQuery
    SOURCES
        tests/next_application_selected_class_subtitle_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationClassNotesSavePort
    SOURCES
        tests/next_application_class_notes_save_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationClassDetailsSaveUseCase
    SOURCES
        tests/next_application_class_details_save_use_case_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationClassDetailsScheduleConflictQuery
    SOURCES
        tests/next_application_class_details_schedule_conflict_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationClassDetailsValidationContextQuery
    SOURCES
        tests/next_application_class_details_validation_context_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationClassDetailsValidationPolicy
    SOURCES
        tests/next_application_class_details_validation_policy_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextScheduleEditorDialogSave
    SOURCES
        tests/schedule_editor_dialog_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextApplicationRosterSaveUseCase
    SOURCES
        tests/next_application_roster_save_use_case_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationRosterReadQuery
    SOURCES
        tests/next_application_roster_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationScheduleBuilderSourceSnapshot
    SOURCES
        tests/next_application_schedule_builder_source_snapshot_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationClassesNavigationSnapshot
    SOURCES
        tests/next_application_classes_navigation_snapshot_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationClassesListReadQuery
    SOURCES
        tests/next_application_classes_list_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesClassesListReadPort
    SOURCES
        tests/next_platform_application_services_classes_list_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

qt_add_executable(
    ClassMngrNextApplicationSpeakingEvaluationSaveUseCaseTests
    tests/next_application_speaking_evaluation_save_use_case_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationSpeakingEvaluationSaveUseCaseTests
    PRIVATE
        cxx_std_23
)
target_include_directories(
    ClassMngrNextApplicationSpeakingEvaluationSaveUseCaseTests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(
    ClassMngrNextApplicationSpeakingEvaluationSaveUseCaseTests
    PRIVATE
        ClassMngrBuildSettings
        ClassMngrNext::Application
        Qt6::Test
)
set_property(
    TARGET ClassMngrNextApplicationSpeakingEvaluationSaveUseCaseTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
add_test(
    NAME ClassMngrNextApplicationSpeakingEvaluationSaveUseCaseTests
    COMMAND ClassMngrNextApplicationSpeakingEvaluationSaveUseCaseTests
)

classmngr_add_qt_test(
    NAME NextApplicationSpeakingEvaluationQuery
    SOURCES
        tests/next_application_speaking_evaluation_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

# Exercise roster-score derivation against a fake typed read port without Qt.
add_executable(
    ClassMngrNextApplicationSpeakingEvaluationRosterScoreImportTests
    tests/next_application_speaking_evaluation_roster_score_import_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationSpeakingEvaluationRosterScoreImportTests
    PRIVATE
        cxx_std_23
)
if(MSVC)
    target_compile_options(
        ClassMngrNextApplicationSpeakingEvaluationRosterScoreImportTests
        PRIVATE
            /utf-8
    )
endif()
set_target_properties(
    ClassMngrNextApplicationSpeakingEvaluationRosterScoreImportTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationSpeakingEvaluationRosterScoreImportTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationSpeakingEvaluationRosterScoreImportTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationSpeakingEvaluationRosterScoreImportTests
    COMMAND ClassMngrNextApplicationSpeakingEvaluationRosterScoreImportTests
)

classmngr_add_qt_test(
    NAME NextApplicationClassDetailsPageQuery
    SOURCES
        tests/next_application_class_details_page_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationMyClassesClassInformationReadQuery
    SOURCES
        tests/next_application_my_classes_class_information_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationScheduleEditorClassInfoQuery
    SOURCES
        tests/next_application_schedule_editor_class_info_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationSubPrepRosterOutputSourceQuery
    SOURCES
        tests/next_application_sub_prep_roster_output_source_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationSubPrepClassDetailsQuery
    SOURCES
        tests/next_application_sub_prep_class_details_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationSubPrepClassInformationState
    SOURCES
        tests/next_application_sub_prep_class_information_state_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationClassTransfer
    SOURCES
        tests/next_application_class_transfer_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationScheduleView
    SOURCES
        tests/next_application_schedule_view_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationCalendarEvent
    SOURCES
        tests/next_application_calendar_event_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationCalendarEventImportPlan
    SOURCES
        tests/next_application_calendar_event_import_plan_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationCalendarEventImportUseCase
    SOURCES
        tests/next_application_calendar_event_import_use_case_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

# Exercise the duplicate-key value without Qt or the legacy runtime.
add_executable(
    ClassMngrNextApplicationCalendarEventImportSignatureTests
    tests/next_application_calendar_event_import_signature_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationCalendarEventImportSignatureTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationCalendarEventImportSignatureTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationCalendarEventImportSignatureTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationCalendarEventImportSignatureTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationCalendarEventImportSignatureTests
    COMMAND ClassMngrNextApplicationCalendarEventImportSignatureTests
)

classmngr_add_qt_test(
    NAME NextApplicationScheduleImportStateValidation
    SOURCES
        tests/next_application_schedule_import_state_validation_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationScheduleImportStateSnapshot
    SOURCES
        tests/next_application_schedule_import_state_snapshot_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationScheduleImportMatchingProjection
    SOURCES
        tests/next_application_schedule_import_matching_projection_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

# Exercise the review decision contract without Qt or the legacy runtime.
add_executable(
    ClassMngrNextApplicationScheduleImportReviewDecisionsTests
    tests/next_application_schedule_import_review_decisions_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationScheduleImportReviewDecisionsTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationScheduleImportReviewDecisionsTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationScheduleImportReviewDecisionsTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationScheduleImportReviewDecisionsTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationScheduleImportReviewDecisionsTests
    COMMAND ClassMngrNextApplicationScheduleImportReviewDecisionsTests
)

# Exercise the Qt-free review readiness orchestration contract.
add_executable(
    ClassMngrNextApplicationScheduleImportReviewReadinessTests
    tests/next_application_schedule_import_review_readiness_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationScheduleImportReviewReadinessTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationScheduleImportReviewReadinessTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationScheduleImportReviewReadinessTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationScheduleImportReviewReadinessTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationScheduleImportReviewReadinessTests
    COMMAND ClassMngrNextApplicationScheduleImportReviewReadinessTests
)

# Exercise plan eligibility without Qt or the legacy runtime.
add_executable(
    ClassMngrNextApplicationScheduleImportApplyUseCaseTests
    tests/next_application_schedule_import_apply_use_case_tests.cpp
)
target_compile_features(ClassMngrNextApplicationScheduleImportApplyUseCaseTests PRIVATE cxx_std_23)
set_target_properties(ClassMngrNextApplicationScheduleImportApplyUseCaseTests PROPERTIES
    AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
set_property(TARGET ClassMngrNextApplicationScheduleImportApplyUseCaseTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE)
target_link_libraries(ClassMngrNextApplicationScheduleImportApplyUseCaseTests
    PRIVATE ClassMngrNext::Application)
add_test(NAME ClassMngrNextApplicationScheduleImportApplyUseCaseTests
    COMMAND ClassMngrNextApplicationScheduleImportApplyUseCaseTests)

# Keep apply decision projection coverage independent of Qt and the legacy runtime.
add_executable(
    ClassMngrNextApplicationScheduleImportApplyReviewDecisionsTests
    tests/next_application_schedule_import_apply_review_decisions_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationScheduleImportApplyReviewDecisionsTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationScheduleImportApplyReviewDecisionsTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationScheduleImportApplyReviewDecisionsTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationScheduleImportApplyReviewDecisionsTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationScheduleImportApplyReviewDecisionsTests
    COMMAND ClassMngrNextApplicationScheduleImportApplyReviewDecisionsTests
)

# Keep the proposed import summary projection independent of Qt and the legacy runtime.
add_executable(
    ClassMngrNextApplicationScheduleImportReviewSummaryProjectionTests
    tests/next_application_schedule_import_review_summary_projection_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationScheduleImportReviewSummaryProjectionTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationScheduleImportReviewSummaryProjectionTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationScheduleImportReviewSummaryProjectionTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationScheduleImportReviewSummaryProjectionTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationScheduleImportReviewSummaryProjectionTests
    COMMAND ClassMngrNextApplicationScheduleImportReviewSummaryProjectionTests
)

# Keep the schedule clear count projection independent of Qt and the legacy runtime.
add_executable(
    ClassMngrNextApplicationScheduleImportSchedulesClearedProjectionTests
    tests/next_application_schedule_import_schedules_cleared_projection_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationScheduleImportSchedulesClearedProjectionTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationScheduleImportSchedulesClearedProjectionTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationScheduleImportSchedulesClearedProjectionTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationScheduleImportSchedulesClearedProjectionTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationScheduleImportSchedulesClearedProjectionTests
    COMMAND ClassMngrNextApplicationScheduleImportSchedulesClearedProjectionTests
)

# Exercise plan eligibility without Qt or the legacy runtime.
add_executable(
    ClassMngrNextApplicationScheduleImportPlanValidationTests
    tests/next_application_schedule_import_plan_validation_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationScheduleImportPlanValidationTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationScheduleImportPlanValidationTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationScheduleImportPlanValidationTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationScheduleImportPlanValidationTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationScheduleImportPlanValidationTests
    COMMAND ClassMngrNextApplicationScheduleImportPlanValidationTests
)

# Exercise the Korean teacher sparse update policy without Qt or the legacy runtime.
add_executable(
    ClassMngrNextApplicationKoreanTeacherImportUpdateTests
    tests/next_application_korean_teacher_import_update_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationKoreanTeacherImportUpdateTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationKoreanTeacherImportUpdateTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationKoreanTeacherImportUpdateTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationKoreanTeacherImportUpdateTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationKoreanTeacherImportUpdateTests
    COMMAND ClassMngrNextApplicationKoreanTeacherImportUpdateTests
)

# Exercise the Native English Teacher sparse update policy without Qt or the legacy runtime.
add_executable(
    ClassMngrNextApplicationNativeEnglishTeacherImportUpdateTests
    tests/next_application_native_english_teacher_import_update_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationNativeEnglishTeacherImportUpdateTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationNativeEnglishTeacherImportUpdateTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationNativeEnglishTeacherImportUpdateTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationNativeEnglishTeacherImportUpdateTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationNativeEnglishTeacherImportUpdateTests
    COMMAND ClassMngrNextApplicationNativeEnglishTeacherImportUpdateTests
)

# Exercise the GS Team sparse update policy without Qt or the legacy runtime.
add_executable(
    ClassMngrNextApplicationGsTeamImportUpdateTests
    tests/next_application_gs_team_import_update_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationGsTeamImportUpdateTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationGsTeamImportUpdateTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationGsTeamImportUpdateTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationGsTeamImportUpdateTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationGsTeamImportUpdateTests
    COMMAND ClassMngrNextApplicationGsTeamImportUpdateTests
)

# Exercise full Teacher Import plan validation without Qt or the legacy runtime.
add_executable(
    ClassMngrNextApplicationTeacherImportPlanValidationTests
    tests/next_application_teacher_import_plan_validation_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationTeacherImportPlanValidationTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationTeacherImportPlanValidationTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationTeacherImportPlanValidationTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationTeacherImportPlanValidationTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationTeacherImportPlanValidationTests
    COMMAND ClassMngrNextApplicationTeacherImportPlanValidationTests
)

# Exercise Teacher Import match cardinality classification without Qt or the legacy runtime.
add_executable(
    ClassMngrNextApplicationTeacherImportMatchCardinalityTests
    tests/next_application_teacher_import_match_cardinality_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationTeacherImportMatchCardinalityTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationTeacherImportMatchCardinalityTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationTeacherImportMatchCardinalityTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationTeacherImportMatchCardinalityTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationTeacherImportMatchCardinalityTests
    COMMAND ClassMngrNextApplicationTeacherImportMatchCardinalityTests
)

# Exercise the application-owned Teacher Import apply orchestration without Qt.
add_executable(
    ClassMngrNextApplicationTeacherImportUseCaseTests
    tests/next_application_teacher_import_use_case_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationTeacherImportUseCaseTests
    PRIVATE
        cxx_std_23
)
if(MSVC)
    target_compile_options(
        ClassMngrNextApplicationTeacherImportUseCaseTests
        PRIVATE
            /utf-8
    )
endif()
set_target_properties(
    ClassMngrNextApplicationTeacherImportUseCaseTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationTeacherImportUseCaseTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationTeacherImportUseCaseTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationTeacherImportUseCaseTests
    COMMAND ClassMngrNextApplicationTeacherImportUseCaseTests
)

# Verify Teacher profile validation and save-then-reload orchestration without Qt.
add_executable(
    ClassMngrNextApplicationTeacherProfileEditTests
    tests/next_application_teacher_profile_edit_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationTeacherProfileEditTests
    PRIVATE
        cxx_std_23
)
if(MSVC)
    target_compile_options(
        ClassMngrNextApplicationTeacherProfileEditTests
        PRIVATE
            /utf-8
    )
endif()
set_target_properties(
    ClassMngrNextApplicationTeacherProfileEditTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationTeacherProfileEditTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationTeacherProfileEditTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationTeacherProfileEditTests
    COMMAND ClassMngrNextApplicationTeacherProfileEditTests
)

classmngr_add_qt_test(
    NAME NextApplicationTeacherProfileReadQuery
    SOURCES
        tests/next_application_teacher_profile_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationNativeEnglishTeacherDirectoryReadQuery
    SOURCES
        tests/next_application_native_english_teacher_directory_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationKoreanTeacherBirthdayDirectoryReadQuery
    SOURCES
        tests/next_application_korean_teacher_birthday_directory_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

# Exercise Native English Teacher directory validation and save orchestration
# without creating a GUI application.
classmngr_add_qt_test(
    NAME NextApplicationNativeEnglishTeacherDirectorySave
    SOURCES
        tests/next_application_native_english_teacher_directory_save_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

# Exercise GS Team's two-namespace name policy and save orchestration without
# creating a GUI application.
classmngr_add_qt_test(
    NAME NextApplicationGsTeamDirectorySave
    SOURCES
        tests/next_application_gs_team_directory_save_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationGsTeamDirectoryReadQuery
    SOURCES
        tests/next_application_gs_team_directory_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

# Verify co-teacher ID validation and legacy unassigned-sentinel translation
# without Qt or persistence.
add_executable(
    ClassMngrNextApplicationClassCoTeacherAssignmentTests
    tests/next_application_class_co_teacher_assignment_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationClassCoTeacherAssignmentTests
    PRIVATE
        cxx_std_23
)
if(MSVC)
    target_compile_options(
        ClassMngrNextApplicationClassCoTeacherAssignmentTests
        PRIVATE
            /EHsc
    )
endif()
set_target_properties(
    ClassMngrNextApplicationClassCoTeacherAssignmentTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationClassCoTeacherAssignmentTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationClassCoTeacherAssignmentTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationClassCoTeacherAssignmentTests
    COMMAND ClassMngrNextApplicationClassCoTeacherAssignmentTests
)

classmngr_add_qt_test(
    NAME CalendarEventImportParity
    SOURCES
        tests/next_feature_calendar_event_import_parity_tests.cpp
    COMPILE_DEFINITIONS
        CLASSMNGR_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Network
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationSubPrepCampusDirectoryQuery
    SOURCES
        tests/next_application_sub_prep_campus_directory_query_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationCalendarEventImportSignatureQueryPort
    SOURCES
        tests/next_application_calendar_event_import_signature_query_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationCalendarEventQueryPort
    SOURCES
        tests/next_application_calendar_event_query_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationCampusDirectory
    SOURCES
        tests/next_application_campus_directory_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationUserPreferences
    SOURCES
        tests/next_application_user_preferences_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerExcelImportTimeoutPort
    SOURCES
        tests/next_platform_settings_manager_excel_import_timeout_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerAutomaticUpdatePreferencesPort
    SOURCES
        tests/next_platform_settings_manager_automatic_update_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerAiCommentCustomWebsitePort
    SOURCES
        tests/next_platform_settings_manager_ai_comment_custom_website_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerAiCommentVoicePreferencesPort
    SOURCES
        tests/next_platform_settings_manager_ai_comment_voice_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerAiCommentProviderPreferencesPort
    SOURCES
        tests/next_platform_settings_manager_ai_comment_provider_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerFontSizePreferencesPort
    SOURCES
        tests/next_platform_settings_manager_font_size_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerDocumentViewerBackgroundPreferencesPort
    SOURCES
        tests/next_platform_settings_manager_document_viewer_background_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerDocumentPageSpacingPreferencesPort
    SOURCES
        tests/next_platform_settings_manager_document_page_spacing_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerSaveModePreferencesPort
    SOURCES
        tests/next_platform_settings_manager_save_mode_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerThemePreferencesPort
    SOURCES
        tests/next_platform_settings_manager_theme_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerDialogGeometryPreferencesPort
    SOURCES
        tests/next_platform_settings_manager_dialog_geometry_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerLanguagePreferencesPort
    SOURCES
        tests/next_platform_settings_manager_language_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerUpcomingBirthdayDismissalPort
    SOURCES
        tests/next_platform_settings_manager_upcoming_birthday_dismissal_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerSkippedUpdateVersionPort
    SOURCES
        tests/next_platform_settings_manager_skipped_update_version_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerPowerPointDataAccessNoticePort
    SOURCES
        tests/next_platform_settings_manager_powerpoint_data_access_notice_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerSidebarDisplayPreferencesPort
    SOURCES
        tests/next_platform_settings_manager_sidebar_display_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerRecentWorkspaceHistoryPort
    SOURCES
        tests/next_platform_settings_manager_recent_workspace_history_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerLastSelectedCampusPort
    SOURCES
        tests/next_platform_settings_manager_last_selected_campus_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformSettingsManagerLastDatabaseDirectoryPort
    SOURCES
        tests/next_platform_settings_manager_last_database_directory_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformLegacyWorkspaceGateway
    SOURCES
        tests/next_platform_legacy_workspace_gateway_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesWorkspacePort
    SOURCES
        tests/next_platform_application_services_workspace_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesDocumentCatalogPort
    SOURCES
        tests/next_platform_application_services_document_catalog_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesSubPrepPrintSourcePort
    SOURCES
        tests/next_platform_application_services_sub_prep_print_source_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextApplicationSubPrepCalendarEventIntervalsQuery
    SOURCES
        tests/next_application_sub_prep_calendar_event_intervals_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesClassNotesPageReadPort
    SOURCES
        tests/next_platform_application_services_class_notes_page_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesSelectedClassGradeReadPort
    SOURCES
        tests/next_platform_application_services_selected_class_grade_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesClassNotesSavePort
    SOURCES
        tests/next_platform_application_services_class_notes_save_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesClassDetailsSavePort
    SOURCES
        tests/next_platform_application_services_class_details_save_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesClassDetailsScheduleConflictPort
    SOURCES
        tests/next_platform_application_services_class_details_schedule_conflict_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesClassDetailsValidationContextPort
    SOURCES
        tests/next_platform_application_services_class_details_validation_context_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesRosterSavePort
    SOURCES
        tests/next_platform_application_services_roster_save_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesRosterReadPort
    SOURCES
        tests/next_platform_application_services_roster_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesClassesNavigationReadPort
    SOURCES
        tests/next_platform_application_services_classes_navigation_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesSpeakingEvaluationSavePort
    SOURCES
        tests/next_platform_application_services_speaking_evaluation_save_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesSpeakingEvaluationReadPort
    SOURCES
        tests/next_platform_application_services_speaking_evaluation_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesClassDetailsPageReadPort
    SOURCES
        tests/next_platform_application_services_class_details_page_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesMyClassesClassInformationReadPort
    SOURCES
        tests/next_platform_application_services_my_classes_class_information_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesSelectedClassSubtitleReadPort
    SOURCES
        tests/next_platform_application_services_selected_class_subtitle_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesScheduleEditorClassInfoReadPort
    SOURCES
        tests/next_platform_application_services_schedule_editor_class_info_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesClassCoTeacherPageReadPort
    SOURCES
        tests/next_platform_application_services_class_co_teacher_page_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesClassCoTeacherTeacherChoicesReadPort
    SOURCES
        tests/next_platform_application_services_class_co_teacher_teacher_choices_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesTeacherProfileEditPersistencePort
    SOURCES
        tests/next_platform_application_services_teacher_profile_edit_persistence_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesTeacherProfileReadPort
    SOURCES
        tests/next_platform_application_services_teacher_profile_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesNativeEnglishTeacherDirectoryReadPort
    SOURCES
        tests/next_platform_application_services_native_english_teacher_directory_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesKoreanTeacherBirthdayDirectoryReadPort
    SOURCES
        tests/next_platform_application_services_korean_teacher_birthday_directory_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesNativeEnglishTeacherDirectorySavePort
    SOURCES
        tests/next_platform_application_services_native_english_teacher_directory_save_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesGsTeamDirectorySavePort
    SOURCES
        tests/next_platform_application_services_gs_team_directory_save_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesGsTeamDirectoryReadPort
    SOURCES
        tests/next_platform_application_services_gs_team_directory_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesClassCoTeacherAssignmentPort
    SOURCES
        tests/next_platform_application_services_class_co_teacher_assignment_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextFeatureClassCoTeacherPage
    SOURCES
        tests/next_feature_class_co_teacher_page_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextFeatureClassNotesPage
    SOURCES
        tests/next_feature_class_notes_page_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextApplicationMyInfoCampusDirectoryQueryPort
    SOURCES
        tests/next_application_my_info_campus_directory_query_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesSubPrepRosterOutputSourcePort
    SOURCES
        tests/next_platform_application_services_sub_prep_roster_output_source_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformSubPrepCampusDirectoryQueryAdapter
    SOURCES
        tests/next_platform_sub_prep_campus_directory_query_adapter_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesCalendarEventPort
    SOURCES
        tests/next_platform_application_services_calendar_event_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesCalendarEventDisplayPreferencesPort
    SOURCES
        tests/next_platform_application_services_calendar_event_display_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesCalendarFirstDayOfWeekPreferencesPort
    SOURCES
        tests/next_platform_application_services_calendar_first_day_of_week_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPort
    SOURCES
        tests/next_platform_application_services_academic_calendar_schedule_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesCalendarEventTypeColorPreferencesPort
    SOURCES
        tests/next_platform_application_services_calendar_event_type_color_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesCustomColorPalettePreferencesPort
    SOURCES
        tests/next_platform_application_services_custom_color_palette_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME ColorUtilsCustomColorPalette
    SOURCES
        tests/colorutils_custom_color_palette_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesCurrentCampusPreferencesPort
    SOURCES
        tests/next_platform_application_services_current_campus_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformCalendarEventImportCampusCodeQuery
    SOURCES
        tests/next_platform_calendar_event_import_campus_code_query_adapter_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformCalendarPageCampusDirectoryQuery
    SOURCES
        tests/next_platform_calendar_page_campus_directory_query_adapter_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformMyInfoCampusDirectoryQueryAdapter
    SOURCES
        tests/next_platform_my_info_campus_directory_query_adapter_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesPersonalDisplayNamePreferencesPort
    SOURCES
        tests/next_platform_application_services_personal_display_name_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesPersonalDetailsSavePort
    SOURCES
        tests/next_platform_application_services_personal_details_save_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesPersonalSignatureImagePort
    SOURCES
        tests/next_platform_application_services_personal_signature_image_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesPersonalSignaturePreferencesPort
    SOURCES
        tests/next_platform_application_services_personal_signature_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesSubPrepPreferencesPort
    SOURCES
        tests/next_platform_application_services_sub_prep_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesSubPrepPersonalZoomPreferencesPort
    SOURCES
        tests/next_platform_application_services_sub_prep_personal_zoom_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextApplicationScheduleSlotStateReadQuery
    SOURCES
        tests/next_application_schedule_slot_state_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationScheduleTestingAssignmentReadQuery
    SOURCES
        tests/next_application_schedule_testing_assignment_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationScheduleTestingClassChoicesReadQuery
    SOURCES
        tests/next_application_schedule_testing_class_choices_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationTestingClassDetailsReadQuery
    SOURCES
        tests/next_application_testing_class_details_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationTestingClassDetailsUpdateUseCase
    SOURCES
        tests/next_application_testing_class_details_update_use_case_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationTestingClassCreateUseCase
    SOURCES
        tests/next_application_testing_class_create_use_case_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationTestingClassDeleteUseCase
    SOURCES
        tests/next_application_testing_class_delete_use_case_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationScheduleTestingAssignmentSaveUseCase
    SOURCES
        tests/next_application_schedule_testing_assignment_save_use_case_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationScheduleSlotStateSave
    SOURCES
        tests/next_application_schedule_slot_state_save_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesScheduleSlotStateReadPort
    SOURCES
        tests/next_platform_application_services_schedule_slot_state_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesScheduleTestingAssignmentReadPort
    SOURCES
        tests/next_platform_application_services_schedule_testing_assignment_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesScheduleTestingClassChoicesReadPort
    SOURCES
        tests/next_platform_application_services_schedule_testing_class_choices_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextApplicationTestingTeacherChoicesReadQuery
    SOURCES
        tests/next_application_testing_teacher_choices_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesTestingTeacherChoicesReadPort
    SOURCES
        tests/next_platform_application_services_testing_teacher_choices_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextApplicationInitialSetupTeacherChoicesReadQuery
    SOURCES
        tests/next_application_initial_setup_teacher_choices_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationClassTeacherAssignmentsReadQuery
    SOURCES
        tests/next_application_class_teacher_assignments_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesInitialSetupTeacherChoicesReadPort
    SOURCES
        tests/next_platform_application_services_initial_setup_teacher_choices_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesClassTeacherAssignmentsReadPort
    SOURCES
        tests/next_platform_application_services_class_teacher_assignments_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesTestingClassDetailsReadPort
    SOURCES
        tests/next_platform_application_services_testing_class_details_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesTestingClassDetailsUpdatePort
    SOURCES
        tests/next_platform_application_services_testing_class_details_update_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesTestingClassCreatePort
    SOURCES
        tests/next_platform_application_services_testing_class_create_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesTestingClassDeletePort
    SOURCES
        tests/next_platform_application_services_testing_class_delete_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesScheduleTestingAssignmentSavePort
    SOURCES
        tests/next_platform_application_services_schedule_testing_assignment_save_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesScheduleSlotStateSavePort
    SOURCES
        tests/next_platform_application_services_schedule_slot_state_save_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesScheduleBuilderSourcePort
    SOURCES
        tests/next_platform_application_services_schedule_builder_source_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesScheduleImportStateSnapshotPort
    SOURCES
        tests/next_platform_application_services_schedule_import_state_snapshot_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesScheduleImportApplyPort
    SOURCES
        tests/next_platform_application_services_schedule_import_apply_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesScheduleDisplayPreferencesPort
    SOURCES
        tests/next_platform_application_services_schedule_display_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesScheduleDisplayModePreferencesPort
    SOURCES
        tests/next_platform_application_services_schedule_display_mode_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesEvaluationDefaultPolicyPort
    SOURCES
        tests/next_platform_application_services_evaluation_default_policy_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesClassDayFilterResetPolicyPort
    SOURCES
        tests/next_platform_application_services_class_day_filter_reset_policy_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesClassSelectionResetPolicyPort
    SOURCES
        tests/next_platform_application_services_class_selection_reset_policy_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesClassVisibilityPreferencesPort
    SOURCES
        tests/next_platform_application_services_class_visibility_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesMiddleSchoolAnalyticsPreferencesPort
    SOURCES
        tests/next_platform_application_services_middle_school_analytics_preferences_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformDocumentContentResourcePort
    SOURCES
        tests/next_platform_document_content_resource_port_tests.cpp
    COMPILE_DEFINITIONS
        CLASSMNGR_RESOURCE_PACK_DIR="${CLASSMNGR_RESOURCE_PACK_OUTPUT_DIR}"
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    DEPENDENCIES
        ClassMngrdocumentsResourcePack
)

classmngr_add_qt_test(
    NAME NextPlatformQtJobWorker
    SOURCES
        tests/next_platform_qt_job_worker_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME FileControllerWorkspaceLifecycle
    SOURCES
        tests/file_controller_workspace_lifecycle_tests.cpp
    LIBRARIES
        Qt6::Test
    OFFSCREEN
)
