cmake_minimum_required(VERSION 3.25)
if(POLICY CMP0218)
    cmake_policy(SET CMP0218 NEW)
endif()

if(POLICY CMP0219)
    cmake_policy(SET CMP0219 NEW)
endif()

project(ClassMngr
    VERSION 0.19.0
    LANGUAGES CXX
)

if(WIN32)
    enable_language(RC)
endif()

include(CTest)
include(GNUInstallDirs)

# Product selection must happen before any Qt package discovery.  The native
# Windows configuration is intentionally able to run on a machine with no Qt
# installation at all.
option(CLASSMNGR_BUILD_QT_DESKTOP
    "Build the retained Qt desktop product and its tests"
    ON
)

option(CLASSMNGR_BUILD_WINDOWS_NATIVE
    "Build the provisional Windows-native desktop shell"
    OFF
)

option(CLASSMNGR_BUILD_WINDOWS_WINUI
    "Build the Windows WinUI 3 desktop application"
    OFF
)

set(_classmngr_windows_qt_transition_default OFF)
if(WIN32 AND CLASSMNGR_BUILD_QT_DESKTOP)
    set(_classmngr_windows_qt_transition_default ON)
endif()
option(CLASSMNGR_BUILD_WINDOWS_QT_TRANSITION
    "Build the retained Windows Qt product during native-port transition"
    ${_classmngr_windows_qt_transition_default}
)

if(CLASSMNGR_BUILD_WINDOWS_NATIVE AND NOT WIN32)
    message(FATAL_ERROR
        "CLASSMNGR_BUILD_WINDOWS_NATIVE is supported only on Windows."
    )
endif()

if(CLASSMNGR_BUILD_WINDOWS_WINUI AND NOT WIN32)
    message(FATAL_ERROR
        "CLASSMNGR_BUILD_WINDOWS_WINUI is supported only on Windows."
    )
endif()

if(CLASSMNGR_BUILD_WINDOWS_QT_TRANSITION
   AND (NOT WIN32 OR NOT CLASSMNGR_BUILD_QT_DESKTOP))
    message(FATAL_ERROR
        "CLASSMNGR_BUILD_WINDOWS_QT_TRANSITION requires the Windows Qt desktop target."
    )
endif()

if(NOT CLASSMNGR_BUILD_QT_DESKTOP
   AND NOT CLASSMNGR_BUILD_WINDOWS_NATIVE
   AND NOT CLASSMNGR_BUILD_WINDOWS_WINUI)
    message(FATAL_ERROR
        "At least one product must be enabled: Qt desktop, Windows native, or Windows WinUI."
    )
endif()

option(CLASSMNGR_ENABLE_WINDOWS_QT_VISUAL_CAPTURE_TESTS
    "Build the interactive Windows Qt Phase 0 visual-capture test target"
    OFF
)

if(CLASSMNGR_ENABLE_WINDOWS_QT_VISUAL_CAPTURE_TESTS
   AND NOT CLASSMNGR_BUILD_QT_DESKTOP)
    message(FATAL_ERROR
        "The Phase 0 visual capture target requires CLASSMNGR_BUILD_QT_DESKTOP."
    )
endif()

add_compile_options(
    $<$<CXX_COMPILER_ID:MSVC>:/FS>
    $<$<CXX_COMPILER_ID:MSVC>:/MP>
)

# A CMake build directory can retain individual Qt component locations after
# Qt6_DIR has been changed.  Qt permits components from the same public version
# to be found this way, but its private ABI is not compatible between separate
# Qt builds.  Clear only component cache entries that are outside the selected
# Qt installation before Qt resolves its dependencies.
if(CLASSMNGR_BUILD_QT_DESKTOP
   AND DEFINED Qt6_DIR AND EXISTS "${Qt6_DIR}/Qt6Config.cmake")
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

option(CLASSMNGR_PREFER_QT_ZLIB_FALLBACK
    "Prefer Qt's bundled zlib fallback on Windows before searching for system zlib"
    OFF
)

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

set(CLASSMNGR_GIT_REVISION "unknown")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        OUTPUT_VARIABLE CLASSMNGR_GIT_REVISION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
if(CLASSMNGR_GIT_REVISION STREQUAL "")
    set(CLASSMNGR_GIT_REVISION "unknown")
endif()
string(TIMESTAMP CLASSMNGR_BUILD_TIMESTAMP "%Y-%m-%dT%H:%M:%SZ" UTC)

configure_file(
    src/core/build_info.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/core/build_info.h
    @ONLY
)

message(STATUS "ClassMngr Qt desktop: ${CLASSMNGR_BUILD_QT_DESKTOP}")
message(STATUS "ClassMngr Windows native: ${CLASSMNGR_BUILD_WINDOWS_NATIVE}")
message(STATUS "ClassMngr Windows WinUI: ${CLASSMNGR_BUILD_WINDOWS_WINUI}")
message(STATUS "ClassMngr Windows Qt transition: ${CLASSMNGR_BUILD_WINDOWS_QT_TRANSITION}")

