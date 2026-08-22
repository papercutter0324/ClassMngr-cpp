classmngr_add_qt_test(
    NAME SharedPolicy
    SOURCES
        tests/shared_policy_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Sql
)

classmngr_add_qt_test(
    NAME DatabaseSchemaManager
    SOURCES
        tests/database_schema_manager_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Sql
)

classmngr_add_qt_test(
    NAME PageComponents
    SOURCES
        tests/page_components_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

classmngr_add_qt_test(
    NAME DialogShell
    SOURCES
        tests/dialog_shell_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

qt_add_resources(ClassMngrDialogShellTests dialog_shell_keyboard_test_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

classmngr_add_qt_test(
    NAME InitialSetupWizard
    SOURCES
        tests/initial_setup_wizard_tests.cpp
    LIBRARIES
        Qt6::Test
        Qt6::Widgets
    OFFSCREEN
)

qt_add_resources(ClassMngrInitialSetupWizardTests initial_setup_keyboard_test_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
)

classmngr_add_qt_test(
        NAME OnScreenKeyboard
        SOURCES
            tests/on_screen_keyboard_tests.cpp
            src/ui/shared/input/hangul_composer.cpp
            src/ui/shared/widgets/on_screen_keyboard.cpp
        LIBRARIES
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
        OFFSCREEN
    )

    qt_add_resources(ClassMngrOnScreenKeyboardTests keyboard_test_resources
        PREFIX "/"
        BASE "${PROJECT_SOURCE_DIR}/resources"
        FILES
            resources/assets/icons/keyboard_dark.svg
            resources/assets/icons/keyboard_light.svg
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

    classmngr_add_qt_test(
        NAME DocumentCatalog
        SOURCES
            tests/document_catalog_tests.cpp
            src/features/documents/document_catalog.cpp
            src/core/resource_packs/resource_pack_manager.cpp
            src/core/updater/version.cpp
        LIBRARIES
            Qt6::Core
            Qt6::Test
    )

    classmngr_add_qt_test(
        NAME FontManager
        SOURCES
            tests/fontmanager_tests.cpp
            src/core/fontmanager.cpp
        COMPILE_DEFINITIONS
            CLASSMNGR_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        LIBRARIES
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    classmngr_add_qt_test(
        NAME CheckboxStyle
        SOURCES
            tests/checkbox_style_tests.cpp
        LIBRARIES
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
        OFFSCREEN
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

    classmngr_add_qt_test(
        NAME FileDialogIconStyle
        SOURCES
            tests/file_dialog_icon_style_tests.cpp
            src/ui/shared/styles/file_dialog_icon_style.cpp
        LIBRARIES
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
        OFFSCREEN
    )

    qt_add_resources(ClassMngrFileDialogIconStyleTests file_dialog_icon_test_resources
        PREFIX "/"
        BASE "${PROJECT_SOURCE_DIR}/resources"
        FILES
            ${CLASSMNGR_FILE_DIALOG_ICONS}
    )

    classmngr_add_qt_test(
        NAME DialogServices
        SOURCES
            tests/dialog_services_tests.cpp
        LIBRARIES
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
        OFFSCREEN
    )

    qt_add_resources(ClassMngrDialogServicesTests dialog_service_test_resources
        PREFIX "/"
        BASE "${PROJECT_SOURCE_DIR}/resources"
        FILES
            ${CLASSMNGR_FILE_DIALOG_ICONS}
    )

    classmngr_add_qt_test(
        NAME ClassTimeRow
        SOURCES
            tests/class_time_row_tests.cpp
            src/features/classes/config/class_info_config.cpp
            src/ui/shared/widgets/no_wheel_combobox.cpp
            src/ui/shared/widgets/sectioncards/class_time_row.cpp
        LIBRARIES
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
        OFFSCREEN
    )
