include_guard(GLOBAL)

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
    set(CLASSMNGR_INSTALLER_VC_REDIST_FILE "" CACHE FILEPATH
        "Architecture-matching VS 2022 v143 redistributable staged outside the app package"
    )

    if(CLASSMNGR_NEXT_RELEASE_PACKAGE)
        set(_classmngr_windows_target_dependencies)
        set(_classmngr_windows_verification_commands
            COMMAND "${Python3_EXECUTABLE}"
                "${PROJECT_SOURCE_DIR}/scripts/verify_next_package.py"
                create
                --package-root "${CLASSMNGR_INSTALLER_SOURCE_DIR}"
                --platform windows
                --architecture "${CLASSMNGR_INSTALLER_ARCH}"
                --version "${PROJECT_VERSION}"
                --qt-version "${Qt6_VERSION}"
            COMMAND "${Python3_EXECUTABLE}"
                "${PROJECT_SOURCE_DIR}/scripts/verify_next_package.py"
                verify
                --package-root "${CLASSMNGR_INSTALLER_SOURCE_DIR}"
                --platform windows
                --architecture "${CLASSMNGR_INSTALLER_ARCH}"
                --version "${PROJECT_VERSION}"
                --qt-version "${Qt6_VERSION}"
                --report "${CMAKE_BINARY_DIR}/reports/windows-${CLASSMNGR_INSTALLER_ARCH}-next-package.json"
        )
    else()
        set(_classmngr_windows_target_dependencies ClassMngr)
        set(_classmngr_windows_verification_commands)
    endif()

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
        set(_classmngr_expected_vc_redist_file
            "vc_redist.${CLASSMNGR_INSTALLER_ARCH}.exe"
        )
        if(CLASSMNGR_INSTALLER_VC_REDIST_FILE)
            get_filename_component(_classmngr_vc_redist_filename
                "${CLASSMNGR_INSTALLER_VC_REDIST_FILE}" NAME
            )
            if(NOT EXISTS "${CLASSMNGR_INSTALLER_VC_REDIST_FILE}"
                OR IS_DIRECTORY "${CLASSMNGR_INSTALLER_VC_REDIST_FILE}")
                message(FATAL_ERROR
                    "Set CLASSMNGR_INSTALLER_VC_REDIST_FILE to the staged, signed "
                    "${_classmngr_expected_vc_redist_file} from the VS 2022 v143 redist tree."
                )
            endif()
            if(NOT _classmngr_vc_redist_filename STREQUAL _classmngr_expected_vc_redist_file)
                message(FATAL_ERROR
                    "CLASSMNGR_INSTALLER_VC_REDIST_FILE must name "
                    "${_classmngr_expected_vc_redist_file}; found ${_classmngr_vc_redist_filename}."
                )
            endif()
        elseif(CLASSMNGR_NEXT_RELEASE_PACKAGE)
            message(FATAL_ERROR
                "Set CLASSMNGR_INSTALLER_VC_REDIST_FILE to the staged, signed "
                "${_classmngr_expected_vc_redist_file} from the VS 2022 v143 redist tree."
            )
        endif()

        add_custom_target(ClassMngrInstaller
            COMMAND
                "${CMAKE_COMMAND}" -E rm -rf
                "${CLASSMNGR_INSTALLER_SOURCE_DIR}"
            COMMAND
                "${CMAKE_COMMAND}"
                --install "${CMAKE_BINARY_DIR}"
                --config "$<CONFIG>"
                --prefix "${CLASSMNGR_INSTALLER_SOURCE_DIR}"
            ${_classmngr_windows_verification_commands}
            COMMAND
                "${CMAKE_COMMAND}" -E env
                "CLASSMNGR_APP_VERSION=${PROJECT_VERSION}"
                "CLASSMNGR_INSTALLER_ARCH=${CLASSMNGR_INSTALLER_ARCH}"
                "CLASSMNGR_INSTALLER_SOURCE_DIR=${CLASSMNGR_INSTALLER_SOURCE_DIR}"
                "CLASSMNGR_INSTALLER_VC_REDIST_PATH=${CLASSMNGR_INSTALLER_VC_REDIST_FILE}"
                "CLASSMNGR_INSTALLER_OUTPUT_DIR=${CLASSMNGR_INSTALLER_OUTPUT_DIR}"
                "CLASSMNGR_INSTALLER_SIGN_TOOL=${CLASSMNGR_INSTALLER_SIGN_TOOL}"
                "${CLASSMNGR_ISCC_EXECUTABLE}"
                "${CLASSMNGR_INSTALLER_SCRIPT}"
            DEPENDS ${_classmngr_windows_target_dependencies}
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