# The portable engine is deliberately configured before either presentation
# target.  Its CMake file does not call find_package(Qt6) or any Qt helper.
include(cmake/engine.cmake)

if(CLASSMNGR_BUILD_QT_DESKTOP)
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

    qt_standard_project_setup()

    if(WIN32)
        set(CLASSMNGR_QT_DESKTOP_TARGET ClassMngrWindowsQtTransition)
    else()
        set(CLASSMNGR_QT_DESKTOP_TARGET ClassMngr)
    endif()

    qt_add_executable(${CLASSMNGR_QT_DESKTOP_TARGET}
        WIN32
        MACOSX_BUNDLE
    )

    set_target_properties(${CLASSMNGR_QT_DESKTOP_TARGET}
        PROPERTIES
            OUTPUT_NAME ClassMngr
    )

    include(cmake/platform/deployment.cmake)

    if(WIN32)
        configure_file(
            resources/windows/ClassMngr.rc.in
            ${CMAKE_CURRENT_BINARY_DIR}/generated/ClassMngr.rc
            @ONLY
        )
        target_sources(${CLASSMNGR_QT_DESKTOP_TARGET}
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
endif()

if(CLASSMNGR_BUILD_WINDOWS_NATIVE)
    include(cmake/platform/windows_native.cmake)
    include(cmake/native_resources.cmake)

    set(CLASSMNGR_NATIVE_TARGET ClassMngrWindowsNative)
    set(CLASSMNGR_NATIVE_MANIFEST
        "${CMAKE_CURRENT_BINARY_DIR}/generated/ClassMngrNative.manifest"
    )
    set(CLASSMNGR_NATIVE_RC
        "${CMAKE_CURRENT_BINARY_DIR}/generated/ClassMngrNative.rc"
    )
    configure_file(
        resources/windows/ClassMngrNative.manifest.in
        "${CLASSMNGR_NATIVE_MANIFEST}"
        @ONLY
    )
    configure_file(
        resources/windows/ClassMngrNative.rc.in
        "${CLASSMNGR_NATIVE_RC}"
        @ONLY
    )
    add_executable(${CLASSMNGR_NATIVE_TARGET} WIN32
        src/platform/windows/native/main.cpp
        src/platform/windows/native/native_application.cpp
        src/platform/windows/native/native_application.h
        src/platform/windows/native/native_identity.h
        ${CLASSMNGR_NATIVE_RC}
    )
    set_target_properties(${CLASSMNGR_NATIVE_TARGET}
        PROPERTIES
            OUTPUT_NAME ClassMngrNative
    )
    # The native RC file carries the reviewed manifest.  Suppress MSVC's
    # automatic manifest so the linker does not embed a second RT_MANIFEST
    # resource with the same name.
    target_link_options(${CLASSMNGR_NATIVE_TARGET}
        PRIVATE
            /MANIFEST:NO
    )
    target_link_libraries(${CLASSMNGR_NATIVE_TARGET}
        PRIVATE
            ClassMngrEngine
            ClassMngrCommonBuildSettings
            ClassMngrWindowsNativeSdk
    )

    add_dependencies(${CLASSMNGR_NATIVE_TARGET}
        ClassMngrWindowsNativeSdkCapabilityTests
        ClassMngrNativeResourceManifest
    )

    if(BUILD_TESTING)
        add_test(
            NAME ClassMngrWindowsNativeShellSmokeTests
            COMMAND ${CLASSMNGR_NATIVE_TARGET} --phase1-smoke-test
        )
        add_test(
            NAME ClassMngrWindowsNativeManifestTests
            COMMAND ${CLASSMNGR_NATIVE_TARGET} --phase1-manifest-test
        )
        add_test(
            NAME ClassMngrWindowsNativeResourceManifestTests
            COMMAND "${CMAKE_COMMAND}"
                -DCLASSMNGR_PROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
                -DCLASSMNGR_NATIVE_RESOURCE_MANIFEST=${CLASSMNGR_NATIVE_RESOURCE_MANIFEST}
                -P "${PROJECT_SOURCE_DIR}/cmake/verify_native_resource_manifest.cmake"
        )
    endif()

    install(TARGETS ${CLASSMNGR_NATIVE_TARGET}
        RUNTIME DESTINATION .
    )
    install(FILES
        ${CLASSMNGR_NATIVE_RESOURCE_MANIFEST}
        DESTINATION resources
    )
    install(FILES
        licenses/fonts/inter/LICENSE.txt
        DESTINATION licenses/fonts/inter
    )
    install(FILES
        licenses/fonts/pretendard/LICENSE.txt
        DESTINATION licenses/fonts/pretendard
    )
    install(FILES
        licenses/fonts/just-another-hand/LICENSE.txt
        DESTINATION licenses/fonts/just-another-hand
    )
endif()

if(CLASSMNGR_BUILD_WINDOWS_WINUI)
    include(cmake/platform/windows_winui.cmake)
    include(cmake/native_resources.cmake)

    classmngr_add_windows_winui_target()
endif()
