include_guard(GLOBAL)

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
        resources/assets/icons/keyboard_dark.svg
        resources/assets/icons/keyboard_light.svg
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
