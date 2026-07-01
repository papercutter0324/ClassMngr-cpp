cmake_minimum_required(VERSION 3.25)

project(ClassMngr
    VERSION 0.5.0
    LANGUAGES CXX
)

include(CTest)
include(GNUInstallDirs)

add_compile_options(
    $<$<CXX_COMPILER_ID:MSVC>:/FS>
)

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

set(CLASSMNGR_QT_DEPLOY_PREAMBLE "")
set(CLASSMNGR_QT_DEPLOY_OPTIONS "")
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
        resources/assets/splash/splash.png
        resources/assets/styles/dark.qss
        resources/assets/styles/light.qss
        ${CLASSMNGR_EMBEDDED_PACK_FILES}
)

set_source_files_properties(
    src/features/my_info/ui/qml/EventCalendar.qml
    PROPERTIES
        QT_RESOURCE_ALIAS EventCalendar.qml
)

set_source_files_properties(
    src/features/my_info/ui/qml/MonthGridDelegate.qml
    PROPERTIES
        QT_RESOURCE_ALIAS MonthGridDelegate.qml
)

qt_add_qml_module(ClassMngr
    URI ClassMngr.MyInfo
    VERSION 1.0
    OUTPUT_DIRECTORY
        ${CMAKE_CURRENT_BINARY_DIR}/qml/ClassMngr/MyInfo
    RESOURCE_PREFIX
        /qt/qml
    QML_FILES
        src/features/my_info/ui/qml/EventCalendar.qml
        src/features/my_info/ui/qml/MonthGridDelegate.qml
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
)

if(WIN32)
    add_custom_command(TARGET ClassMngr POST_BUILD
        COMMAND Qt6::windeployqt
            --qmldir "${PROJECT_SOURCE_DIR}/src/features/my_info/ui/qml"
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
    EXECUTABLE \"$<TARGET_FILE:ClassMngr>\"
    ADDITIONAL_MODULES ${CLASSMNGR_QT_DEPLOY_QML_PLUGINS}
    GENERATE_QT_CONF
${CLASSMNGR_QT_DEPLOY_OPTIONS}
)
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

    qt_add_executable(ClassMngrSubPrepClassNavigationTests
        tests/sub_prep_class_navigation_tests.cpp
        src/features/classes/config/class_info_config.cpp
        src/features/sub_prep/ui/sub_prep_class_navigation.cpp
    )

    target_compile_features(ClassMngrSubPrepClassNavigationTests
        PRIVATE
            cxx_std_23
    )

    target_include_directories(ClassMngrSubPrepClassNavigationTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ClassMngrSubPrepClassNavigationTests
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    add_test(
        NAME ClassMngrSubPrepClassNavigationTests
        COMMAND ClassMngrSubPrepClassNavigationTests
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
        src/features/my_info/academic_calendar_schedule.cpp
        src/features/my_info/ui/academic_calendar_provider.cpp
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

    if(WIN32)
        set_tests_properties(
            ClassMngrUpdaterTests
            ClassMngrResourcePackTests
            ClassMngrFontManagerTests
            ClassMngrTeacherInfoSectionTests
            ClassMngrIntensiveSlotStateRepositoryTests
            ClassMngrSubPrepClassNavigationTests
            ClassMngrUniformWidthTabBarTests
            ClassMngrLanguageServiceTests
            ClassMngrAcademicCalendarTests
            ClassMngrCampusMapTests
            PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )
    endif()
endif()
