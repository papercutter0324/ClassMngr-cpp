#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

QT_DIR="${QT_DIR:-${HOME}/Qt/6.11.1/macos}"
APP_BUNDLE="${PROJECT_ROOT}/build/macos-clang-release/ClassMngr.app"
QML_DIR="${QML_DIR:-${PROJECT_ROOT}/src/features/my_info/ui/qml}"

if [[ $# -gt 0 && "${1}" != -* ]]; then
    APP_BUNDLE="${1}"
    shift
fi

MACDEPLOYQT="${QT_DIR}/bin/macdeployqt"
SQL_DRIVER_DIR="${QT_DIR}/plugins/sqldrivers"

if [[ ! -x "${MACDEPLOYQT}" ]]; then
    echo "macdeployqt not found or not executable: ${MACDEPLOYQT}" >&2
    echo "Set QT_DIR to your Qt macOS installation and try again." >&2
    exit 1
fi

if [[ ! -d "${APP_BUNDLE}" ]]; then
    echo "App bundle not found: ${APP_BUNDLE}" >&2
    exit 1
fi

if [[ ! -d "${SQL_DRIVER_DIR}" ]]; then
    echo "Qt SQL driver directory not found: ${SQL_DRIVER_DIR}" >&2
    exit 1
fi

if [[ ! -d "${QML_DIR}" ]]; then
    echo "QML source directory not found: ${QML_DIR}" >&2
    echo "Set QML_DIR to the directory macdeployqt should scan and try again." >&2
    exit 1
fi

TEMP_DRIVER_DIR="$(mktemp -d "${TMPDIR:-/tmp}/classmngr-sqldrivers.XXXXXX")"
RESTORE_NEEDED=()
BAD_SQL_DRIVERS=(
    libqsqlmimer.dylib
    libqsqlodbc.dylib
    libqsqlpsql.dylib
)

restore_sql_drivers() {
    for driver in "${RESTORE_NEEDED[@]}"; do
        if [[ -e "${TEMP_DRIVER_DIR}/${driver}" ]]; then
            mv "${TEMP_DRIVER_DIR}/${driver}" "${SQL_DRIVER_DIR}/${driver}"
        fi
    done
    rm -rf "${TEMP_DRIVER_DIR}"
}

trap restore_sql_drivers EXIT

for driver in "${BAD_SQL_DRIVERS[@]}"; do
    if [[ -e "${SQL_DRIVER_DIR}/${driver}" ]]; then
        mv "${SQL_DRIVER_DIR}/${driver}" "${TEMP_DRIVER_DIR}/${driver}"
        RESTORE_NEEDED+=("${driver}")
    fi
done

SQL_BUNDLE_DIR="${APP_BUNDLE}/Contents/PlugIns/sqldrivers"

for driver in "${BAD_SQL_DRIVERS[@]}"; do
    if [[ -e "${SQL_BUNDLE_DIR}/${driver}" ]]; then
        mv "${SQL_BUNDLE_DIR}/${driver}" "${TEMP_DRIVER_DIR}/bundled-before-${driver}"
    fi
done

"${MACDEPLOYQT}" "${APP_BUNDLE}" -always-overwrite -qmldir="${QML_DIR}" "$@"

mkdir -p "${SQL_BUNDLE_DIR}"

for driver in "${BAD_SQL_DRIVERS[@]}"; do
    if [[ -e "${SQL_BUNDLE_DIR}/${driver}" ]]; then
        mv "${SQL_BUNDLE_DIR}/${driver}" "${TEMP_DRIVER_DIR}/bundled-${driver}"
    fi
done

if [[ ! -e "${SQL_BUNDLE_DIR}/libqsqlite.dylib" ]]; then
    echo "SQLite SQL driver was not deployed: ${SQL_BUNDLE_DIR}/libqsqlite.dylib" >&2
    exit 1
fi

echo "Deployment complete: ${APP_BUNDLE}"
echo "Kept SQL driver: ${SQL_BUNDLE_DIR}/libqsqlite.dylib"
