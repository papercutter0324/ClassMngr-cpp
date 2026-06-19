cmake_minimum_required(VERSION 3.25)

project(ClassMngr
    VERSION 0.4.0
    LANGUAGES CXX
)

include(CTest)

find_package(Qt6 6.11.1 REQUIRED COMPONENTS
    Core
    Gui
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

set(CLASSMNGR_UPDATE_REQUIRE_SIGNATURE_CPP false)
if(CLASSMNGR_UPDATE_REQUIRE_SIGNATURE)
    set(CLASSMNGR_UPDATE_REQUIRE_SIGNATURE_CPP true)
endif()

set(CLASSMNGR_UPDATE_CHECK_ON_STARTUP_CPP false)
if(CLASSMNGR_UPDATE_CHECK_ON_STARTUP)
    set(CLASSMNGR_UPDATE_CHECK_ON_STARTUP_CPP true)
endif()

configure_file(
    src/core/build_info.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/core/build_info.h
    @ONLY
)

qt_standard_project_setup()

qt_add_executable(ClassMngr
    WIN32
    MACOSX_BUNDLE
)

include(cmake/sources.cmake)

qt_add_resources(ClassMngr app_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/fonts/Inter.ttc
        resources/assets/fonts/PretendardVariable.ttf
        resources/assets/icons/app_icon.ico
        resources/assets/icons/combo_arrow_dark.svg
        resources/assets/icons/combo_arrow_light.svg
        resources/assets/icons/icon_256x256.png
        resources/assets/campuses/bundang.json
        resources/assets/splash/splash.png
        resources/assets/styles/dark.qss
        resources/assets/styles/light.qss
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

target_compile_options(ClassMngr
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/FS>
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

qt_generate_deploy_app_script(
    TARGET ClassMngr
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

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

    if(WIN32)
        set_tests_properties(ClassMngrUpdaterTests PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )
    endif()
endif()
