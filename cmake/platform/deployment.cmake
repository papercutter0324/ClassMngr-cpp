include_guard(GLOBAL)

option(CLASSMNGR_RUN_WINDEPLOYQT "Run windeployqt after building ClassMngr on Windows." OFF)

set(CLASSMNGR_WINDOWS_ARM64 OFF)
if(WIN32 AND (CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64"
   OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$"))
    set(CLASSMNGR_WINDOWS_ARM64 ON)
endif()

set(CLASSMNGR_WINDEPLOYQT_ENV "")
if(WIN32 AND MSVC)
    set(CLASSMNGR_MSVC_INSTALL_DIR "")

    if(DEFINED ENV{VCINSTALLDIR} AND NOT "$ENV{VCINSTALLDIR}" STREQUAL "")
        file(TO_CMAKE_PATH "$ENV{VCINSTALLDIR}" CLASSMNGR_MSVC_INSTALL_DIR)
    elseif(CMAKE_CXX_COMPILER MATCHES "/VC/Tools/MSVC/.*/bin/")
        string(REGEX REPLACE "/VC/Tools/MSVC/.*$" "/VC/" CLASSMNGR_MSVC_INSTALL_DIR "${CMAKE_CXX_COMPILER}")
    endif()

    if(CLASSMNGR_MSVC_INSTALL_DIR)
        list(APPEND CLASSMNGR_WINDEPLOYQT_ENV "VCINSTALLDIR=${CLASSMNGR_MSVC_INSTALL_DIR}")
    endif()
endif()

set(CLASSMNGR_QT_DEPLOY_PREAMBLE "")
set(CLASSMNGR_QT_DEPLOY_OPTIONS "")
set(CLASSMNGR_QT_DEPLOY_POSTAMBLE "")
set(CLASSMNGR_QT_DEPLOY_EXECUTABLE "$<TARGET_FILE:ClassMngr>")
set(CLASSMNGR_QT_DEPLOY_TRANSLATION_OPTIONS "")
set(CLASSMNGR_WINDEPLOYQT_TARGET_ARGS "")

if(CLASSMNGR_WINDOWS_ARM64 AND QT_HOST_PATH)
    find_program(CLASSMNGR_QT_HOST_QTPATHS_EXECUTABLE
        NAMES qtpaths6.exe qtpaths.exe
        PATHS "${QT_HOST_PATH}/bin"
        NO_DEFAULT_PATH
        REQUIRED
    )
    find_program(CLASSMNGR_QT_HOST_LCONVERT_EXECUTABLE
        NAMES lconvert.exe
        PATHS "${QT_HOST_PATH}/bin"
        NO_DEFAULT_PATH
        REQUIRED
    )

    file(TO_CMAKE_PATH "${QT6_INSTALL_PREFIX}"
        CLASSMNGR_QT_TARGET_INSTALL_PREFIX)
    file(TO_CMAKE_PATH "${CLASSMNGR_QT_HOST_QTPATHS_EXECUTABLE}"
        CLASSMNGR_QT_HOST_QTPATHS_EXECUTABLE)
    set(CLASSMNGR_QT_TARGET_QT_CONF
        "${CMAKE_CURRENT_BINARY_DIR}/generated/ClassMngrTargetQt.conf")
    set(CLASSMNGR_QT_TARGET_QTPATHS_WRAPPER
        "${CMAKE_CURRENT_BINARY_DIR}/generated/ClassMngrTargetQtPaths.bat")
    configure_file(
        resources/windows/target-qt.conf.in
        "${CLASSMNGR_QT_TARGET_QT_CONF}"
        @ONLY
        NEWLINE_STYLE CRLF
    )
    configure_file(
        resources/windows/target-qtpaths.bat.in
        "${CLASSMNGR_QT_TARGET_QTPATHS_WRAPPER}"
        @ONLY
        NEWLINE_STYLE CRLF
    )

    list(APPEND CLASSMNGR_WINDEPLOYQT_TARGET_ARGS
        --qtpaths "${CLASSMNGR_QT_TARGET_QTPATHS_WRAPPER}")
    string(APPEND CLASSMNGR_QT_DEPLOY_PREAMBLE
        "set(__QT_DEPLOY_TARGET_QT_PATHS_PATH [[${CLASSMNGR_QT_TARGET_QTPATHS_WRAPPER}]])\n"
    )
    string(APPEND CLASSMNGR_QT_DEPLOY_TRANSLATION_OPTIONS
        "    LCONVERT [[${CLASSMNGR_QT_HOST_LCONVERT_EXECUTABLE}]]\n"
    )
endif()

if(WIN32 AND CLASSMNGR_MSVC_INSTALL_DIR)
    string(APPEND CLASSMNGR_QT_DEPLOY_PREAMBLE
        "set(ENV{VCINSTALLDIR} [[${CLASSMNGR_MSVC_INSTALL_DIR}]])\n"
    )
endif()

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

if(APPLE)
    set(CLASSMNGR_QT_DEPLOY_EXECUTABLE "ClassMngr.app")
    string(APPEND CLASSMNGR_QT_DEPLOY_PREAMBLE [=[
set(CLASSMNGR_BAD_SQL_DRIVERS
    libqsqlmimer.dylib
    libqsqlodbc.dylib
    libqsqlpsql.dylib
)
set(CLASSMNGR_STAGED_SQL_DRIVERS "")
set(CLASSMNGR_QT_SQL_DRIVER_DIR "${__QT_DEPLOY_QT_INSTALL_PREFIX}/${__QT_DEPLOY_QT_INSTALL_PLUGINS}/sqldrivers")
set(CLASSMNGR_SQL_DRIVER_STAGING_DIR "${CMAKE_CURRENT_LIST_DIR}/classmngr-deploy-sqldrivers")
file(MAKE_DIRECTORY "${CLASSMNGR_SQL_DRIVER_STAGING_DIR}")
foreach(driver IN LISTS CLASSMNGR_BAD_SQL_DRIVERS)
    if(EXISTS "${CLASSMNGR_SQL_DRIVER_STAGING_DIR}/${driver}" AND NOT EXISTS "${CLASSMNGR_QT_SQL_DRIVER_DIR}/${driver}")
        file(RENAME "${CLASSMNGR_SQL_DRIVER_STAGING_DIR}/${driver}" "${CLASSMNGR_QT_SQL_DRIVER_DIR}/${driver}")
    endif()
endforeach()
foreach(driver IN LISTS CLASSMNGR_BAD_SQL_DRIVERS)
    if(EXISTS "${CLASSMNGR_QT_SQL_DRIVER_DIR}/${driver}")
        file(RENAME "${CLASSMNGR_QT_SQL_DRIVER_DIR}/${driver}" "${CLASSMNGR_SQL_DRIVER_STAGING_DIR}/${driver}")
        list(APPEND CLASSMNGR_STAGED_SQL_DRIVERS "${driver}")
    endif()
endforeach()
set(CLASSMNGR_APP_SQL_DRIVER_DIR "${QT_DEPLOY_PREFIX}/ClassMngr.app/Contents/PlugIns/sqldrivers")
foreach(driver IN LISTS CLASSMNGR_BAD_SQL_DRIVERS)
    if(EXISTS "${CLASSMNGR_APP_SQL_DRIVER_DIR}/${driver}")
        file(REMOVE "${CLASSMNGR_APP_SQL_DRIVER_DIR}/${driver}")
    endif()
endforeach()
]=])
    string(APPEND CLASSMNGR_QT_DEPLOY_OPTIONS
        "    NO_APP_STORE_COMPLIANCE\n"
    )
    string(APPEND CLASSMNGR_QT_DEPLOY_POSTAMBLE [=[
foreach(driver IN LISTS CLASSMNGR_BAD_SQL_DRIVERS)
    if(EXISTS "${CLASSMNGR_APP_SQL_DRIVER_DIR}/${driver}")
        file(REMOVE "${CLASSMNGR_APP_SQL_DRIVER_DIR}/${driver}")
    endif()
endforeach()
foreach(driver IN LISTS CLASSMNGR_STAGED_SQL_DRIVERS)
    if(EXISTS "${CLASSMNGR_SQL_DRIVER_STAGING_DIR}/${driver}")
        file(RENAME "${CLASSMNGR_SQL_DRIVER_STAGING_DIR}/${driver}" "${CLASSMNGR_QT_SQL_DRIVER_DIR}/${driver}")
    endif()
endforeach()
file(REMOVE_RECURSE "${CLASSMNGR_SQL_DRIVER_STAGING_DIR}")
]=])
endif()

if(WIN32)
    set(CMAKE_INSTALL_BINDIR ".")
    string(APPEND CLASSMNGR_QT_DEPLOY_PREAMBLE
        "set(QT_DEPLOY_BIN_DIR .)\n"
    )
    string(APPEND CLASSMNGR_QT_DEPLOY_OPTIONS
        "    NO_TRANSLATIONS\n"
        "    INCLUDE_PLUGINS qoffscreen\n"
        "    EXCLUDE_PLUGIN_TYPES qmltooling\n"
        "    EXCLUDE_PLUGINS qsqlibase qsqlmimer qsqloci qsqlodbc qsqlpsql\n"
    )
    string(APPEND CLASSMNGR_QT_DEPLOY_POSTAMBLE
        "qt_deploy_translations(\n"
        "    LOCALES en ko\n"
        "${CLASSMNGR_QT_DEPLOY_TRANSLATION_OPTIONS}"
        ")\n"
    )
endif()
