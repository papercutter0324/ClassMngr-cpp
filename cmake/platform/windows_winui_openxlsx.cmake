include_guard(GLOBAL)

if(NOT WIN32)
    message(FATAL_ERROR
        "windows_winui_openxlsx.cmake may only be included for a Windows target."
    )
endif()

set(CLASSMNGR_OPENXLSX_SOURCE_DIRECTORY
    "${PROJECT_SOURCE_DIR}/third_party/openxlsx/source"
)
set(CLASSMNGR_OPENXLSX_PUGIXML_SOURCE_DIRECTORY
    "${PROJECT_SOURCE_DIR}/third_party/openxlsx/dependencies/pugixml"
)
set(CLASSMNGR_OPENXLSX_MINIZ_SOURCE_DIRECTORY
    "${PROJECT_SOURCE_DIR}/third_party/openxlsx/dependencies/miniz"
)
set(CLASSMNGR_OPENXLSX_NOWIDE_SOURCE_DIRECTORY
    "${PROJECT_SOURCE_DIR}/third_party/openxlsx/dependencies/nowide"
)

foreach(_classmngr_openxlsx_required_path IN ITEMS
        CLASSMNGR_OPENXLSX_SOURCE_DIRECTORY
        CLASSMNGR_OPENXLSX_PUGIXML_SOURCE_DIRECTORY
        CLASSMNGR_OPENXLSX_MINIZ_SOURCE_DIRECTORY
        CLASSMNGR_OPENXLSX_NOWIDE_SOURCE_DIRECTORY)
    if(NOT EXISTS "${${_classmngr_openxlsx_required_path}}")
        message(FATAL_ERROR
            "OpenXLSX dependency source is missing: "
            "${${_classmngr_openxlsx_required_path}}. "
            "Initialize the repository submodules before configuring the WinUI target."
        )
    endif()
endforeach()

set(CLASSMNGR_OPENXLSX_OUTPUT_DIRECTORY
    "${CMAKE_CURRENT_BINARY_DIR}/openxlsx"
    CACHE PATH
    "Configuration-specific static OpenXLSX artifacts for the WinUI target"
)
set(CLASSMNGR_OPENXLSX_PROPERTY_SHEET
    "${CMAKE_CURRENT_BINARY_DIR}/generated/winui/ClassMngrOpenXLSX.props"
)

# OpenXLSX's normal dependency manager is intentionally disabled here.  The
# three targets below are added from pinned submodules before OpenXLSX is
# configured, which lets its local-target checks succeed without FetchContent,
# package-manager discovery, or network access.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build OpenXLSX statically for WinUI" FORCE)
set(FETCH_DEPS_AUTO OFF CACHE BOOL "Do not fetch OpenXLSX dependencies" FORCE)
set(FORCE_FETCH_ALL OFF CACHE BOOL "Do not force-fetch OpenXLSX dependencies" FORCE)
set(OPENXLSX_LOCAL_PACKAGES_ONLY ON CACHE BOOL "Use only repository-local OpenXLSX dependencies" FORCE)
set(OPENXLSX_CREATE_DOCS OFF CACHE BOOL "Do not build OpenXLSX documentation" FORCE)
set(OPENXLSX_BUILD_SAMPLES OFF CACHE BOOL "Do not build OpenXLSX samples" FORCE)
set(OPENXLSX_INSTALL_SAMPLE_SOURCES OFF CACHE BOOL "Do not install OpenXLSX samples" FORCE)
set(OPENXLSX_BUILD_TESTS OFF CACHE BOOL "Do not build OpenXLSX tests" FORCE)
set(OPENXLSX_BUILD_BENCHMARKS OFF CACHE BOOL "Do not build OpenXLSX benchmarks" FORCE)
set(OPENXLSX_ENABLE_LIBZIP OFF CACHE BOOL "Use miniz for OpenXLSX zip support" FORCE)
set(OPENXLSX_NOWIDE_STANDALONE ON CACHE BOOL "Use the pinned standalone nowide target" FORCE)
set(OPENXLSX_ENABLE_LTO OFF CACHE BOOL "Keep the dependency build deterministic" FORCE)
set(NOWIDE_INSTALL OFF CACHE BOOL "Do not install the nowide submodule" FORCE)
set(PUGIXML_BUILD_TESTS OFF CACHE BOOL "Do not build PugiXML tests" FORCE)
set(PUGIXML_USE_POSTFIX OFF CACHE BOOL "Keep dependency library names stable" FORCE)
set(MINIZ_STANDALONE_PROJECT OFF CACHE BOOL "Build miniz as an embedded dependency" FORCE)
set(INSTALL_PROJECT OFF CACHE BOOL "Do not install embedded miniz" FORCE)

