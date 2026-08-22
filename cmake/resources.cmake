include_guard(GLOBAL)

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

# Feature-scoped content is built as standalone RCC files.  The generated
# qrc files remain in the build tree so adding an asset reconfigures CMake
# rather than creating tracked boilerplate.
set(CLASSMNGR_RESOURCE_PACK_OUTPUT_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/resource-packs"
)
set(CLASSMNGR_RESOURCE_PACK_FILES)

function(classmngr_add_scoped_resource_pack pack_id asset_directory)
    file(GLOB_RECURSE pack_files CONFIGURE_DEPENDS
        RELATIVE "${asset_directory}"
        "${asset_directory}/*"
    )

    set(qrc_content "<RCC version=\"1.0\">\n<qresource prefix=\"/resource-packs/${pack_id}\">\n")
    set(pack_source_files)
    foreach(relative_path IN LISTS pack_files)
        set(source_path "${asset_directory}/${relative_path}")
        if(IS_DIRECTORY "${source_path}")
            continue()
        endif()

        list(APPEND pack_source_files "${source_path}")
        file(TO_CMAKE_PATH "${source_path}" qrc_source_path)
        string(REPLACE "&" "&amp;" qrc_source_path "${qrc_source_path}")
        string(REPLACE "&" "&amp;" qrc_alias_path "${relative_path}")
        string(APPEND qrc_content
            "  <file alias=\"${qrc_alias_path}\">${qrc_source_path}</file>\n"
        )
    endforeach()
    string(APPEND qrc_content "</qresource>\n</RCC>\n")

    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/resource-packs")
    set(qrc_file "${CMAKE_CURRENT_BINARY_DIR}/generated/resource-packs/${pack_id}.qrc")
    file(WRITE "${qrc_file}" "${qrc_content}")

    set(pack_file "${CLASSMNGR_RESOURCE_PACK_OUTPUT_DIR}/${pack_id}.rcc")
    add_custom_command(
        OUTPUT "${pack_file}"
        COMMAND Qt6::rcc --binary --output "${pack_file}" "${qrc_file}"
        DEPENDS Qt6::rcc "${qrc_file}" ${pack_source_files}
        VERBATIM
    )
    add_custom_target("ClassMngr${pack_id}ResourcePack"
        DEPENDS "${pack_file}"
    )
    add_dependencies(ClassMngr "ClassMngr${pack_id}ResourcePack")
    list(APPEND CLASSMNGR_RESOURCE_PACK_FILES "${pack_file}")
    set(CLASSMNGR_RESOURCE_PACK_FILES
        "${CLASSMNGR_RESOURCE_PACK_FILES}"
        PARENT_SCOPE
    )
endfunction()

classmngr_add_scoped_resource_pack(
    campuses
    "${PROJECT_SOURCE_DIR}/resources/assets/campuses"
)
classmngr_add_scoped_resource_pack(
    documents
    "${PROJECT_SOURCE_DIR}/resources/assets/documents"
)
classmngr_add_scoped_resource_pack(
    files
    "${PROJECT_SOURCE_DIR}/resources/assets/files"
)
classmngr_add_scoped_resource_pack(
    images
    "${PROJECT_SOURCE_DIR}/resources/assets/images"
)
classmngr_add_scoped_resource_pack(
    splash
    "${PROJECT_SOURCE_DIR}/resources/assets/splash"
)

