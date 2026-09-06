include_guard(GLOBAL)

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
                "$<TARGET_BUNDLE_DIR:${CLASSMNGR_QT_DESKTOP_TARGET}>/Contents/Frameworks"
                "$<TARGET_BUNDLE_DIR:${CLASSMNGR_QT_DESKTOP_TARGET}>/Contents/PlugIns"
                "$<TARGET_BUNDLE_DIR:${CLASSMNGR_QT_DESKTOP_TARGET}>/Contents/Resources/qml"
            COMMAND
                "${CMAKE_COMMAND}" -E rm -f
                "$<TARGET_BUNDLE_DIR:${CLASSMNGR_QT_DESKTOP_TARGET}>/Contents/Resources/qt.conf"
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
            DEPENDS ${CLASSMNGR_QT_DESKTOP_TARGET}
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