set(_classmngr_openxlsx_saved_build_testing "${BUILD_TESTING}")
set(BUILD_TESTING OFF)

add_subdirectory(
    "${CLASSMNGR_OPENXLSX_NOWIDE_SOURCE_DIRECTORY}"
    "${CMAKE_CURRENT_BINARY_DIR}/openxlsx-dependencies/nowide"
    EXCLUDE_FROM_ALL
)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/openxlsx-dependencies/miniz")
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/miniz_export.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/openxlsx-dependencies/miniz/miniz_export.h"
    COPYONLY
)
add_library(miniz STATIC
    "${CLASSMNGR_OPENXLSX_MINIZ_SOURCE_DIRECTORY}/miniz.c"
    "${CLASSMNGR_OPENXLSX_MINIZ_SOURCE_DIRECTORY}/miniz_tdef.c"
    "${CLASSMNGR_OPENXLSX_MINIZ_SOURCE_DIRECTORY}/miniz_tinfl.c"
    "${CLASSMNGR_OPENXLSX_MINIZ_SOURCE_DIRECTORY}/miniz_zip.c"
)
set_source_files_properties(
    "${CLASSMNGR_OPENXLSX_MINIZ_SOURCE_DIRECTORY}/miniz.c"
    "${CLASSMNGR_OPENXLSX_MINIZ_SOURCE_DIRECTORY}/miniz_tdef.c"
    "${CLASSMNGR_OPENXLSX_MINIZ_SOURCE_DIRECTORY}/miniz_tinfl.c"
    "${CLASSMNGR_OPENXLSX_MINIZ_SOURCE_DIRECTORY}/miniz_zip.c"
    PROPERTIES
        LANGUAGE CXX
        COMPILE_OPTIONS "/TC"
)
target_include_directories(miniz PUBLIC
    $<BUILD_INTERFACE:${CLASSMNGR_OPENXLSX_MINIZ_SOURCE_DIRECTORY}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/openxlsx-dependencies/miniz>
    $<INSTALL_INTERFACE:include>
)
target_compile_definitions(miniz PUBLIC MINIZ_STATIC_DEFINE)
add_subdirectory(
    "${CLASSMNGR_OPENXLSX_PUGIXML_SOURCE_DIRECTORY}"
    "${CMAKE_CURRENT_BINARY_DIR}/openxlsx-dependencies/pugixml"
    EXCLUDE_FROM_ALL
)
add_subdirectory(
    "${CLASSMNGR_OPENXLSX_SOURCE_DIRECTORY}"
    "${CMAKE_CURRENT_BINARY_DIR}/openxlsx-source"
    EXCLUDE_FROM_ALL
)

set(BUILD_TESTING "${_classmngr_openxlsx_saved_build_testing}")
unset(_classmngr_openxlsx_saved_build_testing)

