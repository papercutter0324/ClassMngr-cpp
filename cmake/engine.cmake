include_guard(GLOBAL)

# This file is intentionally Qt-free.  It is included before the retained Qt
# desktop target is discovered so that a native-only configure can build the
# engine with no Qt package or Qt CMake command available.
add_library(ClassMngrCommonBuildSettings INTERFACE)

target_compile_features(ClassMngrCommonBuildSettings
    INTERFACE
        cxx_std_23
)

target_include_directories(ClassMngrCommonBuildSettings
    INTERFACE
        "${PROJECT_SOURCE_DIR}/src"
        "${PROJECT_SOURCE_DIR}/src/engine/include"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

add_library(ClassMngrEngine STATIC
    "${PROJECT_SOURCE_DIR}/src/engine/semantic_version.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/semantic_version.h"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/result.h"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/document_output.h"
    "${PROJECT_SOURCE_DIR}/src/engine/zip_archive_writer.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/zip_archive_writer.h"
    "${PROJECT_SOURCE_DIR}/src/engine/database_file_format.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/database_file_format.h"
    "${PROJECT_SOURCE_DIR}/src/engine/sqlite_database.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/sqlite_database.h"
    "${PROJECT_SOURCE_DIR}/src/engine/application_settings_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/application_settings_service.h"
    "${PROJECT_SOURCE_DIR}/src/engine/personal_details_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/personal_details_service.h"
    "${PROJECT_SOURCE_DIR}/src/engine/database_schema.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/database_schema.h"
    "${PROJECT_SOURCE_DIR}/src/engine/open_database.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/open_database.h"
    "${PROJECT_SOURCE_DIR}/src/engine/class_repository.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/class_repository.h"
    "${PROJECT_SOURCE_DIR}/src/engine/campus_record_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/campus_record.h"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/campus_record_service.h"
    "${PROJECT_SOURCE_DIR}/src/engine/testing_class_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/testing_class.h"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/testing_class_service.h"
    "${PROJECT_SOURCE_DIR}/src/engine/testing_block_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/testing_block.h"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/testing_block_service.h"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/classroom.h"
    "${PROJECT_SOURCE_DIR}/src/engine/teacher.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/teacher.h"
    "${PROJECT_SOURCE_DIR}/src/engine/class_naming.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/class_naming.h"
    "${PROJECT_SOURCE_DIR}/src/engine/student_name.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/student_name.h"
    "${PROJECT_SOURCE_DIR}/src/engine/roster_validator.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/roster.h"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/roster_validator.h"
    "${PROJECT_SOURCE_DIR}/src/engine/roster_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/roster_service.h"
    "${PROJECT_SOURCE_DIR}/src/engine/speaking_evaluation_validator.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/speaking_evaluation_validator.h"
    "${PROJECT_SOURCE_DIR}/src/engine/speaking_analytics.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/speaking_analytics.h"
    "${PROJECT_SOURCE_DIR}/src/engine/upcoming_birthday_schedule.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/upcoming_birthday_schedule.h"
    "${PROJECT_SOURCE_DIR}/src/engine/validation_result.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/validation_result.h"
    "${PROJECT_SOURCE_DIR}/src/engine/teacher_validator.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/teacher_validator.h"
    "${PROJECT_SOURCE_DIR}/src/engine/teacher_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/teacher_service.h"
    "${PROJECT_SOURCE_DIR}/src/engine/native_english_teacher_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/native_english_teacher.h"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/native_english_teacher_service.h"
    "${PROJECT_SOURCE_DIR}/src/engine/gs_team_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/gs_team_member.h"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/gs_team_service.h"
    "${PROJECT_SOURCE_DIR}/src/engine/teacher_import_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/teacher_import.h"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/teacher_import_service.h"
    "${PROJECT_SOURCE_DIR}/src/engine/intensive_slot_state_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/intensive_slot_state.h"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/intensive_slot_state_service.h"
    "${PROJECT_SOURCE_DIR}/src/engine/class_info_config.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/class_info.h"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/class_info_config.h"
    "${PROJECT_SOURCE_DIR}/src/engine/class_time_validator.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/class_time_validator.h"
    "${PROJECT_SOURCE_DIR}/src/engine/class_info_validator.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/class_info_validator.h"
    "${PROJECT_SOURCE_DIR}/src/engine/class_info_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/class_info_service.h"
    "${PROJECT_SOURCE_DIR}/src/engine/class_schedule_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/class_schedule.h"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/class_schedule_service.h"
    "${PROJECT_SOURCE_DIR}/src/engine/schedule_builder_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/schedule_builder.h"
    "${PROJECT_SOURCE_DIR}/src/engine/schedule_import_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/schedule_import.h"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/schedule_import_service.h"
    "${PROJECT_SOURCE_DIR}/src/engine/speaking_evaluation_report_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/speaking_evaluation_report_service.h"
    "${PROJECT_SOURCE_DIR}/src/engine/speaking_evaluation_persistence_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/speaking_evaluation.h"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/speaking_evaluation_persistence_service.h"
    "${PROJECT_SOURCE_DIR}/src/engine/speaking_evaluation_report_model.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/speaking_evaluation_report_model.h"
    "${PROJECT_SOURCE_DIR}/src/engine/speaking_evaluation_report_template.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/speaking_evaluation_report_template.h"
    "${PROJECT_SOURCE_DIR}/src/engine/speaking_evaluation_batch_report_policy.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/speaking_evaluation_batch_report_policy.h"
    "${PROJECT_SOURCE_DIR}/src/engine/speaking_evaluation_powerpoint_job_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/speaking_evaluation_powerpoint_job_service.h"
    "${PROJECT_SOURCE_DIR}/src/engine/speaking_evaluation_report_output_policy.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/speaking_evaluation_report_output_policy.h"
    "${PROJECT_SOURCE_DIR}/src/engine/speaking_evaluation_report_content.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/speaking_evaluation_report_content.h"
    "${PROJECT_SOURCE_DIR}/src/engine/speaking_evaluation_ai_prompt.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/speaking_evaluation_ai_prompt.h"
    "${PROJECT_SOURCE_DIR}/src/engine/schedule_report_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/schedule_report.h"
    "${PROJECT_SOURCE_DIR}/src/engine/roster_report_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/roster_report.h"
    "${PROJECT_SOURCE_DIR}/src/engine/roster_report_template.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/roster_report_template.h"
    "${PROJECT_SOURCE_DIR}/src/engine/sub_prep_pagination.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/sub_prep_pagination.h"
    "${PROJECT_SOURCE_DIR}/src/engine/sub_prep_class_information_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/sub_prep_class_information.h"
    "${PROJECT_SOURCE_DIR}/src/engine/sub_prep_document.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/sub_prep_document.h"
    "${PROJECT_SOURCE_DIR}/src/engine/sub_prep_package_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/sub_prep_package.h"
    "${PROJECT_SOURCE_DIR}/src/engine/academic_calendar.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/academic_calendar.h"
    "${PROJECT_SOURCE_DIR}/src/engine/calendar_event_rules.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/calendar_event_rules.h"
    "${PROJECT_SOURCE_DIR}/src/engine/calendar_event_validator.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/calendar_event.h"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/calendar_event_validator.h"
    "${PROJECT_SOURCE_DIR}/src/engine/calendar_event_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/calendar_event_service.h"
    "${PROJECT_SOURCE_DIR}/src/engine/class_transfer_service.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/class_transfer.h"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/class_transfer_service.h"
    "${PROJECT_SOURCE_DIR}/src/engine/document_catalog.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/include/classmngr/engine/document_catalog.h"
)

target_link_libraries(ClassMngrEngine
    PUBLIC
        ClassMngrCommonBuildSettings
)

if(WIN32)
    # winsqlite3 is the same published SQLite C API used by the portable
    # implementation and is available for both required Windows platforms.
    target_link_libraries(ClassMngrEngine
        PUBLIC
            winsqlite3
    )
else()
    find_package(SQLite3 REQUIRED)
    if(TARGET SQLite::SQLite3)
        target_link_libraries(ClassMngrEngine PUBLIC SQLite::SQLite3)
    elseif(TARGET SQLite3::SQLite3)
        target_link_libraries(ClassMngrEngine PUBLIC SQLite3::SQLite3)
    else()
        target_include_directories(ClassMngrEngine
            PUBLIC
                ${SQLite3_INCLUDE_DIRS}
        )
        target_link_libraries(ClassMngrEngine
            PUBLIC
                ${SQLite3_LIBRARIES}
        )
    endif()
endif()

# Keep the first extracted slice honest as the native build grows.  This
# audit runs at configure time, so a future Qt dependency cannot be hidden in
# the engine's transitive link interface or in a newly added engine source.
function(classmngr_assert_engine_target_is_qt_free target visited)
    list(FIND visited "${target}" _already_seen)
    if(NOT _already_seen EQUAL -1)
        return()
    endif()

    list(APPEND visited "${target}")
    get_target_property(_direct_links "${target}" LINK_LIBRARIES)
    get_target_property(_interface_links "${target}" INTERFACE_LINK_LIBRARIES)
    foreach(_link_item IN LISTS _direct_links _interface_links)
        if(_link_item MATCHES "Qt6::|Qt[0-9]|ClassMngrQt")
            message(FATAL_ERROR
                "${target} must remain Qt-free; found link item '${_link_item}'."
            )
        endif()
        if(TARGET "${_link_item}")
            classmngr_assert_engine_target_is_qt_free(
                "${_link_item}"
                "${visited}"
            )
        endif()
    endforeach()
endfunction()

classmngr_assert_engine_target_is_qt_free(ClassMngrEngine "")

file(GLOB_RECURSE CLASSMNGR_ENGINE_SOURCE_FILES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/engine/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/engine/*.h"
)
foreach(_engine_source IN LISTS CLASSMNGR_ENGINE_SOURCE_FILES)
    file(READ "${_engine_source}" _engine_source_text)
    if(_engine_source_text MATCHES
            "#[ \t]*include[ \t]*[<\"](Qt|Q[A-Z])")
        message(FATAL_ERROR
            "Qt include found in Qt-free engine source: ${_engine_source}"
        )
    endif()
    if(_engine_source_text MATCHES
            "#[ \t]*include[ \t]*[<\"](windows\\.h|winnt\\.h|windef\\.h|winbase\\.h|d2d[0-9_]*\\.h|dwrite\\.h|d3d[0-9]*\\.h|dxgi[0-9_]*\\.h|dcomp\\.h|objbase\\.h|shellapi\\.h)")
        message(FATAL_ERROR
            "Win32 include found in Qt-free engine source: ${_engine_source}"
        )
    endif()
endforeach()

set_target_properties(ClassMngrEngine
    PROPERTIES
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
)

if(BUILD_TESTING)
    add_executable(ClassMngrEngineTests
        "${PROJECT_SOURCE_DIR}/tests/engine/semantic_version_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineTests
        COMMAND ClassMngrEngineTests
    )

    add_executable(ClassMngrEngineDatabaseFileFormatTests
        "${PROJECT_SOURCE_DIR}/tests/engine/database_file_format_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineDatabaseFileFormatTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineDatabaseFileFormatTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineDatabaseFileFormatTests
        COMMAND ClassMngrEngineDatabaseFileFormatTests
    )

    add_executable(ClassMngrEngineSqliteDatabaseTests
        "${PROJECT_SOURCE_DIR}/tests/engine/sqlite_database_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineSqliteDatabaseTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineSqliteDatabaseTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineSqliteDatabaseTests
        COMMAND ClassMngrEngineSqliteDatabaseTests
    )

    add_executable(ClassMngrEngineApplicationSettingsServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/application_settings_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineApplicationSettingsServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineApplicationSettingsServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineApplicationSettingsServiceTests
        COMMAND ClassMngrEngineApplicationSettingsServiceTests
    )

    add_executable(ClassMngrEnginePersonalDetailsServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/personal_details_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEnginePersonalDetailsServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEnginePersonalDetailsServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEnginePersonalDetailsServiceTests
        COMMAND ClassMngrEnginePersonalDetailsServiceTests
    )

    add_executable(ClassMngrEngineDatabaseSchemaTests
        "${PROJECT_SOURCE_DIR}/tests/engine/database_schema_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineDatabaseSchemaTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineDatabaseSchemaTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineDatabaseSchemaTests
        COMMAND ClassMngrEngineDatabaseSchemaTests
    )

    add_executable(ClassMngrEngineClassRepositoryTests
        "${PROJECT_SOURCE_DIR}/tests/engine/class_repository_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineClassRepositoryTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineClassRepositoryTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineClassRepositoryTests
        COMMAND ClassMngrEngineClassRepositoryTests
    )

    add_executable(ClassMngrEngineCampusRecordServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/campus_record_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineCampusRecordServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineCampusRecordServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineCampusRecordServiceTests
        COMMAND ClassMngrEngineCampusRecordServiceTests
    )

    add_executable(ClassMngrEngineTestingClassServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/testing_class_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineTestingClassServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineTestingClassServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineTestingClassServiceTests
        COMMAND ClassMngrEngineTestingClassServiceTests
    )

    add_executable(ClassMngrEngineTestingBlockServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/testing_block_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineTestingBlockServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineTestingBlockServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineTestingBlockServiceTests
        COMMAND ClassMngrEngineTestingBlockServiceTests
    )

    add_executable(ClassMngrEngineTeacherServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/teacher_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineTeacherServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineTeacherServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineTeacherServiceTests
        COMMAND ClassMngrEngineTeacherServiceTests
    )

    add_executable(ClassMngrEngineClassNamingServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/class_naming_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineClassNamingServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineClassNamingServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineClassNamingServiceTests
        COMMAND ClassMngrEngineClassNamingServiceTests
    )

    add_executable(ClassMngrEngineUpcomingBirthdayScheduleTests
        "${PROJECT_SOURCE_DIR}/tests/engine/upcoming_birthday_schedule_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineUpcomingBirthdayScheduleTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineUpcomingBirthdayScheduleTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineUpcomingBirthdayScheduleTests
        COMMAND ClassMngrEngineUpcomingBirthdayScheduleTests
    )

    add_executable(ClassMngrEngineSpeakingAnalyticsTests
        "${PROJECT_SOURCE_DIR}/tests/engine/speaking_analytics_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineSpeakingAnalyticsTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineSpeakingAnalyticsTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineSpeakingAnalyticsTests
        COMMAND ClassMngrEngineSpeakingAnalyticsTests
    )

    add_executable(ClassMngrEngineRosterValidatorTests
        "${PROJECT_SOURCE_DIR}/tests/engine/roster_validator_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineRosterValidatorTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineRosterValidatorTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineRosterValidatorTests
        COMMAND ClassMngrEngineRosterValidatorTests
    )

    add_executable(ClassMngrEngineRosterServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/roster_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineRosterServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineRosterServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineRosterServiceTests
        COMMAND ClassMngrEngineRosterServiceTests
    )

    add_executable(ClassMngrEngineSpeakingEvaluationValidatorTests
        "${PROJECT_SOURCE_DIR}/tests/engine/speaking_evaluation_validator_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineSpeakingEvaluationValidatorTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineSpeakingEvaluationValidatorTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineSpeakingEvaluationValidatorTests
        COMMAND ClassMngrEngineSpeakingEvaluationValidatorTests
    )

    add_executable(ClassMngrEngineNativeEnglishTeacherServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/native_english_teacher_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineNativeEnglishTeacherServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineNativeEnglishTeacherServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineNativeEnglishTeacherServiceTests
        COMMAND ClassMngrEngineNativeEnglishTeacherServiceTests
    )

    add_executable(ClassMngrEngineGsTeamServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/gs_team_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineGsTeamServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineGsTeamServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineGsTeamServiceTests
        COMMAND ClassMngrEngineGsTeamServiceTests
    )

    add_executable(ClassMngrEngineTeacherImportServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/teacher_import_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineTeacherImportServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineTeacherImportServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineTeacherImportServiceTests
        COMMAND ClassMngrEngineTeacherImportServiceTests
    )

    add_executable(ClassMngrEngineIntensiveSlotStateServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/intensive_slot_state_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineIntensiveSlotStateServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineIntensiveSlotStateServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineIntensiveSlotStateServiceTests
        COMMAND ClassMngrEngineIntensiveSlotStateServiceTests
    )

    add_executable(ClassMngrEngineClassInfoServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/class_info_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineClassInfoServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineClassInfoServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineClassInfoServiceTests
        COMMAND ClassMngrEngineClassInfoServiceTests
    )

    add_executable(ClassMngrEngineClassScheduleServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/class_schedule_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineClassScheduleServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineClassScheduleServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineClassScheduleServiceTests
        COMMAND ClassMngrEngineClassScheduleServiceTests
    )

    add_executable(ClassMngrEngineScheduleBuilderServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/schedule_builder_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineScheduleBuilderServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineScheduleBuilderServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineScheduleBuilderServiceTests
        COMMAND ClassMngrEngineScheduleBuilderServiceTests
    )

    add_executable(ClassMngrEngineScheduleImportServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/schedule_import_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineScheduleImportServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineScheduleImportServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineScheduleImportServiceTests
        COMMAND ClassMngrEngineScheduleImportServiceTests
    )

    add_executable(ClassMngrEngineClassTransferServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/class_transfer_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineClassTransferServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineClassTransferServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineClassTransferServiceTests
        COMMAND ClassMngrEngineClassTransferServiceTests
    )

    add_executable(ClassMngrEngineSpeakingEvaluationReportServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/speaking_evaluation_report_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineSpeakingEvaluationReportServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineSpeakingEvaluationReportServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineSpeakingEvaluationReportServiceTests
        COMMAND ClassMngrEngineSpeakingEvaluationReportServiceTests
    )

    add_executable(ClassMngrEngineSpeakingEvaluationPersistenceServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/speaking_evaluation_persistence_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineSpeakingEvaluationPersistenceServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineSpeakingEvaluationPersistenceServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineSpeakingEvaluationPersistenceServiceTests
        COMMAND ClassMngrEngineSpeakingEvaluationPersistenceServiceTests
    )

    add_executable(ClassMngrEngineSpeakingEvaluationReportModelTests
        "${PROJECT_SOURCE_DIR}/tests/engine/speaking_evaluation_report_model_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineSpeakingEvaluationReportModelTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineSpeakingEvaluationReportModelTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineSpeakingEvaluationReportModelTests
        COMMAND ClassMngrEngineSpeakingEvaluationReportModelTests
    )

    add_executable(ClassMngrEngineSpeakingEvaluationReportTemplateTests
        "${PROJECT_SOURCE_DIR}/tests/engine/speaking_evaluation_report_template_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineSpeakingEvaluationReportTemplateTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineSpeakingEvaluationReportTemplateTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineSpeakingEvaluationReportTemplateTests
        COMMAND ClassMngrEngineSpeakingEvaluationReportTemplateTests
    )

    add_executable(ClassMngrEngineSpeakingEvaluationBatchReportPolicyTests
        "${PROJECT_SOURCE_DIR}/tests/engine/speaking_evaluation_batch_report_policy_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineSpeakingEvaluationBatchReportPolicyTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineSpeakingEvaluationBatchReportPolicyTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineSpeakingEvaluationBatchReportPolicyTests
        COMMAND ClassMngrEngineSpeakingEvaluationBatchReportPolicyTests
    )

    add_executable(ClassMngrEngineSpeakingEvaluationPowerPointJobServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/speaking_evaluation_powerpoint_job_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineSpeakingEvaluationPowerPointJobServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineSpeakingEvaluationPowerPointJobServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineSpeakingEvaluationPowerPointJobServiceTests
        COMMAND ClassMngrEngineSpeakingEvaluationPowerPointJobServiceTests
    )

    add_executable(ClassMngrEngineSpeakingEvaluationReportOutputPolicyTests
        "${PROJECT_SOURCE_DIR}/tests/engine/speaking_evaluation_report_output_policy_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineSpeakingEvaluationReportOutputPolicyTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineSpeakingEvaluationReportOutputPolicyTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineSpeakingEvaluationReportOutputPolicyTests
        COMMAND ClassMngrEngineSpeakingEvaluationReportOutputPolicyTests
    )

    add_executable(ClassMngrEngineSpeakingEvaluationReportContentTests
        "${PROJECT_SOURCE_DIR}/tests/engine/speaking_evaluation_report_content_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineSpeakingEvaluationReportContentTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineSpeakingEvaluationReportContentTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineSpeakingEvaluationReportContentTests
        COMMAND ClassMngrEngineSpeakingEvaluationReportContentTests
    )

    add_executable(ClassMngrEngineSpeakingEvaluationAiPromptTests
        "${PROJECT_SOURCE_DIR}/tests/engine/speaking_evaluation_ai_prompt_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineSpeakingEvaluationAiPromptTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineSpeakingEvaluationAiPromptTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineSpeakingEvaluationAiPromptTests
        COMMAND ClassMngrEngineSpeakingEvaluationAiPromptTests
    )

    add_executable(ClassMngrEngineScheduleReportServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/schedule_report_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineScheduleReportServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineScheduleReportServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineScheduleReportServiceTests
        COMMAND ClassMngrEngineScheduleReportServiceTests
    )

    add_executable(ClassMngrEngineRosterReportServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/roster_report_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineRosterReportServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineRosterReportServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineRosterReportServiceTests
        COMMAND ClassMngrEngineRosterReportServiceTests
    )

    add_executable(ClassMngrEngineRosterReportTemplateTests
        "${PROJECT_SOURCE_DIR}/tests/engine/roster_report_template_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineRosterReportTemplateTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineRosterReportTemplateTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineRosterReportTemplateTests
        COMMAND ClassMngrEngineRosterReportTemplateTests
    )

    add_executable(ClassMngrEngineSubPrepPaginationTests
        "${PROJECT_SOURCE_DIR}/tests/engine/sub_prep_pagination_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineSubPrepPaginationTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineSubPrepPaginationTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineSubPrepPaginationTests
        COMMAND ClassMngrEngineSubPrepPaginationTests
    )

    add_executable(ClassMngrEngineSubPrepClassInformationServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/sub_prep_class_information_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineSubPrepClassInformationServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineSubPrepClassInformationServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineSubPrepClassInformationServiceTests
        COMMAND ClassMngrEngineSubPrepClassInformationServiceTests
    )

    add_executable(ClassMngrEngineSubPrepDocumentTests
        "${PROJECT_SOURCE_DIR}/tests/engine/sub_prep_document_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineSubPrepDocumentTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineSubPrepDocumentTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineSubPrepDocumentTests
        COMMAND ClassMngrEngineSubPrepDocumentTests
    )

    add_executable(ClassMngrEngineSubPrepPackageServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/sub_prep_package_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineSubPrepPackageServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineSubPrepPackageServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineSubPrepPackageServiceTests
        COMMAND ClassMngrEngineSubPrepPackageServiceTests
    )

    add_executable(ClassMngrEngineAcademicCalendarTests
        "${PROJECT_SOURCE_DIR}/tests/engine/academic_calendar_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineAcademicCalendarTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineAcademicCalendarTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineAcademicCalendarTests
        COMMAND ClassMngrEngineAcademicCalendarTests
    )

    add_executable(ClassMngrEngineCalendarEventRulesTests
        "${PROJECT_SOURCE_DIR}/tests/engine/calendar_event_rules_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineCalendarEventRulesTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineCalendarEventRulesTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineCalendarEventRulesTests
        COMMAND ClassMngrEngineCalendarEventRulesTests
    )

    add_executable(ClassMngrEngineCalendarEventValidatorTests
        "${PROJECT_SOURCE_DIR}/tests/engine/calendar_event_validator_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineCalendarEventValidatorTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineCalendarEventValidatorTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineCalendarEventValidatorTests
        COMMAND ClassMngrEngineCalendarEventValidatorTests
    )

    add_executable(ClassMngrEngineCalendarEventServiceTests
        "${PROJECT_SOURCE_DIR}/tests/engine/calendar_event_service_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineCalendarEventServiceTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineCalendarEventServiceTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineCalendarEventServiceTests
        COMMAND ClassMngrEngineCalendarEventServiceTests
    )

    add_executable(ClassMngrEngineDatabaseFixtureRoundTripTests
        "${PROJECT_SOURCE_DIR}/tests/engine/database_fixture_round_trip_tests.cpp"
    )
    target_compile_definitions(ClassMngrEngineDatabaseFixtureRoundTripTests
        PRIVATE
            CLASSMNGR_DATABASE_PORT_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/database-port"
    )
    target_link_libraries(ClassMngrEngineDatabaseFixtureRoundTripTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineDatabaseFixtureRoundTripTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineDatabaseFixtureRoundTripTests
        COMMAND ClassMngrEngineDatabaseFixtureRoundTripTests
    )

    add_executable(ClassMngrEngineDocumentCatalogTests
        "${PROJECT_SOURCE_DIR}/tests/engine/document_catalog_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineDocumentCatalogTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineDocumentCatalogTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineDocumentCatalogTests
        COMMAND ClassMngrEngineDocumentCatalogTests
    )

    add_executable(ClassMngrEngineZipArchiveWriterTests
        "${PROJECT_SOURCE_DIR}/tests/engine/zip_archive_writer_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineZipArchiveWriterTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineZipArchiveWriterTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineZipArchiveWriterTests
        COMMAND ClassMngrEngineZipArchiveWriterTests
    )

    add_executable(ClassMngrEngineDocumentOutputResultTests
        "${PROJECT_SOURCE_DIR}/tests/engine/document_output_result_tests.cpp"
    )
    target_link_libraries(ClassMngrEngineDocumentOutputResultTests
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
    )
    set_target_properties(ClassMngrEngineDocumentOutputResultTests
        PROPERTIES
            CXX_EXTENSIONS OFF
    )
    add_test(
        NAME ClassMngrEngineDocumentOutputResultTests
        COMMAND ClassMngrEngineDocumentOutputResultTests
    )
endif()
