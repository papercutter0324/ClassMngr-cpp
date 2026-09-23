include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/production_sources.cmake")

add_library(ClassMngrBuildSettings INTERFACE)

target_compile_features(ClassMngrBuildSettings
    INTERFACE
        cxx_std_23
)

target_include_directories(ClassMngrBuildSettings
    INTERFACE
        "${PROJECT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_compile_definitions(ClassMngrBuildSettings
    INTERFACE
        CLASSMNGR_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

target_link_libraries(ClassMngrBuildSettings
    INTERFACE
        Qt6::Core
)

function(classmngr_add_production_objects)
    cmake_parse_arguments(PARSE_ARGV 0 _classmngr_production
        ""
        "TARGET"
        "SOURCES;LIBRARIES"
    )

    if(_classmngr_production_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "Unknown arguments to classmngr_add_production_objects: "
            "${_classmngr_production_UNPARSED_ARGUMENTS}"
        )
    endif()
    if(_classmngr_production_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR
            "Missing values for classmngr_add_production_objects arguments: "
            "${_classmngr_production_KEYWORDS_MISSING_VALUES}"
        )
    endif()
    if(NOT DEFINED _classmngr_production_TARGET OR
       NOT _classmngr_production_TARGET)
        message(FATAL_ERROR
            "classmngr_add_production_objects requires TARGET."
        )
    endif()
    if(NOT DEFINED _classmngr_production_SOURCES OR
       NOT _classmngr_production_SOURCES)
        message(FATAL_ERROR
            "classmngr_add_production_objects requires non-empty SOURCES."
        )
    endif()
    if(NOT DEFINED _classmngr_production_LIBRARIES)
        message(FATAL_ERROR
            "classmngr_add_production_objects requires LIBRARIES."
        )
    endif()

    foreach(_classmngr_production_source IN LISTS _classmngr_production_SOURCES)
        if(NOT EXISTS "${PROJECT_SOURCE_DIR}/${_classmngr_production_source}")
            message(FATAL_ERROR
                "Unknown production source for ${_classmngr_production_TARGET}: "
                "${_classmngr_production_source}"
            )
        endif()
    endforeach()
    foreach(_classmngr_production_library IN LISTS _classmngr_production_LIBRARIES)
        if(NOT TARGET "${_classmngr_production_library}")
            message(FATAL_ERROR
                "Unknown production library for ${_classmngr_production_TARGET}: "
                "${_classmngr_production_library}"
            )
        endif()
    endforeach()

    add_library("${_classmngr_production_TARGET}" OBJECT
        ${_classmngr_production_SOURCES}
    )
    target_link_libraries("${_classmngr_production_TARGET}"
        PRIVATE
            ClassMngrBuildSettings
            ${_classmngr_production_LIBRARIES}
    )
    set_target_properties("${_classmngr_production_TARGET}"
        PROPERTIES
            CXX_EXTENSIONS OFF
            POSITION_INDEPENDENT_CODE ON
    )
endfunction()

classmngr_add_production_objects(
    TARGET ClassMngrCore
    SOURCES ${CLASSMNGR_CORE_SOURCES}
    LIBRARIES
        Qt6::Gui
        Qt6::Network
        Qt6::Sql
        Qt6::Widgets
        ZLIB::ZLIB
)
classmngr_add_production_objects(
    TARGET ClassMngrData
    SOURCES ${CLASSMNGR_DATA_SOURCES}
    LIBRARIES
        Qt6::Gui
        Qt6::Sql
)
classmngr_add_production_objects(
    TARGET ClassMngrDomain
    SOURCES ${CLASSMNGR_DOMAIN_SOURCES}
    LIBRARIES
        Qt6::Gui
)
classmngr_add_production_objects(
    TARGET ClassMngrUiShared
    SOURCES ${CLASSMNGR_UI_SHARED_SOURCES}
    LIBRARIES
        Qt6::Gui
        Qt6::Network
        Qt6::Pdf
        Qt6::PdfWidgets
        Qt6::PrintSupport
        Qt6::Widgets
)
target_link_libraries(ClassMngrUiShared
    PRIVATE
        ClassMngrNext::Application
)
classmngr_add_production_objects(
    TARGET ClassMngrFeatures
    SOURCES ${CLASSMNGR_FEATURES_SOURCES}
    LIBRARIES
        Qt6::Concurrent
        Qt6::Gui
        Qt6::Network
        Qt6::Pdf
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickWidgets
        Qt6::Sql
        Qt6::Widgets
        ZLIB::ZLIB
)
classmngr_add_production_objects(
    TARGET ClassMngrAppServices
    SOURCES ${CLASSMNGR_APP_SERVICES_SOURCES}
    LIBRARIES
        Qt6::Gui
        Qt6::Network
        Qt6::Sql
        Qt6::Widgets
)

# Keep the complete legacy runtime module set available to the application and
# focused tests, including QuickControls2 dependencies discovered from QML.
set(_classmngr_legacy_runtime_dependencies
    Qt6::Concurrent
    Qt6::Gui
    Qt6::Network
    Qt6::Pdf
    Qt6::PdfWidgets
    Qt6::PrintSupport
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickWidgets
    Qt6::Sql
    Qt6::Widgets
    ZLIB::ZLIB
)

add_library(ClassMngrRuntime STATIC
    $<TARGET_OBJECTS:ClassMngrCore>
    $<TARGET_OBJECTS:ClassMngrData>
    $<TARGET_OBJECTS:ClassMngrDomain>
    $<TARGET_OBJECTS:ClassMngrUiShared>
    $<TARGET_OBJECTS:ClassMngrFeatures>
    $<TARGET_OBJECTS:ClassMngrAppServices>
)

target_link_libraries(ClassMngrRuntime
    PUBLIC
        ClassMngrBuildSettings
        ${_classmngr_legacy_runtime_dependencies}
)

# macOS test doubles cannot override symbols from a static archive with the
# current Apple linker. A flat-namespace shared runtime reuses the production
# object files while allowing the executable's focused test doubles to
# interpose those definitions at load time.
if(APPLE AND BUILD_TESTING)
    add_library(ClassMngrTestRuntime SHARED
        $<TARGET_OBJECTS:ClassMngrCore>
        $<TARGET_OBJECTS:ClassMngrData>
        $<TARGET_OBJECTS:ClassMngrDomain>
        $<TARGET_OBJECTS:ClassMngrUiShared>
        $<TARGET_OBJECTS:ClassMngrFeatures>
        $<TARGET_OBJECTS:ClassMngrAppServices>
    )

    target_link_libraries(ClassMngrTestRuntime
        PUBLIC
            ClassMngrBuildSettings
            ${_classmngr_legacy_runtime_dependencies}
    )

    target_link_options(ClassMngrTestRuntime
        PRIVATE
            LINKER:-flat_namespace
    )
endif()

if(WIN32)
    target_link_libraries(ClassMngrRuntime
        PUBLIC
            Bcrypt
            Crypt32
            Psapi
    )
endif()

target_sources(ClassMngr
    PRIVATE
        "${PROJECT_SOURCE_DIR}/${CLASSMNGR_MAIN_SOURCE}"
)

target_link_libraries(ClassMngr
    PRIVATE
        ClassMngrRuntime
)
