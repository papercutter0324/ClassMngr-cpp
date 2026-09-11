include_guard(GLOBAL)

# Target-neutral resource inputs.  Qt consumers turn these into RCC/QML/TS
# outputs; the native Windows consumer turns the same inputs into a manifest.
# Keep this file data-only so a native-only configure never needs Qt tooling.
set(CLASSMNGR_RESOURCE_CATALOG_SCOPED_ROOTS
    "${PROJECT_SOURCE_DIR}/resources/assets/campuses"
    "${PROJECT_SOURCE_DIR}/resources/assets/documents"
    "${PROJECT_SOURCE_DIR}/resources/assets/files"
    "${PROJECT_SOURCE_DIR}/resources/assets/images"
    "${PROJECT_SOURCE_DIR}/resources/assets/splash"
    "${PROJECT_SOURCE_DIR}/resources/assets/templates"
)

set(CLASSMNGR_RESOURCE_CATALOG_EMBEDDED_ROOTS
    "${PROJECT_SOURCE_DIR}/resources/assets/campuses"
    "${PROJECT_SOURCE_DIR}/resources/assets/documents"
    "${PROJECT_SOURCE_DIR}/resources/assets/files"
    "${PROJECT_SOURCE_DIR}/resources/assets/templates"
    "${PROJECT_SOURCE_DIR}/resources/assets/roster-designs"
)

if(CMAKE_SCRIPT_MODE_FILE)
    file(GLOB CLASSMNGR_RESOURCE_CATALOG_FILE_DIALOG_ICONS
        "${PROJECT_SOURCE_DIR}/resources/assets/icons/file_dialog/*.svg"
    )
else()
    file(GLOB CLASSMNGR_RESOURCE_CATALOG_FILE_DIALOG_ICONS CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/resources/assets/icons/file_dialog/*.svg"
    )
endif()

set(CLASSMNGR_RESOURCE_CATALOG_FIXED_FILES
    "${PROJECT_SOURCE_DIR}/resources/assets/fonts/Inter.ttc"
    "${PROJECT_SOURCE_DIR}/resources/assets/fonts/JustAnotherHand-Regular.ttf"
    "${PROJECT_SOURCE_DIR}/resources/assets/fonts/PretendardVariable.ttf"
    "${PROJECT_SOURCE_DIR}/resources/assets/fonts/DancingScript-wght.ttf"
    "${PROJECT_SOURCE_DIR}/resources/assets/fonts/GreatVibes-Regular.ttf"
    "${PROJECT_SOURCE_DIR}/resources/assets/fonts/Caveat-wght.ttf"
    "${PROJECT_SOURCE_DIR}/resources/assets/icons/app_icon.ico"
    "${PROJECT_SOURCE_DIR}/resources/assets/icons/check.png"
    "${PROJECT_SOURCE_DIR}/resources/assets/icons/keyboard_dark.svg"
    "${PROJECT_SOURCE_DIR}/resources/assets/icons/keyboard_light.svg"
    "${PROJECT_SOURCE_DIR}/resources/assets/icons/combo_arrow_dark.svg"
    "${PROJECT_SOURCE_DIR}/resources/assets/icons/combo_arrow_light.svg"
    "${PROJECT_SOURCE_DIR}/resources/assets/icons/spin_up_dark.svg"
    "${PROJECT_SOURCE_DIR}/resources/assets/icons/spin_up_light.svg"
    "${PROJECT_SOURCE_DIR}/resources/assets/icons/radio_checked.png"
    "${PROJECT_SOURCE_DIR}/resources/assets/icons/icon_256x256.png"
    "${PROJECT_SOURCE_DIR}/resources/assets/styles/dark.qss"
    "${PROJECT_SOURCE_DIR}/resources/assets/styles/light.qss"
)

set(CLASSMNGR_RESOURCE_CATALOG_QML_FILES
    "${PROJECT_SOURCE_DIR}/src/features/calendar/ui/qml/EventCalendar.qml"
    "${PROJECT_SOURCE_DIR}/src/features/calendar/ui/qml/MonthGridDelegate.qml"
)

set(CLASSMNGR_RESOURCE_CATALOG_TRANSLATION_FILES
    "${PROJECT_SOURCE_DIR}/resources/assets/translations/ClassMngr_en_AU.ts"
    "${PROJECT_SOURCE_DIR}/resources/assets/translations/ClassMngr_en_CA.ts"
    "${PROJECT_SOURCE_DIR}/resources/assets/translations/ClassMngr_en_GB.ts"
    "${PROJECT_SOURCE_DIR}/resources/assets/translations/ClassMngr_en_US.ts"
    "${PROJECT_SOURCE_DIR}/resources/assets/translations/ClassMngr_ko_KR.ts"
)

function(classmngr_collect_resource_catalog_files output_variable)
    set(classmngr_catalog_files)
    foreach(classmngr_catalog_root IN LISTS
            CLASSMNGR_RESOURCE_CATALOG_SCOPED_ROOTS)
        if(CMAKE_SCRIPT_MODE_FILE)
            file(GLOB_RECURSE classmngr_catalog_root_files
                LIST_DIRECTORIES FALSE
                "${classmngr_catalog_root}/*"
            )
        else()
            file(GLOB_RECURSE classmngr_catalog_root_files CONFIGURE_DEPENDS
                LIST_DIRECTORIES FALSE
                "${classmngr_catalog_root}/*"
            )
        endif()
        list(APPEND classmngr_catalog_files
            ${classmngr_catalog_root_files}
        )
    endforeach()

    list(APPEND classmngr_catalog_files
        ${CLASSMNGR_RESOURCE_CATALOG_FIXED_FILES}
        ${CLASSMNGR_RESOURCE_CATALOG_FILE_DIALOG_ICONS}
        ${CLASSMNGR_RESOURCE_CATALOG_QML_FILES}
        ${CLASSMNGR_RESOURCE_CATALOG_TRANSLATION_FILES}
    )
    # These are authoring inputs or editor leftovers, not shipped resource
    # pack entries.  Keep the neutral catalog aligned with the retained Qt
    # pack filters so future native pack readers consume the same inventory.
    set(classmngr_filtered_catalog_files)
    foreach(classmngr_catalog_file IN LISTS classmngr_catalog_files)
        file(TO_CMAKE_PATH
            "${classmngr_catalog_file}"
            classmngr_catalog_file_normalized
        )
        get_filename_component(
            classmngr_catalog_file_name
            "${classmngr_catalog_file_normalized}"
            NAME
        )
        if(classmngr_catalog_file_name MATCHES "^~\\$"
           OR classmngr_catalog_file_normalized MATCHES
                "/templates/speaking-eval/sources/")
            continue()
        endif()
        list(APPEND classmngr_filtered_catalog_files
            "${classmngr_catalog_file}"
        )
    endforeach()
    set(classmngr_catalog_files "${classmngr_filtered_catalog_files}")
    list(REMOVE_DUPLICATES classmngr_catalog_files)
    list(SORT classmngr_catalog_files)
    set(${output_variable} "${classmngr_catalog_files}" PARENT_SCOPE)
endfunction()