file(GLOB_RECURSE CLASSMNGR_SPEAKING_EVAL_TEMPLATE_FILES CONFIGURE_DEPENDS
    RELATIVE "${PROJECT_SOURCE_DIR}/resources/assets/templates"
    "${PROJECT_SOURCE_DIR}/resources/assets/templates/speaking-eval/*"
)
list(FILTER CLASSMNGR_SPEAKING_EVAL_TEMPLATE_FILES
    EXCLUDE REGEX "^speaking-eval/sources/"
)
set(CLASSMNGR_SPEAKING_EVAL_TEMPLATE_QRC_CONTENT
    "<RCC version=\"1.0\">\n<qresource prefix=\"/resource-packs/templates\">\n"
)
set(CLASSMNGR_TEMPLATE_PACK_SOURCE_FILES)
foreach(relative_path IN LISTS CLASSMNGR_SPEAKING_EVAL_TEMPLATE_FILES)
    set(source_path "${PROJECT_SOURCE_DIR}/resources/assets/templates/${relative_path}")
    if(IS_DIRECTORY "${source_path}")
        continue()
    endif()
    list(APPEND CLASSMNGR_TEMPLATE_PACK_SOURCE_FILES "${source_path}")
    file(TO_CMAKE_PATH "${source_path}" qrc_source_path)
    string(REPLACE "&" "&amp;" qrc_source_path "${qrc_source_path}")
    string(REPLACE "&" "&amp;" qrc_alias_path "${relative_path}")
    string(APPEND CLASSMNGR_SPEAKING_EVAL_TEMPLATE_QRC_CONTENT
        "  <file alias=\"${qrc_alias_path}\">${qrc_source_path}</file>\n"
    )
endforeach()
string(APPEND CLASSMNGR_SPEAKING_EVAL_TEMPLATE_QRC_CONTENT
    "</qresource>\n</RCC>\n"
)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/resource-packs")
set(CLASSMNGR_TEMPLATES_PACK_QRC
    "${CMAKE_CURRENT_BINARY_DIR}/generated/resource-packs/templates.qrc"
)
file(WRITE "${CLASSMNGR_TEMPLATES_PACK_QRC}"
    "${CLASSMNGR_SPEAKING_EVAL_TEMPLATE_QRC_CONTENT}"
)
set(CLASSMNGR_TEMPLATES_PACK_FILE
    "${CLASSMNGR_RESOURCE_PACK_OUTPUT_DIR}/templates.rcc"
)
add_custom_command(
    OUTPUT "${CLASSMNGR_TEMPLATES_PACK_FILE}"
    COMMAND Qt6::rcc --binary --output "${CLASSMNGR_TEMPLATES_PACK_FILE}"
        "${CLASSMNGR_TEMPLATES_PACK_QRC}"
    DEPENDS Qt6::rcc "${CLASSMNGR_TEMPLATES_PACK_QRC}"
        ${CLASSMNGR_TEMPLATE_PACK_SOURCE_FILES}
    VERBATIM
)
add_custom_target(ClassMngrtemplatesResourcePack
    DEPENDS "${CLASSMNGR_TEMPLATES_PACK_FILE}"
)
add_dependencies(ClassMngr ClassMngrtemplatesResourcePack)
list(APPEND CLASSMNGR_RESOURCE_PACK_FILES "${CLASSMNGR_TEMPLATES_PACK_FILE}")

list(
    FILTER CLASSMNGR_EMBEDDED_PACK_FILES
    EXCLUDE REGEX "/(campuses|documents|files)/"
)
list(
    FILTER CLASSMNGR_EMBEDDED_PACK_FILES
    EXCLUDE REGEX "/templates/speaking-eval/"
)

foreach(scoped_resource_directory IN ITEMS
        campuses
        documents
        files
        images
        splash
        templates/speaking-eval)
    foreach(embedded_resource_file IN LISTS CLASSMNGR_EMBEDDED_PACK_FILES)
        if(embedded_resource_file MATCHES
                "/assets/${scoped_resource_directory}/")
            message(FATAL_ERROR
                "Feature-scoped resource '${embedded_resource_file}' "
                "was added to the executable bundle."
            )
        endif()
    endforeach()
endforeach()

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
        resources/assets/styles/dark.qss
        resources/assets/styles/light.qss
        ${CLASSMNGR_EMBEDDED_PACK_FILES}
)

target_compile_definitions(ClassMngrBuildSettings
    INTERFACE
        CLASSMNGR_RESOURCE_PACK_DIR="${CLASSMNGR_RESOURCE_PACK_OUTPUT_DIR}"
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
