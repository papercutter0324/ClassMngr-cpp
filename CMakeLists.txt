cmake_minimum_required(VERSION 3.25)
if(POLICY CMP0218)
    cmake_policy(SET CMP0218 NEW)
endif()

if(POLICY CMP0219)
    cmake_policy(SET CMP0219 NEW)
endif()

project(ClassMngr
    VERSION 0.7.0
    LANGUAGES CXX
)

include(CTest)
include(GNUInstallDirs)

add_compile_options(
    $<$<CXX_COMPILER_ID:MSVC>:/FS>
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

set(CLASSMNGR_UPDATE_MANIFEST_URL "" CACHE STRING
    "HTTPS URL for the stable auto-update manifest"
)
set(CLASSMNGR_UPDATE_SIGNATURE_URL "" CACHE STRING
    "HTTPS URL for the detached signature for the stable auto-update manifest"
)
set(CLASSMNGR_UPDATE_PUBLIC_KEY_PEM "" CACHE STRING
    "PEM encoded public key used to verify the auto-update manifest"
)
option(CLASSMNGR_UPDATE_REQUIRE_SIGNATURE
    "Require detached manifest signature verification before parsing update URLs"
    ON
)
option(CLASSMNGR_UPDATE_CHECK_ON_STARTUP
    "Check for updates after the main window is shown"
    OFF
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
    "Check configured resource packs for updates after launch"
    ON
)

set(CLASSMNGR_UPDATE_REQUIRE_SIGNATURE_CPP false)
if(CLASSMNGR_UPDATE_REQUIRE_SIGNATURE)
    set(CLASSMNGR_UPDATE_REQUIRE_SIGNATURE_CPP true)
endif()

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
set(CLASSMNGR_QT_DEPLOY_QML_PLUGINS "\\$")
string(APPEND CLASSMNGR_QT_DEPLOY_QML_PLUGINS "{plugins_found}")
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
set(CLASSMNGR_QT_SQL_DRIVER_DIR "\${__QT_DEPLOY_QT_INSTALL_PREFIX}/\${__QT_DEPLOY_QT_INSTALL_PLUGINS}/sqldrivers")
set(CLASSMNGR_SQL_DRIVER_STAGING_DIR "\${CMAKE_CURRENT_LIST_DIR}/classmngr-deploy-sqldrivers")
file(MAKE_DIRECTORY "\${CLASSMNGR_SQL_DRIVER_STAGING_DIR}")
foreach(driver IN LISTS CLASSMNGR_BAD_SQL_DRIVERS)
    if(EXISTS "\${CLASSMNGR_SQL_DRIVER_STAGING_DIR}/\${driver}" AND NOT EXISTS "\${CLASSMNGR_QT_SQL_DRIVER_DIR}/\${driver}")
        file(RENAME "\${CLASSMNGR_SQL_DRIVER_STAGING_DIR}/\${driver}" "\${CLASSMNGR_QT_SQL_DRIVER_DIR}/\${driver}")
    endif()
endforeach()
foreach(driver IN LISTS CLASSMNGR_BAD_SQL_DRIVERS)
    if(EXISTS "\${CLASSMNGR_QT_SQL_DRIVER_DIR}/\${driver}")
        file(RENAME "\${CLASSMNGR_QT_SQL_DRIVER_DIR}/\${driver}" "\${CLASSMNGR_SQL_DRIVER_STAGING_DIR}/\${driver}")
        list(APPEND CLASSMNGR_STAGED_SQL_DRIVERS "\${driver}")
    endif()
endforeach()
set(CLASSMNGR_APP_SQL_DRIVER_DIR "\${QT_DEPLOY_PREFIX}/ClassMngr.app/Contents/PlugIns/sqldrivers")
foreach(driver IN LISTS CLASSMNGR_BAD_SQL_DRIVERS)
    if(EXISTS "\${CLASSMNGR_APP_SQL_DRIVER_DIR}/\${driver}")
        file(REMOVE "\${CLASSMNGR_APP_SQL_DRIVER_DIR}/\${driver}")
    endif()
endforeach()
]=])
    string(APPEND CLASSMNGR_QT_DEPLOY_OPTIONS
        "    NO_APP_STORE_COMPLIANCE\n"
    )
    string(APPEND CLASSMNGR_QT_DEPLOY_POSTAMBLE [=[
foreach(driver IN LISTS CLASSMNGR_BAD_SQL_DRIVERS)
    if(EXISTS "\${CLASSMNGR_APP_SQL_DRIVER_DIR}/\${driver}")
        file(REMOVE "\${CLASSMNGR_APP_SQL_DRIVER_DIR}/\${driver}")
    endif()
endforeach()
foreach(driver IN LISTS CLASSMNGR_STAGED_SQL_DRIVERS)
    if(EXISTS "\${CLASSMNGR_SQL_DRIVER_STAGING_DIR}/\${driver}")
        file(RENAME "\${CLASSMNGR_SQL_DRIVER_STAGING_DIR}/\${driver}" "\${CLASSMNGR_QT_SQL_DRIVER_DIR}/\${driver}")
    endif()
endforeach()
file(REMOVE_RECURSE "\${CLASSMNGR_SQL_DRIVER_STAGING_DIR}")
]=])
endif()

