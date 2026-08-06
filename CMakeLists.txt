cmake_minimum_required(VERSION 3.25)
if(POLICY CMP0218)
    cmake_policy(SET CMP0218 NEW)
endif()

if(POLICY CMP0219)
    cmake_policy(SET CMP0219 NEW)
endif()

project(ClassMngr
    VERSION 0.13.1
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

option(CLASSMNGR_RUN_WINDEPLOYQT "Run windeployqt after building ClassMngr on Windows." OFF)

set(CLASSMNGR_WINDOWS_ARM64 OFF)
if(WIN32 AND (CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64"
   OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$"))
    set(CLASSMNGR_WINDOWS_ARM64 ON)
endif()

set(CLASSMNGR_WINDEPLOYQT_ENV "")
if(WIN32 AND MSVC)
    set(CLASSMNGR_MSVC_INSTALL_DIR "")

    if(DEFINED ENV{VCINSTALLDIR} AND NOT "$ENV{VCINSTALLDIR}" STREQUAL "")
        file(TO_CMAKE_PATH "$ENV{VCINSTALLDIR}" CLASSMNGR_MSVC_INSTALL_DIR)
    elseif(CMAKE_CXX_COMPILER MATCHES "/VC/Tools/MSVC/.*/bin/")
        string(REGEX REPLACE "/VC/Tools/MSVC/.*$" "/VC/" CLASSMNGR_MSVC_INSTALL_DIR "${CMAKE_CXX_COMPILER}")
    endif()

    if(CLASSMNGR_MSVC_INSTALL_DIR)
        list(APPEND CLASSMNGR_WINDEPLOYQT_ENV "VCINSTALLDIR=${CLASSMNGR_MSVC_INSTALL_DIR}")
    endif()
endif()

set(CLASSMNGR_QT_DEPLOY_PREAMBLE "")
set(CLASSMNGR_QT_DEPLOY_OPTIONS "")
set(CLASSMNGR_QT_DEPLOY_POSTAMBLE "")
set(CLASSMNGR_QT_DEPLOY_EXECUTABLE "$<TARGET_FILE:ClassMngr>")
set(CLASSMNGR_QT_DEPLOY_TRANSLATION_OPTIONS "")
set(CLASSMNGR_WINDEPLOYQT_TARGET_ARGS "")

if(CLASSMNGR_WINDOWS_ARM64 AND QT_HOST_PATH)
    find_program(CLASSMNGR_QT_HOST_QTPATHS_EXECUTABLE
        NAMES qtpaths6.exe qtpaths.exe
        PATHS "${QT_HOST_PATH}/bin"
        NO_DEFAULT_PATH
        REQUIRED
    )
    find_program(CLASSMNGR_QT_HOST_LCONVERT_EXECUTABLE
        NAMES lconvert.exe
        PATHS "${QT_HOST_PATH}/bin"
        NO_DEFAULT_PATH
        REQUIRED
    )

    file(TO_CMAKE_PATH "${QT6_INSTALL_PREFIX}"
        CLASSMNGR_QT_TARGET_INSTALL_PREFIX)
    file(TO_CMAKE_PATH "${CLASSMNGR_QT_HOST_QTPATHS_EXECUTABLE}"
        CLASSMNGR_QT_HOST_QTPATHS_EXECUTABLE)
    set(CLASSMNGR_QT_TARGET_QT_CONF
        "${CMAKE_CURRENT_BINARY_DIR}/generated/ClassMngrTargetQt.conf")
    set(CLASSMNGR_QT_TARGET_QTPATHS_WRAPPER
        "${CMAKE_CURRENT_BINARY_DIR}/generated/ClassMngrTargetQtPaths.bat")
    configure_file(
        resources/windows/target-qt.conf.in
        "${CLASSMNGR_QT_TARGET_QT_CONF}"
        @ONLY
        NEWLINE_STYLE CRLF
    )
    configure_file(
        resources/windows/target-qtpaths.bat.in
        "${CLASSMNGR_QT_TARGET_QTPATHS_WRAPPER}"
        @ONLY
        NEWLINE_STYLE CRLF
    )

    list(APPEND CLASSMNGR_WINDEPLOYQT_TARGET_ARGS
        --qtpaths "${CLASSMNGR_QT_TARGET_QTPATHS_WRAPPER}")
    string(APPEND CLASSMNGR_QT_DEPLOY_PREAMBLE
        "set(__QT_DEPLOY_TARGET_QT_PATHS_PATH [[${CLASSMNGR_QT_TARGET_QTPATHS_WRAPPER}]])\n"
    )
    string(APPEND CLASSMNGR_QT_DEPLOY_TRANSLATION_OPTIONS
        "    LCONVERT [[${CLASSMNGR_QT_HOST_LCONVERT_EXECUTABLE}]]\n"
    )
endif()

if(WIN32 AND CLASSMNGR_MSVC_INSTALL_DIR)
    string(APPEND CLASSMNGR_QT_DEPLOY_PREAMBLE
        "set(ENV{VCINSTALLDIR} [[${CLASSMNGR_MSVC_INSTALL_DIR}]])\n"
    )
endif()

if(UNIX AND NOT APPLE)
    string(APPEND CLASSMNGR_QT_DEPLOY_PREAMBLE
        "find_program(CLASSMNGR_PATCHELF_EXECUTABLE patchelf)\n"
        "if(NOT CLASSMNGR_PATCHELF_EXECUTABLE)\n"
        "    message(FATAL_ERROR \"patchelf is required to deploy ClassMngr on Linux. Install patchelf and rerun cmake --install.\")\n"
        "endif()\n"
        "set(__QT_DEPLOY_USE_PATCHELF ON)\n"
        "set(__QT_DEPLOY_PATCHELF_EXECUTABLE patchelf)\n"
    )
    string(APPEND CLASSMNGR_QT_DEPLOY_OPTIONS
        "    POST_INCLUDE_REGEXES [[.*/libQt6.*[.]so([.].*)?$]]\n"
    )
endif()

if(APPLE)
    set(CLASSMNGR_QT_DEPLOY_EXECUTABLE "ClassMngr.app")
    string(APPEND CLASSMNGR_QT_DEPLOY_PREAMBLE [=[
set(CLASSMNGR_BAD_SQL_DRIVERS
    libqsqlmimer.dylib
    libqsqlodbc.dylib
    libqsqlpsql.dylib
)
set(CLASSMNGR_STAGED_SQL_DRIVERS "")
set(CLASSMNGR_QT_SQL_DRIVER_DIR "${__QT_DEPLOY_QT_INSTALL_PREFIX}/${__QT_DEPLOY_QT_INSTALL_PLUGINS}/sqldrivers")
set(CLASSMNGR_SQL_DRIVER_STAGING_DIR "${CMAKE_CURRENT_LIST_DIR}/classmngr-deploy-sqldrivers")
file(MAKE_DIRECTORY "${CLASSMNGR_SQL_DRIVER_STAGING_DIR}")
foreach(driver IN LISTS CLASSMNGR_BAD_SQL_DRIVERS)
    if(EXISTS "${CLASSMNGR_SQL_DRIVER_STAGING_DIR}/${driver}" AND NOT EXISTS "${CLASSMNGR_QT_SQL_DRIVER_DIR}/${driver}")
        file(RENAME "${CLASSMNGR_SQL_DRIVER_STAGING_DIR}/${driver}" "${CLASSMNGR_QT_SQL_DRIVER_DIR}/${driver}")
    endif()
endforeach()
foreach(driver IN LISTS CLASSMNGR_BAD_SQL_DRIVERS)
    if(EXISTS "${CLASSMNGR_QT_SQL_DRIVER_DIR}/${driver}")
        file(RENAME "${CLASSMNGR_QT_SQL_DRIVER_DIR}/${driver}" "${CLASSMNGR_SQL_DRIVER_STAGING_DIR}/${driver}")
        list(APPEND CLASSMNGR_STAGED_SQL_DRIVERS "${driver}")
    endif()
endforeach()
set(CLASSMNGR_APP_SQL_DRIVER_DIR "${QT_DEPLOY_PREFIX}/ClassMngr.app/Contents/PlugIns/sqldrivers")
foreach(driver IN LISTS CLASSMNGR_BAD_SQL_DRIVERS)
    if(EXISTS "${CLASSMNGR_APP_SQL_DRIVER_DIR}/${driver}")
        file(REMOVE "${CLASSMNGR_APP_SQL_DRIVER_DIR}/${driver}")
    endif()
endforeach()
]=])
    string(APPEND CLASSMNGR_QT_DEPLOY_OPTIONS
        "    NO_APP_STORE_COMPLIANCE\n"
    )
    string(APPEND CLASSMNGR_QT_DEPLOY_POSTAMBLE [=[
foreach(driver IN LISTS CLASSMNGR_BAD_SQL_DRIVERS)
    if(EXISTS "${CLASSMNGR_APP_SQL_DRIVER_DIR}/${driver}")
        file(REMOVE "${CLASSMNGR_APP_SQL_DRIVER_DIR}/${driver}")
    endif()
endforeach()
foreach(driver IN LISTS CLASSMNGR_STAGED_SQL_DRIVERS)
    if(EXISTS "${CLASSMNGR_SQL_DRIVER_STAGING_DIR}/${driver}")
        file(RENAME "${CLASSMNGR_SQL_DRIVER_STAGING_DIR}/${driver}" "${CLASSMNGR_QT_SQL_DRIVER_DIR}/${driver}")
    endif()
endforeach()
file(REMOVE_RECURSE "${CLASSMNGR_SQL_DRIVER_STAGING_DIR}")
]=])
endif()

if(WIN32)
    set(CMAKE_INSTALL_BINDIR ".")
    string(APPEND CLASSMNGR_QT_DEPLOY_PREAMBLE
        "set(QT_DEPLOY_BIN_DIR .)\n"
    )
    string(APPEND CLASSMNGR_QT_DEPLOY_OPTIONS
        "    NO_TRANSLATIONS\n"
        "    EXCLUDE_PLUGIN_TYPES qmltooling\n"
        "    EXCLUDE_PLUGINS qsqlibase qsqlmimer qsqloci qsqlodbc qsqlpsql\n"
    )
    string(APPEND CLASSMNGR_QT_DEPLOY_POSTAMBLE
        "qt_deploy_translations(\n"
        "    LOCALES en ko\n"
        "${CLASSMNGR_QT_DEPLOY_TRANSLATION_OPTIONS}"
        ")\n"
    )
endif()

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

file(GLOB_RECURSE CLASSMNGR_CAMPUS_MAP_IMAGES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/resources/assets/campuses/*.png"
    "${PROJECT_SOURCE_DIR}/resources/assets/campuses/*.jpg"
    "${PROJECT_SOURCE_DIR}/resources/assets/campuses/*.jpeg"
)

file(GLOB_RECURSE CLASSMNGR_EMBEDDED_PACK_FILES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/resources/assets/campuses/*"
    "${PROJECT_SOURCE_DIR}/resources/assets/documents/*"
    "${PROJECT_SOURCE_DIR}/resources/assets/files/*"
    "${PROJECT_SOURCE_DIR}/resources/assets/templates/*"
    "${PROJECT_SOURCE_DIR}/resources/assets/roster-designs/*"
)

file(GLOB CLASSMNGR_FILE_DIALOG_ICONS CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/resources/assets/icons/file_dialog/*.svg"
)

list(FILTER CLASSMNGR_EMBEDDED_PACK_FILES EXCLUDE REGEX "/~\\$[^/]+$")
list(
    FILTER CLASSMNGR_EMBEDDED_PACK_FILES
    EXCLUDE REGEX "/templates/speaking-eval/sources/"
)

qt_add_resources(ClassMngr app_resources
    BIG_RESOURCES
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/fonts/Inter.ttc
        resources/assets/fonts/JustAnotherHand-Regular.ttf
        resources/assets/fonts/PretendardVariable.ttf
        resources/assets/icons/app_icon.ico
        resources/assets/icons/check.png
        resources/assets/icons/combo_arrow_dark.svg
        resources/assets/icons/combo_arrow_light.svg
        resources/assets/icons/spin_up_dark.svg
        resources/assets/icons/spin_up_light.svg
        resources/assets/icons/radio_checked.png
        resources/assets/icons/icon_256x256.png
        ${CLASSMNGR_FILE_DIALOG_ICONS}
        resources/assets/images/dyb.png
        resources/assets/splash/splash.png
        resources/assets/styles/dark.qss
        resources/assets/styles/light.qss
        ${CLASSMNGR_EMBEDDED_PACK_FILES}
)

if(APPLE)
    set(CLASSMNGR_MACOS_ICON resources/assets/icons/app_icon.icns)

    set_source_files_properties(
        ${CLASSMNGR_MACOS_ICON}
        PROPERTIES
            MACOSX_PACKAGE_LOCATION Resources
    )

    target_sources(ClassMngr
        PRIVATE
            ${CLASSMNGR_MACOS_ICON}
    )

    set_target_properties(ClassMngr
        PROPERTIES
            MACOSX_BUNDLE_ICON_FILE app_icon.icns
    )
endif()

set_source_files_properties(
    src/features/calendar/ui/qml/EventCalendar.qml
    PROPERTIES
        QT_RESOURCE_ALIAS EventCalendar.qml
)

set_source_files_properties(
    src/features/calendar/ui/qml/MonthGridDelegate.qml
    PROPERTIES
        QT_RESOURCE_ALIAS MonthGridDelegate.qml
)

qt_add_qml_module(ClassMngr
    URI ClassMngr.Calendar
    VERSION 1.0
    OUTPUT_DIRECTORY
        ${CMAKE_CURRENT_BINARY_DIR}/qml/ClassMngr/Calendar
    RESOURCE_PREFIX
        /qt/qml
    QML_FILES
        src/features/calendar/ui/qml/EventCalendar.qml
        src/features/calendar/ui/qml/MonthGridDelegate.qml
)

qt_add_translations(
    TARGETS ClassMngr
    TS_FILES
        resources/assets/translations/ClassMngr_en_AU.ts
        resources/assets/translations/ClassMngr_en_CA.ts
        resources/assets/translations/ClassMngr_en_GB.ts
        resources/assets/translations/ClassMngr_en_US.ts
        resources/assets/translations/ClassMngr_ko_KR.ts
)

target_compile_features(ClassMngr
    PRIVATE
        cxx_std_23
)

set_target_properties(ClassMngr PROPERTIES
    CXX_EXTENSIONS OFF
)

target_include_directories(ClassMngr
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_compile_definitions(ClassMngr
    PRIVATE
        CLASSMNGR_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

target_link_libraries(ClassMngr
    PRIVATE
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

if(WIN32)
    target_link_libraries(ClassMngr
        PRIVATE
            Bcrypt
            Crypt32
    )
endif()

if(WIN32 AND CLASSMNGR_RUN_WINDEPLOYQT)
    add_custom_command(TARGET ClassMngr POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E env
            ${CLASSMNGR_WINDEPLOYQT_ENV}
            "$<TARGET_FILE:Qt6::windeployqt>"
            --qmldir "${PROJECT_SOURCE_DIR}/src/features/calendar/ui/qml"
            --skip-plugin-types qmltooling
            --exclude-plugins qsqlibase,qsqlmimer,qsqloci,qsqlodbc,qsqlpsql
            --translations en,ko
            ${CLASSMNGR_WINDEPLOYQT_TARGET_ARGS}
            --dir "$<TARGET_FILE_DIR:ClassMngr>"
            "$<TARGET_FILE:ClassMngr>"
        COMMENT "Deploying Qt runtime dependencies"
    )
endif()

install(TARGETS ClassMngr
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
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

qt6_generate_deploy_script(
    TARGET ClassMngr
    OUTPUT_SCRIPT deploy_script
    CONTENT "
${CLASSMNGR_QT_DEPLOY_PREAMBLE}
qt_deploy_qml_imports(TARGET ClassMngr PLUGINS_FOUND plugins_found)
qt_deploy_runtime_dependencies(
    EXECUTABLE \"${CLASSMNGR_QT_DEPLOY_EXECUTABLE}\"
    ADDITIONAL_MODULES \${plugins_found}
    GENERATE_QT_CONF
${CLASSMNGR_QT_DEPLOY_OPTIONS}
)
${CLASSMNGR_QT_DEPLOY_POSTAMBLE}
"
)
install(SCRIPT ${deploy_script})

if(APPLE)
    set(CLASSMNGR_DMG_STAGING_DIR
        "${CMAKE_BINARY_DIR}/installer-stage"
    )
    set(CLASSMNGR_INSTALLER_OUTPUT_DIR
        "${PROJECT_SOURCE_DIR}/dist"
        CACHE PATH
        "Directory where the macOS disk image is written"
    )
    set(CLASSMNGR_DMG_OUTPUT
        "${CLASSMNGR_INSTALLER_OUTPUT_DIR}/ClassMngr-${PROJECT_VERSION}-macos-universal.dmg"
    )

    find_program(CLASSMNGR_HDIUTIL_EXECUTABLE hdiutil)
    find_program(CLASSMNGR_CODESIGN_EXECUTABLE codesign)

    if(CLASSMNGR_HDIUTIL_EXECUTABLE AND CLASSMNGR_CODESIGN_EXECUTABLE)
        add_custom_target(ClassMngrInstaller
            COMMAND
                "${CMAKE_COMMAND}" -E rm -rf
                "$<TARGET_BUNDLE_DIR:ClassMngr>/Contents/Frameworks"
                "$<TARGET_BUNDLE_DIR:ClassMngr>/Contents/PlugIns"
                "$<TARGET_BUNDLE_DIR:ClassMngr>/Contents/Resources/qml"
            COMMAND
                "${CMAKE_COMMAND}" -E rm -f
                "$<TARGET_BUNDLE_DIR:ClassMngr>/Contents/Resources/qt.conf"
            COMMAND
                "${CMAKE_COMMAND}" -E rm -rf
                "${CLASSMNGR_DMG_STAGING_DIR}"
            COMMAND
                "${CMAKE_COMMAND}" -E make_directory
                "${CLASSMNGR_DMG_STAGING_DIR}"
                "${CLASSMNGR_INSTALLER_OUTPUT_DIR}"
            COMMAND
                "${CMAKE_COMMAND}"
                --install "${CMAKE_BINARY_DIR}"
                --prefix "${CLASSMNGR_DMG_STAGING_DIR}"
            COMMAND
                "${CMAKE_COMMAND}" -E create_symlink
                "/Applications"
                "${CLASSMNGR_DMG_STAGING_DIR}/Applications"
            COMMAND
                "${CLASSMNGR_CODESIGN_EXECUTABLE}"
                --force
                --deep
                --sign -
                "${CLASSMNGR_DMG_STAGING_DIR}/ClassMngr.app"
            COMMAND
                "${CLASSMNGR_HDIUTIL_EXECUTABLE}" create
                -volname "ClassMngr ${PROJECT_VERSION}"
                -srcfolder "${CLASSMNGR_DMG_STAGING_DIR}"
                -ov
                -format UDZO
                "${CLASSMNGR_DMG_OUTPUT}"
            DEPENDS ClassMngr
            BYPRODUCTS "${CLASSMNGR_DMG_OUTPUT}"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            COMMENT "Building ClassMngr macOS universal disk image"
            VERBATIM
        )
    else()
        add_custom_target(ClassMngrInstaller
            COMMAND
                "${CMAKE_COMMAND}" -E echo
                "hdiutil and codesign are required. Build the macOS installer on macOS with the standard developer tools installed."
            COMMAND
                "${CMAKE_COMMAND}" -E false
            COMMENT "hdiutil and codesign are required to build ClassMngrInstaller"
            VERBATIM
        )
    endif()
endif()

if(WIN32)
    if(CMAKE_SIZEOF_VOID_P LESS 8)
        message(FATAL_ERROR
            "ClassMngr supports only 64-bit Windows targets (x64 or ARM64)."
        )
    endif()

    if(CLASSMNGR_WINDOWS_ARM64)
        set(CLASSMNGR_INSTALLER_ARCH "arm64")
    else()
        set(CLASSMNGR_INSTALLER_ARCH "x64")
    endif()

    set(CLASSMNGR_INSTALLER_SOURCE_DIR
        "${CMAKE_BINARY_DIR}/installer-stage/ClassMngr-windows-${CLASSMNGR_INSTALLER_ARCH}"
    )
    set(CLASSMNGR_INSTALLER_OUTPUT_DIR
        "${PROJECT_SOURCE_DIR}/dist"
        CACHE PATH
        "Directory where the Windows installer executable is written"
    )
    set(CLASSMNGR_INSTALLER_SCRIPT
        "${PROJECT_SOURCE_DIR}/scripts/installer/ClassMngr.iss"
    )

    find_program(CLASSMNGR_ISCC_EXECUTABLE
        NAMES ISCC.exe iscc.exe iscc
        PATHS
            "C:/Program Files/Inno Setup 7"
            "C:/Program Files (x86)/Inno Setup 7"
            "C:/Program Files (x86)/Inno Setup 6"
            "C:/Program Files/Inno Setup 6"
    )

    set(CLASSMNGR_INSTALLER_SIGN_TOOL "" CACHE STRING
        "Optional Inno Setup SignTool name used to sign ClassMngr and Setup"
    )

    if(CLASSMNGR_ISCC_EXECUTABLE)
        add_custom_target(ClassMngrInstaller
            COMMAND
                "${CMAKE_COMMAND}" -E rm -rf
                "${CLASSMNGR_INSTALLER_SOURCE_DIR}"
            COMMAND
                "${CMAKE_COMMAND}"
                --install "${CMAKE_BINARY_DIR}"
                --config "$<CONFIG>"
                --prefix "${CLASSMNGR_INSTALLER_SOURCE_DIR}"
            COMMAND
                "${CMAKE_COMMAND}" -E env
                "CLASSMNGR_APP_VERSION=${PROJECT_VERSION}"
                "CLASSMNGR_INSTALLER_ARCH=${CLASSMNGR_INSTALLER_ARCH}"
                "CLASSMNGR_INSTALLER_SOURCE_DIR=${CLASSMNGR_INSTALLER_SOURCE_DIR}"
                "CLASSMNGR_INSTALLER_OUTPUT_DIR=${CLASSMNGR_INSTALLER_OUTPUT_DIR}"
                "CLASSMNGR_INSTALLER_SIGN_TOOL=${CLASSMNGR_INSTALLER_SIGN_TOOL}"
                "${CLASSMNGR_ISCC_EXECUTABLE}"
                "${CLASSMNGR_INSTALLER_SCRIPT}"
            DEPENDS ClassMngr
            BYPRODUCTS
                "${CLASSMNGR_INSTALLER_OUTPUT_DIR}/ClassMngr-${PROJECT_VERSION}-win-${CLASSMNGR_INSTALLER_ARCH}.exe"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            COMMENT "Building ClassMngr Windows ${CLASSMNGR_INSTALLER_ARCH} installer"
            VERBATIM
        )
    else()
        add_custom_target(ClassMngrInstaller
            COMMAND
                "${CMAKE_COMMAND}" -E echo
                "Inno Setup Compiler was not found. Install Inno Setup 7 or 6, or add ISCC.exe to PATH."
            COMMAND
                "${CMAKE_COMMAND}" -E false
            COMMENT "Inno Setup Compiler is required to build ClassMngrInstaller"
            VERBATIM
        )
    endif()
endif()

if(BUILD_TESTING)
    find_package(Qt6 6.11.1 REQUIRED COMPONENTS
        Test
    )

    qt_add_executable(ClassMngrUpdaterTests
        tests/updater_tests.cpp
        src/core/updater/github_release.cpp
        src/core/updater/update_configuration.cpp
        src/core/updater/update_downloader.cpp
        src/core/updater/update_installer.cpp
        src/core/updater/update_service.cpp
        src/core/updater/update_signature_verifier.cpp
        src/core/updater/version.cpp
        src/ui/shared/dialogs/update_dialog.cpp
    )

    target_compile_features(ClassMngrUpdaterTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrUpdaterTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_BINARY_DIR}/generated
    )

    target_link_libraries(ClassMngrUpdaterTests
        PRIVATE
            Qt6::Core
            Qt6::Network
            Qt6::Test
            Qt6::Widgets
    )

    if(WIN32)
        target_link_libraries(ClassMngrUpdaterTests
            PRIVATE
                Bcrypt
                Crypt32
        )
    endif()

    add_test(
        NAME ClassMngrUpdaterTests
        COMMAND ClassMngrUpdaterTests
    )

    qt_add_executable(ClassMngrResourcePackTests
        tests/resource_pack_tests.cpp
        src/core/resource_packs/resource_pack_manager.cpp
        src/core/resource_packs/resource_pack_manifest.cpp
        src/core/updater/version.cpp
    )

    set(CLASSMNGR_TEST_CAMPUS_PACK_PATH
        "${CMAKE_CURRENT_BINARY_DIR}/test-campuses.rcc"
    )

    qt_add_binary_resources(ClassMngrTestCampusPack
        tests/fixtures/resource_packs/campuses.qrc
        DESTINATION
            "${CLASSMNGR_TEST_CAMPUS_PACK_PATH}"
    )

    add_dependencies(
        ClassMngrResourcePackTests
        ClassMngrTestCampusPack
    )

    target_compile_features(ClassMngrResourcePackTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrResourcePackTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_compile_definitions(ClassMngrResourcePackTests
        PRIVATE
            CLASSMNGR_TEST_CAMPUS_PACK_PATH="${CLASSMNGR_TEST_CAMPUS_PACK_PATH}"
    )

    target_link_libraries(ClassMngrResourcePackTests
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    add_test(
        NAME ClassMngrResourcePackTests
        COMMAND ClassMngrResourcePackTests
    )

    qt_add_executable(ClassMngrDocumentCatalogTests
        tests/document_catalog_tests.cpp
        src/features/documents/document_catalog.cpp
        src/core/resource_packs/resource_pack_manager.cpp
        src/core/updater/version.cpp
    )

    target_compile_features(ClassMngrDocumentCatalogTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrDocumentCatalogTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrDocumentCatalogTests
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    add_test(
        NAME ClassMngrDocumentCatalogTests
        COMMAND ClassMngrDocumentCatalogTests
    )

    qt_add_executable(ClassMngrFontManagerTests
        tests/fontmanager_tests.cpp
        src/core/fontmanager.cpp
    )

    target_compile_features(ClassMngrFontManagerTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrFontManagerTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_compile_definitions(ClassMngrFontManagerTests
        PRIVATE
            CLASSMNGR_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )

    target_link_libraries(ClassMngrFontManagerTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrFontManagerTests
        COMMAND ClassMngrFontManagerTests
    )

    qt_add_executable(ClassMngrCheckboxStyleTests
        tests/checkbox_style_tests.cpp
    )

    qt_add_resources(ClassMngrCheckboxStyleTests checkbox_style_test_resources
        PREFIX "/"
        BASE "${PROJECT_SOURCE_DIR}/resources"
        FILES
            resources/assets/icons/check.png
            resources/assets/icons/combo_arrow_dark.svg
            resources/assets/icons/combo_arrow_light.svg
            resources/assets/icons/radio_checked.png
            resources/assets/icons/spin_up_dark.svg
            resources/assets/icons/spin_up_light.svg
            resources/assets/styles/dark.qss
            resources/assets/styles/light.qss
    )

    target_compile_features(ClassMngrCheckboxStyleTests
        PRIVATE
            cxx_std_23
    )

    target_link_libraries(ClassMngrCheckboxStyleTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrCheckboxStyleTests
        COMMAND ClassMngrCheckboxStyleTests
    )

    set_tests_properties(
        ClassMngrCheckboxStyleTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(ClassMngrFileDialogIconStyleTests
        tests/file_dialog_icon_style_tests.cpp
        src/ui/shared/styles/file_dialog_icon_style.cpp
    )

    qt_add_resources(ClassMngrFileDialogIconStyleTests file_dialog_icon_test_resources
        PREFIX "/"
        BASE "${PROJECT_SOURCE_DIR}/resources"
        FILES
            ${CLASSMNGR_FILE_DIALOG_ICONS}
    )

    target_compile_features(ClassMngrFileDialogIconStyleTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrFileDialogIconStyleTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrFileDialogIconStyleTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrFileDialogIconStyleTests
        COMMAND ClassMngrFileDialogIconStyleTests
    )

    set_tests_properties(
        ClassMngrFileDialogIconStyleTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(ClassMngrClassTimeRowTests
        tests/class_time_row_tests.cpp
        src/features/classes/config/class_info_config.cpp
        src/ui/shared/widgets/no_wheel_combobox.cpp
        src/ui/shared/widgets/sectioncards/class_time_row.cpp
    )

    target_compile_features(ClassMngrClassTimeRowTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrClassTimeRowTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrClassTimeRowTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrClassTimeRowTests
        COMMAND ClassMngrClassTimeRowTests
    )

    set_tests_properties(
        ClassMngrClassTimeRowTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(ClassMngrTeacherInfoSectionTests
        tests/teacher_info_section_tests.cpp
        src/core/fontmanager.cpp
        src/features/teacher/ui/teacher_model.cpp
        src/ui/shared/widgets/no_wheel_combobox.cpp
        src/ui/shared/widgets/sections/teacher_info_section.cpp
    )

    target_compile_features(ClassMngrTeacherInfoSectionTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrTeacherInfoSectionTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_compile_definitions(ClassMngrTeacherInfoSectionTests
        PRIVATE
            CLASSMNGR_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )

    target_link_libraries(ClassMngrTeacherInfoSectionTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrTeacherInfoSectionTests
        COMMAND ClassMngrTeacherInfoSectionTests
    )

    qt_add_executable(ClassMngrTeacherInfoPageTests
        tests/teacher_info_page_tests.cpp
        tests/teacher_info_page_test_stubs.cpp
        src/core/fontmanager.cpp
        src/core/utils/sidebar_node_naming.cpp
        src/features/teacher/ui/teacher_info_page.cpp
        src/ui/shared/pages/basepage.cpp
        src/ui/shared/widgets/no_wheel_combobox.cpp
        src/ui/shared/widgets/sectioncards/teacher_section_card.cpp
    )

    target_compile_features(ClassMngrTeacherInfoPageTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrTeacherInfoPageTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_compile_definitions(ClassMngrTeacherInfoPageTests
        PRIVATE
            CLASSMNGR_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )

    target_link_libraries(ClassMngrTeacherInfoPageTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Sql
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrTeacherInfoPageTests
        COMMAND ClassMngrTeacherInfoPageTests
    )

    set_tests_properties(
        ClassMngrTeacherInfoPageTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(ClassMngrIntensiveSlotStateRepositoryTests
        tests/intensive_slot_state_repository_tests.cpp
        src/data/repositories/intensive_slot_state_repository.cpp
    )

    target_compile_features(ClassMngrIntensiveSlotStateRepositoryTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrIntensiveSlotStateRepositoryTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrIntensiveSlotStateRepositoryTests
        PRIVATE
            Qt6::Core
            Qt6::Sql
            Qt6::Test
    )

    add_test(
        NAME ClassMngrIntensiveSlotStateRepositoryTests
        COMMAND ClassMngrIntensiveSlotStateRepositoryTests
    )

    qt_add_executable(ClassMngrTestingBlockRepositoryTests
        tests/testing_block_repository_tests.cpp
        src/data/database/database_transaction.cpp
        src/data/repositories/testing_block_repository.cpp
    )

    target_compile_features(ClassMngrTestingBlockRepositoryTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrTestingBlockRepositoryTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrTestingBlockRepositoryTests
        PRIVATE
            Qt6::Core
            Qt6::Sql
            Qt6::Test
    )

    add_test(
        NAME ClassMngrTestingBlockRepositoryTests
        COMMAND ClassMngrTestingBlockRepositoryTests
    )

    qt_add_executable(ClassMngrTestingClassRepositoryTests
        tests/testing_class_repository_tests.cpp
        src/data/database/database_transaction.cpp
        src/data/repositories/class_repository.cpp
        src/data/repositories/testing_class_repository.cpp
        src/domain/models/classroom.cpp
    )

    target_compile_features(ClassMngrTestingClassRepositoryTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrTestingClassRepositoryTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrTestingClassRepositoryTests
        PRIVATE
            Qt6::Core
            Qt6::Sql
            Qt6::Test
    )

    add_test(
        NAME ClassMngrTestingClassRepositoryTests
        COMMAND ClassMngrTestingClassRepositoryTests
    )

    qt_add_executable(ClassMngrCalendarEventRepositoryTests
        tests/calendar_event_repository_tests.cpp
        src/data/repositories/calendar_event_repository.cpp
    )

    target_compile_features(ClassMngrCalendarEventRepositoryTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrCalendarEventRepositoryTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrCalendarEventRepositoryTests
        PRIVATE
            Qt6::Core
            Qt6::Sql
            Qt6::Test
    )

    add_test(
        NAME ClassMngrCalendarEventRepositoryTests
        COMMAND ClassMngrCalendarEventRepositoryTests
    )

    qt_add_executable(ClassMngrDataServiceLifecycleTests
        tests/data_service_lifecycle_tests.cpp
        src/data/data_service.cpp
        src/data/database/database_schema_manager.cpp
        src/data/database/database_transaction.cpp
        src/data/repositories/calendar_event_repository.cpp
        src/data/repositories/campus_record_repository.cpp
        src/data/repositories/class_info_repository.cpp
        src/data/repositories/class_repository.cpp
        src/data/repositories/class_transfer_repository.cpp
        src/data/repositories/intensive_slot_state_repository.cpp
        src/data/repositories/testing_block_repository.cpp
        src/data/repositories/testing_class_repository.cpp
        src/data/repositories/gs_team_repository.cpp
        src/data/repositories/native_english_teacher_repository.cpp
        src/data/repositories/roster_repository.cpp
        src/data/repositories/schedule_import_repository.cpp
        src/data/repositories/settings_repository.cpp
        src/data/repositories/speaking_eval_repository.cpp
        src/data/repositories/teacher_repository.cpp
        src/data/repositories/teacher_import_repository.cpp
        src/domain/models/classroom.cpp
        src/domain/models/roster.cpp
        src/features/classes/config/class_info_config.cpp
    )

    target_compile_features(ClassMngrDataServiceLifecycleTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrDataServiceLifecycleTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrDataServiceLifecycleTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Sql
            Qt6::Test
    )

    add_test(
        NAME ClassMngrDataServiceLifecycleTests
        COMMAND ClassMngrDataServiceLifecycleTests
    )

    qt_add_executable(ClassMngrClassTransferTests
        tests/class_transfer_tests.cpp
        src/core/utils/sidebar_node_naming.cpp
        src/data/data_service.cpp
        src/data/database/database_schema_manager.cpp
        src/data/database/database_transaction.cpp
        src/data/repositories/calendar_event_repository.cpp
        src/data/repositories/campus_record_repository.cpp
        src/data/repositories/class_info_repository.cpp
        src/data/repositories/class_repository.cpp
        src/data/repositories/class_transfer_repository.cpp
        src/data/repositories/intensive_slot_state_repository.cpp
        src/data/repositories/testing_block_repository.cpp
        src/data/repositories/testing_class_repository.cpp
        src/data/repositories/gs_team_repository.cpp
        src/data/repositories/native_english_teacher_repository.cpp
        src/data/repositories/roster_repository.cpp
        src/data/repositories/schedule_import_repository.cpp
        src/data/repositories/settings_repository.cpp
        src/data/repositories/speaking_eval_repository.cpp
        src/data/repositories/teacher_repository.cpp
        src/data/repositories/teacher_import_repository.cpp
        src/domain/models/classroom.cpp
        src/domain/models/roster.cpp
        src/features/classes/config/class_info_config.cpp
        src/features/classes/services/class_transfer_json_codec.cpp
        src/features/classes/ui/class_export_dialog.cpp
        src/features/classes/ui/class_import_dialog.cpp
    )

    target_compile_features(ClassMngrClassTransferTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrClassTransferTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrClassTransferTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Sql
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrClassTransferTests
        COMMAND ClassMngrClassTransferTests
    )

    qt_add_executable(ClassMngrCalendarImportTests
        tests/calendar_import_tests.cpp
        src/features/calendar/calendar_event_campus_filter.cpp
        src/features/calendar/academic_calendar_event_parser.cpp
        src/features/calendar/calendar_workbook_reader.cpp
    )

    target_compile_features(ClassMngrCalendarImportTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrCalendarImportTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrCalendarImportTests
        PRIVATE
            Qt6::Core
            Qt6::Test
            ZLIB::ZLIB
    )

    add_test(
        NAME ClassMngrCalendarImportTests
        COMMAND ClassMngrCalendarImportTests
    )

    qt_add_executable(ClassMngrScheduleImportTests
        tests/schedule_import_tests.cpp
        src/data/database/database_schema_manager.cpp
        src/data/database/database_transaction.cpp
        src/data/repositories/class_info_repository.cpp
        src/data/repositories/class_repository.cpp
        src/data/repositories/schedule_import_repository.cpp
        src/data/repositories/teacher_repository.cpp
        src/domain/models/classroom.cpp
        src/features/calendar/calendar_workbook_reader.cpp
        src/features/classes/config/class_info_config.cpp
        src/features/schedule/import/schedule_workbook_parser.cpp
    )

    target_compile_features(ClassMngrScheduleImportTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrScheduleImportTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrScheduleImportTests
        PRIVATE
            Qt6::Core
            Qt6::Sql
            Qt6::Test
            ZLIB::ZLIB
    )

    add_test(
        NAME ClassMngrScheduleImportTests
        COMMAND ClassMngrScheduleImportTests
    )

    qt_add_executable(ClassMngrTeacherImportTests
        tests/teacher_import_tests.cpp
        src/data/database/database_schema_manager.cpp
        src/data/database/database_transaction.cpp
        src/data/repositories/gs_team_repository.cpp
        src/data/repositories/teacher_import_repository.cpp
        src/features/calendar/calendar_workbook_reader.cpp
        src/features/teacher/import/sectioned_contact_list_template.cpp
        src/features/teacher/import/teacher_import_file_validator.cpp
        src/features/teacher/import/teacher_import_template_registry.cpp
    )

    target_compile_features(ClassMngrTeacherImportTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrTeacherImportTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrTeacherImportTests
        PRIVATE
            Qt6::Core
            Qt6::Sql
            Qt6::Test
            ZLIB::ZLIB
    )

    add_test(
        NAME ClassMngrTeacherImportTests
        COMMAND ClassMngrTeacherImportTests
    )

    qt_add_executable(ClassMngrTeacherImportDialogTests
        tests/teacher_import_dialog_tests.cpp
        src/features/calendar/calendar_workbook_reader.cpp
        src/features/teacher/import/sectioned_contact_list_template.cpp
        src/features/teacher/import/teacher_import_file_validator.cpp
        src/features/teacher/import/teacher_import_template_registry.cpp
        src/features/teacher/ui/teacher_import_dialog.cpp
    )

    target_compile_features(ClassMngrTeacherImportDialogTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrTeacherImportDialogTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrTeacherImportDialogTests
        PRIVATE
            Qt6::Concurrent
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
            ZLIB::ZLIB
    )

    add_test(
        NAME ClassMngrTeacherImportDialogTests
        COMMAND ClassMngrTeacherImportDialogTests
    )

    qt_add_executable(ClassMngrStaffDirectoryPageTests
        tests/staff_directory_page_tests.cpp
        tests/staff_directory_page_test_stubs.cpp
        src/core/fontmanager.cpp
        src/features/teacher/ui/staff_directory_page.cpp
        src/ui/shared/pages/basepage.cpp
    )

    target_compile_features(ClassMngrStaffDirectoryPageTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrStaffDirectoryPageTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrStaffDirectoryPageTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Sql
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrStaffDirectoryPageTests
        COMMAND ClassMngrStaffDirectoryPageTests
    )

    set_tests_properties(
        ClassMngrStaffDirectoryPageTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(ClassMngrClassTabNavigationModelTests
        tests/class_tab_navigation_model_tests.cpp
        src/features/classes/config/class_info_config.cpp
        src/features/classes/models/class_tab_navigation_model.cpp
        src/ui/shared/widgets/sidebar/sidebar_definitions.cpp
    )

    target_compile_features(ClassMngrClassTabNavigationModelTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrClassTabNavigationModelTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrClassTabNavigationModelTests
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    add_test(
        NAME ClassMngrClassTabNavigationModelTests
        COMMAND ClassMngrClassTabNavigationModelTests
    )

    qt_add_executable(ClassMngrSubPrepClassInformationModelTests
        tests/sub_prep_class_information_model_tests.cpp
        src/core/utils/sidebar_node_naming.cpp
        src/domain/models/classroom.cpp
        src/features/classes/config/class_info_config.cpp
        src/features/sub_prep/ui/sub_prep_class_information_model.cpp
    )

    target_compile_features(ClassMngrSubPrepClassInformationModelTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSubPrepClassInformationModelTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrSubPrepClassInformationModelTests
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    add_test(
        NAME ClassMngrSubPrepClassInformationModelTests
        COMMAND ClassMngrSubPrepClassInformationModelTests
    )

    qt_add_executable(ClassMngrScheduleWidgetTests
        tests/schedule_widget_tests.cpp
        tests/schedule_widget_test_stubs.cpp
        src/domain/models/classroom.cpp
        src/features/schedule/ui/schedule_editor_dialog.h
        src/features/schedule/ui/schedule_page.cpp
        src/features/schedule/ui/schedule_page.h
        src/features/schedule/ui/schedule_print_dialog.h
        src/features/schedule/ui/schedule_settings_dialog.cpp
        src/features/schedule/ui/schedule_settings_dialog.h
        src/features/schedule/ui/schedule_widget.cpp
        src/features/schedule/ui/schedule_widget.h
        src/features/schedule/ui/testing_assignment_dialog.cpp
        src/features/schedule/ui/testing_assignment_dialog.h
        src/ui/shared/pages/basepage.cpp
        src/ui/shared/pages/basepage.h
    )

    target_compile_features(ClassMngrScheduleWidgetTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrScheduleWidgetTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrScheduleWidgetTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::PrintSupport
            Qt6::Sql
            Qt6::Widgets
            Qt6::Test
    )

    add_test(
        NAME ClassMngrScheduleWidgetTests
        COMMAND ClassMngrScheduleWidgetTests
    )

    qt_add_executable(ClassMngrTestingClassesPageTests
        tests/testing_classes_page_tests.cpp
        tests/schedule_widget_test_stubs.cpp
        src/core/utils/colorutils.cpp
        src/core/utils/sidebar_node_naming.cpp
        src/domain/models/classroom.cpp
        src/domain/models/roster.cpp
        src/features/classes/ui/testing_classes_page.cpp
        src/features/classes/ui/testing_classes_page.h
        src/features/roster/ui/roster_column_layout_controller.cpp
        src/features/roster/ui/roster_editor_widget.cpp
        src/features/roster/ui/roster_editor_widget.h
        src/features/roster/ui/roster_editor_widget_columns.cpp
        src/features/roster/ui/roster_editor_widget_students.cpp
        src/features/roster/ui/roster_editor_widget_transfer.cpp
        src/features/roster/ui/roster_header_view.cpp
        src/features/roster/ui/roster_item_delegate.cpp
        src/features/roster/ui/roster_model.cpp
        src/features/roster/ui/roster_model_columns.cpp
        src/features/roster/ui/roster_model_names.cpp
        src/features/roster/ui/roster_model_rows.cpp
        src/features/roster/ui/roster_model_validation.cpp
        src/features/roster/ui/roster_table_view.cpp
        src/features/roster/ui/roster_editor_widget_ui.cpp
        src/features/schedule/ui/schedule_editor_dialog.h
        src/features/schedule/ui/schedule_print_dialog.h
        src/ui/shared/pages/basepage.cpp
        src/ui/shared/widgets/clickable_color_preview.cpp
        src/ui/shared/widgets/no_wheel_combobox.cpp
    )

    target_compile_features(ClassMngrTestingClassesPageTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrTestingClassesPageTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrTestingClassesPageTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::PrintSupport
            Qt6::Sql
            Qt6::Widgets
            Qt6::Test
    )

    add_test(
        NAME ClassMngrTestingClassesPageTests
        COMMAND ClassMngrTestingClassesPageTests
    )

    set_tests_properties(
        ClassMngrTestingClassesPageTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(ClassMngrScheduleImportDialogTests
        tests/schedule_import_dialog_tests.cpp
        tests/schedule_widget_test_stubs.cpp
        src/core/utils/colorutils.cpp
        src/domain/models/classroom.cpp
        src/features/calendar/calendar_workbook_reader.cpp
        src/features/classes/config/class_info_config.cpp
        src/features/schedule/import/schedule_workbook_parser.cpp
        src/features/schedule/ui/schedule_editor_dialog.h
        src/features/schedule/ui/schedule_import_dialog.cpp
        src/features/schedule/ui/schedule_import_dialog.h
        src/features/schedule/ui/schedule_import_dialog_shared.cpp
        src/features/schedule/ui/schedule_import_dialog_shared.h
        src/features/schedule/ui/schedule_import_review_dialog.cpp
        src/features/schedule/ui/schedule_import_review_dialog.h
        src/features/schedule/ui/schedule_print_dialog.h
        src/features/schedule/ui/schedule_settings_dialog.cpp
        src/features/schedule/ui/schedule_settings_dialog.h
        src/features/schedule/ui/schedule_time_formatter.cpp
        src/features/schedule/ui/schedule_time_formatter.h
        src/features/schedule/ui/schedule_widget.cpp
        src/features/schedule/ui/schedule_widget.h
        src/features/schedule/ui/testing_assignment_dialog.cpp
        src/features/schedule/ui/testing_assignment_dialog.h
        src/ui/shared/widgets/no_wheel_combobox.cpp
    )

    target_compile_features(ClassMngrScheduleImportDialogTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrScheduleImportDialogTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrScheduleImportDialogTests
        PRIVATE
            Qt6::Concurrent
            Qt6::Core
            Qt6::Gui
            Qt6::PrintSupport
            Qt6::Sql
            Qt6::Test
            Qt6::Widgets
            ZLIB::ZLIB
    )

    add_test(
        NAME ClassMngrScheduleImportDialogTests
        COMMAND ClassMngrScheduleImportDialogTests
    )

    set_tests_properties(
        ClassMngrScheduleImportDialogTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(ClassMngrSubPrepPageTests
        tests/sub_prep_page_tests.cpp
        tests/schedule_widget_test_stubs.cpp
        src/core/utils/sidebar_node_naming.cpp
        src/domain/models/classroom.cpp
        src/features/campus/data/campus_json_codec.cpp
        src/features/campus/data/campus_json_repository.cpp
        src/features/classes/config/class_info_config.cpp
        src/features/classes/models/class_tab_navigation_model.cpp
        src/features/classes/models/class_tab_navigation_model.h
        src/features/schedule/ui/schedule_editor_dialog.h
        src/features/schedule/ui/schedule_print_dialog.h
        src/features/schedule/ui/schedule_settings_dialog.cpp
        src/features/schedule/ui/schedule_settings_dialog.h
        src/features/sub_prep/ui/sub_prep_class_information_model.cpp
        src/features/sub_prep/ui/sub_prep_page.cpp
        src/features/sub_prep/ui/sub_prep_page_class_information.cpp
        src/features/sub_prep/ui/sub_prep_page.h
        src/features/sub_prep/ui/sub_prep_page_settings.cpp
        src/features/sub_prep/ui/sub_prep_page_ui.cpp
        src/features/sub_prep/ui/sub_prep_print_dialog.cpp
        src/features/sub_prep/ui/sub_prep_print_dialog.h
        src/features/sub_prep/services/sub_prep_package_service.cpp
        src/features/sub_prep/services/sub_prep_package_service.h
        src/features/sub_prep/services/sub_prep_print_service.cpp
        src/features/sub_prep/services/sub_prep_print_service.h
        src/ui/shared/pages/basepage.cpp
        src/ui/shared/pages/basepage.h
        src/ui/shared/widgets/sectioncards/class_info_section_card.cpp
        src/ui/shared/widgets/sectioncards/class_info_section_card.h
        src/ui/shared/widgets/uniform_width_tab_bar.cpp
        src/ui/shared/widgets/uniform_width_tab_bar.h
        src/features/schedule/ui/schedule_widget.cpp
        src/features/schedule/ui/schedule_widget.h
        src/features/schedule/ui/testing_assignment_dialog.cpp
        src/features/schedule/ui/testing_assignment_dialog.h
    )

    target_compile_features(ClassMngrSubPrepPageTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSubPrepPageTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_compile_definitions(ClassMngrSubPrepPageTests
        PRIVATE
            CLASSMNGR_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )

    target_link_libraries(ClassMngrSubPrepPageTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Pdf
            Qt6::PrintSupport
            Qt6::Sql
            Qt6::Widgets
            Qt6::Test
    )

    add_test(
        NAME ClassMngrSubPrepPageTests
        COMMAND ClassMngrSubPrepPageTests
    )

    set_tests_properties(
        ClassMngrScheduleWidgetTests
        ClassMngrSubPrepPageTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(ClassMngrUniformWidthTabBarTests
        tests/uniform_width_tab_bar_tests.cpp
        src/features/classes/ui/classes_navigation_tabs.cpp
        src/features/classes/ui/classes_navigation_tabs.h
        src/ui/shared/widgets/uniform_width_tab_bar.cpp
    )

    target_compile_features(ClassMngrUniformWidthTabBarTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrUniformWidthTabBarTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrUniformWidthTabBarTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrUniformWidthTabBarTests
        COMMAND ClassMngrUniformWidthTabBarTests
    )

    set_tests_properties(
        ClassMngrUniformWidthTabBarTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(ClassMngrSidebarStructureTests
        tests/sidebar_structure_tests.cpp
        src/core/fontmanager.cpp
        src/core/resource_packs/resource_pack_manager.cpp
        src/core/updater/version.cpp
        src/features/documents/document_catalog.cpp
        src/ui/shared/widgets/sidebar/sidebar.cpp
        src/ui/shared/widgets/sidebar/sidebar_classes.cpp
        src/ui/shared/widgets/sidebar/sidebar_context_menu.cpp
        src/ui/shared/widgets/sidebar/sidebar_definitions.cpp
        src/ui/shared/widgets/sidebar/sidebar_marquee_delegate.cpp
        src/ui/shared/widgets/sidebar/sidebar_overflow.cpp
        src/ui/shared/widgets/sidebar/sidebar_selection.cpp
        src/ui/shared/widgets/sidebar/sidebar_teachers.cpp
        src/ui/shared/widgets/sidebar/sidebar_tree.cpp
    )

    target_compile_features(ClassMngrSidebarStructureTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSidebarStructureTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_compile_definitions(ClassMngrSidebarStructureTests
        PRIVATE
            CLASSMNGR_TEST_DOCUMENTS_PATH="${PROJECT_SOURCE_DIR}/resources/assets/documents"
    )

    target_link_libraries(ClassMngrSidebarStructureTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrSidebarStructureTests
        COMMAND ClassMngrSidebarStructureTests
    )

    set_tests_properties(
        ClassMngrSidebarStructureTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(ClassMngrBasePageTests
        tests/basepage_tests.cpp
        src/ui/shared/pages/basepage.cpp
    )

    target_compile_features(ClassMngrBasePageTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrBasePageTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrBasePageTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrBasePageTests
        COMMAND ClassMngrBasePageTests
    )

    qt_add_executable(ClassMngrSchedulePrintModelTests
        tests/schedule_print_model_tests.cpp
        src/features/schedule/ui/schedule_time_formatter.cpp
        src/features/schedule/ui/schedule_view_model.cpp
    )

    target_compile_features(ClassMngrSchedulePrintModelTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSchedulePrintModelTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrSchedulePrintModelTests
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    add_test(
        NAME ClassMngrSchedulePrintModelTests
        COMMAND ClassMngrSchedulePrintModelTests
    )

    qt_add_executable(ClassMngrScheduleBuilderTests
        tests/schedule_builder_tests.cpp
        tests/schedule_builder_test_stubs.cpp
        src/features/schedule/ui/schedule_builder.cpp
    )

    target_compile_features(ClassMngrScheduleBuilderTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrScheduleBuilderTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrScheduleBuilderTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Sql
            Qt6::Test
    )

    add_test(
        NAME ClassMngrScheduleBuilderTests
        COMMAND ClassMngrScheduleBuilderTests
    )

    qt_add_executable(ClassMngrSchedulePrintPdfTests
        tests/schedule_print_pdf_tests.cpp
        src/core/fontmanager.cpp
        src/features/schedule/services/schedule_print_service.cpp
        src/features/schedule/ui/schedule_time_formatter.cpp
        src/features/schedule/ui/schedule_view_model.cpp
    )

    target_compile_features(ClassMngrSchedulePrintPdfTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSchedulePrintPdfTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrSchedulePrintPdfTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Pdf
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrSchedulePrintPdfTests
        COMMAND ClassMngrSchedulePrintPdfTests
    )

    qt_add_executable(ClassMngrSubPrepPrintPdfTests
        tests/sub_prep_print_pdf_tests.cpp
        src/core/fontmanager.cpp
        src/features/schedule/ui/schedule_time_formatter.cpp
        src/features/schedule/ui/schedule_view_model.cpp
        src/features/sub_prep/services/sub_prep_print_service.cpp
    )

    target_compile_features(ClassMngrSubPrepPrintPdfTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSubPrepPrintPdfTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrSubPrepPrintPdfTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Pdf
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrSubPrepPrintPdfTests
        COMMAND ClassMngrSubPrepPrintPdfTests
    )

    set_tests_properties(
        ClassMngrSubPrepPrintPdfTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )

    qt_add_executable(ClassMngrSubPrepPackageServiceTests
        tests/sub_prep_package_service_tests.cpp
        src/core/fontmanager.cpp
        src/core/utils/sidebar_node_naming.cpp
        src/domain/models/classroom.cpp
        src/domain/models/roster.cpp
        src/features/roster/services/roster_template_print_service.cpp
        src/features/schedule/ui/schedule_time_formatter.cpp
        src/features/schedule/ui/schedule_view_model.cpp
        src/features/sub_prep/services/sub_prep_package_service.cpp
        src/features/sub_prep/services/sub_prep_print_service.cpp
    )

    target_compile_features(ClassMngrSubPrepPackageServiceTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSubPrepPackageServiceTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrSubPrepPackageServiceTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Pdf
            Qt6::PrintSupport
            Qt6::Sql
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrSubPrepPackageServiceTests
        COMMAND ClassMngrSubPrepPackageServiceTests
    )

    set_tests_properties(ClassMngrSubPrepPackageServiceTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )

    qt_add_executable(ClassMngrRosterModelTests
        tests/roster_model_tests.cpp
        src/domain/models/roster.cpp
        src/features/roster/ui/roster_model.cpp
        src/features/roster/ui/roster_model_columns.cpp
        src/features/roster/ui/roster_model_names.cpp
        src/features/roster/ui/roster_model_rows.cpp
        src/features/roster/ui/roster_model_validation.cpp
    )

    target_compile_features(ClassMngrRosterModelTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrRosterModelTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrRosterModelTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
    )

    add_test(
        NAME ClassMngrRosterModelTests
        COMMAND ClassMngrRosterModelTests
    )

    qt_add_executable(ClassMngrRosterTemplatePrintServiceTests
        tests/roster_template_print_service_tests.cpp
        src/core/fontmanager.cpp
        src/domain/models/classroom.cpp
        src/features/roster/services/roster_template_print_service.cpp
    )

    target_compile_features(ClassMngrRosterTemplatePrintServiceTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrRosterTemplatePrintServiceTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_compile_definitions(ClassMngrRosterTemplatePrintServiceTests
        PRIVATE
            CLASSMNGR_TEST_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )

    target_link_libraries(ClassMngrRosterTemplatePrintServiceTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Pdf
            Qt6::Sql
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrRosterTemplatePrintServiceTests
        COMMAND ClassMngrRosterTemplatePrintServiceTests
    )

    set_tests_properties(ClassMngrRosterTemplatePrintServiceTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )

    qt_add_executable(ClassMngrLanguageServiceTests
        tests/language_service_tests.cpp
        src/core/language_service.cpp
        src/core/settingsmanager.cpp
    )

    target_compile_features(ClassMngrLanguageServiceTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrLanguageServiceTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrLanguageServiceTests
        PRIVATE
            Qt6::Core
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrLanguageServiceTests
        COMMAND ClassMngrLanguageServiceTests
    )

    qt_add_executable(ClassMngrAiCommentOptionsTests
        tests/ai_comment_options_tests.cpp
        src/core/settingsmanager.cpp
        src/ui/shared/actions/action_registry.cpp
        src/ui/shared/state/ai_comment_options.cpp
        src/ui/shared/styles/themed_icon_utils.cpp
    )

    target_compile_features(ClassMngrAiCommentOptionsTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrAiCommentOptionsTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrAiCommentOptionsTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrAiCommentOptionsTests
        COMMAND ClassMngrAiCommentOptionsTests
    )

    set_tests_properties(ClassMngrAiCommentOptionsTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    if(APPLE)
        qt_add_executable(ClassMngrPowerPointDataAccessNoticeTests
            tests/powerpoint_data_access_notice_tests.cpp
            src/core/settingsmanager.cpp
            src/ui/shared/actions/action_registry.cpp
            src/ui/shared/styles/themed_icon_utils.cpp
        )

        target_compile_features(ClassMngrPowerPointDataAccessNoticeTests
            PRIVATE
                cxx_std_23
        )

        target_include_directories(ClassMngrPowerPointDataAccessNoticeTests
            PRIVATE
                ${PROJECT_SOURCE_DIR}/src
        )

        target_link_libraries(ClassMngrPowerPointDataAccessNoticeTests
            PRIVATE
                Qt6::Core
                Qt6::Gui
                Qt6::Test
                Qt6::Widgets
        )

        add_test(
            NAME ClassMngrPowerPointDataAccessNoticeTests
            COMMAND ClassMngrPowerPointDataAccessNoticeTests
        )

        set_tests_properties(ClassMngrPowerPointDataAccessNoticeTests
            PROPERTIES
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        )
    endif()

    qt_add_executable(ClassMngrAcademicCalendarTests
        tests/academic_calendar_tests.cpp
        src/features/calendar/academic_calendar_schedule.cpp
        src/features/calendar/ui/academic_calendar_provider.cpp
    )

    target_compile_features(ClassMngrAcademicCalendarTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrAcademicCalendarTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrAcademicCalendarTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Sql
            Qt6::Test
    )

    add_test(
        NAME ClassMngrAcademicCalendarTests
        COMMAND ClassMngrAcademicCalendarTests
    )

    qt_add_executable(ClassMngrCampusMapTests
        tests/campus_map_tests.cpp
        src/features/campus/data/campus_json_codec.cpp
        src/features/campus/data/campus_json_repository.cpp
        src/features/campus/ui/campus_map_preview.cpp
    )

    qt_add_resources(ClassMngrCampusMapTests campus_map_test_resources
        PREFIX "/"
        BASE "${PROJECT_SOURCE_DIR}/resources"
        FILES
            ${CLASSMNGR_CAMPUS_MAP_IMAGES}
    )

    target_compile_features(ClassMngrCampusMapTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrCampusMapTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrCampusMapTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrCampusMapTests
        COMMAND ClassMngrCampusMapTests
    )

    qt_add_executable(ClassMngrCampusDashboardPageTests
        tests/campus_dashboard_page_tests.cpp
        src/core/fontmanager.cpp
        src/core/resource_packs/resource_pack_manager.cpp
        src/core/settingsmanager.cpp
        src/core/updater/version.cpp
        src/features/campus/data/campus_json_codec.cpp
        src/features/campus/data/campus_json_repository.cpp
        src/features/campus/ui/campus_dashboard_page.cpp
        src/features/campus/ui/campus_dashboard_page_address.cpp
        src/features/campus/ui/campus_dashboard_page_address_ui.cpp
        src/features/campus/ui/campus_dashboard_page_data.cpp
        src/features/campus/ui/campus_dashboard_page_detail.cpp
        src/features/campus/ui/campus_dashboard_page_directions_ui.cpp
        src/features/campus/ui/campus_dashboard_page_form.cpp
        src/features/campus/ui/campus_dashboard_page_housing.cpp
        src/features/campus/ui/campus_dashboard_page_information_ui.cpp
        src/features/campus/ui/campus_dashboard_page_map_ui.cpp
        src/features/campus/ui/campus_dashboard_page_ui.cpp
        src/features/campus/ui/campus_map_preview.cpp
        src/ui/shared/pages/basepage.cpp
        src/ui/shared/widgets/no_wheel_combobox.cpp
        src/ui/shared/widgets/sectioncards/class_info_section_card.cpp
    )

    target_compile_features(ClassMngrCampusDashboardPageTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrCampusDashboardPageTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_compile_definitions(ClassMngrCampusDashboardPageTests
        PRIVATE
            CLASSMNGR_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )

    target_link_libraries(ClassMngrCampusDashboardPageTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    add_test(
        NAME ClassMngrCampusDashboardPageTests
        COMMAND ClassMngrCampusDashboardPageTests
    )

    set_tests_properties(
        ClassMngrCampusDashboardPageTests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(ClassMngrSignatureImageProcessorTests
        tests/signature_image_processor_tests.cpp
        src/features/my_info/data/signature_image_processor.cpp
    )

    target_compile_features(ClassMngrSignatureImageProcessorTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSignatureImageProcessorTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrSignatureImageProcessorTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
    )

    add_test(
        NAME ClassMngrSignatureImageProcessorTests
        COMMAND ClassMngrSignatureImageProcessorTests
    )

    qt_add_executable(ClassMngrSpeakingEvalReportWidgetTests
        tests/speaking_eval_report_widget_tests.cpp
        src/core/fontmanager.cpp
        src/features/speaking_eval/ui/speaking_eval_comment_edit.cpp
        src/features/speaking_eval/ui/speaking_eval_report_assets_p.cpp
        src/features/speaking_eval/ui/speaking_eval_report_widget.cpp
        src/features/speaking_eval/ui/speaking_eval_report_widget_interaction.cpp
        src/features/speaking_eval/ui/speaking_eval_report_widget_rendering.cpp
    )

    target_compile_features(ClassMngrSpeakingEvalReportWidgetTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSpeakingEvalReportWidgetTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrSpeakingEvalReportWidgetTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    qt_add_resources(ClassMngrSpeakingEvalReportWidgetTests report_test_resources
        PREFIX "/"
        BASE "${PROJECT_SOURCE_DIR}/resources"
        FILES
            resources/assets/fonts/Inter.ttc
            resources/assets/fonts/JustAnotherHand-Regular.ttf
            resources/assets/fonts/PretendardVariable.ttf
            resources/assets/templates/speaking-eval/advanced-background.png
            resources/assets/templates/speaking-eval/advanced-grey-a.png
            resources/assets/templates/speaking-eval/advanced-grey-aplus.png
            resources/assets/templates/speaking-eval/advanced-grey-b.png
            resources/assets/templates/speaking-eval/advanced-grey-bplus.png
            resources/assets/templates/speaking-eval/advanced-grey-c.png
            resources/assets/templates/speaking-eval/advanced-manifest.json
            resources/assets/templates/speaking-eval/advanced-sprites.png
            resources/assets/templates/speaking-eval/advanced-yellow-a.png
            resources/assets/templates/speaking-eval/advanced-yellow-aplus.png
            resources/assets/templates/speaking-eval/advanced-yellow-b.png
            resources/assets/templates/speaking-eval/advanced-yellow-bplus.png
            resources/assets/templates/speaking-eval/advanced-yellow-c.png
            resources/assets/templates/speaking-eval/standard-background.png
            resources/assets/templates/speaking-eval/standard-manifest.json
            resources/assets/templates/speaking-eval/standard-sprites.png
            resources/assets/templates/speaking-eval/standard-yellow-a.png
            resources/assets/templates/speaking-eval/standard-yellow-aplus.png
            resources/assets/templates/speaking-eval/standard-yellow-b.png
            resources/assets/templates/speaking-eval/standard-yellow-bplus.png
            resources/assets/templates/speaking-eval/standard-yellow-c.png
    )

    add_test(
        NAME ClassMngrSpeakingEvalReportWidgetTests
        COMMAND ClassMngrSpeakingEvalReportWidgetTests
    )

    qt_add_executable(ClassMngrSpeakingEvalBatchReportServiceTests
        tests/speaking_eval_batch_report_service_tests.cpp
        src/core/fontmanager.cpp
        src/core/resource_packs/resource_pack_manager.cpp
        src/core/settingsmanager.cpp
        src/core/updater/version.cpp
        src/core/zip_archive_writer.cpp
        src/features/speaking_eval/services/speaking_eval_ai_prompt.cpp
        src/features/speaking_eval/services/speaking_eval_batch_report_service.cpp
        src/features/speaking_eval/ui/speaking_eval_ai_batch_dialog.cpp
        src/features/speaking_eval/ui/speaking_eval_comment_edit.cpp
        src/features/speaking_eval/ui/speaking_eval_delegate.cpp
        src/features/speaking_eval/ui/speaking_eval_model.cpp
        src/features/speaking_eval/ui/speaking_eval_report_assets_p.cpp
        src/features/speaking_eval/ui/speaking_eval_private_notes_editor.cpp
        src/features/speaking_eval/ui/speaking_eval_notes_dialog.cpp
        src/features/speaking_eval/ui/speaking_eval_report_dialog.cpp
        src/features/speaking_eval/ui/speaking_eval_report_widget.cpp
        src/features/speaking_eval/ui/speaking_eval_report_widget_interaction.cpp
        src/features/speaking_eval/ui/speaking_eval_report_widget_rendering.cpp
        src/features/speaking_eval/ui/speaking_eval_table_view.cpp
        src/ui/shared/printing/pdf_print_dialog.cpp
        src/ui/shared/printing/pdf_print_dialog_pages.cpp
        src/ui/shared/printing/pdf_print_dialog_printer.cpp
        src/ui/shared/printing/pdf_print_dialog_printing.cpp
        src/ui/shared/printing/pdf_print_dialog_widgets.cpp
        src/ui/shared/printing/pdf_print_service.cpp
        src/ui/shared/styles/role_style_registry.cpp
        src/ui/shared/state/ai_comment_options.cpp
        src/ui/shared/widgets/no_wheel_combobox.cpp
    )

    target_compile_features(ClassMngrSpeakingEvalBatchReportServiceTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSpeakingEvalBatchReportServiceTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_compile_definitions(ClassMngrSpeakingEvalBatchReportServiceTests
        PRIVATE
            CLASSMNGR_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )

    target_link_libraries(ClassMngrSpeakingEvalBatchReportServiceTests
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Pdf
            Qt6::PdfWidgets
            Qt6::PrintSupport
            Qt6::Test
            Qt6::Widgets
            ZLIB::ZLIB
    )

    qt_add_resources(ClassMngrSpeakingEvalBatchReportServiceTests batch_report_test_resources
        PREFIX "/"
        BASE "${PROJECT_SOURCE_DIR}/resources"
        FILES
            resources/assets/fonts/Inter.ttc
            resources/assets/fonts/JustAnotherHand-Regular.ttf
            resources/assets/fonts/PretendardVariable.ttf
            resources/assets/templates/speaking-eval/advanced-background.png
            resources/assets/templates/speaking-eval/advanced-grey-a.png
            resources/assets/templates/speaking-eval/advanced-grey-aplus.png
            resources/assets/templates/speaking-eval/advanced-grey-b.png
            resources/assets/templates/speaking-eval/advanced-grey-bplus.png
            resources/assets/templates/speaking-eval/advanced-grey-c.png
            resources/assets/templates/speaking-eval/advanced-manifest.json
            resources/assets/templates/speaking-eval/advanced-sprites.png
            resources/assets/templates/speaking-eval/advanced-yellow-a.png
            resources/assets/templates/speaking-eval/advanced-yellow-aplus.png
            resources/assets/templates/speaking-eval/advanced-yellow-b.png
            resources/assets/templates/speaking-eval/advanced-yellow-bplus.png
            resources/assets/templates/speaking-eval/advanced-yellow-c.png
            resources/assets/templates/speaking-eval/standard-background.png
            resources/assets/templates/speaking-eval/standard-manifest.json
            resources/assets/templates/speaking-eval/standard-sprites.png
            resources/assets/templates/speaking-eval/standard-yellow-a.png
            resources/assets/templates/speaking-eval/standard-yellow-aplus.png
            resources/assets/templates/speaking-eval/standard-yellow-b.png
            resources/assets/templates/speaking-eval/standard-yellow-bplus.png
            resources/assets/templates/speaking-eval/standard-yellow-c.png
            "resources/assets/documents/Speaking Evaluations/SpeakingEvaluationTemplate-Full.pptx"
            "resources/assets/documents/Speaking Evaluations/SpeakingEvaluationTemplate_Advanced-Full.pptx"
    )

    add_test(
        NAME ClassMngrSpeakingEvalBatchReportServiceTests
        COMMAND ClassMngrSpeakingEvalBatchReportServiceTests
    )

    qt_add_executable(ClassMngrStartupPerformanceTests
        tests/startup_performance_tests.cpp
    )

    add_dependencies(
        ClassMngrStartupPerformanceTests
        ClassMngr
    )

    target_compile_features(ClassMngrStartupPerformanceTests
        PRIVATE
            cxx_std_23
    )

    target_link_libraries(ClassMngrStartupPerformanceTests
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    add_test(
        NAME ClassMngrStartupPerformanceTests
        COMMAND ClassMngrStartupPerformanceTests
    )

    qt_add_executable(ClassMngrDatabaseFileFormatTests
        tests/database_file_format_tests.cpp
        src/app/startup_database_path.cpp
        src/core/database_file_format.cpp
    )

    target_compile_features(ClassMngrDatabaseFileFormatTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrDatabaseFileFormatTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrDatabaseFileFormatTests
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    add_test(
        NAME ClassMngrDatabaseFileFormatTests
        COMMAND ClassMngrDatabaseFileFormatTests
    )

    set_tests_properties(
        ClassMngrStartupPerformanceTests
        PROPERTIES
        ENVIRONMENT
            "CLASSMNGR_TEST_APP_PATH=$<TARGET_FILE:ClassMngr>"
    )

    if(WIN32)
        set_tests_properties(
            ClassMngrUpdaterTests
            ClassMngrResourcePackTests
            ClassMngrFontManagerTests
            ClassMngrTeacherInfoPageTests
            ClassMngrTeacherInfoSectionTests
            ClassMngrIntensiveSlotStateRepositoryTests
            ClassMngrTestingBlockRepositoryTests
            ClassMngrTestingClassRepositoryTests
            ClassMngrCalendarEventRepositoryTests
            ClassMngrDataServiceLifecycleTests
            ClassMngrCalendarImportTests
            ClassMngrScheduleImportTests
            ClassMngrTeacherImportTests
            ClassMngrTeacherImportDialogTests
            ClassMngrStaffDirectoryPageTests
            ClassMngrClassTabNavigationModelTests
            ClassMngrSubPrepClassInformationModelTests
            ClassMngrScheduleWidgetTests
            ClassMngrTestingClassesPageTests
            ClassMngrScheduleImportDialogTests
            ClassMngrSubPrepPageTests
            ClassMngrUniformWidthTabBarTests
            ClassMngrSidebarStructureTests
            ClassMngrScheduleBuilderTests
            ClassMngrSchedulePrintModelTests
            ClassMngrSchedulePrintPdfTests
            ClassMngrSubPrepPrintPdfTests
            ClassMngrRosterModelTests
            ClassMngrRosterTemplatePrintServiceTests
            ClassMngrLanguageServiceTests
            ClassMngrAcademicCalendarTests
            ClassMngrCampusMapTests
            ClassMngrSpeakingEvalReportWidgetTests
            ClassMngrSpeakingEvalBatchReportServiceTests
            ClassMngrStartupPerformanceTests
            PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )
    endif()
endif()
