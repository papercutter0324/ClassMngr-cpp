include_guard(GLOBAL)

if(WIN32 AND CLASSMNGR_RUN_WINDEPLOYQT)
    add_custom_command(TARGET ${CLASSMNGR_QT_DESKTOP_TARGET} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E env
            ${CLASSMNGR_WINDEPLOYQT_ENV}
            "$<TARGET_FILE:Qt6::windeployqt>"
            --qmldir "${PROJECT_SOURCE_DIR}/src/features/calendar/ui/qml"
            --skip-plugin-types qmltooling
            --include-plugins qoffscreen
            --exclude-plugins qsqlibase,qsqlmimer,qsqloci,qsqlodbc,qsqlpsql
            --translations en,ko
            ${CLASSMNGR_WINDEPLOYQT_TARGET_ARGS}
            --dir "$<TARGET_FILE_DIR:${CLASSMNGR_QT_DESKTOP_TARGET}>"
            "$<TARGET_FILE:${CLASSMNGR_QT_DESKTOP_TARGET}>"
        COMMENT "Deploying Qt runtime dependencies"
    )
endif()

install(TARGETS ${CLASSMNGR_QT_DESKTOP_TARGET}
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES
    licenses/fonts/inter/LICENSE.txt
    DESTINATION licenses/fonts/inter
)

if(APPLE)
    install(FILES
        ${CLASSMNGR_RESOURCE_PACK_FILES}
        DESTINATION "ClassMngr.app/Contents/Resources/resource-packs"
    )
else()
    set(CLASSMNGR_RESOURCE_PACK_INSTALL_DESTINATION
        "${CMAKE_INSTALL_BINDIR}/resources/resource-packs")
    cmake_path(NORMAL_PATH CLASSMNGR_RESOURCE_PACK_INSTALL_DESTINATION)
    install(FILES
        ${CLASSMNGR_RESOURCE_PACK_FILES}
        DESTINATION "${CLASSMNGR_RESOURCE_PACK_INSTALL_DESTINATION}"
    )
endif()

install(FILES
    licenses/fonts/pretendard/LICENSE.txt
    DESTINATION licenses/fonts/pretendard
)

install(FILES
    licenses/fonts/just-another-hand/LICENSE.txt
    DESTINATION licenses/fonts/just-another-hand
)

qt6_generate_deploy_script(
    TARGET ${CLASSMNGR_QT_DESKTOP_TARGET}
    OUTPUT_SCRIPT deploy_script
    CONTENT "
${CLASSMNGR_QT_DEPLOY_PREAMBLE}
qt_deploy_qml_imports(
    TARGET ${CLASSMNGR_QT_DESKTOP_TARGET}
    PLUGINS_FOUND plugins_found
)
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