set(_classmngr_openxlsx_targets
    OpenXLSX
    nowide
    miniz
    pugixml-static
)
foreach(_classmngr_openxlsx_target IN LISTS _classmngr_openxlsx_targets)
    if(NOT TARGET "${_classmngr_openxlsx_target}")
        message(FATAL_ERROR
            "OpenXLSX dependency target was not created: ${_classmngr_openxlsx_target}"
        )
    endif()

    set_target_properties("${_classmngr_openxlsx_target}"
        PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY "${CLASSMNGR_OPENXLSX_OUTPUT_DIRECTORY}"
            LIBRARY_OUTPUT_DIRECTORY "${CLASSMNGR_OPENXLSX_OUTPUT_DIRECTORY}"
            RUNTIME_OUTPUT_DIRECTORY "${CLASSMNGR_OPENXLSX_OUTPUT_DIRECTORY}"
    )
    if(MSVC)
        set_property(TARGET "${_classmngr_openxlsx_target}"
            PROPERTY MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}"
        )
    endif()
    foreach(_classmngr_openxlsx_configuration IN ITEMS Debug Release)
        string(TOUPPER "${_classmngr_openxlsx_configuration}" _classmngr_openxlsx_configuration_upper)
        set_target_properties("${_classmngr_openxlsx_target}"
            PROPERTIES
                ARCHIVE_OUTPUT_DIRECTORY_${_classmngr_openxlsx_configuration_upper}
                    "${CLASSMNGR_OPENXLSX_OUTPUT_DIRECTORY}/${_classmngr_openxlsx_configuration}"
                LIBRARY_OUTPUT_DIRECTORY_${_classmngr_openxlsx_configuration_upper}
                    "${CLASSMNGR_OPENXLSX_OUTPUT_DIRECTORY}/${_classmngr_openxlsx_configuration}"
                RUNTIME_OUTPUT_DIRECTORY_${_classmngr_openxlsx_configuration_upper}
                    "${CLASSMNGR_OPENXLSX_OUTPUT_DIRECTORY}/${_classmngr_openxlsx_configuration}"
        )
    endforeach()
endforeach()

add_custom_target(ClassMngrOpenXLSXBuild)
add_dependencies(
    ClassMngrOpenXLSXBuild
    OpenXLSX
    nowide
    miniz
    pugixml-static
)

set(CLASSMNGR_OPENXLSX_CONSUMER_INCLUDE_DIRECTORIES
    "${CLASSMNGR_OPENXLSX_SOURCE_DIRECTORY}"
    "${CLASSMNGR_OPENXLSX_PUGIXML_SOURCE_DIRECTORY}/src"
    "${CLASSMNGR_OPENXLSX_MINIZ_SOURCE_DIRECTORY}"
    "${CLASSMNGR_OPENXLSX_NOWIDE_SOURCE_DIRECTORY}/include"
)
add_library(ClassMngrOpenXLSXConsumer INTERFACE)
target_include_directories(ClassMngrOpenXLSXConsumer INTERFACE
    ${CLASSMNGR_OPENXLSX_CONSUMER_INCLUDE_DIRECTORIES}
)
target_compile_definitions(ClassMngrOpenXLSXConsumer INTERFACE
    ENABLE_NOWIDE
    OPENXLSX_STATIC_DEFINE
)
target_link_libraries(ClassMngrOpenXLSXConsumer INTERFACE
    OpenXLSX::OpenXLSX
    pugixml::static
    miniz
    nowide::nowide
)
add_dependencies(ClassMngrOpenXLSXConsumer ClassMngrOpenXLSXBuild)

if(BUILD_TESTING)
    add_executable(ClassMngrOpenXLSXSmoke EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/tests/openxlsx_native_smoke.cpp"
    )
    target_link_libraries(ClassMngrOpenXLSXSmoke PRIVATE ClassMngrOpenXLSXConsumer)
    add_test(NAME ClassMngrOpenXLSXSmoke COMMAND ClassMngrOpenXLSXSmoke)
endif()

set(CLASSMNGR_OPENXLSX_PUGIXML_INCLUDE_DIRECTORY
    "${CLASSMNGR_OPENXLSX_PUGIXML_SOURCE_DIRECTORY}/src"
)
set(CLASSMNGR_OPENXLSX_MINIZ_INCLUDE_DIRECTORY
    "${CLASSMNGR_OPENXLSX_MINIZ_SOURCE_DIRECTORY}"
)
set(CLASSMNGR_OPENXLSX_NOWIDE_INCLUDE_DIRECTORY
    "${CLASSMNGR_OPENXLSX_NOWIDE_SOURCE_DIRECTORY}/include"
)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/winui")
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/windows_winui_openxlsx.props.in"
    "${CLASSMNGR_OPENXLSX_PROPERTY_SHEET}"
    @ONLY
)

unset(_classmngr_openxlsx_target)
unset(_classmngr_openxlsx_targets)
unset(_classmngr_openxlsx_configuration)
unset(_classmngr_openxlsx_configuration_upper)
