cmake_minimum_required(VERSION 3.25)
if(POLICY CMP0218)
    cmake_policy(SET CMP0218 NEW)
endif()

if(POLICY CMP0219)
    cmake_policy(SET CMP0219 NEW)
endif()

project(ClassMngr
    VERSION 0.17.0
    LANGUAGES CXX
)

if(WIN32)
    enable_language(RC)
endif()

include(CTest)
include(GNUInstallDirs)

add_compile_options(
    $<$<CXX_COMPILER_ID:MSVC>:/FS>
    $<$<CXX_COMPILER_ID:MSVC>:/MP>
)

# A CMake build directory can retain individual Qt component locations after
# Qt6_DIR has been changed.  Qt permits components from the same public version
# to be found this way, but its private ABI is not compatible between separate
# Qt builds.  Clear only component cache entries that are outside the selected
# Qt installation before Qt resolves its dependencies.
if(DEFINED Qt6_DIR AND EXISTS "${Qt6_DIR}/Qt6Config.cmake")
    file(REAL_PATH "${Qt6_DIR}/.." _classmngr_qt6_cmake_root)
    get_cmake_property(_classmngr_cache_variables CACHE_VARIABLES)

    foreach(_classmngr_cache_variable IN LISTS _classmngr_cache_variables)
        if(_classmngr_cache_variable MATCHES "^Qt6.+_DIR$"
           AND NOT _classmngr_cache_variable STREQUAL "Qt6_DIR"
           AND NOT "${${_classmngr_cache_variable}}" MATCHES "-NOTFOUND$")
            file(REAL_PATH "${${_classmngr_cache_variable}}" _classmngr_component_dir)
            cmake_path(IS_PREFIX _classmngr_qt6_cmake_root
                "${_classmngr_component_dir}" NORMALIZE
                _classmngr_component_uses_selected_qt)

            if(NOT _classmngr_component_uses_selected_qt)
                message(STATUS
                    "Discarding stale ${_classmngr_cache_variable}: "
                    "${${_classmngr_cache_variable}}")
                unset(${_classmngr_cache_variable} CACHE)
            endif()
        endif()
    endforeach()
endif()

find_package(Qt6 6.11.1 REQUIRED COMPONENTS
    Concurrent
    Core
    Gui
    Pdf
    PdfWidgets
    PrintSupport
    Qml
    Quick
    QuickControls2
    QuickWidgets
    Sql
    Widgets
    Network
    LinguistTools
)

option(CLASSMNGR_PREFER_QT_ZLIB_FALLBACK
    "Prefer Qt's bundled zlib fallback on Windows before searching for system zlib"
    OFF
)

macro(classmngr_try_qt_zlib_fallback)
    find_package(Qt6ZlibPrivate 6.11.1 QUIET)
    if(TARGET Qt6::ZlibPrivate AND NOT TARGET ZLIB::ZLIB)
        add_library(ZLIB::ZLIB ALIAS Qt6::ZlibPrivate)
        set(ZLIB_FOUND TRUE)
        message(STATUS "Using Qt bundled zlib fallback")
    endif()
endmacro()

if(WIN32 AND CLASSMNGR_PREFER_QT_ZLIB_FALLBACK)
    classmngr_try_qt_zlib_fallback()
endif()

if(NOT ZLIB_FOUND)
    find_package(ZLIB QUIET)
endif()

if(NOT ZLIB_FOUND AND WIN32)
    classmngr_try_qt_zlib_fallback()
endif()

if(NOT ZLIB_FOUND)
    find_package(ZLIB REQUIRED)
endif()

set(CLASSMNGR_UPDATE_API_URL
    "https://api.github.com/repos/papercutter0324/ClassMngr-cpp/releases?per_page=10"
    CACHE STRING
    "GitHub Releases API URL used for main-program update checks"
)
option(CLASSMNGR_UPDATE_CHECK_ON_STARTUP
    "Check GitHub Releases for updates during application startup"
    ON
)

set(CLASSMNGR_RESOURCE_PACK_MANIFEST_URL "" CACHE STRING
    "HTTPS URL for the resource-pack manifest"
)
set(CLASSMNGR_RESOURCE_PACK_SIGNATURE_URL "" CACHE STRING
    "HTTPS URL for the detached resource-pack manifest signature"
)
set(CLASSMNGR_RESOURCE_PACK_PUBLIC_KEY_PEM "" CACHE STRING
    "PEM public key for resource packs; defaults to the application update key"
)
option(CLASSMNGR_RESOURCE_PACK_REQUIRE_SIGNATURE
    "Require resource-pack manifest signature verification"
    ON
)
option(CLASSMNGR_RESOURCE_PACK_CHECK_ON_STARTUP
    "Enable the dormant resource-pack startup updater"
    OFF
)

set(CLASSMNGR_UPDATE_CHECK_ON_STARTUP_CPP false)
if(CLASSMNGR_UPDATE_CHECK_ON_STARTUP)
    set(CLASSMNGR_UPDATE_CHECK_ON_STARTUP_CPP true)
endif()

set(CLASSMNGR_RESOURCE_PACK_REQUIRE_SIGNATURE_CPP false)
if(CLASSMNGR_RESOURCE_PACK_REQUIRE_SIGNATURE)
    set(CLASSMNGR_RESOURCE_PACK_REQUIRE_SIGNATURE_CPP true)
endif()

set(CLASSMNGR_RESOURCE_PACK_CHECK_ON_STARTUP_CPP false)
if(CLASSMNGR_RESOURCE_PACK_CHECK_ON_STARTUP)
    set(CLASSMNGR_RESOURCE_PACK_CHECK_ON_STARTUP_CPP true)
endif()

configure_file(
    src/core/build_info.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/core/build_info.h
    @ONLY
)

qt_standard_project_setup()

include(cmake/platform/deployment.cmake)
qt_add_executable(ClassMngr
    WIN32
    MACOSX_BUNDLE
)

if(WIN32)
    configure_file(
        resources/windows/ClassMngr.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/generated/ClassMngr.rc
        @ONLY
    )
    target_sources(ClassMngr
        PRIVATE
            ${CMAKE_CURRENT_BINARY_DIR}/generated/ClassMngr.rc
    )
endif()

include(cmake/sources.cmake)
include(cmake/resources.cmake)
include(cmake/deployment.cmake)
include(cmake/platform/macos.cmake)
include(cmake/platform/windows.cmake)
if(BUILD_TESTING)
    find_package(Qt6 6.11.1 REQUIRED COMPONENTS
        Test
    )

    include(cmake/tests.cmake)
    include(cmake/tests/foundations.cmake)
    include(cmake/tests/data_and_imports.cmake)
    include(cmake/tests/features.cmake)
    include(cmake/tests/pages_and_output.cmake)
    include(cmake/tests/models_and_services.cmake)
    include(cmake/tests/platform_and_reports.cmake)

    classmngr_finalize_test_targets()
endif()
