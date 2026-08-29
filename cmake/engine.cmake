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
)

target_link_libraries(ClassMngrEngine
    PUBLIC
        ClassMngrCommonBuildSettings
)

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
endif()
