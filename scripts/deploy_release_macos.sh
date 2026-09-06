#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

QT_DIR="${QT_DIR:-${QT_MACOS_PREFIX:-${HOME}/Qt/6.12.0/macos}}"
APP_BUNDLE="${PROJECT_ROOT}/dist/ClassMngr-macos/ClassMngr.app"
QML_DIR="${QML_DIR:-${PROJECT_ROOT}/src/features/calendar/ui/qml}"

if [[ $# -gt 0 && "${1}" != -* ]]; then
    APP_BUNDLE="${1}"
    shift
fi

MACDEPLOYQT="${QT_DIR}/bin/macdeployqt"
SQL_DRIVER_DIR="${QT_DIR}/plugins/sqldrivers"

if [[ ! -x "${MACDEPLOYQT}" ]]; then
    echo "macdeployqt not found or not executable: ${MACDEPLOYQT}" >&2
    echo "Set QT_DIR or QT_MACOS_PREFIX to your Qt macOS installation and try again." >&2
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
VALIDATION_REPORT="${TEMP_DRIVER_DIR}/external-qt-dependencies.txt"
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

validate_no_external_qt_dependencies() {
    : > "${VALIDATION_REPORT}"

    while IFS= read -r -d '' candidate; do
        if ! otool -L "${candidate}" > "${TEMP_DRIVER_DIR}/otool-libraries.txt" 2>/dev/null; then
            continue
        fi

        otool -l "${candidate}" > "${TEMP_DRIVER_DIR}/otool-load-commands.txt" 2>/dev/null || true

        if grep -F "${QT_DIR}/" "${TEMP_DRIVER_DIR}/otool-libraries.txt" > /dev/null \
            || grep -F "path ${QT_DIR}/" "${TEMP_DRIVER_DIR}/otool-load-commands.txt" > /dev/null; then
            {
                echo "${candidate}"
                grep -F "${QT_DIR}/" "${TEMP_DRIVER_DIR}/otool-libraries.txt" || true
                grep -F "path ${QT_DIR}/" "${TEMP_DRIVER_DIR}/otool-load-commands.txt" || true
                echo
            } >> "${VALIDATION_REPORT}"
        fi
    done < <(
        find "${APP_BUNDLE}/Contents" \
            -type f \
            \( -perm -111 -o -name '*.dylib' -o -name '*.so' \) \
            -print0
    )

    if [[ -s "${VALIDATION_REPORT}" ]]; then
        echo "Deployment left references to the external Qt installation." >&2
        echo "This can make the app load two Qt runtimes and crash while creating the macOS platform plugin." >&2
        echo "External references:" >&2
        cat "${VALIDATION_REPORT}" >&2
        exit 1
    fi
}

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

validate_no_external_qt_dependencies

echo "Deployment complete: ${APP_BUNDLE}"
echo "Kept SQL driver: ${SQL_BUNDLE_DIR}/libqsqlite.dylib"
