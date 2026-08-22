include_guard(GLOBAL)

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
        Qt6::Concurrent
        Qt6::Core
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

function(classmngr_add_production_objects target directory)
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/${directory}/*.cpp"
        "${PROJECT_SOURCE_DIR}/${directory}/*.h"
        "${PROJECT_SOURCE_DIR}/${directory}/*.ui"
    )

    add_library("${target}" OBJECT ${sources})
    target_link_libraries("${target}"
        PRIVATE
            ClassMngrBuildSettings
    )
    set_target_properties("${target}"
        PROPERTIES
            CXX_EXTENSIONS OFF
            POSITION_INDEPENDENT_CODE ON
    )
endfunction()

classmngr_add_production_objects(ClassMngrCore src/core)
classmngr_add_production_objects(ClassMngrData src/data)
classmngr_add_production_objects(ClassMngrDomain src/domain)
classmngr_add_production_objects(ClassMngrUiShared src/ui)
classmngr_add_production_objects(ClassMngrFeatures src/features)
classmngr_add_production_objects(ClassMngrAppServices src/app)

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
    )

    target_link_libraries(ClassMngr
        PRIVATE
            Psapi
    )
endif()

target_sources(ClassMngr
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/main.cpp"
)

target_link_libraries(ClassMngr
    PRIVATE
        ClassMngrRuntime
)
