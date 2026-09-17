include_guard(GLOBAL)

qt_add_executable(ClassMngrNext
    "${PROJECT_SOURCE_DIR}/src/next/main.cpp"
)

target_compile_features(ClassMngrNext
    PRIVATE
        cxx_std_23
)

set_target_properties(ClassMngrNext
    PROPERTIES
        CXX_EXTENSIONS OFF
)

target_compile_definitions(ClassMngrNext
    PRIVATE
        CLASSMNGR_NEXT_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(ClassMngrNext
    PRIVATE
        Qt6::Core
)

if(BUILD_TESTING)
    add_test(
        NAME ClassMngrNextLaunch
        COMMAND ClassMngrNext
    )

    set_tests_properties(ClassMngrNextLaunch
        PROPERTIES
            PASS_REGULAR_EXPRESSION
                "ClassMngrNext launch version=${PROJECT_VERSION}"
    )
endif()
