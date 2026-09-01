include_guard(GLOBAL)

# Declare a QtTest executable with the project-wide target and CTest defaults.
# Feature-specific resources and platform libraries remain next to the call site.
function(classmngr_add_qt_test)
    set(options
        OFFSCREEN
        MANUAL_FINALIZATION
    )
    set(one_value_arguments
        NAME
        WORKING_DIRECTORY
    )
    set(multi_value_arguments
        SOURCES
        LIBRARIES
        INCLUDE_DIRECTORIES
        COMPILE_DEFINITIONS
        DEPENDENCIES
        ENVIRONMENT
        ENVIRONMENT_MODIFICATION
    )

    cmake_parse_arguments(PARSE_ARGV 0 CLASSMNGR_TEST
        "${options}"
        "${one_value_arguments}"
        "${multi_value_arguments}"
    )

    if(CLASSMNGR_TEST_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "classmngr_add_qt_test received unknown arguments: "
            "${CLASSMNGR_TEST_UNPARSED_ARGUMENTS}"
        )
    endif()

    if(NOT CLASSMNGR_TEST_NAME)
        message(FATAL_ERROR "classmngr_add_qt_test requires NAME")
    endif()

    if(NOT CLASSMNGR_TEST_SOURCES)
        message(FATAL_ERROR
            "classmngr_add_qt_test(${CLASSMNGR_TEST_NAME}) requires SOURCES"
        )
    endif()

    if(NOT CLASSMNGR_TEST_LIBRARIES)
        message(FATAL_ERROR
            "classmngr_add_qt_test(${CLASSMNGR_TEST_NAME}) requires LIBRARIES"
        )
    endif()

    set(target "ClassMngr${CLASSMNGR_TEST_NAME}Tests")
    if(TARGET "${target}")
        message(FATAL_ERROR "Test target ${target} already exists")
    endif()

    set(test_sources ${CLASSMNGR_TEST_SOURCES})
    list(FILTER test_sources EXCLUDE REGEX "^${PROJECT_SOURCE_DIR}/src/")
    list(FILTER test_sources EXCLUDE REGEX "^src/")

    set(qt_executable_options)
    if(CLASSMNGR_TEST_MANUAL_FINALIZATION)
        list(APPEND qt_executable_options MANUAL_FINALIZATION)
    endif()

    qt_add_executable("${target}"
        ${qt_executable_options}
        ${test_sources}
    )

    target_compile_features("${target}"
        PRIVATE
            cxx_std_23
    )

    target_include_directories("${target}"
        PRIVATE
            "${PROJECT_SOURCE_DIR}/src"
            ${CLASSMNGR_TEST_INCLUDE_DIRECTORIES}
    )

    if(CLASSMNGR_TEST_COMPILE_DEFINITIONS)
        target_compile_definitions("${target}"
            PRIVATE
                ${CLASSMNGR_TEST_COMPILE_DEFINITIONS}
        )
    endif()

    target_link_libraries("${target}"
        PRIVATE
            ClassMngrRuntime
            ${CLASSMNGR_TEST_LIBRARIES}
    )

    if(CLASSMNGR_TEST_DEPENDENCIES)
        add_dependencies("${target}"
            ${CLASSMNGR_TEST_DEPENDENCIES}
        )
    endif()

    add_test(
        NAME "${target}"
        COMMAND "${target}"
    )

    set(test_environment ${CLASSMNGR_TEST_ENVIRONMENT})
    if(CLASSMNGR_TEST_OFFSCREEN)
        list(APPEND test_environment "QT_QPA_PLATFORM=offscreen")
    endif()

    if(test_environment)
        set_tests_properties("${target}"
            PROPERTIES
                ENVIRONMENT "${test_environment}"
        )
    endif()

    if(CLASSMNGR_TEST_ENVIRONMENT_MODIFICATION)
        set_tests_properties("${target}"
            PROPERTIES
                ENVIRONMENT_MODIFICATION
                    "${CLASSMNGR_TEST_ENVIRONMENT_MODIFICATION}"
        )
    endif()

    if(CLASSMNGR_TEST_WORKING_DIRECTORY)
        set_tests_properties("${target}"
            PROPERTIES
                WORKING_DIRECTORY "${CLASSMNGR_TEST_WORKING_DIRECTORY}"
        )
    endif()
endfunction()

# Enforce shared runtime ownership for every test, including specialized tests
# whose Qt resource setup still requires direct target declarations.
function(classmngr_finalize_test_targets)
    get_property(test_names DIRECTORY "${PROJECT_SOURCE_DIR}" PROPERTY TESTS)

    foreach(test_name IN LISTS test_names)
        if(NOT TARGET "${test_name}")
            continue()
        endif()

        get_target_property(target_qml_module_uri
            "${test_name}" QT_QML_MODULE_URI
        )
        if(NOT target_qml_module_uri)
            set_property(TARGET "${test_name}"
                PROPERTY QT_QML_MODULE_NO_IMPORT_SCAN TRUE
            )
        endif()

        get_target_property(target_sources "${test_name}" SOURCES)
        set(test_sources)
        set(has_production_overrides FALSE)
        foreach(source IN LISTS target_sources)
            if(source MATCHES "^${PROJECT_SOURCE_DIR}/src/" OR source MATCHES "^src/")
                continue()
            endif()

            if(source MATCHES "test_stubs[.]cpp$")
                set(has_production_overrides TRUE)
            elseif(EXISTS "${PROJECT_SOURCE_DIR}/${source}")
                file(READ "${PROJECT_SOURCE_DIR}/${source}" test_source_content)
                if(test_source_content MATCHES
                    "(ApplicationServices|DataService|ResourcePackManager|RosterTemplatePrintService|ThemeService)::"
                    )
                    set(has_production_overrides TRUE)
                endif()
            endif()

            list(APPEND test_sources "${source}")
        endforeach()

        set_property(TARGET "${test_name}" PROPERTY SOURCES "${test_sources}")

        # Apple's current linker no longer honors -multiply_defined suppress.
        # Use the flat-namespace shared runtime for the few tests that provide
        # focused production overrides; their executable definitions can then
        # interpose the shared definitions without recompiling production.
        if(APPLE AND has_production_overrides)
            get_target_property(test_libraries "${test_name}" LINK_LIBRARIES)
            if(test_libraries)
                list(REMOVE_ITEM test_libraries ClassMngrRuntime)
                set_property(
                    TARGET "${test_name}"
                    PROPERTY LINK_LIBRARIES "${test_libraries}"
                )
            endif()
            target_link_libraries("${test_name}" PRIVATE ClassMngrTestRuntime)
        else()
            target_link_libraries("${test_name}" PRIVATE ClassMngrRuntime)

            if(has_production_overrides AND MSVC)
                target_link_options("${test_name}" PRIVATE /FORCE:MULTIPLE)
            elseif(
                has_production_overrides
                AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"
                )
                target_link_options("${test_name}" PRIVATE
                    LINKER:--allow-multiple-definition
                )
            endif()
        endif()
    endforeach()
endfunction()
