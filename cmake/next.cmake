include_guard(GLOBAL)

# Keep the next-generation layer and feature boundaries separate from the
# existing v1 targets. These interface targets intentionally have no sources.
set(_classmngr_next_layer_targets
    Domain
    Application
    Persistence
    Resources
    Platform
    UiShared
)

set(_classmngr_next_feature_targets
    Calendar
    Campus
    Classes
    Documents
    MyInfo
    Roster
    Schedule
    Setup
    SpeakingEvaluation
    SubPrep
    Teacher
)

foreach(_classmngr_next_target IN LISTS
        _classmngr_next_layer_targets
        _classmngr_next_feature_targets)
    add_library(ClassMngrNext${_classmngr_next_target} INTERFACE)
    add_library(ClassMngrNext::${_classmngr_next_target} ALIAS
        ClassMngrNext${_classmngr_next_target})
endforeach()

target_link_libraries(ClassMngrNextApplication
    INTERFACE
        ClassMngrNext::Domain
        ClassMngrNext::Persistence
)

target_link_libraries(ClassMngrNextPersistence
    INTERFACE
        ClassMngrNext::Domain
)

target_link_libraries(ClassMngrNextPlatform
    INTERFACE
        ClassMngrNext::Application
)

target_link_libraries(ClassMngrNextUiShared
    INTERFACE
        ClassMngrNext::Application
        ClassMngrNext::Resources
)

foreach(_classmngr_next_feature IN LISTS _classmngr_next_feature_targets)
    target_link_libraries(ClassMngrNext${_classmngr_next_feature}
        INTERFACE
            ClassMngrNext::Application
            ClassMngrNext::Resources
            ClassMngrNext::UiShared
    )
endforeach()

function(classmngr_next_assert_dependencies target)
    get_target_property(_classmngr_next_actual_dependencies
        "${target}"
        INTERFACE_LINK_LIBRARIES
    )
    if("${_classmngr_next_actual_dependencies}" STREQUAL
       "_classmngr_next_actual_dependencies-NOTFOUND")
        set(_classmngr_next_actual_dependencies)
    endif()

    set(_classmngr_next_expected_dependencies ${ARGN})
    if(NOT "${_classmngr_next_actual_dependencies}" STREQUAL
           "${_classmngr_next_expected_dependencies}")
        message(FATAL_ERROR
            "Unexpected dependencies for ${target}: "
            "expected [${_classmngr_next_expected_dependencies}], "
            "found [${_classmngr_next_actual_dependencies}]"
        )
    endif()
endfunction()

classmngr_next_assert_dependencies(ClassMngrNextDomain)
classmngr_next_assert_dependencies(ClassMngrNextApplication
    ClassMngrNext::Domain
    ClassMngrNext::Persistence
)
classmngr_next_assert_dependencies(ClassMngrNextPersistence
    ClassMngrNext::Domain
)
classmngr_next_assert_dependencies(ClassMngrNextResources)
classmngr_next_assert_dependencies(ClassMngrNextPlatform
    ClassMngrNext::Application
)
classmngr_next_assert_dependencies(ClassMngrNextUiShared
    ClassMngrNext::Application
    ClassMngrNext::Resources
)

foreach(_classmngr_next_feature IN LISTS _classmngr_next_feature_targets)
    classmngr_next_assert_dependencies(ClassMngrNext${_classmngr_next_feature}
        ClassMngrNext::Application
        ClassMngrNext::Resources
        ClassMngrNext::UiShared
    )
endforeach()

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
