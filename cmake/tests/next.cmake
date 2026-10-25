include_guard(GLOBAL)

add_executable(ClassMngrNextCalendarResourceKeysTests
    cmake/tests/next_calendar_resource_keys.cpp
)
target_compile_features(ClassMngrNextCalendarResourceKeysTests
    PRIVATE
        cxx_std_23
)
set_target_properties(ClassMngrNextCalendarResourceKeysTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
classmngr_next_add_calendar_qml_resources(ClassMngrNextCalendarResourceKeysTests)
target_link_libraries(ClassMngrNextCalendarResourceKeysTests
    PRIVATE
        Qt6::Core
)
add_test(
    NAME ClassMngrNextCalendarResourceKeysTests
    COMMAND ClassMngrNextCalendarResourceKeysTests
)

# Load the generated catalog from an explicit root and stage only requested
# fixtures, proving document bodies are not opened during catalog initialization.
add_executable(ClassMngrNextResourceLoaderTests
    cmake/tests/next_resource_loader.cpp
)
target_compile_features(ClassMngrNextResourceLoaderTests
    PRIVATE
        cxx_std_23
)
set_target_properties(ClassMngrNextResourceLoaderTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(TARGET ClassMngrNextResourceLoaderTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_include_directories(ClassMngrNextResourceLoaderTests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)
target_compile_definitions(ClassMngrNextResourceLoaderTests
    PRIVATE
        CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG="${CLASSMNGR_NEXT_RESOURCE_CATALOG}"
        CLASSMNGR_NEXT_TEST_RESOURCE_ROOT="${CLASSMNGR_NEXT_RESOURCE_ROOT}"
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY="${_classmngr_next_translation_output_directory}"
)
classmngr_next_add_calendar_qml_resources(ClassMngrNextResourceLoaderTests)
target_link_libraries(ClassMngrNextResourceLoaderTests
    PRIVATE
        ClassMngrNext::Resources
)
add_dependencies(ClassMngrNextResourceLoaderTests
    ClassMngrNextResourcePackage
)
add_test(
    NAME ClassMngrNextResourceLoaderTests
    COMMAND ClassMngrNextResourceLoaderTests
)
set_tests_properties(ClassMngrNextResourceLoaderTests
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Project document metadata without staging or opening any document bodies.
add_executable(ClassMngrNextResourceDocumentCatalogTests
    cmake/tests/next_resource_document_catalog.cpp
)
target_compile_features(ClassMngrNextResourceDocumentCatalogTests
    PRIVATE
        cxx_std_23
)
set_target_properties(ClassMngrNextResourceDocumentCatalogTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(TARGET ClassMngrNextResourceDocumentCatalogTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_include_directories(ClassMngrNextResourceDocumentCatalogTests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)
target_compile_definitions(ClassMngrNextResourceDocumentCatalogTests
    PRIVATE
        CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG="${CLASSMNGR_NEXT_RESOURCE_CATALOG}"
        CLASSMNGR_NEXT_TEST_RESOURCE_ROOT="${CLASSMNGR_NEXT_RESOURCE_ROOT}"
)
target_link_libraries(ClassMngrNextResourceDocumentCatalogTests
    PRIVATE
        ClassMngrNext::Platform
)
add_dependencies(ClassMngrNextResourceDocumentCatalogTests
    ClassMngrNextResourcePackage
)
add_test(
    NAME ClassMngrNextResourceDocumentCatalogTests
    COMMAND ClassMngrNextResourceDocumentCatalogTests
)
set_tests_properties(ClassMngrNextResourceDocumentCatalogTests
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ClassMngrNextResourceRootResolverTests
    cmake/tests/next_resource_root_resolver.cpp
)
target_compile_features(ClassMngrNextResourceRootResolverTests
    PRIVATE
        cxx_std_23
)
set_target_properties(ClassMngrNextResourceRootResolverTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(TARGET ClassMngrNextResourceRootResolverTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_include_directories(ClassMngrNextResourceRootResolverTests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(ClassMngrNextResourceRootResolverTests
    PRIVATE
        ClassMngrNext::Resources
)
add_test(
    NAME ClassMngrNextResourceRootResolverTests
    COMMAND ClassMngrNextResourceRootResolverTests
)

# Keep PowerPoint batch template compatibility independent of Qt and the
# feature layer.
add_executable(
    ClassMngrNextApplicationSpeakingEvaluationReportTemplatePolicyTests
    tests/next_application_speaking_evaluation_report_template_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationSpeakingEvaluationReportTemplatePolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationSpeakingEvaluationReportTemplatePolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationSpeakingEvaluationReportTemplatePolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationSpeakingEvaluationReportTemplatePolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationSpeakingEvaluationReportTemplatePolicyTests
    COMMAND ClassMngrNextApplicationSpeakingEvaluationReportTemplatePolicyTests
)

# Academic year schedule validation remains independent of Qt and the feature layer.
add_executable(
    ClassMngrNextApplicationAcademicYearSchedulePolicyTests
    tests/next_application_academic_year_schedule_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationAcademicYearSchedulePolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationAcademicYearSchedulePolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationAcademicYearSchedulePolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationAcademicYearSchedulePolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationAcademicYearSchedulePolicyTests
    COMMAND ClassMngrNextApplicationAcademicYearSchedulePolicyTests
)

# Schedule slot policy remains independent of Qt and the feature layer.
add_executable(
    ClassMngrNextApplicationScheduleSlotPolicyTests
    tests/next_application_schedule_slot_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationScheduleSlotPolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationScheduleSlotPolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationScheduleSlotPolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationScheduleSlotPolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationScheduleSlotPolicyTests
    COMMAND ClassMngrNextApplicationScheduleSlotPolicyTests
)

# Schedule source selection remains independent of Qt and the feature layer.
add_executable(
    ClassMngrNextApplicationScheduleDisplayModeSourcePolicyTests
    tests/next_application_schedule_display_mode_source_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationScheduleDisplayModeSourcePolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationScheduleDisplayModeSourcePolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationScheduleDisplayModeSourcePolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationScheduleDisplayModeSourcePolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationScheduleDisplayModeSourcePolicyTests
    COMMAND ClassMngrNextApplicationScheduleDisplayModeSourcePolicyTests
)

# Keep Testing-mode grade suppression independent of Qt and normalization.
add_executable(
    ClassMngrNextApplicationScheduleTestingGradeSuppressionPolicyTests
    tests/next_application_schedule_testing_grade_suppression_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationScheduleTestingGradeSuppressionPolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationScheduleTestingGradeSuppressionPolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationScheduleTestingGradeSuppressionPolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationScheduleTestingGradeSuppressionPolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationScheduleTestingGradeSuppressionPolicyTests
    COMMAND ClassMngrNextApplicationScheduleTestingGradeSuppressionPolicyTests
)

# Class Analytics composition/calculation must remain Qt-free.
add_executable(
    ClassMngrNextApplicationClassAnalyticsDashboardReadQueryTests
    tests/next_application_class_analytics_dashboard_read_query_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationClassAnalyticsDashboardReadQueryTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationClassAnalyticsDashboardReadQueryTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationClassAnalyticsDashboardReadQueryTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationClassAnalyticsDashboardReadQueryTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationClassAnalyticsDashboardReadQueryTests
    COMMAND ClassMngrNextApplicationClassAnalyticsDashboardReadQueryTests
)

# Class Analytics grade attention remains independent of Qt and the feature layer.
add_executable(
    ClassMngrNextApplicationClassAnalyticsGradeAttentionPolicyTests
    tests/next_application_class_analytics_grade_attention_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationClassAnalyticsGradeAttentionPolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationClassAnalyticsGradeAttentionPolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationClassAnalyticsGradeAttentionPolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationClassAnalyticsGradeAttentionPolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationClassAnalyticsGradeAttentionPolicyTests
    COMMAND ClassMngrNextApplicationClassAnalyticsGradeAttentionPolicyTests
)

# Strongest/focus criterion selection remains Qt-free and preserves ties.
add_executable(
    ClassMngrNextApplicationClassAnalyticsCriterionExtremaPolicyTests
    tests/next_application_class_analytics_criterion_extrema_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationClassAnalyticsCriterionExtremaPolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationClassAnalyticsCriterionExtremaPolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationClassAnalyticsCriterionExtremaPolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationClassAnalyticsCriterionExtremaPolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationClassAnalyticsCriterionExtremaPolicyTests
    COMMAND ClassMngrNextApplicationClassAnalyticsCriterionExtremaPolicyTests
)

# Ranking order uses prepared integer keys and remains independent of Qt.
add_executable(
    ClassMngrNextApplicationClassAnalyticsRankingOrderPolicyTests
    tests/next_application_class_analytics_ranking_order_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationClassAnalyticsRankingOrderPolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationClassAnalyticsRankingOrderPolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationClassAnalyticsRankingOrderPolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationClassAnalyticsRankingOrderPolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationClassAnalyticsRankingOrderPolicyTests
    COMMAND ClassMngrNextApplicationClassAnalyticsRankingOrderPolicyTests
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesClassAnalyticsDashboardReadPort
    SOURCES
        tests/next_platform_application_services_class_analytics_dashboard_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Sql
        Qt6::Test
    OFFSCREEN
)

# Keep selected-campus ID validation and read-result semantics Qt-free.
add_executable(
    ClassMngrNextApplicationCampusDashboardSelectedCampusReadQueryTests
    tests/next_application_campus_dashboard_selected_campus_read_query_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationCampusDashboardSelectedCampusReadQueryTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationCampusDashboardSelectedCampusReadQueryTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationCampusDashboardSelectedCampusReadQueryTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationCampusDashboardSelectedCampusReadQueryTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationCampusDashboardSelectedCampusReadQueryTests
    COMMAND ClassMngrNextApplicationCampusDashboardSelectedCampusReadQueryTests
)

# Keep the Campus Dashboard save-port value contract independent of Qt.
add_executable(
    ClassMngrNextApplicationCampusDashboardCampusSavePortTests
    tests/next_application_campus_dashboard_campus_save_port_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationCampusDashboardCampusSavePortTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationCampusDashboardCampusSavePortTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationCampusDashboardCampusSavePortTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationCampusDashboardCampusSavePortTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationCampusDashboardCampusSavePortTests
    COMMAND ClassMngrNextApplicationCampusDashboardCampusSavePortTests
)

# Keep reserved Campus Dashboard save-identity rules independent of Qt.
add_executable(
    ClassMngrNextApplicationCampusDashboardCampusIdentitySavePolicyTests
    tests/next_application_campus_dashboard_campus_identity_save_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationCampusDashboardCampusIdentitySavePolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationCampusDashboardCampusIdentitySavePolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationCampusDashboardCampusIdentitySavePolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationCampusDashboardCampusIdentitySavePolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationCampusDashboardCampusIdentitySavePolicyTests
    COMMAND ClassMngrNextApplicationCampusDashboardCampusIdentitySavePolicyTests
)

# Keep roster-print class-info ID and repository-result policy independent of Qt.
add_executable(
    ClassMngrNextApplicationRosterPrintClassInfoReadQueryTests
    tests/next_application_roster_print_class_info_read_query_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationRosterPrintClassInfoReadQueryTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationRosterPrintClassInfoReadQueryTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationRosterPrintClassInfoReadQueryTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationRosterPrintClassInfoReadQueryTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationRosterPrintClassInfoReadQueryTests
    COMMAND ClassMngrNextApplicationRosterPrintClassInfoReadQueryTests
)

add_executable(
    ClassMngrNextApplicationRosterTemplatePrintSourceReadQueryTests
    tests/next_application_roster_template_print_source_read_query_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationRosterTemplatePrintSourceReadQueryTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationRosterTemplatePrintSourceReadQueryTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationRosterTemplatePrintSourceReadQueryTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationRosterTemplatePrintSourceReadQueryTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationRosterTemplatePrintSourceReadQueryTests
    COMMAND ClassMngrNextApplicationRosterTemplatePrintSourceReadQueryTests
)

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

# Keep transfer orchestration and port failure behavior independent of Qt.
add_executable(ClassMngrNextApplicationRosterRowTransferUseCaseTests
    tests/next_application_roster_row_transfer_use_case_tests.cpp)
target_compile_features(ClassMngrNextApplicationRosterRowTransferUseCaseTests PRIVATE cxx_std_23)
set_target_properties(ClassMngrNextApplicationRosterRowTransferUseCaseTests PROPERTIES
    AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
set_property(TARGET ClassMngrNextApplicationRosterRowTransferUseCaseTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE)
target_link_libraries(ClassMngrNextApplicationRosterRowTransferUseCaseTests
    PRIVATE ClassMngrNext::Application)
add_test(NAME ClassMngrNextApplicationRosterRowTransferUseCaseTests
    COMMAND ClassMngrNextApplicationRosterRowTransferUseCaseTests)

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

add_executable(
    ClassMngrNextApplicationSpeakingEvaluationRosterNamesReadQueryTests
    tests/next_application_speaking_evaluation_roster_names_read_query_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationSpeakingEvaluationRosterNamesReadQueryTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationSpeakingEvaluationRosterNamesReadQueryTests
    PROPERTIES
        AUTOMOC ON
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationSpeakingEvaluationRosterNamesReadQueryTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationSpeakingEvaluationRosterNamesReadQueryTests
    PRIVATE
        ClassMngrNext::Application
        Qt6::Test
)
add_test(
    NAME ClassMngrNextApplicationSpeakingEvaluationRosterNamesReadQueryTests
    COMMAND ClassMngrNextApplicationSpeakingEvaluationRosterNamesReadQueryTests
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

# Keep speaking-evaluation AI prompt construction independent of Qt.
add_executable(
    ClassMngrNextApplicationSpeakingEvaluationAiCommentPromptTests
    tests/next_application_speaking_evaluation_ai_comment_prompt_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationSpeakingEvaluationAiCommentPromptTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationSpeakingEvaluationAiCommentPromptTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationSpeakingEvaluationAiCommentPromptTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationSpeakingEvaluationAiCommentPromptTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationSpeakingEvaluationAiCommentPromptTests
    COMMAND ClassMngrNextApplicationSpeakingEvaluationAiCommentPromptTests
)

classmngr_add_qt_test(
    NAME NextPlatformSpeakingEvaluationQtTextMatchPort
    SOURCES
        tests/next_platform_speaking_evaluation_qt_text_match_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

# Keep speaking-evaluation report content policy independent of Qt.
add_executable(
    ClassMngrNextApplicationSpeakingEvaluationReportContentTests
    tests/next_application_speaking_evaluation_report_content_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationSpeakingEvaluationReportContentTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationSpeakingEvaluationReportContentTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationSpeakingEvaluationReportContentTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationSpeakingEvaluationReportContentTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationSpeakingEvaluationReportContentTests
    COMMAND ClassMngrNextApplicationSpeakingEvaluationReportContentTests
)

# Keep speaking-evaluation schedule-label grouping independent of Qt.
add_executable(
    ClassMngrNextApplicationSpeakingEvaluationScheduleLabelProjectionTests
    tests/next_application_speaking_evaluation_schedule_label_projection_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationSpeakingEvaluationScheduleLabelProjectionTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationSpeakingEvaluationScheduleLabelProjectionTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationSpeakingEvaluationScheduleLabelProjectionTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationSpeakingEvaluationScheduleLabelProjectionTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationSpeakingEvaluationScheduleLabelProjectionTests
    COMMAND ClassMngrNextApplicationSpeakingEvaluationScheduleLabelProjectionTests
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

# Keep Initial Setup's two-of-three identity threshold independent of Qt.
add_executable(
    ClassMngrNextApplicationInitialSetupTeacherIdentityValidationTests
    tests/next_application_initial_setup_teacher_identity_validation_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationInitialSetupTeacherIdentityValidationTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationInitialSetupTeacherIdentityValidationTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationInitialSetupTeacherIdentityValidationTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationInitialSetupTeacherIdentityValidationTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationInitialSetupTeacherIdentityValidationTests
    COMMAND ClassMngrNextApplicationInitialSetupTeacherIdentityValidationTests
)

# Keep the Initial Setup personal-name requirement independent of Qt.
add_executable(
    ClassMngrNextApplicationInitialSetupPersonalNameRequirementTests
    tests/next_application_initial_setup_personal_name_requirement_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationInitialSetupPersonalNameRequirementTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationInitialSetupPersonalNameRequirementTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationInitialSetupPersonalNameRequirementTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationInitialSetupPersonalNameRequirementTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationInitialSetupPersonalNameRequirementTests
    COMMAND ClassMngrNextApplicationInitialSetupPersonalNameRequirementTests
)

# Keep Initial Setup's Teacher Entry skip decision independent of Qt.
add_executable(
    ClassMngrNextApplicationInitialSetupTeacherEntrySkipPolicyTests
    tests/next_application_initial_setup_teacher_entry_skip_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationInitialSetupTeacherEntrySkipPolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationInitialSetupTeacherEntrySkipPolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationInitialSetupTeacherEntrySkipPolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationInitialSetupTeacherEntrySkipPolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationInitialSetupTeacherEntrySkipPolicyTests
    COMMAND ClassMngrNextApplicationInitialSetupTeacherEntrySkipPolicyTests
)

# Keep Initial Setup's four required Class Details facts independent of Qt.
add_executable(
    ClassMngrNextApplicationInitialSetupClassDetailsRequirementsTests
    tests/next_application_initial_setup_class_details_requirements_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationInitialSetupClassDetailsRequirementsTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationInitialSetupClassDetailsRequirementsTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationInitialSetupClassDetailsRequirementsTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationInitialSetupClassDetailsRequirementsTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationInitialSetupClassDetailsRequirementsTests
    COMMAND ClassMngrNextApplicationInitialSetupClassDetailsRequirementsTests
)

# Keep Initial Setup's Class Times row-presence requirement independent of Qt.
add_executable(
    ClassMngrNextApplicationInitialSetupClassTimesRequirementTests
    tests/next_application_initial_setup_class_times_requirement_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationInitialSetupClassTimesRequirementTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationInitialSetupClassTimesRequirementTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationInitialSetupClassTimesRequirementTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationInitialSetupClassTimesRequirementTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationInitialSetupClassTimesRequirementTests
    COMMAND ClassMngrNextApplicationInitialSetupClassTimesRequirementTests
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

# Keep asynchronous startup orchestration app-less and Qt-free.
add_executable(
    ClassMngrNextApplicationStartupUpdateCheckUseCaseTests
    tests/next_application_startup_update_check_use_case_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationStartupUpdateCheckUseCaseTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationStartupUpdateCheckUseCaseTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationStartupUpdateCheckUseCaseTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationStartupUpdateCheckUseCaseTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationStartupUpdateCheckUseCaseTests
    COMMAND ClassMngrNextApplicationStartupUpdateCheckUseCaseTests
)

# Keep repeatable manual update-check policy and completion app-less and Qt-free.
add_executable(
    ClassMngrNextApplicationManualUpdateCheckUseCaseTests
    tests/next_application_manual_update_check_use_case_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationManualUpdateCheckUseCaseTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationManualUpdateCheckUseCaseTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationManualUpdateCheckUseCaseTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationManualUpdateCheckUseCaseTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationManualUpdateCheckUseCaseTests
    COMMAND ClassMngrNextApplicationManualUpdateCheckUseCaseTests
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
        ClassMngrNext::Platform
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

add_test(
    NAME NextApplicationUpcomingBirthdayScheduleUseCaseOrdering
    COMMAND ClassMngrNextApplicationUpcomingBirthdayScheduleUseCaseTests
        sortsEveryBucketByDateNameGroupAndPosition
)
add_test(
    NAME NextApplicationUpcomingBirthdayScheduleUseCaseInvalidReferenceDate
    COMMAND ClassMngrNextApplicationUpcomingBirthdayScheduleUseCaseTests
        invalidReferenceDateReturnsEmptySchedule
)
if(WIN32)
    set_tests_properties(
        NextApplicationUpcomingBirthdayScheduleUseCaseOrdering
        NextApplicationUpcomingBirthdayScheduleUseCaseInvalidReferenceDate
        PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
endif()

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

# Shared Sub Prep package request preflight has no Qt dependency.
add_executable(
    ClassMngrNextApplicationSubPrepPackageRequestValidationTests
    tests/next_application_sub_prep_package_request_validation_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationSubPrepPackageRequestValidationTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationSubPrepPackageRequestValidationTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationSubPrepPackageRequestValidationTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationSubPrepPackageRequestValidationTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationSubPrepPackageRequestValidationTests
    COMMAND ClassMngrNextApplicationSubPrepPackageRequestValidationTests
)

# Keep Sub Prep selected-class projection independent of Qt.
add_executable(
    ClassMngrNextApplicationSubPrepSelectedClassIdsTests
    tests/next_application_sub_prep_selected_class_ids_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationSubPrepSelectedClassIdsTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationSubPrepSelectedClassIdsTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationSubPrepSelectedClassIdsTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationSubPrepSelectedClassIdsTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationSubPrepSelectedClassIdsTests
    COMMAND ClassMngrNextApplicationSubPrepSelectedClassIdsTests
)

classmngr_add_qt_test(
    NAME NextApplicationSubPrepVacationSelectionPolicy
    SOURCES
        tests/next_application_sub_prep_vacation_selection_policy_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

# Keep Sub Prep class-information selection independent of Qt.
add_executable(
    ClassMngrNextApplicationSubPrepClassInformationSelectionPolicyTests
    tests/next_application_sub_prep_class_information_selection_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationSubPrepClassInformationSelectionPolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationSubPrepClassInformationSelectionPolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationSubPrepClassInformationSelectionPolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationSubPrepClassInformationSelectionPolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationSubPrepClassInformationSelectionPolicyTests
    COMMAND ClassMngrNextApplicationSubPrepClassInformationSelectionPolicyTests
)

# Keep Sub Prep meeting-time grouping independent of Qt.
add_executable(
    ClassMngrNextApplicationSubPrepMeetingTimeGroupingPolicyTests
    tests/next_application_sub_prep_meeting_time_grouping_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationSubPrepMeetingTimeGroupingPolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationSubPrepMeetingTimeGroupingPolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationSubPrepMeetingTimeGroupingPolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationSubPrepMeetingTimeGroupingPolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationSubPrepMeetingTimeGroupingPolicyTests
    COMMAND ClassMngrNextApplicationSubPrepMeetingTimeGroupingPolicyTests
)

# Keep Sub Prep navigation-metadata precedence independent of Qt.
add_executable(
    ClassMngrNextApplicationSubPrepNavigationMetadataPolicyTests
    tests/next_application_sub_prep_navigation_metadata_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationSubPrepNavigationMetadataPolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationSubPrepNavigationMetadataPolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationSubPrepNavigationMetadataPolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationSubPrepNavigationMetadataPolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationSubPrepNavigationMetadataPolicyTests
    COMMAND ClassMngrNextApplicationSubPrepNavigationMetadataPolicyTests
)

# Keep Sub Prep navigation-order reduction independent of Qt.
add_executable(
    ClassMngrNextApplicationSubPrepNavigationScheduleOrderPolicyTests
    tests/next_application_sub_prep_navigation_schedule_order_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationSubPrepNavigationScheduleOrderPolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationSubPrepNavigationScheduleOrderPolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationSubPrepNavigationScheduleOrderPolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationSubPrepNavigationScheduleOrderPolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationSubPrepNavigationScheduleOrderPolicyTests
    COMMAND ClassMngrNextApplicationSubPrepNavigationScheduleOrderPolicyTests
)

# Keep Sub Prep visible-class ordering independent of Qt.
add_executable(
    ClassMngrNextApplicationSubPrepVisibleClassOrderingPolicyTests
    tests/next_application_sub_prep_visible_class_ordering_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationSubPrepVisibleClassOrderingPolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationSubPrepVisibleClassOrderingPolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationSubPrepVisibleClassOrderingPolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationSubPrepVisibleClassOrderingPolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationSubPrepVisibleClassOrderingPolicyTests
    COMMAND ClassMngrNextApplicationSubPrepVisibleClassOrderingPolicyTests
)

# Keep Sub Prep print grouping and ordering independent of Qt.
add_executable(
    ClassMngrNextApplicationSubPrepPrintOrderPolicyTests
    tests/next_application_sub_prep_print_order_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationSubPrepPrintOrderPolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationSubPrepPrintOrderPolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationSubPrepPrintOrderPolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationSubPrepPrintOrderPolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationSubPrepPrintOrderPolicyTests
    COMMAND ClassMngrNextApplicationSubPrepPrintOrderPolicyTests
)

# Keep Class Tab tuple comparison independent of Qt and feature formatting.
add_executable(
    ClassMngrNextApplicationClassTabNavigationOrderingPolicyTests
    tests/next_application_class_tab_navigation_ordering_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationClassTabNavigationOrderingPolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationClassTabNavigationOrderingPolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationClassTabNavigationOrderingPolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationClassTabNavigationOrderingPolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationClassTabNavigationOrderingPolicyTests
    COMMAND ClassMngrNextApplicationClassTabNavigationOrderingPolicyTests
)

# Keep Testing Classes list ordering independent of Qt.
add_executable(
    ClassMngrNextApplicationTestingClassesOrderingPolicyTests
    tests/next_application_testing_classes_ordering_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationTestingClassesOrderingPolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationTestingClassesOrderingPolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationTestingClassesOrderingPolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationTestingClassesOrderingPolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationTestingClassesOrderingPolicyTests
    COMMAND ClassMngrNextApplicationTestingClassesOrderingPolicyTests
)

# Preferred-name selection precedence remains independent of Qt.
add_executable(
    ClassMngrNextApplicationPreferredNameSelectionPolicyTests
    tests/next_application_preferred_name_selection_policy_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationPreferredNameSelectionPolicyTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationPreferredNameSelectionPolicyTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationPreferredNameSelectionPolicyTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationPreferredNameSelectionPolicyTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationPreferredNameSelectionPolicyTests
    COMMAND ClassMngrNextApplicationPreferredNameSelectionPolicyTests
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
    NAME NextApplicationSelectedClassSubtitleBatchReadQuery
    SOURCES
        tests/next_application_selected_class_subtitle_batch_read_query_tests.cpp
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
    NAME NextScheduleEditorDialogParity
    SOURCES
        tests/schedule_editor_dialog_parity_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
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
    NAME NextApplicationRosterColumnNamesBatchReadQuery
    SOURCES
        tests/next_application_roster_column_names_batch_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationRosterAvailabilityBatchReadQuery
    SOURCES
        tests/next_application_roster_availability_batch_read_query_tests.cpp
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
    NAME NextApplicationClassByIdReadQuery
    SOURCES
        tests/next_application_class_by_id_read_query_tests.cpp
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
    NAME NextPlatformApplicationServicesClassByIdReadPort
    SOURCES
        tests/next_platform_application_services_class_by_id_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
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
    NAME NextApplicationMyClassesClassInformationBatchReadQuery
    SOURCES
        tests/next_application_my_classes_class_information_batch_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationMyClassesClassSummaryBatchReadQuery
    SOURCES
        tests/next_application_my_classes_class_summary_batch_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationMyClassesTeacherProfileBatchReadQuery
    SOURCES
        tests/next_application_my_classes_teacher_profile_batch_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationMyClassesStudentCountBatchReadQuery
    SOURCES
        tests/next_application_my_classes_student_count_batch_read_query_tests.cpp
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
    NAME NextApplicationClassTransferApplyUseCase
    SOURCES tests/next_application_class_transfer_apply_use_case_tests.cpp
    LIBRARIES ClassMngrNext::Application Qt6::Core Qt6::Test
)

classmngr_add_qt_test(
    NAME ClassTransferApplyPackageAdapter
    SOURCES tests/class_transfer_apply_package_adapter_tests.cpp
    LIBRARIES Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesClassTransferApplyPort
    SOURCES tests/next_platform_application_services_class_transfer_apply_port_tests.cpp
    LIBRARIES Qt6::Test Qt6::Widgets
    OFFSCREEN
)

# Keep class-transfer preview orchestration independent of Qt and feature services.
add_executable(
    ClassMngrNextApplicationClassTransferPreviewQueryTests
    tests/next_application_class_transfer_preview_query_tests.cpp
)
target_compile_features(
    ClassMngrNextApplicationClassTransferPreviewQueryTests
    PRIVATE
        cxx_std_23
)
set_target_properties(
    ClassMngrNextApplicationClassTransferPreviewQueryTests
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)
set_property(
    TARGET ClassMngrNextApplicationClassTransferPreviewQueryTests
    PROPERTY CLASSMNGR_STANDALONE_CPP_TEST TRUE
)
target_link_libraries(
    ClassMngrNextApplicationClassTransferPreviewQueryTests
    PRIVATE
        ClassMngrNext::Application
)
add_test(
    NAME ClassMngrNextApplicationClassTransferPreviewQueryTests
    COMMAND ClassMngrNextApplicationClassTransferPreviewQueryTests
)

classmngr_add_qt_test(
    NAME NextApplicationClassTransferExportQuery
    SOURCES tests/next_application_class_transfer_export_query_tests.cpp
    LIBRARIES ClassMngrNext::Application Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformClassTransferExportSource
    SOURCES tests/next_platform_class_transfer_export_source_tests.cpp
    LIBRARIES Qt6::Test Qt6::Widgets
    OFFSCREEN
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
    NAME NextApplicationTeacherDisplayNameBatchReadQuery
    SOURCES
        tests/next_application_teacher_display_name_batch_read_query_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationTeacherImportLatestSourceDateReadQuery
    SOURCES
        tests/next_application_teacher_import_latest_source_date_read_query_tests.cpp
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
    NAME NextApplicationNativeEnglishTeacherBirthdayDirectoryReadQuery
    SOURCES
        tests/next_application_native_english_teacher_birthday_directory_read_query_tests.cpp
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

classmngr_add_qt_test(
    NAME NextApplicationGsTeamBirthdayDirectoryReadQuery
    SOURCES
        tests/next_application_gs_team_birthday_directory_read_query_tests.cpp
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
    NAME NextPlatformApplicationServicesSubPrepClassDetailsPort
    SOURCES
        tests/next_platform_application_services_sub_prep_class_details_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesSubPrepScheduleSummaryPort
    SOURCES
        tests/next_platform_application_services_sub_prep_schedule_summary_port_tests.cpp
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
    NAME NextPlatformRosterWindowSave
    SOURCES
        tests/next_platform_roster_window_save_tests.cpp
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
    NAME NextPlatformApplicationServicesSpeakingEvaluationRosterNamesReadPort
    SOURCES
        tests/next_platform_application_services_speaking_evaluation_roster_names_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesRosterColumnNamesBatchReadPort
    SOURCES
        tests/next_platform_application_services_roster_column_names_batch_read_port_tests.cpp
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
    NAME NextPlatformApplicationServicesMyClassesClassInformationBatchReadPort
    SOURCES
        tests/next_platform_application_services_my_classes_class_information_batch_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesMyClassesClassSummaryBatchReadPort
    SOURCES
        tests/next_platform_application_services_my_classes_class_summary_batch_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesMyClassesTeacherProfileBatchReadPort
    SOURCES
        tests/next_platform_application_services_my_classes_teacher_profile_batch_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesMyClassesStudentCountBatchReadPort
    SOURCES
        tests/next_platform_application_services_my_classes_student_count_batch_read_port_tests.cpp
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
    NAME NextPlatformApplicationServicesSelectedClassSubtitleBatchReadPort
    SOURCES
        tests/next_platform_application_services_selected_class_subtitle_batch_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesRosterPrintClassInfoReadPort
    SOURCES
        tests/next_platform_application_services_roster_print_class_info_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesRosterTemplatePrintSourceReadPort
    SOURCES
        tests/next_platform_application_services_roster_template_print_source_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Sql
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
        tests/teacher_choices_cursor_error_fixture.h
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
    NAME NextPlatformApplicationServicesTeacherDisplayNameBatchReadPort
    SOURCES
        tests/next_platform_application_services_teacher_display_name_batch_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesTeacherImportLatestSourceDateReadPort
    SOURCES
        tests/next_platform_application_services_teacher_import_latest_source_date_read_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesTeacherImportApplyPort
    SOURCES
        tests/next_platform_application_services_teacher_import_apply_port_tests.cpp
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
    NAME NextPlatformApplicationServicesNativeEnglishTeacherBirthdayDirectoryReadPort
    SOURCES
        tests/next_platform_application_services_native_english_teacher_birthday_directory_read_port_tests.cpp
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
    NAME NextPlatformApplicationServicesGsTeamBirthdayDirectoryReadPort
    SOURCES
        tests/next_platform_application_services_gs_team_birthday_directory_read_port_tests.cpp
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
    NAME ClassCoTeacherAssignmentPersistenceParity
    SOURCES
        tests/class_co_teacher_assignment_persistence_parity_tests.cpp
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

add_test(
    NAME NextFeatureClassCoTeacherWideTeacherIdentity
    COMMAND ClassMngrNextFeatureClassCoTeacherPageTests
        wideTeacherIdentitySurvivesSelectionSaveAndReadback
)
set_tests_properties(
    NextFeatureClassCoTeacherWideTeacherIdentity
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
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
    NAME NextPlatformCampusDashboardSelectedCampusReadAdapter
    SOURCES
        tests/next_platform_campus_dashboard_selected_campus_read_adapter_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextPlatformCampusDashboardCampusRepositoryAdapter
    SOURCES
        tests/next_platform_campus_dashboard_campus_repository_adapter_tests.cpp
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
    NAME NextApplicationClassCreateUseCase
    SOURCES
        tests/next_application_class_create_use_case_tests.cpp
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
    NAME NextApplicationClassDeleteUseCase
    SOURCES
        tests/next_application_class_delete_use_case_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationTeacherDeleteUseCase
    SOURCES
        tests/next_application_teacher_delete_use_case_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationTeacherCreateUseCase
    SOURCES
        tests/next_application_teacher_create_use_case_tests.cpp
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
    NAME NextApplicationScheduleTestingLayoutClearUseCase
    SOURCES
        tests/next_application_schedule_testing_layout_clear_use_case_tests.cpp
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
    NAME NextPlatformApplicationServicesClassCreatePort
    SOURCES
        tests/next_platform_application_services_class_create_port_tests.cpp
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
    NAME NextPlatformApplicationServicesClassDeletePort
    SOURCES
        tests/next_platform_application_services_class_delete_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesTeacherDeletePort
    SOURCES
        tests/next_platform_application_services_teacher_delete_port_tests.cpp
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME NextPlatformApplicationServicesTeacherCreatePort
    SOURCES
        tests/next_platform_application_services_teacher_create_port_tests.cpp
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
    NAME NextPlatformApplicationServicesScheduleTestingLayoutClearPort
    SOURCES
        tests/next_platform_application_services_schedule_testing_layout_clear_port_tests.cpp
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
        CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG="${CLASSMNGR_NEXT_RESOURCE_CATALOG}"
        CLASSMNGR_NEXT_TEST_RESOURCE_ROOT="${CLASSMNGR_NEXT_RESOURCE_ROOT}"
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    DEPENDENCIES
        ClassMngrNextResourcePackage
)

classmngr_add_qt_test(
    NAME NextPlatformDocumentExportResourcePort
    SOURCES
        tests/next_platform_document_export_resource_port_tests.cpp
    COMPILE_DEFINITIONS
        CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG="${CLASSMNGR_NEXT_RESOURCE_CATALOG}"
        CLASSMNGR_NEXT_TEST_RESOURCE_ROOT="${CLASSMNGR_NEXT_RESOURCE_ROOT}"
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Test
    DEPENDENCIES
        ClassMngrNextResourcePackage
)

classmngr_add_qt_test(
    NAME NextPlatformQtPdfSessionRelease
    SOURCES
        tests/next_platform_qtpdf_session_release_tests.cpp
    COMPILE_DEFINITIONS
        CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG="${CLASSMNGR_NEXT_RESOURCE_CATALOG}"
        CLASSMNGR_NEXT_TEST_RESOURCE_ROOT="${CLASSMNGR_NEXT_RESOURCE_ROOT}"
    LIBRARIES
        ClassMngrNext::Platform
        Qt6::Pdf
        Qt6::Test
    DEPENDENCIES
        ClassMngrNextResourcePackage
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
