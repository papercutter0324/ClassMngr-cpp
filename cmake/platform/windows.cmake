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
        add_custom_target(ClassMngrInstaller
            COMMAND
                "${CMAKE_COMMAND}" -E rm -rf
                "${CLASSMNGR_INSTALLER_SOURCE_DIR}"
            COMMAND
                "${CMAKE_COMMAND}"
                --install "${CMAKE_BINARY_DIR}"
                --config "$<CONFIG>"
                --prefix "${CLASSMNGR_INSTALLER_SOURCE_DIR}"
            COMMAND
                "${CMAKE_COMMAND}" -E env
                "CLASSMNGR_APP_VERSION=${PROJECT_VERSION}"
                "CLASSMNGR_INSTALLER_ARCH=${CLASSMNGR_INSTALLER_ARCH}"
                "CLASSMNGR_INSTALLER_SOURCE_DIR=${CLASSMNGR_INSTALLER_SOURCE_DIR}"
                "CLASSMNGR_INSTALLER_OUTPUT_DIR=${CLASSMNGR_INSTALLER_OUTPUT_DIR}"
                "CLASSMNGR_INSTALLER_SIGN_TOOL=${CLASSMNGR_INSTALLER_SIGN_TOOL}"
                "${CLASSMNGR_ISCC_EXECUTABLE}"
                "${CLASSMNGR_INSTALLER_SCRIPT}"
            DEPENDS ClassMngr
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