if(WIN32)
    set(CMAKE_INSTALL_BINDIR ".")
    string(APPEND CLASSMNGR_QT_DEPLOY_PREAMBLE
        "set(QT_DEPLOY_BIN_DIR .)\n"
    )
endif()

qt_add_executable(ClassMngr
    WIN32
    MACOSX_BUNDLE
)

include(cmake/sources.cmake)

file(GLOB_RECURSE CLASSMNGR_CAMPUS_MAP_IMAGES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/resources/assets/campuses/*.png"
    "${PROJECT_SOURCE_DIR}/resources/assets/campuses/*.jpg"
    "${PROJECT_SOURCE_DIR}/resources/assets/campuses/*.jpeg"
)

file(GLOB_RECURSE CLASSMNGR_EMBEDDED_PACK_FILES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/resources/assets/campuses/*"
    "${PROJECT_SOURCE_DIR}/resources/assets/files/*"
    "${PROJECT_SOURCE_DIR}/resources/assets/templates/*"
    "${PROJECT_SOURCE_DIR}/resources/assets/roster-designs/*"
)

list(FILTER CLASSMNGR_EMBEDDED_PACK_FILES EXCLUDE REGEX "/~\\$[^/]+$")

qt_add_resources(ClassMngr app_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/fonts/Inter.ttc
        resources/assets/fonts/PretendardVariable.ttf
        resources/assets/icons/app_icon.ico
        resources/assets/icons/check_dark.svg
        resources/assets/icons/check_light.svg
        resources/assets/icons/combo_arrow_dark.svg
        resources/assets/icons/combo_arrow_light.svg
        resources/assets/icons/spin_up_dark.svg
        resources/assets/icons/spin_up_light.svg
        resources/assets/icons/icon_256x256.png
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

if(WIN32 AND CLASSMNGR_RUN_WINDEPLOYQT)
    add_custom_command(TARGET ClassMngr POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E env
            ${CLASSMNGR_WINDEPLOYQT_ENV}
            "$<TARGET_FILE:Qt6::windeployqt>"
            --qmldir "${PROJECT_SOURCE_DIR}/src/features/calendar/ui/qml"
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

qt_generate_deploy_script(
    TARGET ClassMngr
    OUTPUT_SCRIPT deploy_script
    CONTENT "
${CLASSMNGR_QT_DEPLOY_PREAMBLE}
qt_deploy_qml_imports(TARGET ClassMngr PLUGINS_FOUND plugins_found)
qt_deploy_runtime_dependencies(
    EXECUTABLE \"${CLASSMNGR_QT_DEPLOY_EXECUTABLE}\"
    ADDITIONAL_MODULES ${CLASSMNGR_QT_DEPLOY_QML_PLUGINS}
    GENERATE_QT_CONF
${CLASSMNGR_QT_DEPLOY_OPTIONS}
)
${CLASSMNGR_QT_DEPLOY_POSTAMBLE}
"
)
install(SCRIPT ${deploy_script})

if(WIN32)
    set(CLASSMNGR_INSTALLER_SOURCE_DIR
        "${PROJECT_SOURCE_DIR}/dist/ClassMngr-windows-x64"
        CACHE PATH
        "Directory packaged by the Windows installer"
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
            "C:/Program Files (x86)/Inno Setup 6"
            "C:/Program Files/Inno Setup 6"
    )

    if(CLASSMNGR_ISCC_EXECUTABLE)
        add_custom_target(ClassMngrInstaller
            COMMAND
                "${CMAKE_COMMAND}"
                --install "${CMAKE_BINARY_DIR}"
                --config "$<CONFIG>"
            COMMAND
                "${CMAKE_COMMAND}" -E env
                "CLASSMNGR_APP_VERSION=${PROJECT_VERSION}"
                "CLASSMNGR_INSTALLER_SOURCE_DIR=${CLASSMNGR_INSTALLER_SOURCE_DIR}"
                "CLASSMNGR_INSTALLER_OUTPUT_DIR=${CLASSMNGR_INSTALLER_OUTPUT_DIR}"
                "${CLASSMNGR_ISCC_EXECUTABLE}"
                "${CLASSMNGR_INSTALLER_SCRIPT}"
            DEPENDS ClassMngr
            BYPRODUCTS
                "${CLASSMNGR_INSTALLER_OUTPUT_DIR}/ClassMngrSetup-${PROJECT_VERSION}-x64.exe"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            COMMENT "Building ClassMngr Windows installer"
            VERBATIM
        )
    else()
        add_custom_target(ClassMngrInstaller
            COMMAND
                "${CMAKE_COMMAND}" -E echo
                "Inno Setup Compiler was not found. Install Inno Setup 6 or add ISCC.exe to PATH."
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
        src/core/updater/update_downloader.cpp
        src/core/updater/update_manifest.cpp
        src/core/updater/version.cpp
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
    )

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
        src/features/schedule/ui/schedule_print_dialog.h
        src/features/schedule/ui/schedule_widget.cpp
        src/features/schedule/ui/schedule_widget.h
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

    qt_add_executable(ClassMngrSubPrepPageTests
        tests/sub_prep_page_tests.cpp
        tests/schedule_widget_test_stubs.cpp
        src/core/utils/sidebar_node_naming.cpp
        src/domain/models/classroom.cpp
        src/features/campus/data/campus_json_codec.cpp
        src/features/campus/data/campus_json_repository.cpp
        src/features/classes/config/class_info_config.cpp
        src/features/schedule/ui/schedule_editor_dialog.h
        src/features/schedule/ui/schedule_print_dialog.h
        src/features/sub_prep/ui/sub_prep_class_information_model.cpp
        src/features/sub_prep/ui/sub_prep_page.cpp
        src/features/sub_prep/ui/sub_prep_page_class_information.cpp
        src/features/sub_prep/ui/sub_prep_page.h
        src/features/sub_prep/ui/sub_prep_page_settings.cpp
        src/features/sub_prep/ui/sub_prep_page_ui.cpp
        src/features/sub_prep/ui/sub_prep_print_dialog.cpp
        src/features/sub_prep/ui/sub_prep_print_dialog.h
        src/features/sub_prep/services/sub_prep_print_service.cpp
        src/features/sub_prep/services/sub_prep_print_service.h
        src/ui/shared/pages/basepage.cpp
        src/ui/shared/pages/basepage.h
        src/ui/shared/widgets/sectioncards/class_info_section_card.cpp
        src/ui/shared/widgets/sectioncards/class_info_section_card.h
        src/features/schedule/ui/schedule_widget.cpp
        src/features/schedule/ui/schedule_widget.h
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

    qt_add_executable(ClassMngrSidebarStructureTests
        tests/sidebar_structure_tests.cpp
        src/core/fontmanager.cpp
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

    qt_add_executable(ClassMngrSpeakingEvalReportWidgetTests
        tests/speaking_eval_report_widget_tests.cpp
        src/core/fontmanager.cpp
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
            resources/assets/images/dyb.png
    )

    add_test(
        NAME ClassMngrSpeakingEvalReportWidgetTests
        COMMAND ClassMngrSpeakingEvalReportWidgetTests
    )

    qt_add_executable(ClassMngrSpeakingEvalBatchReportServiceTests
        tests/speaking_eval_batch_report_service_tests.cpp
        src/core/fontmanager.cpp
        src/features/speaking_eval/services/speaking_eval_batch_report_service.cpp
        src/features/speaking_eval/ui/speaking_eval_report_widget.cpp
        src/features/speaking_eval/ui/speaking_eval_report_widget_interaction.cpp
        src/features/speaking_eval/ui/speaking_eval_report_widget_rendering.cpp
        src/ui/shared/printing/pdf_print_dialog.cpp
        src/ui/shared/printing/pdf_print_dialog_pages.cpp
        src/ui/shared/printing/pdf_print_dialog_printer.cpp
        src/ui/shared/printing/pdf_print_dialog_printing.cpp
        src/ui/shared/printing/pdf_print_dialog_widgets.cpp
        src/ui/shared/printing/pdf_print_service.cpp
        src/ui/shared/styles/role_style_registry.cpp
    )

    target_compile_features(ClassMngrSpeakingEvalBatchReportServiceTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSpeakingEvalBatchReportServiceTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
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
    )

    qt_add_resources(ClassMngrSpeakingEvalBatchReportServiceTests batch_report_test_resources
        PREFIX "/"
        BASE "${PROJECT_SOURCE_DIR}/resources"
        FILES
            resources/assets/images/dyb.png
            resources/assets/files/evaluations/SpeakingEvaluationTemplate-Full.pptx
            resources/assets/files/evaluations/SpeakingEvaluationTemplate_Advanced-Full.pptx
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
            ClassMngrTeacherInfoSectionTests
            ClassMngrIntensiveSlotStateRepositoryTests
            ClassMngrCalendarEventRepositoryTests
            ClassMngrCalendarImportTests
            ClassMngrClassTabNavigationModelTests
            ClassMngrSubPrepClassInformationModelTests
            ClassMngrScheduleWidgetTests
            ClassMngrSubPrepPageTests
            ClassMngrUniformWidthTabBarTests
            ClassMngrSidebarStructureTests
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
